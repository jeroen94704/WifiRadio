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

#include "lcd.h"				// Peter Fleury's LCD Library
								// http://homepage.hispeed.ch/peterfleury/avr-lcd44780.html

// Defines

#define	BAUD			9600	// USART baud rate, must agree with router ttyS0 settings
#define	SER_BUFF_LEN	80		// longest character line to accept from serial port
#define	LCD_WIDTH		16		// visible width of LCD display
#define DDRAM_WIDTH		40		// internal line length of LCD display
#define	PAGEDELAY		3000	// delay between LCD pages, in ms
#define SHIFTDELAY		300		// horizontal shift rate of LCD when display > visible area

// Function prototypes

void inituart(void);
void uart_putchar(unsigned char data);
unsigned char uart_getchar(void);
void uart_flush(void);
void getline(unsigned char *serbuffer);
void lcd_putvis(unsigned char *s);
void lcd_print(unsigned char *s);

int main(void)
{
	unsigned char serbuffer[SER_BUFF_LEN];	// serial buffer
	unsigned char name[SER_BUFF_LEN];		// name of current radio station
	unsigned char title[SER_BUFF_LEN];		// artist & title of current song

	/* initialization routines */
	
    inituart();				// initialize AVR serial port (USART0)
    lcd_init(LCD_DISP_ON);	// initialize LCD display
    lcd_clrscr();
    
    /* main program loop */
    
    for(;;){	// loop forever
		
		// Grab name and title data from the router
		// Note: program execution will stall until serial data is received!
		getline(serbuffer);		// grab a line of data from the serial port
		
		// decide what to do with the data
		if (strstr(serbuffer, "Name: "))	// is this is a Name: line?
			strcpy(name, serbuffer+(sizeof("Name: ")-1));	// yes, copy for Name display later
		else if (strstr(serbuffer, "Title: "))	// is this a Title: line?
			strcpy(title, serbuffer+(sizeof("Title: ")-1));	// yes, copy for Title display later
			
		getline(serbuffer);		// grab a line of data from the serial port
			
		// decide what to do with the data
		if (strstr(serbuffer, "Name: "))	// is this is a Name: line?
			strcpy(name, serbuffer+(sizeof("Name: ")-1));	// yes, copy for Name display later
		else if (strstr(serbuffer, "Title: "))	// is this a Title: line?
			strcpy(title, serbuffer+(sizeof("Title: ")-1));	// yes, copy for Title display later
		
		// now that we hopefully have both strings, display them
        lcd_print(name);
        _delay_ms(PAGEDELAY);
        
        lcd_print(title);
        _delay_ms(PAGEDELAY);

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
		;
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

void lcd_print(unsigned char *s)		// print with variable display format
{
	int i = 0;
	int length, shift, newline;
    
    length = strlen(s);		// get length of message to display

	/* 	this simple line wrap routine tries to avoid breaking lines in the middle of
		a word.  if it sees a character at the newline point it will start looking for
		the next space and stick the newline there.
		
		ideally we want to:
			- avoid wrapping words
			- avoid scrolling the display horizontally as much as possible
			- fit the most data onto the display at one time
	*/
	if (length > LCD_WIDTH) {	// message won't fit on one visible line
		newline = length/2 + 1;	// start by breaking line in the middle
		
		while (s[newline] != ' ' && (newline < DDRAM_WIDTH)) {
			newline++;	// try to avoid wrapping words unless we hit end of line
		}
	}
	else
		newline = LCD_WIDTH;	// short message, don't break into two lines
	
	lcd_clrscr();	// clear screen
		
	// print message
	while ((s[i] != '\0') && (i < 2*DDRAM_WIDTH)) {	// print whatever fits in DDRAM
	if (i == newline) 
		lcd_putc('\n');		// insert line break 
	lcd_putc(s[i]);
	i++;
	}
		
	if (newline > LCD_WIDTH) {	// line(s) wider than visible area
		shift = newline - LCD_WIDTH;
		_delay_ms(2*SHIFTDELAY);
		}
	else
		shift = 0;	// entire message fits within visible display (no scrolling)
	

	// horizontal shift/scroll routine	
	for(i=0;i<shift;i++) {
		_delay_ms(SHIFTDELAY);
		lcd_command(LCD_MOVE_DISP_LEFT);
	}
	
	_delay_ms(2*SHIFTDELAY);
	
	for(i=shift;i>0;i--) {
		_delay_ms(SHIFTDELAY);
		lcd_command(LCD_MOVE_DISP_RIGHT);
	}
}
