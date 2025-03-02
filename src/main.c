#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "pico/time.h"

// External functions for prediction logic (assumed to be provided elsewhere)
extern void setLogFunction(void (*log_function)(uint8_t *string));
extern void predict(float *input, uint32_t *input_shape, uint32_t shape_len, float **result);
extern void setLogFunctionC(void (*log_function)(uint8_t *string));

// Logging function for the neural network
static void log_fn(uint8_t *string)
{
    printf("%s\n", string);
}

// Custom error handling function (replaces original 'panic')
void my_panic(const char *message)
{
    printf("PANIC: %s\n", message);
    while (1)
    {
    } // Halts execution
}

#define SPI_PORT spi0

// SPI slave pins
static const uint CS_PIN = 17;   // SPI0 CSn
static const uint SCK_PIN = 18;  // SPI0 SCK
static const uint MOSI_PIN = 19; // SPI0 MOSI
static const uint MISO_PIN = 16; // SPI0 MISO

// LED pin
static const uint LED_PIN = PICO_DEFAULT_LED_PIN; // GPIO 25 on Pico

// Buffer for SPI data reception
#define BUFFER_SIZE 2048
int16_t rx_buffer[BUFFER_SIZE]; // Signed 16-bit data
volatile uint32_t rx_count = 0;

// Buffer to accumulate 8000 samples
#define AUDIO_BUFFER_SIZE 8000
int16_t audio_buffer[AUDIO_BUFFER_SIZE];
uint32_t audio_idx = 0;

// DMA channel for SPI reception
int dma_rx;

// DMA interrupt handler
void dma_handler()
{
    if (dma_hw->ints0 & (1 << dma_rx))
    {
        dma_hw->ints0 = (1 << dma_rx); // Clear the interrupt
        rx_count += BUFFER_SIZE;

        // Figure out how much space is left in audio_buffer
        int space_left = AUDIO_BUFFER_SIZE - audio_idx;
        int samples_to_copy = (space_left < BUFFER_SIZE) ? space_left : BUFFER_SIZE;

        // Copy only what fits
        for (int i = 0; i < samples_to_copy; i++)
        {
            audio_buffer[audio_idx++] = rx_buffer[i];
        }

        // If the buffer’s full, stop and wait
        if (audio_idx >= AUDIO_BUFFER_SIZE)
        {
            printf("Audio buffer full, waiting for prediction\n");
        }
        else
        {
            // Restart DMA if there’s still space
            dma_channel_set_write_addr(dma_rx, rx_buffer, false);
            dma_channel_set_trans_count(dma_rx, BUFFER_SIZE, true);
        }
    }
}

int main()
{
    stdio_init_all();
    printf("Initializing SPI slave and prediction\n");

    // Initialize LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // LED off at startup

    // Configure SPI pins
    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(CS_PIN, GPIO_FUNC_SPI);

    // Initialize SPI as slave
    spi_init(SPI_PORT, 10000000); // 10 MHz, must match master
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Configure DMA channel for reception
    dma_rx = dma_claim_unused_channel(true);
    if (dma_rx < 0)
    {
        my_panic("Failed to allocate DMA channel - no channel available");
    }
    dma_channel_config c = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, false));
    channel_config_set_read_increment(&c, false); // Fixed source (SPI DR)
    channel_config_set_write_increment(&c, true); // Incrementing destination
    dma_channel_configure(dma_rx, &c,
                          rx_buffer,                 // Destination
                          &spi_get_hw(SPI_PORT)->dr, // Source (SPI data register)
                          BUFFER_SIZE,               // Transfer count
                          false                      // Don’t start yet
    );

    // Set up DMA interrupt handler
    dma_channel_set_irq0_enabled(dma_rx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Start DMA
    dma_channel_start(dma_rx);

    // Set logging functions for prediction
    setLogFunction(log_fn);
    setLogFunctionC(log_fn);

    // Input shape for neural network
    uint32_t input_shape[] = {1, 1, 8000};

    // Buffer for prediction results (9 classes)
    float result_buffer[9];
    float *result = result_buffer;

    while (true)
    {
        if (audio_idx >= AUDIO_BUFFER_SIZE)
        {
            // Turn on LED during prediction
            gpio_put(LED_PIN, 1);

            // Convert data to float and normalize
            float input_data[AUDIO_BUFFER_SIZE];
            for (int i = 0; i < AUDIO_BUFFER_SIZE; i++)
            {
                // Normalize from int16_t (-32768 to 32767) to float (-1 to 1)
                input_data[i] = (float)audio_buffer[i] / 32768.0f;
            }

            // Print a subset of input data for debugging (first 10 values)
            printf("Input data (first 10 samples): ");
            for (int i = 0; i < 10; i++)
            {
                printf("%f ", input_data[i]);
            }
            printf("...\n");

            // Perform prediction
            absolute_time_t start_time = get_absolute_time();
            predict(input_data, input_shape, 3, &result);
            absolute_time_t end_time = get_absolute_time();
            int64_t diff_us = absolute_time_diff_us(start_time, end_time);

            // Print results
            printf("\n=== Prediction completed ===\n");
            printf("Prediction time: %lld microseconds\n", diff_us);
            printf("Class probabilities:\n");
            float max_prob = result[0];
            int predicted_class = 0;
            for (int i = 0; i < 9; i++)
            {
                printf("Class %d: %.6f\n", i, result[i]);
                if (result[i] > max_prob)
                {
                    max_prob = result[i];
                    predicted_class = i;
                }
            }
            printf("Predicted class: %d with confidence %.2f%%\n", predicted_class, max_prob * 100);

            // Turn off LED after prediction
            gpio_put(LED_PIN, 0);

            // Reset audio buffer index
            audio_idx = 0;

            // Restart DMA to receive the next batch of data
            dma_channel_set_write_addr(dma_rx, rx_buffer, false);
            dma_channel_set_trans_count(dma_rx, BUFFER_SIZE, true);
        }

        // Optional debug output (can be removed)
        sleep_ms(100);
        // printf("Total samples received: %u, Audio index: %u\n", rx_count, audio_idx);
    }

    return 0;
}