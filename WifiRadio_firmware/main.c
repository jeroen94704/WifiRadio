/*
 * Firmware (c)2012 Jeroen Bouwens. Parts of the code were lifted from Jeff Keyzer's 
 * code, and from the SparkFun turorial on beginning embedded electronics. 
 * More information at:
 *
 * http://mightyohm.com/blog/
 *
 * and 
 *
 * http://www.sparkfun.com/tutorials/93
 *
 * Built and tested with an ATmega368 @ 16MHz and a Sparkfun 20x4 character LCD display
 *
 */

//=========== Includes ===========

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>

#include "lcd.h"				// Peter Fleury's LCD Library

//=========== Defines ===========

#define	SER_BUFF_LEN	200		// longest character line to accept from serial port
#define STR_LEN			21		// Longest substring (artist, trackname) to accept. Width of the display + terminating 0
#define	BAUD			9600	// USART baud rate, must agree with router ttyS0 settings
#define	LCD_WIDTH		20		// visible width of LCD display
#define	PAGEDELAY		3000	// delay between LCD pages, in ms

#define BOOL unsigned char
#define TRUE 1
#define FALSE 0

#define UPBUTTON 0
#define DOWNBUTTON 1
#define LEFTBUTTON 2
#define RIGHTBUTTON 3
#define ENTERBUTTON 4
#define SWITCHBUTTON 5

#define UPBUTTONPIN PINB&1
#define DOWNBUTTONPIN PINB&2
#define LEFTBUTTONPIN PINB&4
#define RIGHTBUTTONPIN PIND&128
#define ENTERBUTTONPIN PIND&64
#define SWITCHBUTTONPIN PIND&32

#define PM_PLAYING 0
#define PM_BROWSING 2

//=========== Function prototypes ===========

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
void displayDirEntries(void);
BOOL processButtonPress(int buttonIndex, int buttonPin);
BOOL processResponse(char *RXserbuffer);
void sendCommand(char* command);
void sendCommandParams(char* cmd, int param1, int param2);

char serRXbuffer[SER_BUFF_LEN];	// serial buffer
char serTXbuffer[20];	// serial buffer


//=========== Global Variables ===========

unsigned char gButtonPressSeen;	// Bits used to keep track of which button presses have been seen
unsigned char gPlayerMode;		// Keep track whether we are playing something or browsing the collection
int gCurrentListSelectedIndex;	// The selected item in the sublist of 4 currently shown on the display
int gCurrentListStartIndex;		// The index in the total list of the first item in the current sublist of 4
char gDirEntries[4][STR_LEN];	// Buffer holding track/dir names to display in browsing mode
int gNumDirEntries;				// How many dir entries did I receive? (should always be 1,2,3 or 4)
BOOL gWaitingForReply;			// Indicates whether a request was sent to which a reply is expected but not yet received
int gTimeOutCounter;			// Counter for the response timeout mechanism (in case the router fails to respond)

// Timer1 overflow interrupt service routine (ISR)
SIGNAL (TIMER1_OVF_vect) // SIGNAL call makes sure we don't interrupt the interrupt
{
	TCNT1 = 0x10000 - (F_CPU/1024/10);	// Reset the timer
	
	// Timeout mechanism, just in case the router fails to respond to a button press for 
	// which I expect a reply
	if(gWaitingForReply && gTimeOutCounter < 20)
	{
		gTimeOutCounter++;
	}
	else
	{
		// Timeout occurred
		gWaitingForReply = FALSE;
		gTimeOutCounter=0;
	}
	
	// Don't process button presses while I am are waiting for a reply
	if(!gWaitingForReply)
	{
		// When I am playing, handle button presses accordingly
		if(gPlayerMode == PM_PLAYING)
		{
			if(processButtonPress(UPBUTTON, UPBUTTONPIN) == TRUE)
			{
				sendCommand("cmd:volup\n");  
			}
			if(processButtonPress(DOWNBUTTON, DOWNBUTTONPIN) == TRUE)
			{
				sendCommand("cmd:voldown\n");
			}
			if(processButtonPress(LEFTBUTTON, LEFTBUTTONPIN) == TRUE)
			{
				sendCommand("cmd:prev\n");
			}
			if(processButtonPress(RIGHTBUTTON, RIGHTBUTTONPIN) == TRUE)
			{
				sendCommand("cmd:next\n");
			}
			if(processButtonPress(SWITCHBUTTON, SWITCHBUTTONPIN) == TRUE)
			{
				gPlayerMode = PM_BROWSING;

				// Retrieve the first list of items to show
				gCurrentListStartIndex = 0;
				gCurrentListSelectedIndex = 0;
				sendCommand("cmd:getfirsttracks\n");  
				gWaitingForReply = TRUE;
			}
		}
		// When I am browsing, handle button presses accordingly
		else if(gPlayerMode == PM_BROWSING)
		{
			if(processButtonPress(UPBUTTON, UPBUTTONPIN) == TRUE)
			{
				// Move up in the current sublist of 4 until I hit the top.
				gCurrentListSelectedIndex--;
				if(gCurrentListSelectedIndex == -1 && gCurrentListStartIndex > 0)
				{
					// Move up 4 places in the total list, if this is still possible
					gCurrentListStartIndex -= 4;
					if(gCurrentListStartIndex < 0)
					{
						gCurrentListStartIndex = 0;
					}
					
					// Switching to the "previous" page, so set the bottom entry as selected
					gCurrentListSelectedIndex = 3;
					
					// Retrieve the new list of items to show
					sendCommandParams("gettracks", gCurrentListStartIndex, gCurrentListSelectedIndex);		
					gWaitingForReply = TRUE;
				}
				else
				{
					displayDirEntries();
				}
			}
				
			if(processButtonPress(DOWNBUTTON, DOWNBUTTONPIN) == TRUE)
			{
				// Move down in the current sublist of 4 until I hit the bottom.
				gCurrentListSelectedIndex++;
				
				if(gCurrentListSelectedIndex == 4)
				{
					// Move down 4 places in the total list
					gCurrentListStartIndex += 4;
					
					// Switching to the "next" page, so set the top entry as selected
					gCurrentListSelectedIndex = 0;
					
					// Retrieve the new list of items to show
					sendCommandParams("gettracks", gCurrentListStartIndex, gCurrentListSelectedIndex);		
					gWaitingForReply = TRUE;
				}	
				else
				{
					// In case of a sublist shorter than 4 items, make sure I don't go past the last item
					if(gCurrentListSelectedIndex >= gNumDirEntries)
					{
						gCurrentListSelectedIndex = gNumDirEntries - 1;
					}

					displayDirEntries();
				}
			}
			
			if(processButtonPress(LEFTBUTTON, LEFTBUTTONPIN) == TRUE)
			{
				sendCommand("cmd:dirup\n");
				gCurrentListStartIndex = 0;
				gCurrentListSelectedIndex = 0;
				gWaitingForReply = TRUE;
			}
			
			if(processButtonPress(RIGHTBUTTON, RIGHTBUTTONPIN) == TRUE)
			{
				sendCommandParams("dirdown", gCurrentListStartIndex, gCurrentListSelectedIndex);		
				gCurrentListStartIndex = 0;
				gCurrentListSelectedIndex = 0;
				gWaitingForReply = TRUE;
			}
			
			if(processButtonPress(ENTERBUTTON, ENTERBUTTONPIN) == TRUE)
			{
				gPlayerMode = PM_PLAYING;
				sendCommandParams("play", gCurrentListStartIndex, gCurrentListSelectedIndex);		
			}

			if(processButtonPress(SWITCHBUTTON, SWITCHBUTTONPIN) == TRUE)
			{
				gPlayerMode = PM_PLAYING;
				sendCommand("cmd:loadstreams\n");
			}
		}
	}
}

// Send a command with 2 parameters through the serial port 
void sendCommandParams(char* cmd, int param1, int param2)
{
	char stringBuffer[40];
	sprintf(stringBuffer, "cmd:%s %d %d\n", cmd, param1, param2);
	sendCommand(stringBuffer);		
}

// Handle a button press. This returns TRUE when a new button press on the selected pin is detected 
// Once a button press is detected, the function returns FALSE, until the button is released and 
// pressed again
BOOL processButtonPress(int buttonIndex, int buttonPin)
{
	BOOL result = FALSE;
	
	if(!(gButtonPressSeen & (1 << buttonIndex)))
	{
		if(!(buttonPin))
		{
			gButtonPressSeen |=  (1 << buttonIndex);
			result=TRUE;
		}
	}
	else
	{
		if((buttonPin))
		{
			gButtonPressSeen &=  !(1 << buttonIndex);
		}
		
	}
	
	return result;
}

// Send a string to the router through the serial port
void sendCommand(char* command)
{
	sprintf(serTXbuffer, command);	// Place command in send buffer
	putstring(serTXbuffer);	// transmit over serial link		
}

// Main function. Apart from some initialization, this function contains
// and endless loop which handles messages received from the router over 
// the serial line
int main(void)
{
	char artist[STR_LEN];	// Artist name
	char title[STR_LEN];	// Title of current song
	int playlistLength;		// Length of the playlist
	int songNum;			// song number in the current playlist
	int songTime;			// Total length of the song in seconds
	int songElapsed;		// Elapsed time within the song in seconds
	
    ioinit();		// Setup IO pins and defaults
    inituart();		// initialize AVR serial port (USART0)

	// Blink once to indicate succesful startup
	PORTC = 0b0100000;
	_delay_ms(100);
	PORTC = 0x00;   // All LED's are off

    // initialize LCD display
    lcd_init(LCD_DISP_ON);

    // Display splash screen
    lcd_clrscr();	
    lcd_puts("    MPD Boombox\n   Jeroen Bouwens\n Sponsored by Sioux\n  Embedded Systems");
    _delay_ms(2000);
	
	// Initialize variables and the timer
	gButtonPressSeen = 0;
	gPlayerMode = PM_PLAYING;
	init_timer1();
	sei();		// enable interrupts
    
    // Main program loop
    for(;;) // Loop forever
	{	
		// Grab a line of data from the serial port
		// Note: program execution will stall until serial data is received!
		getline(serRXbuffer);		

		// Blink once when a message was received
		PORTC |= 0b0100000;
		_delay_ms(100);
		PORTC &= 0b1011111;
			
		// If I am playing, show track info
		if(gPlayerMode == PM_PLAYING)
		{
			processPlayingLine(serRXbuffer, artist, title, &playlistLength, &songNum, &songTime, &songElapsed);
				
			lcd_clrscr();	// clear screen

			displayTime(songElapsed, playlistLength, songNum);
			displayProgressBar(songTime, songElapsed);
			displayTrackInfo(title, artist);
		}
		// If I am browsing 
		else
		{
			if(processResponse(serRXbuffer) == TRUE)
			{
				displayDirEntries();
			}
		}
    }
	 
    return 0;   // Never reached
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
    DDRC  |= 0b0100000; // PC5 is output
	
	DDRB  &= 0b11111000; // PB0, 1, 2 are input
	PORTB |= 0b00000111; // Enable internal pull-up resistors	

	DDRD  &= 0b00011111; // PB5, 6, 7 are input
	PORTD |= 0b11100000; // Enable internal pull-up resistors
}

void putstring(char *buffer)	// send a string to the UART
{
	int bufpos = 0;
	
	cli();	// disable interrupts so we don't mangle the data
	
	// start sending characters over the serial port until we reach the end of the string
	while ((bufpos < (SER_BUFF_LEN - 1)) && (buffer[bufpos] != '\0')) 
	{
		uart_putchar(buffer[bufpos]);
			
		bufpos++;
	}
	
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

// Process a message in the response format
BOOL processResponse(char *RXserbuffer)
{
	// The following code assumes the message has the following format:
	//		resp: param1,param2,param3,param4
	// It wil store the params in the gDirEntries array. Less than 4 params
	// will also work
	// NOTE: All params, including the last one, must be followed by a comma

	// Check if this is a response message
	char *responsePtr = strstr(RXserbuffer, "resp: ");
	if(responsePtr)
	{
		gNumDirEntries = 0;
		char *respStart = responsePtr + sizeof("resp: ") - 1; // Skip the indentifier part
		char *commaPtr = strstr(respStart, ","); 			   // Find the comma separating param1 and param2
		
		// Assume there are 4 parameters. If less than 4 are found, fill in blanks
		for(int i=0; i<4; i++)
		{
			if(commaPtr)
			{
				int strLen = commaPtr - respStart; 
				strncpy(gDirEntries[i], respStart , strLen);
				gDirEntries[i][strLen] = '\0';		// Make the string zero-terminated
				respStart = commaPtr+1;
				commaPtr = strstr(respStart, ",");
				gNumDirEntries++; 					// Some administration
			}
			else
			{
				gDirEntries[i][0] = '\0';	
			}
		}		
		gWaitingForReply = FALSE;
		return TRUE;
	}
	
	return FALSE;
}

// Process a message in the track information format
void processPlayingLine(char *RXserbuffer, char *artist, char *title, 
						int *playlistLength, int *songNum, int *songTime, int *songElapsed)
{
	// The following code assumes the message has one of the following two formats:
	//
	// 	Artis: <artist> Title: <title> playlistlength: <playlistlength> song: <song> time: <time>
	//
	//  Title: <title> Name: <name> playlistlength: <playlistlength> song: <song> time: <time>
	//
	// Time is in the format <elapsedSeconds>:<totalDurationSeconds>
	// The order of the fields DOES MATTER! 

	char *artistPtr 	= strstr(RXserbuffer, "Artist: ");
	char *titlePtr 		= strstr(RXserbuffer, "Title: ");
	char *namePtr		= strstr(RXserbuffer, "Name: ");
	char *plLengthPtr	= strstr(RXserbuffer, "playlistlength: ");
	char *songPtr 		= strstr(RXserbuffer, "song: ");
	char *timePtr 		= strstr(RXserbuffer, "time: ");

	int stringLength = 0;	

	if(artistPtr && titlePtr)
	{
		char *artistStart = artistPtr + sizeof("Artist: ") - 1;
		stringLength = titlePtr - (artistPtr + sizeof("Artist: ") - 1);
		stringLength = stringLength > 20 ? 20 : stringLength;
		strncpy(artist, artistStart , stringLength);
	}
	artist[stringLength] = '\0';
	
	stringLength = 0;
	if(titlePtr && namePtr) // If we're playing a stream the "artist" field will not be present and there will be a "name" field between title and playlistlength
	{
		char *titleStart = titlePtr + sizeof("Title: ") - 1;			// Skip the field name
		stringLength = namePtr - (titlePtr + sizeof("Title: ") - 1);	// Determine the length of the field value
		stringLength = stringLength > 20 ? 20 : stringLength; 			// Truncate to the maximum width of the display
		strncpy(title, titleStart, stringLength);						// Copy the field value to the appropriate buffer
	}
	else if(titlePtr && plLengthPtr) // There's no "name" field, so we're playing an MP3 and there will be a playlistlength field after the title
	{
		char *titleStart = titlePtr + sizeof("Title: ") - 1;
		stringLength = plLengthPtr - (titlePtr + sizeof("Title: ") - 1);
		stringLength = stringLength > 20 ? 20 : stringLength;
		strncpy(title, titleStart, stringLength);
		
	}
	title[stringLength] = '\0';
	
	if(namePtr && plLengthPtr)
	{
		char *nameStart = namePtr + sizeof("Name: ") - 1;
		stringLength = plLengthPtr - (namePtr + sizeof("Name: ") - 1);
		stringLength = stringLength > 20 ? 20 : stringLength;
		strncpy(artist, nameStart , stringLength);
		artist[stringLength] = '\0';
	}
	
	if(plLengthPtr && songPtr)
	{
		char *pllStart = plLengthPtr + sizeof("playlistlength: ") - 1;
		stringLength = songPtr - (plLengthPtr + sizeof("playlistlength: ") - 1);
		pllStart[stringLength] = '\0';
		*playlistLength = atoi(pllStart);
	}
	
	if(songPtr && timePtr)
	{
		char *songStart = songPtr + sizeof("song: ") - 1;
		stringLength = timePtr - (songPtr + sizeof("song: ") - 1);
		songStart[stringLength] = '\0';
		*songNum = atoi(songStart) + 1;
	}
	
	if(timePtr)
	{
		char *timeStart = timePtr + sizeof("time: ") - 1;
		char *songTotalTime = strstr(timeStart, ":") + 1;
		*(songTotalTime-1) = '\0'; // Insert a terminating \0 to separate the elapsed time from the total time
		*songElapsed = atoi(timeStart);
		*songTime = atoi(songTotalTime);
	}	
}

// Display the track name and artist, or the stream name and track name
void displayTrackInfo(char *trackName, char *artistName)
{

	lcd_gotoxy(10-strlen(artistName)/2,1);
	lcd_puts(artistName);

	lcd_gotoxy(10-strlen(trackName)/2,2);
	lcd_puts(trackName);
} 

// Display the progress bar that indicates the percentage of a track that has 
// elapsed. Note that this will have no function when playing a stream, since
// this function needs to know how long a track lasts, which is unknown for a 
// stream
void displayProgressBar(int songLength, int songElapsed)
{
	if(songLength > 0)
	{
		// Elapsed part
		for(int i=0; i<((songElapsed*100)/songLength)/5; i++)
		{
			lcd_gotoxy(i, 3);
			lcd_putc('#');
		}
		
		// Still remaining part
		for(int i=((songElapsed*100)/songLength)/5; i<20; i++)
		{
			lcd_gotoxy(i, 3);
			lcd_putc('-');
		}
	}
	else
	{
		// Playing a stream, fill with '-'s
		for(int i=0; i<20; i++)
		{
			lcd_gotoxy(i, 3);
			lcd_putc('-');
		}		
	}
}

// Display the track elapsed time, and the playlist info (position in playlist + playlist length)
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

// Display the (at most) 4 directory entries received from the router, and indicate which
// is the currently selected one.
void displayDirEntries()
{
	lcd_clrscr();
			
	for(int y=0; y<4; y++)
	{
		lcd_gotoxy(1, y);
		lcd_puts(gDirEntries[y]);
		
		if(y == gCurrentListSelectedIndex)
		{
			lcd_gotoxy(0, y);
			lcd_puts(">");
		}
	}
}
