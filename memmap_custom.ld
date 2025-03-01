MEMORY
{
    FLASH(rx)  : ORIGIN = 0x10000000, LENGTH = 2048K  /* 2 MB Flash */
    RAM(rwx)   : ORIGIN = 0x20000000, LENGTH = 520K   /* All 520 KB SRAM */
    SCRATCH_X(rwx) : ORIGIN = 0x1fff0000, LENGTH = 4K /* Scratch X */
    SCRATCH_Y(rwx) : ORIGIN = 0x1fff1000, LENGTH = 4K /* Scratch Y */
}

INCLUDE "memmap_default.ld"