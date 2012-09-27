// (c) 2012 Jeroen Bouwens
// 
// Server-side sketch, to be run on a Winode, allowing control of said  Winode over the RF12 
// wireless connection
//
// Supported functions:
//    - Set state of digital output pins
//    - Set value of PWM output pins
//    - Values of Analog and Digital input pins are periodically broadcast
//
// WARNING: No security whatsoever is implemented in the wireless protocol, 
// meaning anyone can sniff out the protocol and/or take over control 
//
// ## Protocol description:
// 
// Accepted messages are of the form:
//
// +---+---+---+
// |CMD|###|XXX|
// +---+---+---+
//
// Where:
//   * CMD : Command. Accepted values are "DWR" (Digital write) or "AWR" (Analog write, i.e. PWM)
//   * ### : Pin ID. Can be D0..D13 Digital 0-13. 
//   * XXX : Value. For PWM pins, this can be any value from 000-255. For digital output pins, this is either 000 or 255.
//
// Note: Only pins 3, 5, 6, 9, 10, and 11 support analog write (PWM). 
//
// The module periodically broadcasts the values for all input pins, both analog and digital. The messages it 
// sends are of the form:
//
// +---+---+
// |###|XXX|
// +---+---+
//
// Where:
//   * ### : Pin ID. Can be D0..D13 or A0..A5 for Digital 0-13 or Analog 0-5
//   * XXX : Value. For digital input pins, this is either 000 or 255. For analog input pins, this can be any value from 000 or 255.
// 


#include <JeeLib.h>

#define DEBUG 1 // Print debug messages over the serial line

int nodeID = 1;

boolean digitalInput[] = 
{
  false, false, false, false,
  false, false, false , false,
  false, false, false, false,
  false, false
};

boolean analogInput[] = { true, true, false, false, false, false };

char txBuff[6];

void setup ()
{
  Serial.begin(57600);
  Serial.println("Winode RF12 Server");
  rf12_initialize(nodeID, RF12_868MHZ, 33);
}

boolean parsePayload(volatile uint8_t* data, int len)
{

}

void broadcastInputValues()
{
  for(int i=0; i < 14; i++)
  {
    if(digitalInput[i])
    {
      // Broadcast message containing value of analog pin X
      boolean value = digitalRead(i);
      sprintf(txBuff, "A%02d%03d\0", i, value);
#ifdef DEBUG
      Serial.print("Sending message: ");
      Serial.print(txBuff);
      Serial.print(". Length = ");
      Serial.println(sizeof txBuff, DEC);
#endif
      rf12_sendStart(0, txBuff, sizeof txBuff);
      rf12_sendWait(0);
      rf12_recvDone();
    }
  }

  for(int i=0; i < 6; i++)
  {
    if(analogInput[i] && rf12_canSend())
    {
      // Broadcast message containing value of analog pin X
      int value = analogRead(i);
      sprintf(txBuff, "A%02d%03d\0", i, value);
#ifdef DEBUG
      Serial.print("Sending message: ");
      Serial.print(txBuff);
      Serial.print(". Length = ");
      Serial.println(sizeof txBuff, DEC);
#endif
      rf12_sendStart(0, txBuff, sizeof txBuff);
      rf12_sendWait(0);
      rf12_recvDone();
    }
  }

}

void loop () 
{
  if (rf12_recvDone() && rf12_crc == 0) 
  {
    Serial.print("Received");
    parsePayload(rf12_data, rf12_len);
  }

  broadcastInputValues();
  
  delay(100);
}


