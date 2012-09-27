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
//   * ### : Pin ID. Can be D00..D13 Digital 0-13. 
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
int activityLED = 6;
bool ledState;

MilliTimer sendTimer;
byte needToSend;

void setup ()
{
  Serial.begin(57600);
  Serial.println("Winode RF12 Server");
  rf12_initialize(nodeID, RF12_868MHZ, 33);
  pinMode(activityLED, OUTPUT);     
  ledState = false;
  digitalWrite(activityLED, HIGH);  
}

boolean parsePayload(volatile uint8_t* data, int len)
{
  if(strncmp((const char *)data, "CMDD", 4) == 0)
  {
    char pinID[3];
    strncpy(pinID, (const char*)data+4, 2);      
    pinID[2] = '\0';
    
    char value[4];
    strncpy(value, (const char*)data+6, 3);      
    value[3] = '\0';
    
    int pinNr = atoi(pinID);
    switch(pinNr)
    {
      case 0:
      case 1:
      case 2:
      case 4:
      case 7:
      case 8:
      case 12:
      case 13:
        digitalWrite(pinNr, (atoi(value) == 0) ? LOW : HIGH);
      break;  

      case 3:
      case 5:
      case 6:
      case 9:
      case 10:
      case 11:
        analogWrite(pinNr, atoi(value));
      break;
    }
    Serial.println("parsed: ");
  }
  else
  {
    Serial.println("couldn't parse");
  }
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
    Serial.println("Received");
    parsePayload(rf12_data, rf12_len);
  }

    if (sendTimer.poll(100))
    {
        needToSend = 1;
    }

    if (needToSend && rf12_canSend()) 
    {
        needToSend = 0;        
        broadcastInputValues(); 
    }      
}


