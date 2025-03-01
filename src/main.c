#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "pico/time.h"

// Includi la logica di predizione (assumiamo funzioni esterne)
extern void setLogFunction(void (*log_function)(uint8_t *string));
extern void predict(float *input, uint32_t *input_shape, uint32_t shape_len, float **result);

// Funzione di logging per la rete neurale
static void log_fn(uint8_t *string)
{
    printf("%s\n", string);
}

#define SPI_PORT spi0

// Pin per SPI slave
static const uint CS_PIN = 17;   // SPI0 CSn
static const uint SCK_PIN = 18;  // SPI0 SCK
static const uint MOSI_PIN = 19; // SPI0 MOSI
static const uint MISO_PIN = 16; // SPI0 MISO

// Pin per il LED
static const uint LED_PIN = PICO_DEFAULT_LED_PIN; // GPIO 25 sul Pico

// Buffer per la ricezione dei dati SPI
#define BUFFER_SIZE 2048
int16_t rx_buffer[BUFFER_SIZE]; // Modificato a int16_t per dati signed
volatile uint32_t rx_count = 0;

// Buffer per accumulare 8000 campioni
#define AUDIO_BUFFER_SIZE 8000
int16_t audio_buffer[AUDIO_BUFFER_SIZE];
uint32_t audio_idx = 0;

// DMA channel per la ricezione SPI
int dma_rx;

// Funzione di callback per il DMA
void dma_handler()
{
    if (dma_hw->ints0 & (1 << dma_rx))
    {
        dma_hw->ints0 = (1 << dma_rx); // Pulisci l’interrupt
        rx_count += BUFFER_SIZE;

        // Copia i dati ricevuti nel buffer audio
        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            if (audio_idx < AUDIO_BUFFER_SIZE)
            {
                audio_buffer[audio_idx++] = rx_buffer[i];
            }
        }

        // Riavvia il DMA
        dma_channel_set_write_addr(dma_rx, rx_buffer, false);
        dma_channel_set_trans_count(dma_rx, BUFFER_SIZE, true);
    }
}

int main()
{
    stdio_init_all();
    printf("Inizializzazione SPI slave e predizione\n");

    // Inizializza il LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0); // LED spento all’avvio

    // Configura i pin SPI
    gpio_set_function(SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(CS_PIN, GPIO_FUNC_SPI);

    // Inizializza SPI come slave
    spi_init(SPI_PORT, 10000000); // 10 MHz, deve corrispondere al master
    spi_set_slave(SPI_PORT, true);
    spi_set_format(SPI_PORT, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Configura il canale DMA per la ricezione
    dma_rx = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(SPI_PORT, false));
    channel_config_set_read_increment(&c, false); // Sorgente fissa (SPI DR)
    channel_config_set_write_increment(&c, true); // Destinazione incrementa
    dma_channel_configure(dma_rx, &c,
                          rx_buffer,                 // Destinazione
                          &spi_get_hw(SPI_PORT)->dr, // Sorgente (registro dati SPI)
                          BUFFER_SIZE,               // Numero di trasferimenti
                          false                      // Non avviare subito
    );

    // Imposta l’handler per l’interrupt del DMA
    dma_channel_set_irq0_enabled(dma_rx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Avvia il DMA
    dma_channel_start(dma_rx);

    // Imposta la funzione di logging per la predizione
    setLogFunction(log_fn);

    // Forma dell’input per la rete neurale
    uint32_t input_shape[] = {1, 1, 8000};

    // Buffer per i risultati della predizione (9 classi come in Zig)
    float result_buffer[9];
    float *result = result_buffer;

    while (true)
    {
        // Attendi finché non hai accumulato 8000 campioni
        if (audio_idx >= AUDIO_BUFFER_SIZE)
        {
            // Accendi il LED durante la predizione
            gpio_put(LED_PIN, 1);

            // Converti i dati in float e normalizzali
            float input_data[AUDIO_BUFFER_SIZE];
            for (int i = 0; i < AUDIO_BUFFER_SIZE; i++)
            {
                // Normalizza da int16_t (-32768 a 32767) a float (-1 a 1)
                input_data[i] = (float)audio_buffer[i];
            }

            // Esegui la predizione
            absolute_time_t start_time = get_absolute_time();
            predict(input_data, input_shape, 3, &result);
            absolute_time_t end_time = get_absolute_time();
            int64_t diff_us = absolute_time_diff_us(start_time, end_time);

            // Stampa i risultati
            printf("\n=== Predizione completata ===\n");
            printf("Tempo di predizione: %lld microsecondi\n", diff_us);
            printf("Probabilità per classe:\n");
            float max_prob = result[0];
            int predicted_class = 0;
            for (int i = 0; i < 9; i++)
            { // 9 classi come in Zig
                printf("Classe %d: %.6f\n", i, result[i]);
                if (result[i] > max_prob)
                {
                    max_prob = result[i];
                    predicted_class = i;
                }
            }
            printf("Classe predetta: %d con confidenza %.2f%%\n", predicted_class, max_prob * 100);

            // Spegni il LED dopo la predizione
            gpio_put(LED_PIN, 0);

            // Resetta l’indice del buffer audio
            audio_idx = 0;
        }

        // Debug opzionale (rimuovibile)
        sleep_ms(100);
        printf("Campioni ricevuti totali: %u, Indice audio: %u\n", rx_count, audio_idx);
    }

    return 0;
}