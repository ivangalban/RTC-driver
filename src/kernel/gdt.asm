; We can't use C to load the GDT therefore we need this.
[bits 32]

global gdt_load
gdt_load:
  ; This will be called from C. The address to the descriptor is the only
  ; argument this function receives.
  push eax
  mov eax, [esp + 8]  ; eax | eip | arg
  lgdt [eax]
  pop eax
  ret

global gdt_load_ltr
gdt_load_ltr:
  push eax
  mov eax, [esp + 8]
  and eax, 0x0000ffff
  ltr ax
  pop eax      ; eip | arg
  ret
