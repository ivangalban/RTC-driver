[bits 32]

; Called like in C, this function switch us to user mode. Its arguments are:
;  (ss, esp, cs, eip)
;   [esp + 16] user mode eip
;   [esp + 12] user mode cs
;   [esp +  8] user mode esp
;   [esp +  4] user mode ss
; It'll make nothing smart, which means the caller must set the right RPLs.
; Except for assuming SS = DS = ES = FS = GS.
global proc_switch_to_lower_privilege_level
proc_switch_to_lower_privilege_level:
  ; cli ; Clear interrupts to avoid conflicts. IRET will reactivate them.

  ; Set the stack. Since we won't return from here we can safely use the
  ; registers the way we like.
  mov eax, [esp + 4]  ; ss
  mov ebx, [esp + 8]  ; esp
  mov ecx, [esp + 12] ; cs
  mov edx, [esp + 16] ; eip

  push eax
  push ebx
  pushfd
  push ecx
  push edx

  ; Set the rest of the segments to the same value as SS.
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  ; Do the switch.
  iretd
