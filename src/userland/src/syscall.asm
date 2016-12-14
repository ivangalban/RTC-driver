[bits 32]

SYSCALL_FB_PRINTF equ 0
SYSCALL_EXIT      equ 1

global fb_printf
fb_printf:
  ; TODO: Implementar la llamada al sistema fb_printf. Los registros quedarán
  ;       como sigue:
  ;         eax : código de identificación de fb_printf
  ;         ebx : dirección de memoria de la cadena de formato.
  ;         ecx : valor del único parámetro extra a fb_printf.
  ret
  
global exit
exit:
  ; eip | ebx
  mov eax, SYSCALL_EXIT
  mov ebx, [esp + 4]
  int 0x80
  ret ; Though this should not ret.
