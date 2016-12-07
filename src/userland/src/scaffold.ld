OUTPUT_FORMAT("a.out-i386-linux")
ENTRY(_start)
SECTIONS {
  .text SIZEOF_HEADERS : {
    *(.text)
  },
  .data : {
    *(.data)
    *(.rodata)
  }
  .bss : {
    *(.bss)
  }
}
