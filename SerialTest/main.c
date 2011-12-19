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
#include <avr/interrupt.h>
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
void putstring(unsigned char *buffer);
void init_timer1(void);

//enum Tags { Artist, Title, PlaylistLength, Song, Time };

unsigned char serbuffer[SER_BUFF_LEN];	// serial buffer


int buttonPressSeen;

// Timer1 overflow interrupt service routine (ISR)
SIGNAL (TIMER1_OVF_vect) // SIGNAL call makes sure we don't interrupt the interrupt
{
	TCNT1 = 0x10000 - (F_CPU/1024/8);	// reset timer


	if(buttonPressSeen == 0)
	{
		PORTC = 0b0010000;
		_delay_ms(10);
		PORTC = 0x00;
		if(!(PINB & 2))
		{
			buttonPressSeen = 1;
			sprintf(serbuffer, "next\n");	// convert ADC value to string
			putstring(serbuffer);	// transmit over serial link
		}
	}
	else
	{
		PORTC = 0b0001000;
		_delay_ms(10);
		PORTC = 0x00;
		if((PINB & 2))
		{
			buttonPressSeen = 0;
		}
		
	}
}


int main(void)
{
	unsigned char name[SER_BUFF_LEN];		// name of current radio station
	unsigned char title[SER_BUFF_LEN];		// artist & title of current song
	unsigned char RXserbuffer[SER_BUFF_LEN];	// serial buffer
	\
	/* initialization routines */
	
    ioinit();		// Setup IO pins and defaults
    inituart();		// initialize AVR serial port (USART0)
	buttonPressSeen = 0;

	PORTC = 0b0001000;
	_delay_ms(100);
	PORTC = 0b0010000;
	_delay_ms(100);
	PORTC = 0b0100000;
	_delay_ms(100);
	PORTC = 0x00;   // All LED's are off
    
	init_timer1();
	sei();		// enable interrupts
    
    /* main program loop */    
    while(1) // loop forever
	{	
		
		// Grab name and title data from the router
		// Note: program execution will stall until serial data is received!
		getline(RXserbuffer);		// grab a line of data from the serial port

		// Read line from serial port
		PORTC = 0b0100000;
		_delay_ms(10);
		PORTC = 0b0000000;

		//Tags received = processMessage(serbuffer);
		
		// decide what to do with the data
		if (strstr(RXserbuffer, "song: "))
			strcpy(name, RXserbuffer+(sizeof("song: ")-1));
		
		// Blink the lights "tracknum" times
		int tracknum = atoi(name);
		for(int i=0; i<tracknum; i++)
		{
	//		PORTC = 0b0100000;
	//		_delay_ms(50);
	//		PORTC = 0x00;
	//		_delay_ms(300);
		 
		}
		//_delay_ms(1000);
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

    DDRB &= 0b11111101; // B1 is input
	PORTB |= 0b00000010; // Enable internal pull-up resistor for B1
}

void putstring(unsigned char *buffer)	// send a string to the UART
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

	
	TCNT1 = 0x10000 - (F_CPU/1024/8);
	
	TIFR1=0;
	TIMSK1 |= (1 << TOIE1);
}