/***************************************************
  eLua based Adafruit IO synced notification light.
  
  eLua is a modified version of Lua with some quirks, built to run on NodeMCU hardware (Opensource IoT hardware)
  
  Built originally for use with viDiscordServer discord bot.
  
  Reads a "lightbyte" variable from Adafruit.io (MQTT protocol) and flashes LED accordingly.

  Can be set to use NeoPixel stick or a single RGB LED
 ****************************************************/
#include <ESP8266WiFi.h>
#include <math.h>

//Needed for Adafruit.io connection
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

//Needed for Neopixels
#include <Adafruit_NeoPixel.h>

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "WIFI_SSID_HERE"
#define WLAN_PASS       "WIFI_PASSWORD_HERE"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "ADAFRUIT_IO_USERNAME"
#define AIO_KEY         "ADAFRUIT_IO_FEEDKEY"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'viMonitorFeed' for subscribing to changes.
Adafruit_MQTT_Subscribe viMonitorFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/feedname");

/************************* Discord Monitor  *********************************/

//Debugging Mode:
bool Debugging = false;

//Hardware Config:
/*
    Currently you must only have one of the three following options set to true:
    FourPinRGBLEDMode, fully functional.
    NeoPixelStickMode, partially implemented, untested.
    NeoPixelSingleMode, unimplimented
*/

  //FourPinRGBLED Setup
  bool FourPinRGBLEDMode = true;
  //Pins:
  const int RPIN = 5; //(D1 on NodeMCU)
  const int GPIN = 4; //(D2 on NodeMCU)
  const int BPIN = 0; //(D3 on NodeMCU)
  float BrightnessMultiplier = 25000; //For certain kind of bulb lightings.

  //NeoPixelStick Setup
  //For this mode to work may also need a 5v pin (tested on NodeMCU 3v3 (3 Volt pin) and also worked.)
  bool NeoPixelStickMode = false;
  const int NeoPixelPin = 5; //(D1 on NodeMCU)
  const int NumStickLEDS = 8;
  const int NeoPixStickBrightness = 120;
  const int NeoPixStickDelay = 50;
  Adafruit_NeoPixel myNeoStick(NumStickLEDS, NeoPixelPin, NEO_GRB + NEO_KHZ800);
  
  //For a single neopixel LED
  bool NeoPixelSingleMode = false;

//Initialize global variable LightByte
String LightByteOriginalVal = "00000000";
String LightByte = "00000000";
int LightByteLength = LightByte.length();

int CurrentLight = 1; //Start out at this for first run to avoid errors between currentlight/previous light.

float Brightness = .04;


unsigned long previousMillis = 0;
const long interval = 1000; //Timer rate in milliseconds

/*************************** Sketch Code ************************************/

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  Serial.begin(115200);
  delay(10);
  
  //FourPinRGBLEDSetup:
  if(FourPinRGBLEDMode){
      pinMode(RPIN, OUTPUT);
      pinMode(GPIN, OUTPUT);
      pinMode(BPIN, OUTPUT);
  }
  
  if(NeoPixelStickMode){
    myNeoStick.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
    myNeoStick.show();            // Turn OFF all pixels ASAP
    myNeoStick.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)
  }
  
  Serial.println("Global LightByte.length = " + String(LightByteLength));

  Serial.println(F("Adafruit MQTT demo"));

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Setup MQTT subscription for viMonitorFeed feed.
  mqtt.subscribe(&viMonitorFeed);

  Serial.println("LightByte Init = " + String(LightByte.length()));
  
}

uint32_t x=0; // What is this?


void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &viMonitorFeed) {

      LightByte = (char *)viMonitorFeed.lastread;
      
      Serial.println("Got: " + String((char *)viMonitorFeed.lastread) + " from Adafruit.io.");
      

      
    }
  }
  
  //NeopixelStickMode
    if(NeoPixelStickMode){
      NeoPixelStickModeRun();
    }

  //Timer:
   unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      if (FourPinRGBLEDMode) {
        Serial.println("FourPinRGBLEDMode Mode");
        FourPinRGBLEDModeRun();
      }
      else if(NeoPixelSingleMode){
        Serial.print("NeoPixelSingleMode");
      }
      else{
        Serial.println("NeoPixelStickMode");
      }
    } 

  //End Timer

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  
}


//Remember LUA "Arrays" / "Tables" start at index 1 instead of 0.
void FourPinRGBLEDModeRun(){
//Scan through LightByte every x seconds and display next notification LED

int previousLight = CurrentLight; //Contains last lit light/where we started.

//Fixes first run issue where previousLight would be out of bounds:
if(previousLight == 0){
  previousLight = 1;
}

//Used to add one to current light right here

while(CurrentLight <= LightByteLength){

  //Debugging Output
  if(Debugging){
    Serial.println("LightByte = " + String(LightByte));
    Serial.println(String("Checking LightByte at Index: ") + String(CurrentLight));
    Serial.println("LightByte Value at Current Index = " + String(LightByte.substring(CurrentLight,CurrentLight+1)));
    Serial.println("CurrentLight = " + String(CurrentLight));
    Serial.println("previousLight = " + String(previousLight));
  }
  
  //Light Check
  if(LightByte.substring(CurrentLight,CurrentLight+1) == "1"){
  
    //Blinkt.setpixel(0,ColorsArray[1][1],ColorsArray[1][2],ColorsArray[1][3],Brightness)
    //Set LED on:
    if(FourPinRGBLEDMode){
      showLight(CurrentLight);
    }  
    Serial.println("Light " + String(CurrentLight) + " is on."); 
    CurrentLight += 1;
    break;
    }
    else  {
    CurrentLight += 1;
    
    if(CurrentLight > LightByteLength){
      CurrentLight = 0;
    }
    
    if(CurrentLight == previousLight and LightByte.substring(CurrentLight,CurrentLight+1) == "0"){ //Searched through all lights, and this one is also empty; Blank LED.
      Serial.println("No Lights");
      
      if(FourPinRGBLEDMode){
        OffFourPinRGBLED();
      }
      
      break;
    }
}
}
}

//Takes color index 0-7 and returns color RGB values at that index, colors are: ROYGBIVW respectively (R=1,O=2,Y=3,etc.)
void showLight (int ColorIndex){

  /**Color Reference:
  //Color Array Setup
  int ColorRed[]= {255, 0, 0}; //Red
  int ColorOrange[]= {255, 127, 0}; //Orange
  int ColorYellow[]= {255, 255, 0}; //Yellow
  int ColorGreen[]= {0, 255, 0}; //Green
  int ColorBlue[]= {0, 0, 255}; //Blue
  int ColorIndigo[]= {75, 0, 130}; //Indigo
  int ColorViolet[]= {148, 0, 211}; //Violet
  int ColorWhite[]= {255, 255, 255}; //White
  **/

  int myR = 0;
  int myG = 0;
  int myB = 0;

  switch(ColorIndex) {
  case 0: myR = 255; myG = 0; myB = 0; //Red
  break;
  case 1: myR = 255; myG = 127; myB = 0; //Orange
  break;
  case 2: myR = 255; myG = 255; myB = 0; //Yellow
  break;
  case 3: myR = 0; myG = 255; myB = 0; //Green
  break;
  case 4: myR = 0; myG = 0; myB = 255; //Blue
  break;
  case 5: myR = 75; myG = 0; myB = 130; //Indigo
  break;
  case 6: myR = 148; myG = 0; myB = 211; //Violet
  break;
  case 7: myR = 255; myG = 255; myB = 255; //White
  break;
  
  
  //Apply brightness multiplier (for bulb brightness, still experimenting.
  
  default: Serial.println("Out of index range, returning 0's");
           myR = 0; myG = 0; myB = 0;
  break;
  }
  
  if(FourPinRGBLEDMode){
    myR = myR * BrightnessMultiplier;
    myG = myG * BrightnessMultiplier;
    myB = myB * BrightnessMultiplier;
    LightFourPinRGBLED(myR,myG,myB);
  }

}

void LightFourPinRGBLED(int R, int G, int B){

  Serial.println("Lighting 4 Pin RGB LED with = R:" + String(R) + " G:" + String(G) + " B:" + String(B));

  analogWrite(RPIN, R);
  analogWrite(GPIN, G);
  analogWrite(BPIN, B); 

}

void OffFourPinRGBLED(){
  analogWrite(RPIN, 0);
  analogWrite(GPIN, 0);
  analogWrite(BPIN, 0); 
}


void NeoPixelStickModeRun(){
   
   for(int myIter = 0; myIter < LightByteLength ; myIter += 1)
   {
   
      Serial.println("myIter = " + String(myIter));   
      if (LightByte.substring(myIter,myIter+1) == "1"){
      
          /**Color Reference:
          //Color Array Setup
          int ColorRed[]= {255, 0, 0}; //Red
          int ColorOrange[]= {255, 127, 0}; //Orange
          int ColorYellow[]= {255, 255, 0}; //Yellow
          int ColorGreen[]= {0, 255, 0}; //Green
          int ColorBlue[]= {0, 0, 255}; //Blue
          int ColorIndigo[]= {75, 0, 130}; //Indigo
          int ColorViolet[]= {148, 0, 211}; //Violet
          int ColorWhite[]= {255, 255, 255}; //White
          **/

          int myR = 0;
          int myG = 0;
          int myB = 0;

          switch(myIter) {
          case 0: myR = 255; myG = 0; myB = 0; //Red
          break;
          case 1: myR = 255; myG = 127; myB = 0; //Orange
          break;
          case 2: myR = 255; myG = 255; myB = 0; //Yellow
          break;
          case 3: myR = 0; myG = 255; myB = 0; //Green
          break;
          case 4: myR = 0; myG = 0; myB = 255; //Blue
          break;
          case 5: myR = 75; myG = 0; myB = 130; //Indigo
          break;
          case 6: myR = 148; myG = 0; myB = 211; //Violet
          break;
          case 7: myR = 255; myG = 255; myB = 255; //White
          break;
          
          //Light up coressponding pixel with correct color.
          //Keeping this in loop sets color in RAM first then applys on myNeoStick.show();
          myNeoStick.setPixelColor(myIter, myNeoStick.Color(myR,myG,myB)); 
          
        }

        //Show lights
        myNeoStick.show();
      
      }
      else{
        //Not a "1" in this position, blank pixel.
        myNeoStick.setPixelColor(myIter, myNeoStick.Color(0,0,0)); 
      }  
      
   }
   
   myNeoStick.show();
   
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
