MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 2048K  /* Flash disponibile (modifica se necessario) */
    RAM(rwx)  : ORIGIN = 0x20000000, LENGTH = 520K   /* Tutta la SRAM dell'RP2350 */
}

/* Includi il linker script predefinito per il resto delle definizioni */
INCLUDE memmap_default.ld