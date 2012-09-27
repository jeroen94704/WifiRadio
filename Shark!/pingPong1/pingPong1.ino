// Demo of a sketch which sends and receives packets
// 2010-05-17 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

// with thanks to Peter G for creating a test sketch and pointing out the issue
// see http://news.jeelabs.org/2010/05/20/a-subtle-rf12-detail/

#include <JeeLib.h>

#define WIMIN
//#define NANODE

#ifdef NANODE
int rxLed = 5;
int txLed = 6;
bool invertLed = true;
#endif

#ifdef WIMIN
int rxLed = 6;
int txLed = 6;
bool invertLed = false;
#endif

int nodeID = 2;
MilliTimer sendTimer;
char payload[] = "Hello!";
byte needToSend;

void sendLed(bool on)
{
  digitalWrite(txLed, (invertLed ? on : !on) ? LOW : HIGH);  
}

void recvLed(bool on)
{
  digitalWrite(rxLed, (invertLed ? on : !on) ? LOW : HIGH);  
}

void setup () 
{
    Serial.begin(57600);
    Serial.println(57600);
    Serial.println("Send and Receive");
    rf12_initialize(nodeID, RF12_868MHZ, 33);

    pinMode(rxLed, OUTPUT);     
    pinMode(txLed, OUTPUT);
  
    sendLed(false);
    recvLed(false);
}

void loop () 
{
    if (rf12_recvDone() && rf12_crc == 0) 
    {
        recvLed(true);
        Serial.print("OK ");
        for (byte i = 0; i < rf12_len; ++i)
            Serial.print(rf12_data[i]);
        Serial.println();
        delay(100); // otherwise led blinking isn't visible
        recvLed(false);
    }
    
    if (sendTimer.poll(500))
    {
        needToSend = 1;
    }

    if (needToSend && rf12_canSend()) 
    {
        needToSend = 0;
        
        sendLed(true);
        rf12_sendStart(0, payload, sizeof payload);
        Serial.println("Send started");
        delay(100); // otherwise led blinking isn't visible
        sendLed(false);
    }
}
