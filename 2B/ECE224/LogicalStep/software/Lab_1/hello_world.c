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
#include <stdlib.h>
#include "sys/alt_irq.h"
#include "system.h"
#include "altera_avalon_pio_regs.h"

int background()
{
	int j;
	int x = 0;
	int grainsize = 4;
	int g_taskProcessed = 0;

	for (j = 0; j < grainsize; j++)
	{
		g_taskProcessed++;
	}
	return x;
}

static void interrupt(void *context, alt_u32 id)
{
	IOWR(RESPONSE_OUT_BASE, 0, 1);
	IOWR(RESPONSE_OUT_BASE, 0, 0);
	IOWR(STIMULUS_IN_BASE, 3, 0x0);
}

int main()
{
	// variables//
	int missed = 0;
	int TaskCounter = 0;
	int charNum = 0;
	int latency = 0;
	int multi = 0;
	int previousState = 0;
	int currentState;

	// mode selection//
	if (IORD(SWITCH_PIO_BASE, 0) == 1) {
		printf("Yes Tight Polling method selected.\n");
	} else {
		printf("Interrupt method selected.\n");
	}
	printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n");
	printf("Please, press PB0 to continue.\n");

	while ((IORD(BUTTON_PIO_BASE, 0) & 0x01))
	{
	};

	if (IORD(SWITCH_PIO_BASE, 0) == 1) { // switch is ON (tight polling)
			for (int i = 2; i <= 5000; i += 2) {
			IOWR(EGM_BASE, 2, i);
			IOWR(EGM_BASE, 3, i/2);
			TaskCounter = 0;
			previousState = 1;
			IOWR(EGM_BASE, 0, 1);
			while (IORD(STIMULUS_IN_BASE, 0) == 0 && IORD(EGM_BASE, 1)) { }
			IOWR(RESPONSE_OUT_BASE, 0, 1);
			IOWR(RESPONSE_OUT_BASE, 0, 0);
			currentState = IORD(STIMULUS_IN_BASE, 0);
				while (((previousState == currentState) || (previousState && !currentState)) && IORD(EGM_BASE, 1)) {
					background();
					TaskCounter++;
					previousState = currentState;
					currentState = IORD(STIMULUS_IN_BASE, 0);
				}
			charNum = TaskCounter - 1;
			while (IORD(EGM_BASE, 1)) { // check busy register
				while (IORD(STIMULUS_IN_BASE, 0) == 0 && IORD(EGM_BASE, 1)) { }// poll to check for rising edge
						IOWR(RESPONSE_OUT_BASE, 0, 1);
						IOWR(RESPONSE_OUT_BASE, 0, 0);
				for (int j = 0; j < charNum; j++) {
					background();
					TaskCounter++;
				}
			}
			// disable EGM
			IOWR(EGM_BASE, 0, 0x00);

			latency = IORD(EGM_BASE, 4);
			missed = IORD(EGM_BASE, 5);
			multi = IORD(EGM_BASE, 6);
			printf("%i,%i,%i,%i,%i,%i\n", i, i/2, TaskCounter, latency, missed, multi);
		}
	}
	else { // switch is OFF (interrupt service handler)
		for (int i = 2; i <= 5000; i += 2) {
			IOWR(EGM_BASE, 2, i); // set the period
			IOWR(EGM_BASE, 3, i/2); // set the pulse width
			TaskCounter = 0;
			IOWR(EGM_BASE, 0, 1); // enable the EGM
			alt_irq_register(STIMULUS_IN_IRQ, (void *)0, interrupt); // initialize the interrupt handler
			IOWR(STIMULUS_IN_BASE, 2, 1); // allow interrupts to STIMULUS_IN_BASE
			while (IORD(EGM_BASE, 1)) {
				background();
				TaskCounter++;
			}
			latency = IORD(EGM_BASE, 4);
			missed = IORD(EGM_BASE, 5);
			multi = IORD(EGM_BASE, 6);
			printf("%i,%i,%i,%i,%i,%i\n", i, i/2, TaskCounter, latency, missed, multi);

			// disable EGM
			IOWR(EGM_BASE, 0, 0x00);
		}
	}



	return 0;
}
