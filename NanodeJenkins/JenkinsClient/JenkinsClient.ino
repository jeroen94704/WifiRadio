// Demo using DHCP and DNS to perform a web client request.
// 2011-06-08 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

#include <EtherCard.h>
#include <NanodeMAC.h>

// Pachube stuff
#define FEED    "53426"
#define APIKEY  "8w03zvQrraU_iSe0fIDWKY_rDE-SAKxLWU1YMGhSdEVNND0g"
char pachubeSite[] PROGMEM = "api.pachube.com";

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0,0,0,0,0,0 };
NanodeMAC mac( mymac );

byte Ethernet::buffer[700];
bool dnsLookupSucceeded = false;
bool alternativeSite = true;
bool sendingToPachube = false;
Stash stash;

char website1[] PROGMEM = "p8tch.jeroen.ws";
int port1 = 80;

char website2[] PROGMEM = "macawi";
int port2 = 8080;


// Implements an RGB color triplet 
class RGBColor
{
public:  
  // Initialize the RGB triplet to he provided values
  RGBColor(byte aR, byte aG, byte aB) :
    R(aR),
    G(aG),
    B(aB)
  {
  }
  
  // Assignment
  RGBColor& operator=(const RGBColor& rhs)
  {
    R = rhs.R;
    G = rhs.G;
    B = rhs.B;
    return *this;
  }

  byte R;
  byte G;
  byte B;
};

// Class to control an RGB LED connected to 3 pins.
class RGBLed
{
public:
  // Initialize the LED using the provided pins
  RGBLed(byte rPin, byte gPin, byte bPin);
  
  // Immediately set the color to the provided RGB value
  void setTo(RGBColor newColor);
  
  // Fade from the current color to a new color in approximately 1 second
  void fadeTo(RGBColor newColor);
  
  // Switch the LED off
  void off();
  
  // Switch the LED on, back to its last color
  void on();

private:
  void setToCurrent();
  RGBColor currentColor;
  byte redPin;
  byte greenPin;
  byte bluePin;
};

RGBLed::RGBLed(byte rPin, byte gPin, byte bPin) :
  redPin(rPin),
  greenPin(gPin),
  bluePin(bPin),
  currentColor(0,0,0)
{
  pinMode(redPin, OUTPUT);      // sets the digital pin as output
  pinMode(greenPin, OUTPUT);      // sets the digital pin as output
  pinMode(bluePin, OUTPUT);      // sets the digital pin as output

  setToCurrent();
}

void RGBLed::setToCurrent()
{
  analogWrite(redPin, 255-currentColor.R);
  analogWrite(greenPin, 255-currentColor.G);
  analogWrite(bluePin, 255-currentColor.B);    
}

void RGBLed::setTo(RGBColor newColor)
{
  currentColor = newColor;
  setToCurrent();
}

void RGBLed::fadeTo(RGBColor newColor)
{
  // Linear interpolation from currentColor to newColor, using fixed point with a multiplier of 256
  
  int rStep =  newColor.R - currentColor.R;
  int gStep =  newColor.G - currentColor.G;
  int bStep =  newColor.B - currentColor.B;
  
  int rValFP = currentColor.R << 8;
  int gValFP = currentColor.G << 8;
  int bValFP = currentColor.B << 8;
  
  for(int i=0; i<255; i++)
  {
    currentColor.R = rValFP >> 8;
    currentColor.G = gValFP >> 8;
    currentColor.B = bValFP >> 8;
    setToCurrent();
    
    rValFP += rStep;
    gValFP += gStep;
    bValFP += bStep;
    
    delay(10);
  }
  
  currentColor = newColor;
  setToCurrent();
}

void RGBLed::off()
{
  analogWrite(redPin, 255);
  analogWrite(greenPin, 255);
  analogWrite(bluePin, 255);
}

void RGBLed::on()
{
  setToCurrent();
}

#define BUILDFAILED RGBColor(255, 0, 0)
#define BUILDSUCCEEDED RGBColor(0, 255, 0)
#define BUILDING RGBColor(0, 0, 255)

enum BuildStatus 
{
  Unknown,
  Succeeded,
  Failed,
  Building
};

namespace JenkinsMonitor
{

  BuildStatus currentStatus;
  RGBLed buildStatusLED(5,6,9);
  uint32_t timer;

  void signalError()
  {
    for(int i=0; i<2; i++)
    {
      buildStatusLED.setTo(BUILDING);
      delay(333);
      buildStatusLED.setTo(BUILDFAILED);
      delay(333);
      buildStatusLED.setTo(BUILDSUCCEEDED);
      delay(333);
    }
  }
  
    // called when the client request is complete
  static void my_callback (byte status, word off, word len)
  {
    if(status == 0)
    {
      Ethernet::buffer[off+len] = '\0';
      
      if(strstr((const char*)Ethernet::buffer + off, "activity=\"Building\"") != 0)
      {
        currentStatus = Building;
        buildStatusLED.setTo(BUILDING);
      }
      else if(strstr((const char*)Ethernet::buffer + off, "lastBuildStatus=\"Failure\"") != 0 && currentStatus != Failed)
      {
        currentStatus = Failed;
        buildStatusLED.setTo(BUILDFAILED);
        delay(500);
        buildStatusLED.off();
        delay(500);
        buildStatusLED.on();
        delay(500);
        buildStatusLED.off();
        delay(500);
        buildStatusLED.on();
      }
      else if(strstr((const char*)Ethernet::buffer + off, "lastBuildStatus=\"Success\"") != 0 && currentStatus != Succeeded)
      {
        currentStatus = Succeeded;
        buildStatusLED.setTo(BUILDSUCCEEDED);
      }
    }
  }
  
  void Process()
  {
    if (millis() > timer) 
    {
      timer = millis() + 5000;
      if(dnsLookupSucceeded)
      {
        ether.browseUrl(PSTR("/"), "cc.xml", alternativeSite ? website1 : website2, my_callback);
      }      
      else
      {
        signalError();
      }
    }
  }
  
  void Initialize()
  {
    timer = millis();
    currentStatus = Unknown;
  }
}

void printIPToPachube()
{
  if(ether.dnsLookup(pachubeSite))
  {
    // generate the IP address as payload - by using a separate stash,
    // we can determine the size of the generated message ahead of time
    byte sd = stash.create();
    stash.print("IP,");
    char buf[16];
    sprintf(buf, "%d.%d.%d.%d", ether.myip[0],ether.myip[1],ether.myip[2],ether.myip[3]);
    stash.println(buf);
    stash.save();
    
    // generate the header with payload - note that the stash size is used,
    // and that a "stash descriptor" is passed in as argument using "$H"
    Stash::prepare(PSTR("PUT http://$F/v2/feeds/$F.csv HTTP/1.0" "\r\n"
                        "Host: $F" "\r\n"
                        "X-PachubeApiKey: $F" "\r\n"
                        "Content-Length: $D" "\r\n"
                        "\r\n"
                        "$H"),
            pachubeSite, PSTR(FEED), pachubeSite, PSTR(APIKEY), stash.size(), sd);

    // send the packet - this also releases all stash buffers once done
    ether.tcpSend();
    sendingToPachube = true;
  }
  
  // Process the packetloop for a while to ensure the request gets processed properly
  JenkinsMonitor::timer = millis();
  while(millis() < JenkinsMonitor::timer+10000)
  {
    ether.packetLoop(ether.packetReceive());
  }
}

BufferFiller bfill;

namespace WebServer
{
  void switchSites()
  {
    dnsLookupSucceeded = ether.dnsLookup(alternativeSite ? website2 : website1);
    if(dnsLookupSucceeded)
    {
      EtherCard::hisport = alternativeSite ? port1 : port2;
      alternativeSite = !alternativeSite;
    }
    else
    {
      // revert back to the old state
      dnsLookupSucceeded = ether.dnsLookup(alternativeSite ? website1 : website2);
    }
  }
  
  void generatePage(BufferFiller& buf)
  {
    buf.emit_p(PSTR(
"HTTP/1.0 200 OK\r\n"
"Content-Type: text/html\r\n"
"Pragma: no-cache\r\n"
"\r\n"
"<html>"
  "<head><title>"
    "Configure"
  "</title></head>"
  "<body>"
    "<h3>The current cc.xml URL is:</h3>"
    "<p>http://$F/cc.xml, port $D</p>"
    "<p>Click <a href='/switch'>here</a> to switch</p>"
  "</body>"
"</html>"), alternativeSite ? website1 : website2, alternativeSite ? port1 : port2);
  }
  
  void Process(word len, word pos)
  {
    if(pos)
    {
      bfill = ether.tcpOffset() ;
      char *data = (char *) Ethernet::buffer + pos ;
      if (strncmp("GET /switch ", data, 6) == 0)
      {
        switchSites();
      }
      
      generatePage(bfill);
      ether.httpServerReply(bfill.position());
    }
  }
}

void setup () 
{
  JenkinsMonitor::Initialize();
  
  ether.begin(sizeof Ethernet::buffer, mymac);
  ether.dhcpSetup();

  printIPToPachube();

  EtherCard::hisport = alternativeSite ? port1 : port2;
  dnsLookupSucceeded = ether.dnsLookup(alternativeSite ? website1 : website2);
}

void loop () 
{
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  WebServer::Process(len, pos);
  JenkinsMonitor::Process();
}
