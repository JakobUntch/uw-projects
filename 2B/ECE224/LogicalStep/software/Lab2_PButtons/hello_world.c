/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdio.h>
#include <altera_avalon_timer_regs.h>
#include <altera_avalon_timer.h>
#include "sys/alt_irq.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"

int counter = 0;
int currentState = 1; // not pressed
int buttonState = 0;

static void interrupt(void *context, alt_u32 id) { //interrupt service handler
	buttonState = (IORD(BUTTON_PIO_BASE,0) & 0x01);
	if (currentState == 1) {
		if (buttonState){
			counter = 0;
		} else counter++;
		if (counter == 10){
			IOWR(LED_PIO_BASE, 0, 1);
			currentState = 0;
		}
	} else {
		if (!buttonState){
			counter = 0;
		} else counter++;
		if (counter == 10){
			IOWR(LED_PIO_BASE, 0, 0);
			currentState = 1;
		}
	}
	IOWR(TIMER_0_BASE, 0, 0x02);
	IOWR(TIMER_0_BASE, 2, 1);
	IOWR(TIMER_0_BASE, 1, 0x05);
}

int main()
{
	alt_irq_register(TIMER_0_IRQ, (void *) 0, interrupt); // initialize the interrupt handler
	IOWR(TIMER_0_BASE, 2, 1);
	IOWR(TIMER_0_BASE, 1, 0x05);
	while(1){}

  return 0;
}
