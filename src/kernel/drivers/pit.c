#include <pit.h>
#include <io.h>
#include <pic.h>
#include <fb.h>

void pit_init() {
	counter = 0;
	itr_set_interrupt_handler(PIC_TIMER_IRQ,
	                    pit_interrupt_handler,
	                    IDT_PRESENT | IDT_DPL_RING_0 | IDT_GATE_INTR);

	//outb(PIT_CMD_REG_DATA_PORT, PIT_BINARYMODE | PIT_MODE0 |
	//						 PIT_LOBYTE_HIBYTE | PIT_CHANNEL0);
	outb(PIT_CMD_REG_DATA_PORT, 0x36);
	outb(PIT_CHANNEL0_DATA_PORT, (u8)PIT_RELOAD_VALUE);
	outb(PIT_CHANNEL0_DATA_PORT, (u8)PIT_RELOAD_VALUE >> 8);
}

/* Interrupts handler. */
void pit_interrupt_handler(itr_cpu_regs_t regs,
                              itr_intr_data_t data,
                              itr_stack_state_t stack) {
	fb_printf("counter: %dd\n", counter);
	++counter;
	pic_send_eoi(data.irq);
}

void pit_interrupt_disabled() {

}