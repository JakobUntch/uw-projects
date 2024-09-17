/*=========================================================================*/
/*  Includes                                                               */
/*=========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <system.h>
#include <altera_avalon_timer_regs.h>
#include <altera_avalon_timer.h>
#include "altera_avalon_pio_regs.h"
#include <sys/alt_alarm.h>
#include <io.h>
#include "sys/alt_irq.h"

#include "fatfs.h"
#include "diskio.h"

#include "ff.h"
#include "monitor.h"
#include "uart.h"

#include "alt_types.h"

#include <altera_up_avalon_audio.h>
#include <altera_up_avalon_audio_and_video_config.h>

/*=========================================================================*/
/*  DEFINE: All Structures and Common Constants                            */
/*=========================================================================*/

#define ESC 27
#define CLEAR_LCD_STRING "[2J"

/*=========================================================================*/
/*  DEFINE: Macros                                                         */
/*=========================================================================*/

#define PSTR(_a)  _a

/*=========================================================================*/
/*  DEFINE: Prototypes                                                     */
/*=========================================================================*/

/*=========================================================================*/
/*  DEFINE: Definition of all local Data                                   */
/*=========================================================================*/
static alt_alarm alarm;
static unsigned long Systick = 0;
static volatile unsigned short Timer; /* 1000Hz increment timer */

/*=========================================================================*/
/*  DEFINE: Definition of all local Procedures                             */
/*=========================================================================*/

/***************************************************************************/
/*  TimerFunction                                                          */
/*                                                                         */
/*  This timer function will provide a 10ms timer and                      */
/*  call ffs_DiskIOTimerproc.                                              */
/*                                                                         */
/*  In    : none                                                           */
/*  Out   : none                                                           */
/*  Return: none                                                           */
/***************************************************************************/
static alt_u32 TimerFunction(void *context) {
	static unsigned short wTimer10ms = 0;

	(void) context;

	Systick++;
	wTimer10ms++;
	Timer++; /* Performance counter for this module */

	if (wTimer10ms == 10) {
		wTimer10ms = 0;
		ffs_DiskIOTimerproc(); /* Drive timer procedure of low level disk I/O module */
	}

	return (1);
} /* TimerFunction */

/***************************************************************************/
/*  IoInit                                                                 */
/*                                                                         */
/*  Init the hardware like GPIO, UART, and more...                         */
/*                                                                         */
/*  In    : none                                                           */
/*  Out   : none                                                           */
/*  Return: none                                                           */
/***************************************************************************/
static void IoInit(void) {
	uart0_init(115200);

	/* Init diskio interface */
	ffs_DiskIOInit();

	//SetHighSpeed();

	/* Init timer system */
	alt_alarm_start(&alarm, 1, &TimerFunction, NULL);

} /* IoInit */

/*=========================================================================*/
/*  DEFINE: All code exported                                              */
/*=========================================================================*/

uint32_t acc_size; /* Work register for fs command */
uint16_t acc_files, acc_dirs;
FILINFO Finfo;
FATFS Fatfs[_VOLUMES]; /* File system object for each logical drive */
FIL File1, File2; /* File objects */
DIR Dir; /* Directory object */
uint8_t Buff[512] __attribute__ ((aligned(4))); /* Working buffer */

char *ptr;
uint8_t res = 0;
char fnames[20][20];
uint32_t fsizes[20];
int buttonState = 0x0F;
int counter = 0;
int buttonPressed = 0;
int WAVcounter = 0;
int c = 0;
long p1 = 0;
int c1, c2, c3, c4 = 0;
int currentState = 0; // not pressed
unsigned int l_buf = 0;
unsigned int r_buf = 0;
uint32_t d = 0;
uint32_t cnt, blen = sizeof(Buff);
int paused = 0;
int stopped = 1;
int next = 0;
int prev = 0;
int dbl = 0;
int half = 0;
int mono = 0;
int findex[20];
FILE *lcd;

void LCD_Display() {
	fprintf(lcd, "%c%s", ESC, CLEAR_LCD_STRING);
	fprintf(lcd, "%d: %s\n", findex[c], fnames[c]);
	if (stopped) {
		fprintf(lcd, "%s\n", "STOPPED");
	} else if (paused) {
		fprintf(lcd, "%s\n", "PAUSED");
	} else {
		if (dbl) {
			fprintf(lcd, "%s\n", "PBACK–DBL SPD");
		} else if (half) {
			fprintf(lcd, "%s\n", "PBACK–HALF SPD");
		} else if (mono) {
			fprintf(lcd, "%s\n", "PBACK–MONO");
		} else {
			fprintf(lcd, "%s\n", "BACK-NORM SPD");
		}
	}
}

int isWav(char *filename) {
	filename = strrchr(filename, '.');
	if (filename == NULL || strcmp(filename, ".WAV") != 0)
		return 0;
	else
		return 1;
}

void Initialization() {
	disk_initialize((uint8_t ) 0);
	f_mount((uint8_t) 0, &Fatfs[0]);

	while (*ptr == ' ') {
		ptr++;
	}
	WAVcounter = 0;
	res = f_opendir(&Dir, ptr);
	for (int i = 0;; i++) {
		res = f_readdir(&Dir, &Finfo);
		if ((res != FR_OK) || !Finfo.fname[0])
			return;
		if (isWav(Finfo.fname)){
			findex[WAVcounter] = i;
			strcpy(fnames[WAVcounter], Finfo.fname);
			fsizes[WAVcounter] = (uint32_t) Finfo.fsize;
			WAVcounter++;
		}
	}
	return;
}

static void f_play() {
	int i = 3;
	d = fsizes[c];
	alt_up_audio_dev * audio_dev;
	audio_dev = alt_up_audio_open_dev("/dev/Audio");
	if (audio_dev == NULL) return;
	unsigned int *pout = NULL;
	int mode = 0;
	if ((IORD(SWITCH_PIO_BASE, 0) & 0x03) == 1) {
		mode = 1;
		dbl = 1;
		half = 0;
		mono = 0;
	} else if ((IORD(SWITCH_PIO_BASE, 0) & 0x03) == 2) {
		mode = 2;
		dbl = 0;
		half = 1;
		mono = 0;
	} else if ((IORD(SWITCH_PIO_BASE, 0) & 0x03) == 3) {
		mode = 3;
		dbl = 0;
		half = 0;
		mono = 1;
	} else{
		dbl = 0;
		half = 0;
		mono = 0;
	}
	LCD_Display();
	while (d) {
		if (buttonState == 0x0E) { //NEXT
			paused = 0;
			prev = 0;
			next = 1;
			stopped = 0;
			buttonState = 0x0F;
			return;
		} else if (buttonState == 0x0D) {
			if (!paused) {
				paused = 1;
				prev = 0;
				next = 0;
				stopped = 0;
			} else {
				paused = 0;
				prev = 0;
				next = 0;
				stopped = 0;
			}
			LCD_Display();
			buttonState = 0x0F;
		} else if (buttonState == 0x0B) { //STOP
			paused = 0;
			prev = 0;
			next = 0;
			stopped = 1;
			f_lseek(&File1, 0);
			buttonState = 0x0F;
			LCD_Display();
			return;
			//update LCD Display
		} else if (buttonState == 0x07) { //PREV
			paused = 0;
			next = 0;
			prev = 1;
			stopped = 0;
			buttonState = 0x0F;
			return;
		}
		if (!paused) {

			if (d >= blen) {
				cnt = blen;
				d -= blen;
			} else {
				cnt = d;
				d = 0;
			}
			res = f_read(&File1, Buff, cnt, pout);
			if (res != FR_OK) return;

			while (i < cnt) {
				while (alt_up_audio_write_fifo_space(audio_dev,
				ALT_UP_AUDIO_RIGHT) == 0) {
				}
				l_buf = (Buff[i] << 8) | Buff[i - 1];
				r_buf = (Buff[i - 2] << 8) | Buff[i - 3];

				if (mode == 3) {
					alt_up_audio_write_fifo(audio_dev, &(r_buf), 1,
					ALT_UP_AUDIO_LEFT);
					alt_up_audio_write_fifo(audio_dev, &(r_buf), 1,
					ALT_UP_AUDIO_RIGHT);
				} else {
					alt_up_audio_write_fifo(audio_dev, &(l_buf), 1,
					ALT_UP_AUDIO_LEFT);
					alt_up_audio_write_fifo(audio_dev, &(r_buf), 1,
					ALT_UP_AUDIO_RIGHT);
				}
				if (mode == 1)
					i += 8;
				else if (mode == 2)
					i += 2;
				else
					i += 4;
			}
			i = 3;
			next = 0;
			prev = 0;
			paused = 0;
			stopped = 1;
		}
	}
	LCD_Display();
	Initialization();
	return;
}

static void interrupt(void *context, alt_u32 id) { //interrupt service handler
	currentState = IORD(BUTTON_PIO_BASE, 0);
	if (!buttonPressed && currentState != 0xF) {
		buttonState = currentState;
		buttonPressed = 1;
	}
	if (currentState == 0xF) {
		counter++;
		if (counter < 10) {
			IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 5);
		}
	} else {
		counter = 0;
		IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 5);
	}
	if (counter == 10) {
		counter = 0;
		buttonPressed = 0;
		IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 8);
		IOWR(BUTTON_PIO_BASE, 2, 0xF);
	}
}

static void buttons(void *context, alt_u32 id) { //interrupt service handler
	IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0);
	IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE, 0xFFFF);
	IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE, 2);
	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 5);
	IOWR(BUTTON_PIO_BASE, 2, 0x0);
	IOWR(BUTTON_PIO_BASE, 3, 0x0F);
}

/***************************************************************************/
/*  main                                                                   */
/***************************************************************************/
int main(void) {
	IOWR(BUTTON_PIO_BASE, 2, 0x0F);
	IOWR(BUTTON_PIO_BASE, 3, 0x0F);
	alt_irq_register(BUTTON_PIO_IRQ, (void *) 0, buttons); // initialize the interrupt handler
	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, 0x8);
	alt_irq_register(TIMER_0_IRQ, (void *) 0, interrupt); // initialize the interrupt handler
	alt_up_audio_dev * audio_dev;
	lcd = fopen("/dev/lcd_display", "w");
	/* used for audio record/playback */

// open the Audio port
	audio_dev = alt_up_audio_open_dev("/dev/Audio");
	if (audio_dev == NULL)
		return 1;

	IoInit();

	IOWR(SEVEN_SEG_PIO_BASE, 1, 0x0007);

//initialization

	Initialization();

	LCD_Display();

//start of code
	while (1) {
		if (buttonState == 0x0E) { //NEXT
			c++;
			if (c >= WAVcounter)
				c = 0;
			f_lseek(&File1, 0); //reset file pointer
			f_close(&File1); //close the file
			Initialization();
			LCD_Display();
			buttonState = 0x0F;
			//run LCD Display functions for new file
		} else if (buttonState == 0x0D) { //PLAY AND PAUSE;
			f_open(&File1, fnames[c], (uint8_t) 1);
			stopped = 0;
			paused = 0;
			buttonState = 0x0F;
			f_play();
			f_lseek(&File1, 0);
			while (next || prev) {
				if (next == 1) {
					c++;
					if (c >= WAVcounter)
						c = 0;
					Initialization();
					f_open(&File1, fnames[c], (uint8_t) 1);
					f_play();
				} else if (prev == 1) {
					c--;
					if (c < 0)
						c = WAVcounter - 1;
					Initialization();
					f_open(&File1, fnames[c], (uint8_t) 1);
					f_play();

				}
			}
			buttonState = 0x0F;

			//update LCD Display
		} else if (buttonState == 0x0B) { //STOP
			f_lseek(&File1, 0);
			stopped = 1;
			LCD_Display();
			buttonState = 0x0F;
			//update LCD Display
		} else if (buttonState == 0x07) { //PREV
			c--;
			if (c < 0)
				c = WAVcounter - 1;
			f_lseek(&File1, 0); //reset file pointer
			f_close(&File1); //close the file
			Initialization();
			LCD_Display();
			buttonState = 0x0F;
		}
	}
	fclose(lcd);
	return (0);
}
