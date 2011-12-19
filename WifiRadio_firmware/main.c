/*
 * This code is based on the firmware made by Jeff Keyzer for his Wifi Radio project. More information is available at
 * http://mightyohm.com/blog/
 *
 * Built and tested with an ATmega368 @ 16MHz and a Sparkfun 20x4 character LCD display
 *
 */

// Includes

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>

#include "lcd.h"				// Peter Fleury's LCD Library

// Defines

#define	SER_BUFF_LEN	150		// longest character line to accept from serial port
#define STR_LEN			21		// Longest substring (artist, trackname) to accept. Width of the display + terminating 0
#define	BAUD			9600	// USART baud rate, must agree with router ttyS0 settings
#define	LCD_WIDTH		20		// visible width of LCD display
#define DDRAM_WIDTH		40		// internal line length of LCD display
#define	PAGEDELAY		3000	// delay between LCD pages, in ms
#define SHIFTDELAY		300		// horizontal shift rate of LCD when display > visible area

// Function prototypes
int atoi ( const char * str );

void inituart(void);
void ioinit(void);
void uart_putchar(unsigned char data);
unsigned char uart_getchar(void);
void uart_flush(void);
void getline(char *serbuffer);
void putstring(char *buffer);
void init_timer1(void);

void lcd_print(char *s);
void processPlayingLine(char *RXserbuffer, char *artist, char *title, int *playlistLength, int *songNum, int *songTime, int *songElapsed);
void displayTime(int songElapsed, int playlistLength, int songNum);
void displayProgressBar(int songLength, int songElapsed);
void displayTrackInfo(char *trackName, char *artistName);

char serRXbuffer[SER_BUFF_LEN];	// serial buffer
char serTXbuffer[SER_BUFF_LEN];	// serial buffer


int buttonPressSeen;

// Timer1 overflow interrupt service routine (ISR)
SIGNAL (TIMER1_OVF_vect) // SIGNAL call makes sure we don't interrupt the interrupt
{
	TCNT1 = 0x10000 - (F_CPU/1024/4);	// reset timer

	PORTC = 0b0100000;
	_delay_ms(10);
	PORTC = 0x00;   // All LED's are off

	if(buttonPressSeen == 0)
	{
		if(!(PINB & 2))
		{
			buttonPressSeen = 1;
			sprintf(serTXbuffer, "next\n");	// convert ADC value to string
			putstring(serTXbuffer);	// transmit over serial link
		}
	}
	else
	{
		if((PINB & 2))
		{
			buttonPressSeen = 0;
		}
		
	}
}


int main(void)
{
	char artist[STR_LEN];	// Artist name
	char title[STR_LEN];	// Title of current song
	int playlistLength;				// Length of the playlist
	int songNum;					// song number in the current playlist
	int songTime;					// Total length of the song in seconds
	int songElapsed;				// Elapsed time within the song in seconds
	
    ioinit();		// Setup IO pins and defaults
    inituart();		// initialize AVR serial port (USART0)

	PORTC = 0b0100000;
	_delay_ms(100);
	PORTC = 0x00;   // All LED's are off

    // initialize LCD display
    lcd_init(LCD_DISP_ON);

    // Display splash screen
    lcd_clrscr();	
    lcd_puts("    MPD Boombox\n   Jeroen Bouwens\n\n Sponsored by Sioux");
    _delay_ms(2000);
	
	buttonPressSeen = 0;
	init_timer1();
	sei();		// enable interrupts
    
    /* main program loop */    
    for(;;) // loop forever
	{	
		// Note: program execution will stall until serial data is received!
		getline(serRXbuffer);		// grab a line of data from the serial port
				
		processPlayingLine(serRXbuffer, artist, title, &playlistLength, &songNum, &songTime, &songElapsed);
				
		lcd_clrscr();	// clear screen

		displayTime(songElapsed, playlistLength, songNum);
		displayProgressBar(songTime, songElapsed);
		displayTrackInfo(title, artist);
    }
	 
    return 0;   /* never reached */
}

void inituart(void)	// Initialize USART0 to desired baud rate
{
	// Set baud rate generator based on F_CPU
	UBRR0H = (unsigned char)(F_CPU/(16UL*BAUD)-1)>>8;
	UBRR0L = (unsigned char)(F_CPU/(16UL*BAUD)-1);
	
	// Enable USART0 transmitter and receiver
	UCSR0B = (1<<RXEN0) | (1<<TXEN0);

}

unsigned char uart_getchar(void)
{
	while(!(UCSR0A & (1 << RXC0)))	// wait until character is received
		;	// do nothing
	
	return UDR0;
}

void uart_putchar(unsigned char data)	// send a character over the serial port
{

	while(!(UCSR0A & (1 << UDRE0))) // wait until UART is ready for tx
		;       // do nothing
	UDR0 = data;    // send char

}

void uart_flush(void)	// flush RX buffer
{
	unsigned char dummy;
	
	while(UCSR0A & (1<<RXC0))	// while there is a character in the RX queue
		dummy = UDR0;			// flush the queue by reading it
}

void getline(char *buffer)	// get a single line from the UART rcvr
{
	int bufpos = 0;
	
	uart_flush();	// clear RX buffer of stale chars if present
	
	// read a line from UART, stop if buffer is full or we reach newline
	while ((bufpos < (SER_BUFF_LEN - 1)) && (buffer[bufpos] = uart_getchar()) != '\n') {

		if (buffer[bufpos] == '\r')
			bufpos--;	// strip carriage returns in case data contains both CR&LF
			
		bufpos++;
	}
	
	buffer[bufpos] = '\0';	// turn buffer into a string
}

void ioinit (void)
{
    //1 = output, 0 = input
    DDRC |= 0b0100000; // PC5 is output
	DDRB &= 0b11111101; // PB1 is input
	PORTB |= 0b00000010; // Enable internal pull-up resistor	
}

void putstring(char *buffer)	// send a string to the UART
{
	int bufpos = 0;
	
	cli();	// disable interrupts so we don't mangle the data
	//uart_flush();	// clear RX buffer of stale chars if present
	
	// start sending characters over the serial port until we reach the end of the string
	while ((bufpos < (SER_BUFF_LEN - 1)) && (buffer[bufpos] != '\0')) 
	{
		uart_putchar(buffer[bufpos]);
			
		bufpos++;
	}
	
	//uart_putchar('\n');
	sei();	// enable interrupts again
	
}

void init_timer1(void) 
{
	// initialize TIMER1 to trigger an overflow interrupt every 0.5sec
	TCCR1A = 0;
	TCCR1B |= (1 << CS12) | (1 << CS10);

	
	TCNT1 = 0x10000 - (F_CPU/1024/4);
	
	TIFR1=0;
	TIMSK1 |= (1 << TOIE1);
}

void processPlayingLine(char *RXserbuffer, char *artist, char *title, 
						int *playlistLength, int *songNum, int *songTime, int *songElapsed)
{
	char *artistPtr 	= strstr(RXserbuffer, "Artist: ");
	char *titlePtr 	= strstr(RXserbuffer, "Title: ");
	char *plLengthPtr	= strstr(RXserbuffer, "playlistlength: ");
	char *songPtr 		= strstr(RXserbuffer, "song: ");
	char *timePtr 		= strstr(RXserbuffer, "time: ");
	
	if(artistPtr && titlePtr && plLengthPtr && songPtr && timePtr)
	{
		char *artistStart = artistPtr + sizeof("Artist: ") - 1;
		int artistLength = titlePtr - (artistPtr + sizeof("Artist: ") - 1);
		artistLength = artistLength > 20 ? 20 : artistLength;
		strncpy(artist, artistStart , artistLength);
		artist[artistLength] = '\0';
	
		char *titleStart = titlePtr + sizeof("Title: ") - 1;
		int titleLength = plLengthPtr - (titlePtr + sizeof("Title: ") - 1);
		titleLength = titleLength > 20 ? 20 : titleLength;
		strncpy(title, titleStart, titleLength);
		title[titleLength] = '\0';

		char *pllStart = plLengthPtr + sizeof("playlistlength: ") - 1;
		int pllLength = songPtr - (plLengthPtr + sizeof("playlistlength: ") - 1);
		pllStart[pllLength] = '\0';
		*playlistLength = atoi(pllStart);

		char *songStart = songPtr + sizeof("song: ") - 1;
		int songLength = timePtr - (songPtr + sizeof("song: ") - 1);
		songStart[songLength] = '\0';
		*songNum = atoi(songStart) + 1;

		char *timeStart = timePtr + sizeof("time: ") - 1;
		char *songTotalTime = strstr(timeStart, ":") + 1;
		*(songTotalTime-1) = '\0'; // Insert a terminating \0 to separate the elapsed time from the total time
		*songElapsed = atoi(timeStart);
		*songTime = atoi(songTotalTime);
	}
}

void displayTrackInfo(char *trackName, char *artistName)
{

	if(strlen(artistName) > 20)
	{
		artistName[17]='.';
		artistName[18]='.';
		artistName[19]='.';
		artistName[20]='\0';
	}
	lcd_gotoxy(10-strlen(artistName)/2,1);
	lcd_puts(artistName);

	if(strlen(trackName) > 20)
	{
		trackName[17]='.';
		trackName[18]='.';
		trackName[19]='.';
		trackName[20]='\0';
	}
	lcd_gotoxy(10-strlen(trackName)/2,2);
	lcd_puts(trackName);
} 

void displayProgressBar(int songLength, int songElapsed)
{
	for(int i=0; i<((songElapsed*100)/songLength)/5; i++)
	{
		lcd_gotoxy(i, 3);
		lcd_putc('#');
	}

	for(int i=((songElapsed*100)/songLength)/5; i<20; i++)
	{
		lcd_gotoxy(i, 3);
		lcd_putc('-');
	}
}

void displayTime(int songElapsed, int playlistLength, int songNum)
{
	char stringBuffer[20];
 
	sprintf(stringBuffer, "%d:%02d", songElapsed / 60, songElapsed % 60);
	lcd_gotoxy(0, 0);
	lcd_puts(stringBuffer);

	sprintf(stringBuffer, "(%d of %d)", songNum, playlistLength);
	lcd_gotoxy(20-strlen(stringBuffer), 0);
	lcd_puts(stringBuffer);
}