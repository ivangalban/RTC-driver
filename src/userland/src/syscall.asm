[bits 32]

SYSCALL_FB_PRINTF equ 0
SYSCALL_EXIT      equ 1

global fb_printf
fb_printf:
  ; eip | ebx | ecx
  mov eax, SYSCALL_FB_PRINTF
  mov ebx, [esp + 4]
  mov ecx, [esp + 8]
  int 0x80
  ret

global exit
exit:
  ; eip | ebx
  mov eax, SYSCALL_EXIT
  mov ebx, [esp + 4]
  int 0x80
  ret ; Though this should not ret.
