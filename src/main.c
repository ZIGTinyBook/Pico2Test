#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/time.h"

#define SPI_PORT spi0

// Default pins for SPI0 on the Pico when using it in slave mode:
static const uint CS_PIN   = 17; // SPI0 CSn
static const uint SCK_PIN  = 18; // SPI0 SCK
static const uint MOSI_PIN = 19; // SPI0 MOSI
static const uint MISO_PIN = 16; // SPI0 MISO

int main() {
    // Initialize I/O for USB serial
    stdio_init_all();
    // Give time to connect USB
    sleep_ms(5000);
    
    printf("\n=== Raspberry Pi Pico SPI Slave Demo ===\n");
    printf("Waiting for SPI master to send 16-bit samples...\n");
    printf("Connect SCK->GPIO18, MOSI->GPIO19, MISO->GPIO16, CS->GPIO17, GND->GND.\n");

    // =============== SPI Slave Configuration ===============
    // Initialize SPI0 at 0 Hz (the master will control the clock speed).
    spi_init(SPI_PORT, 0);

    // Set the pins to the SPI function
    gpio_set_function(MISO_PIN, GPIO_FUNC_SPI);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(CS_PIN,   GPIO_FUNC_SPI);

    // Put SPI into slave mode
    spi_set_slave(SPI_PORT, true);

    // We'll use 8 bits per transfer, mode 0, MSB first
    // The master will clock out data in the same format
    spi_set_format(
        SPI_PORT,
        8,          // 8 bits
        SPI_CPOL_0, // Clock Polarity
        SPI_CPHA_0, // Clock Phase
        SPI_MSB_FIRST
    );

    printf("SPI0 configured in SLAVE mode.\n");
    printf("Now reading incoming data...\n");

    // Main loop: read 2 bytes at a time (16 bits) from the FIFO
    while (true) {
        // We want to read a 16-bit sample, which arrives as 2 bytes
        // We'll wait until at least 2 bytes are available in the hardware FIFO
        if (spi_is_readable(SPI_PORT) && (spi_get_hw(SPI_PORT)->sr & SPI_SSPSR_RNE_BITS)) {
            // First byte (high or low depends on how master sends it)
            uint8_t byte1 = (uint8_t)spi_get_hw(SPI_PORT)->dr;
            
            // Wait for second byte
            while (!(spi_is_readable(SPI_PORT))) {
                tight_loop_contents(); // spin until next byte
            }
            uint8_t byte2 = (uint8_t)spi_get_hw(SPI_PORT)->dr;

            // Reconstruct the 16-bit sample. 
            // If your Master sends MSB first, combine them as follows:
            int16_t sample = (int16_t)((byte1 << 8) | (byte2 & 0xFF));

            // Print out the sample
            printf("Received sample: 0x%02x\n", sample);
        }
        else {
            // No data right now, do something else or just sleep
            sleep_ms(1);
        }
    }

    return 0;
}
