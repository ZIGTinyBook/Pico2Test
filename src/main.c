#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "pico/time.h"

#define SPI_PORT spi0

// Pin per SPI slave (verifica con il tuo setup)
static const uint CS_PIN = 17;   // SPI0 CSn
static const uint SCK_PIN = 18;  // SPI0 SCK
static const uint MOSI_PIN = 19; // SPI0 MOSI
static const uint MISO_PIN = 16; // SPI0 MISO

// Buffer per la ricezione dei dati
#define BUFFER_SIZE 2048
uint16_t rx_buffer[BUFFER_SIZE];
volatile uint32_t rx_count = 0;

// DMA channel per la ricezione SPI
int dma_rx;

// Funzione di callback per il DMA
void dma_handler()
{
    if (dma_hw->ints0 & (1 << dma_rx))
    {
        dma_hw->ints0 = (1 << dma_rx); // Pulisci l'interrupt
        rx_count += BUFFER_SIZE;
        printf("DMA completato, rx_count: %u\n", rx_count); // Debug
        // Riavvia il DMA con la destinazione al buffer
        dma_channel_set_write_addr(dma_rx, rx_buffer, false);
        dma_channel_set_trans_count(dma_rx, BUFFER_SIZE, true);
    }
}

int main()
{
    stdio_init_all();
    printf("Inizializzazione SPI slave\n");

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
    channel_config_set_write_increment(&c, true); // Destinazione incrementa (buffer)
    dma_channel_configure(dma_rx, &c,
                          rx_buffer,                 // Destinazione
                          &spi_get_hw(SPI_PORT)->dr, // Sorgente (registro dati SPI)
                          BUFFER_SIZE,               // Numero di trasferimenti
                          false                      // Non avviare subito
    );

    // Imposta l'handler per l'interrupt del DMA
    dma_channel_set_irq0_enabled(dma_rx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Avvia il DMA
    dma_channel_start(dma_rx);

    // Loop principale
    while (true)
    {
        sleep_ms(1000);
        printf("Campioni ricevuti nell'ultimo secondo: %u\n", rx_count);
        printf("Primo campione: %u\n", rx_buffer[0]); // Debug
        rx_count = 0;                                 // Resetta il contatore
    }

    return 0;
}