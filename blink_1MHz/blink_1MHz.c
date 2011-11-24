/*
    5-10-07
    Copyright Spark Fun Electronics© 2007
    Nathan Seidle
    nathan at sparkfun.com
    
    ATmega168
	
	Example Blink
	Toggles all IO pins at 1Hz
*/

#include <avr/io.h>

//Define functions
//======================
void ioinit(void);      //Initializes IO
void delay_ms(uint16_t x); //General purpose delay
//======================

int main (void)
{
    ioinit(); //Setup IO pins and defaults

    DDRB &= 0b11111101; // B1 is input
	PORTB |= 0b00000010; // Enable internal pull-up resistor


    while(1)
    {
		if(!(PINB & 2))
		{
			PORTC = 0b0001000;
			delay_ms(100);

			PORTC = 0b0010000;
			delay_ms(100);

			PORTC = 0b0100000;
			delay_ms(100);
		}
		else
		{
			PORTC = 0b0100000;
			delay_ms(50);

			PORTC = 0b0010000;
			delay_ms(150);
			PORTC = 0b0001000;
			delay_ms(100);
		}
    }
   
    return(0);
}

void ioinit (void)
{
    //1 = output, 0 = input
    DDRB = 0b11111111; //All outputs
    DDRC = 0b11111111; //All outputs
    DDRD = 0b11111110; //PORTD (RX on PD0)
}

//General short delays
void delay_ms(uint16_t x)
{
  uint8_t y, z;
  for ( ; x > 0 ; x--){
    for ( y = 0 ; y < 90 ; y++){
      for ( z = 0 ; z < 6 ; z++){
        asm volatile ("nop");
      }
    }
  }
}