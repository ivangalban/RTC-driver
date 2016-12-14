#include <typedef.h>
#include <interrupts.h>
#include <syscall.h>
#include <fb.h>
#include <mem.h>
#include <hw.h>
#include <proc.h>
#include <gdt.h>

#define SYSCALL_IRQ                   0x80

/* Let's store our interrupts in a static array. It's not the must efficient
 * way to do this but it's more readable. */
#define SYSCALL_TOTAL                 2

static void syscall_fb_printf(itr_cpu_regs_t cpu_regs,
                              itr_intr_data_t intr_data,
                              itr_stack_state_t stack) {
  /* TODO: Imprimir con fb_printf la cadena de formato en ebx con su primer
   *       y único parámetro en ecx. */
}

static void syscall_exit(itr_cpu_regs_t cpu_regs,
                         itr_intr_data_t intr_data,
                         itr_stack_state_t stack) {
  /* TODO: Do a real exit. */
  fb_printf("exit called with %dd\n", cpu_regs.ebx);
  hw_hlt();
}

static interrupt_handler_t syscalls[SYSCALL_TOTAL] = {
  syscall_fb_printf,
  syscall_exit
};

/* This is the interrupt router. */
static void syscall(itr_cpu_regs_t cpu_regs,
             itr_intr_data_t intr_data,
             itr_stack_state_t stack) {
  if (cpu_regs.eax < SYSCALL_TOTAL) {
    syscalls[cpu_regs.eax](cpu_regs, intr_data, stack);
  }
  else {
    /* TODO: Kill the offender. */
  }
}

/* Set the interrupt handler. */
void syscall_init() {
  itr_set_interrupt_handler(SYSCALL_IRQ,
                            syscall,
                            IDT_PRESENT | IDT_DPL_RING_3 | IDT_GATE_INTR);
}
