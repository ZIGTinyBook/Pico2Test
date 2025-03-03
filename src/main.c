#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <stdlib.h>

#define DATA_SIZE 784

extern void setLogFunction(void (*log_function)(uint8_t *string));
extern void predict(float *input, uint32_t *input_shape, uint32_t shape_len, float **result);

static void log_fn(uint8_t *string)
{
    printf("%s\n", string);
}

int main()
{
    stdio_init_all();
    sleep_ms(1000); // Wait for USB CDC

    printf("\nMNIST Prediction Demo 1.0\n");

    // Set up logging
    setLogFunction(log_fn);

    // Initialize LED
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Create a mock MNIST input (28x28 grayscale image)
    float input_data[DATA_SIZE];
    for(int i = 0; i < DATA_SIZE; i++)
    {
        input_data[i] = 0.0f;
    }

    // Draw a "0" pattern
    // Top and bottom horizontal lines
    // for(int x = 8; x < 20; x++) {
    //     input_data[8 * 28 + x] = 100.0f;  // Top line
    //     input_data[20 * 28 + x] = 100.0f; // Bottom line
    // }

    // // Left and right vertical lines
    // for(int y = 8; y < 21; y++) {
    //     input_data[y * 28 + 8] = 100.0f;  // Left line
    //     input_data[y * 28 + 19] = 100.0f; // Right line
    // }

    // Set up input shape (NCHW format)
    uint32_t input_shape[] = {1, 1, 28, 28};
    float *result;
    while (true)
    {
        
        gpio_put(LED_PIN, 1);
        printf("\n\n=== Let me guess your handwritten digit ===\n");

        // Wait for input
        printf("Let me cook your input...\n");

        int scanned_count = 0;
        while (scanned_count < DATA_SIZE)
        {
            int scanned = scanf("%f", &input_data[scanned_count]);
            if (scanned == 1)
            {
                scanned_count++;
            }
        }
        

        // Print the input image
        printf("\nInput image (28x28):\n");
        for (int i = 0; i < 28; i++)
        {
            for (int j = 0; j < 28; j++)
            {
                printf("%c", input_data[i * 28 + j] > 0.5f ? '#' : '.');
            }
            printf("\n");
        }

        // Measure prediction time
        absolute_time_t start_time = get_absolute_time();
        predict(input_data, input_shape, 4, &result);
        absolute_time_t end_time = get_absolute_time();
        int64_t diff_us = absolute_time_diff_us(start_time, end_time);

        float max_prob = result[0];
        int predicted_digit = 0;

        for (int i = 0; i < 10; i++)
        {
            if (result[i] > max_prob)
            {
                max_prob = result[i];
                predicted_digit = i;
            }
        }

        // Print timing and predictions
        printf("\n=== Stats ===\n");
        printf("\nPrediction took %lld microseconds\n", diff_us);
        printf("\n=== Prediction ===\n");
        printf("Mhhhh let me guess....\nthat's a \n\n %d \n\n right?.\n", predicted_digit);
        printf("Nah no guessing is needed I know for sure this is the right answer.\n");
        printf("I'm always right.\n");

        gpio_put(LED_PIN, 0);
    }

    return 0;
}
