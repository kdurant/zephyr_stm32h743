
SECTIONS
{
    /* Place flash_apply_func in SRAM3 so it survives internal flash erase */
    .ram_func : ALIGN(4)
    {
        __ram_func_start = .;
        *(.ram_func)
        *(.ram_func.*)
        __ram_func_end = .;
    } > SRAM3 AT > FLASH
    __ram_func_size = __ram_func_end - __ram_func_start;
    __ram_func_load_start = LOADADDR(.ram_func);
}
INSERT AFTER .text;
