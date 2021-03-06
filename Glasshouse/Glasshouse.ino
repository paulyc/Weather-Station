#include <SPI.h>
#include <LoRa.h>
#include "LEDs.h"
#include "soil.h"
#include "BME280.h"
#include "soilTemp.h"
#include "counters.h"
#include <Arduino.h>
#include "batteryVolts.h"
#include "windSpeed.h"
#include "windVane.h"
#include "loraUtil.h"
#include "heartbeat.h"

int count = 0;
unsigned long previousMillisSensors = 0;
const long intervalSensors = 300000;              // Interval to take reading on all sensors other than wind.
unsigned long previousMillisWind = 0;
long intervalWind = 10000;                        // Interval to take reading on wind speed sensors.
unsigned long previousMillisVane = 0;
const long intervalVane = 6000;                  // Interval to take reading on wind vane sensors.
unsigned long previousMillisCallBack = 0;
const long intervalCallBack = 10000;              // Interval to allow time for callback to occurr before next wind reading thus preventing interrupt conficts.
unsigned long previousMillisSlowPrint = 0;
const long intervalSlowPrint = 100;
const byte interruptPinA = 7;
int rainValue = 0;
int rainValuex2 =0;
//float soilTemp =0;
String initiator = "";
//int z = -1;                     // This is the cycle counter and gets reset to zero on successful call back from the server recieving the data.
int callBackListeningStatus = 0;
int sleepStatus = 0;

////////////////////////////////////////////////////////////////////////////
  
void setup()
{
  setupHeartBeat();
  Serial.begin(96000);

  delay(2000);
  tone(3,1000,1000);
  Serial.println("Starting ..... ");
  setupSoilTemp();
  //Serial.println("LoRa Sender");
  //if (!LoRa.begin(868E6)) 
  //{
  //  Serial.println("Starting LoRa failed!");
  //  while (1);
  //}
  // locate devices on the bus
  loraSetup();
  LoRa.sleep();              // Sends the radio to sleep
  Serial.println("");

  setupLEDs();
  setupBME280();
  soilSetup();
  setupWindVane();
  windSpeedSetup();

    
  pinMode(interruptPinA, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPinA), rain, CHANGE);  // Triggers void rain().
 
  Serial.println(" ..... All is good ..... Continuing  .....");
  Serial.println(" ");

} // setup

void loop()
{
  heartBeat();
  if ( z==0)
  {
    rainValuex2 =0;
    rainValue =0;
  }

  //flashYellowLED();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisSensors >= intervalSensors)        // Need relatively low number of samples of temperatures, humidity etc as they dont change quickly.
  {
    Serial.print("..1..");
    previousMillisSensors = currentMillis;
    //tone(3,1000,1000);
    Serial.print("..2..");
    soilTempFunction();
    Serial.print("..3..");
    soil();            // Read soil moisture.
    Serial.print("..4..");
    
    digitalWrite(9, LOW);  // Reset the watchdog by turning off and on again
    Serial.print(".. Reset Watchdog ..");
    delay(1000);
    digitalWrite(9, HIGH);  // HIGH enables watchdog
    BME280Readings();
    digitalWrite(9, LOW);  // Reset the watchdog by trurning in off and on again
    Serial.print(".. Reset Watchdog again ..");
    delay(1000);
    digitalWrite(9, HIGH);  // HIGH enables watchdog
    
    Serial.print("..5..");
    batteryVolts();
    rainValue = rainValuex2/2;
    Serial.println("..6.. out of 6");
    printToSerial();
    //const char webAddress[100] = "http://www.goatindustries.co.uk/weather2/send.php?";
    String dataString = "";
    dataString = initiator + "tempout="+BME280Temperature + "&tempsoil="+soilTemp + "&moisture="+moisture + "&tempint="+hours + "&pressure="+pressure + "&humidity="+humidity + "&volts="+batteryVoltsValue + "&GLASSHOUSE";
    Serial.print("Sending packet: ");
    Serial.print(dataString);
    //Serial.print("  ");
    //Serial.println(count);
    //Serial.println("");
    // send packet
    LoRa.beginPacket();
    LoRa.print(dataString);
    //LoRa.print(count);
    LoRa.endPacket();
    Serial.println("");
    flashYellowLED();
    callBackListeningStatus = 1;                                         // Allow time to recieve callback or else interrupts may collide.
    previousMillisCallBack = currentMillis;
    
  }
// Allow time to recieve callback or else interrupts may collide.   
  if ( (currentMillis - previousMillisCallBack <= intervalCallBack) && (z>10)  )  // On successful callback z is reset to 0.
  {
    onReceive(LoRa.parsePacket());
    callBackListeningStatus = 1;
    sleepStatus = 0;
    slowPrint();                                                     // Prints "... Listening ..." every so often.
  }
  else
  {
    callBackListeningStatus = 0;
  }
  if (  (sleepStatus == 0)&&(callBackListeningStatus == 0)  )
  {
    LoRa.sleep();              // Sends the radio to sleep
    sleepStatus = 1;
    Serial.println("LoRa sent to sleep.");
  }
  
  if (  (currentMillis - previousMillisWind >= intervalWind) && (callBackListeningStatus == 0)  )            // Need relatively high number of wind samples to catch high wind gusts.
  {
    //Serial.print("callBackListeningStatus = ");
    //Serial.print(callBackListeningStatus);
    //Serial.print("  ");
    previousMillisWind = currentMillis;
    //windSpeed();
    counter();
    flashBlueLED();
  }
  if ( windSpeedValue < 5 )
  {
    intervalWind = 10000;    // If wind speed is very low, it needs more time to measure? 
  }
  else
  {
    intervalWind = 2000;
  }
  if (  (currentMillis - previousMillisVane >= intervalVane)  && (callBackListeningStatus == 0)  )            // Need relatively moderate number of wind vane samples as too many wastes power.
  {
    previousMillisVane = currentMillis;
    //windVane();
    //counter();
  }
} // loop


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void rain()
{
  //flashRedLED(); .... Cant use millis.
  rainValuex2 ++;
  Serial.print("Rain interupt: ");Serial.println(rainValuex2);
}
void printToSerial()
{
  unsigned long currentMillis = millis();
  Serial.println("");
  Serial.print("currentMillis / 1000: ");Serial.println(currentMillis/1000);
  Serial.print("Z: ");Serial.println(z); 
  Serial.print("Soil temp C: ");Serial.println(soilTemp);
  Serial.print("Soil moisture: "); Serial.print(moisture); Serial.println(" %");
  Serial.print("BME 260 temp: "); Serial.println(BME280Temperature);
  Serial.print("BME 260 pressure: "); Serial.println(pressure);
  Serial.print("BME 260 humidiy: "); Serial.println(humidity);
  Serial.print("Battery volts: ");Serial.println(batteryVoltsValue);
  Serial.print("Rain Value: ");Serial.println(rainValue);
  Serial.print("Average wind speed: ");Serial.println(knotsAv);
  Serial.print("Max wind gust: ");Serial.println(knotsMax);
  Serial.print("Wind vane value: ");Serial.println(vaneValue);
  Serial.println("");
}

void onReceive(int packetSize) 
{
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";
  String success = "success";

  while (LoRa.available()) 
  {
    incoming += (char)LoRa.read();
  }
  int foundpos = -1;
  for (int i = 0; i <= incoming.length() - success.length(); i++) 
  {
    if (incoming.substring(i,success.length()+i) == success) 
    {
      foundpos = i;
      Serial.println("");
      Serial.print("callBackListeningStatus = ");
      Serial.println(callBackListeningStatus);
      Serial.println(".........................  Call back was successful !!!! .............................");

      z=0;           // Reset the main counter.
      rainValuex2 =0;
      rainValue =0;
      tone(3,500,1500);
      //windVane();                    // This is here so that wind vane variables get reset quickly.
    }
  }

//  if (incomingLength != incoming.length()) 
//  {   // check length for error
//    Serial.println("error: message length does not match length");
//    return;                             // skip rest of function
//  }

  // if the recipient isn't this device or broadcast,
  //if (recipient != localAddress && recipient != 0xFF) 
  //{
  //  Serial.println("This message is not for me.");
  //  return;                             // skip rest of function
 // }

  // if message is for this device, or broadcast, print details:

  Serial.println("");
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
}
void slowPrint()
{
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillisSlowPrint) > intervalSlowPrint)
  {
    Serial.print(".... ");
    previousMillisSlowPrint = currentMillis;
  }
}
