[bits 32]

; Called like in C, this function switch us to user mode. Its arguments are:
;   (eip, cs, eflags, esp, ss, ds, es, fs, gs)
; It'll make nothing smart, which means the caller must set the right RPLs.
global proc_switch_to_lower_privilege_level
proc_switch_to_lower_privilege_level:
  ; cli ; Clear interrupts to avoid conflicts. IRET will reactivate them.

  ; Set the segments that can/must be directly set.
  %define arg_gs      esp + 36
  %define arg_fs      esp + 32
  %define arg_es      esp + 28
  %define arg_ds      esp + 24

  xchg bx, bx

  xor eax, eax
  mov word ax, [arg_ds]
  mov ds, eax
  mov word ax, [arg_es]
  mov es, eax
  mov word ax, [arg_fs]
  mov fs, eax
  mov word ax, [arg_gs]
  mov gs, eax

  ; Following C call convention, our stack has eip, then the args, which are
  ; precisely in the order we need them. Thus, we should just take eip from
  ; the stack and we'll be set.
  add esp, 4

  ; Do the switch.
  iretd
