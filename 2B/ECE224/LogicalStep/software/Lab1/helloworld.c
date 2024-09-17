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


//functions
int background() { //background task function
	int j;
	int x = 0;
	int grainsize = 4;
	int g_taskProcessed = 0;

	for (j = 0; j < grainsize; j++) {
		g_taskProcessed++;
	}
	return x;
}

static void interrupt(void *context, alt_u32 id) { //interrupt service handler
	IOWR(RESPONSE_OUT_BASE, 0, 1);
	IOWR(LED_PIO_BASE, 0, 2);
	IOWR(RESPONSE_OUT_BASE, 0, 0); //send a response by setting signal high then immediately low.
	IOWR(LED_PIO_BASE, 0, 0); //pulses LED 01
	IOWR(STIMULUS_IN_BASE, 3, 0x0); //reset the interrupt bit, allowing for a new interrupt to be handled.
}


//main
int main() {

	// variables//
	int missed = 0; //tracks how many stimulus pulses are missed
	int TaskCounter = 0; //tracks how many tasks are successfully completed
	int charNum = 0; //used to determine the number of tasks to run per cycle (calculated in char cycle)
	int latency = 0; //average latency from stimulus to response
	int multi = 0; //tracks how many times the same signal is serviced multiple times
	int previousState = 0; //keeps track of previous state of stimulus pulse
	int currentState = 0; //keeps track of current state of stimulus pulse

	// mode selection//
	if (IORD(SWITCH_PIO_BASE, 0) == 1) printf("Tight Polling method selected.\n");
	else printf("Interrupt method selected.\n");
	printf("Period, Pulse_Width, BG_Tasks Run, Latency, Missed, Multiple\n");
	printf("Please, press PB0 to continue.\n");

	while ((IORD(BUTTON_PIO_BASE, 0) & 0x01)) {}; //run until PB0 button is pressed

	//tight polling
	if (IORD(SWITCH_PIO_BASE, 0) == 1) { // switch is ON (tight polling)
		for (int i = 2; i <= 5000; i += 2) { //repeat test for periods 2 to 5000
			//initialization
			IOWR(EGM_BASE, 2, i); //period
			IOWR(EGM_BASE, 3, i / 2); //pulse width
			TaskCounter = 0; //reset taskCounter
			charNum = 0; //reset charNum
			previousState = 1; //sets previous state to 1 since stimulus pulse is sent immediately upon activation
			IOWR(EGM_BASE, 0, 1); //activates the EGM
			IOWR(LED_PIO_BASE, 0, 4);
			IOWR(LED_PIO_BASE, 0, 0); //pulses LED 02

			//characterization cycle
			while (IORD(STIMULUS_IN_BASE, 0) == 0 && IORD(EGM_BASE, 1)) { //while EGM enabled and no stimulus pulse detected run until stimulus pulse is received
			}
			IOWR(RESPONSE_OUT_BASE, 0, 1);
			IOWR(RESPONSE_OUT_BASE, 0, 0); //send a response upon receiving stimulus pulse
			currentState = IORD(STIMULUS_IN_BASE, 0); //set current state equal to the stimulus pulse (1 if high, 0 if low)
			while (!(!previousState && currentState) && IORD(EGM_BASE, 1)) { //run until previousState is 0 and currentState is 1 (a new pulse has been received)
				IOWR(LED_PIO_BASE, 0, 1);
				background(); //runs background tasks
				IOWR(LED_PIO_BASE, 0, 0); //pulses LED 00
				TaskCounter++; //increments completed tasks
				previousState = currentState; //sets previousState to currentState
				currentState = IORD(STIMULUS_IN_BASE, 0); //updates currentState to the current value of the stimulus pulse
			}
			charNum = TaskCounter - 1; //number of tasks that we should run per cycle determined by the number of successful tasks we ran during the char cycle

			//tight polling cycles
			while (IORD(EGM_BASE, 1)) { // check busy register
				while ((IORD(STIMULUS_IN_BASE, 0) == 0) && (IORD(EGM_BASE, 1))) {} //run until stimulus pulse (rising edge) is received
				if (!IORD(EGM_BASE, 1)) break; // if not busy, don't send a signal
				IOWR(RESPONSE_OUT_BASE, 0, 1);
				IOWR(RESPONSE_OUT_BASE, 0, 0); //send response pulse
				for (int j = 0; j < charNum; ++j) { //runs charNum (taskCounter - 1) background tasks until tight polling for stimulus
					IOWR(LED_PIO_BASE, 0, 1);
					background();
					IOWR(LED_PIO_BASE, 0, 0); //pulses LED 00
					TaskCounter++; //run background task and increment counter
				}
			}

			//final results
			latency = IORD(EGM_BASE, 4); //average latency
			missed = IORD(EGM_BASE, 5); //total missed
			multi = IORD(EGM_BASE, 6); //total multiple responses
			printf("%i, %i, %i, %i, %i, %i\n", i, i / 2, TaskCounter, latency, missed, multi); //output results
			IOWR(EGM_BASE, 0, 0x00); //disable EGM
		}
	}

	//interrupt service handler
	else { // switch is OFF (interrupt service handler)
		alt_irq_register(STIMULUS_IN_IRQ, (void *) 0, interrupt); // initialize the interrupt handler
		IOWR(STIMULUS_IN_BASE, 2, 1); // allow interrupts to STIMULUS_IN_BASE
		for (int i = 2; i <= 5000; i += 2) {
			//initialization
			IOWR(EGM_BASE, 2, i); // set the period
			IOWR(EGM_BASE, 3, i / 2); // set the pulse width
			TaskCounter = 0; //reset taskCounter
			IOWR(EGM_BASE, 0, 1); // enable the EGM
			IOWR(LED_PIO_BASE, 0, 4);
			IOWR(LED_PIO_BASE, 0, 0); //pulses LED 02

			//background tasks
			while (IORD(EGM_BASE, 1)) {
				IOWR(LED_PIO_BASE, 0, 1);
				background();
				IOWR(LED_PIO_BASE, 0, 0); //pulse LED 00
				TaskCounter++; //runs background task and increments counter
			}

			//final results
			latency = IORD(EGM_BASE, 4); //average latency
			missed = IORD(EGM_BASE, 5); //total missed
			multi = IORD(EGM_BASE, 6); //total multiple responses
			printf("%i, %i, %i, %i, %i, %i\n", i, i / 2, TaskCounter, latency, missed, multi); //output results
			IOWR(EGM_BASE, 0, 0x00); //disable EGM
		}
	}

	return 0;
}
