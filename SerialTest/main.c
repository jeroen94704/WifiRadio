/* Name: main.c
 * Author: Jeff Keyzer	http://mightyohm.com 
 * Copyright: MightyOhm Engineering, 2008
 * License: Creative Commons Attribution-Share Alike 3.0
 *
 * -- LCD Display Driver for Wifi Radio Project --
 * This program accepts serial data from a shell script running on an ASUS WL-520GU wireless
 * router.  The shell script queries a local mpd server and returns the name of the current
 * radio stream as well as the artist/title of the current song.  The AVR expects to get
 * two lines of serial data with each query, one for the stream name prefixed with "Name: "
 * and one for the artist/title prefixed "Title: "
 *
 * Built and tested with an ATmega168 @ 16MHz and a Sparkfun 16x2 character LCD display
 *
 * More information is available at
 * http://mightyohm.com/blog/
 */

// Includes

#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>

// Defines

#define	BAUD			9600	// USART baud rate, must agree with router ttyS0 settings
#define	SER_BUFF_LEN	80		// longest character line to accept from serial port
#define	LCD_WIDTH		16		// visible width of LCD display
#define DDRAM_WIDTH		40		// internal line length of LCD display
#define	PAGEDELAY		3000	// delay between LCD pages, in ms
#define SHIFTDELAY		300		// horizontal shift rate of LCD when display > visible area

// Function prototypes

void inituart(void);
void ioinit(void);
void uart_putchar(unsigned char data);
unsigned char uart_getchar(void);
void uart_flush(void);
void getline(unsigned char *serbuffer);

enum Tags { Artist, Title, PlaylistLength, Song, Time };

int main(void)
{
	unsigned char serbuffer[SER_BUFF_LEN];	// serial buffer
	unsigned char name[SER_BUFF_LEN];		// name of current radio station
	unsigned char title[SER_BUFF_LEN];		// artist & title of current song

	/* initialization routines */
	
    ioinit();		// Setup IO pins and defaults
    inituart();		// initialize AVR serial port (USART0)
	PORTC = 0x00;   // All LED's are off
    
    /* main program loop */    
    for(;;) // loop forever
	{	
		
		// Start loop indicator
		PORTC = 0b0111000;
		_delay_ms(10);
		PORTC = 0b0000000;

		// Grab name and title data from the router
		// Note: program execution will stall until serial data is received!
		getline(serbuffer);		// grab a line of data from the serial port

		Tags received = processMessage(serbuffer);
		
		// decide what to do with the data
		if (strstr(serbuffer, "song: "))
			strcpy(name, serbuffer+(sizeof("song: ")-1));
		
		// Blink the lights "tracknum" times
		 int tracknum = atoi(name);
		 for(int i=0; i<tracknum; i++)
		 {
			PORTC = 0b0100000;
			_delay_ms(50);
			PORTC = 0x00;
			_delay_ms(300);
			 
		 }
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
	{
	}
	
	return UDR0;
}

void uart_flush(void)	// flush RX buffer
{
	unsigned char dummy;
	
	while(UCSR0A & (1<<RXC0))	// while there is a character in the RX queue
		dummy = UDR0;			// flush the queue by reading it
}

void getline(unsigned char *buffer)	// get a single line from the UART rcvr
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
    DDRC = 0b11111111; //All outputs
}
