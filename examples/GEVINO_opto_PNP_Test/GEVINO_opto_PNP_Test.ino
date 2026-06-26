// Test for GEVINO opto PNP v1.0
// 26/05/2020   Giorgio Evangelista 
#include "gevino_opto_pnp_io.h"

// Install https://github.com/sandeepmistry/arduino-CAN/Also from Arduino Library Manager
// Change on MCP2515.h
//#define MCP2515_DEFAULT_CS_PIN          10
//#define MCP2515_DEFAULT_INT_PIN         -1
#include <CAN.h>
byte CanOk = 0;

// LoRa
#include <LoRa.h>
const int csPin = 10;          // LoRa radio chip select
const int resetPin = 11;       // LoRa radio reset
const int irqPin = -1;         // change for your board; must be a hardware interrupt pin

// W5500
#include <SPI.h>
#include <Ethernet.h>
byte mac[6];
IPAddress ip(192, 168, 1, 199);
EthernetServer server(80);

// RTC
#include <Wire.h>
#include <DS3231.h>
DS3231 clock;
RTCDateTime dt;

// FRAM
// For 32 bit FRAM: https://github.com/RobTillaart/FRAM_I2C
// For 16 bit FRAM use the plain "FRAM fram;" class.
#include "FRAM.h"
FRAM32 fram32;
FRAM fram16;

//  TEST_SD
#include <SD.h>
File myFile;

int TestResult;
void setup() {

  gevino_io_setup();

  SerialUSB.begin(115200);
  Serial1.begin(115200);

  unsigned long t = millis(); 
  while(!SerialUSB && millis()-t < 3000);

  SerialUSB.println("V1");

  mac[0] = 0x02;  // unicast, locally administered
  mac[1] = (*(uint32_t *)0x0080A00C ^ *(uint32_t *)0x0080A044) & 0xFF;
  mac[2] = ((*(uint32_t *)0x0080A00C ^ *(uint32_t *)0x0080A048) >> 8) & 0xFF;
  mac[3] = ((*(uint32_t *)0x0080A040 ^ *(uint32_t *)0x0080A044) >> 16) & 0xFF;
  mac[4] = ((*(uint32_t *)0x0080A040 ^ *(uint32_t *)0x0080A048) >> 24) & 0xFF;
  mac[5] = (*(uint32_t *)0x0080A00C ^ *(uint32_t *)0x0080A040 ^
            *(uint32_t *)0x0080A044 ^ *(uint32_t *)0x0080A048) & 0xFF;
  
  char macStr[24];
  sprintf(macStr, "MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  SerialUSB.println(macStr);

// Change baud at SIM808
  Serial.begin(9600);
  Serial.println("AT+IPR=115200");
  delay(100);
  Serial.println("AT&W_SAVE");
  delay(100);
  Serial.begin(115200);
  
// TEST_SD  -  Write file on SD card.
  TestResult = SD.begin(SD_CS);
  if( TestResult ) SerialUSB.println("SD OK"); 
  else  SerialUSB.println("SD FAIL"); 
  
// Test RTC
  TestResult = clock.begin();
  TestResult = clock.isReady();
  if( TestResult ) SerialUSB.println("RTC OK");
  else  SerialUSB.println("RTC FAIL");

// FRAM (32 bit first, fall back to 16 bit)
  byte i32 = fram32.begin(0x50);
  byte i16 = fram16.begin(0x50);
  if (!i32)       SerialUSB.println("Found 32bit I2C FRAM");
  else if (!i16)  SerialUSB.println("Found 16bit I2C FRAM");
  else            SerialUSB.println("Not Found I2C FRAM");

// W5500
  Ethernet.init(ETH_CS);
  int ethDhcp = Ethernet.begin(mac, 5000);
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    SerialUSB.println("ETH module FAIL");
  } else {
    SerialUSB.println("ETH module OK");
    if (ethDhcp) SerialUSB.println("ETH network OK");
    else         SerialUSB.println("ETH network FAIL");
  }

  if (!CAN.begin(500E3)) SerialUSB.println("CAN FAIL");
  else{
    SerialUSB.println("CAN Ok");
    CanOk = 1;
  }

// LoRa at 433Mhz
  if (!LoRa.begin(433E6)) SerialUSB.println("LoRa FAIL");
  else  SerialUSB.println("LoRa OK");
  gevino_io_setup();  // LoRa change In10 (D9) on output. 
  
// Test ESP
  Serial.println("AT");
  delay(500);
  Serial.println("ATI");
  delay(500);
  Serial.println("AT+GMR");

// Test SIM800 on Serial1
  Serial1.println("AT");
  delay(500);
  Serial1.println("ATI");
  delay(500);
  Serial1.println("AT+CSQ");

  testLeds();

}

unsigned long TimerForSec;
void loop() {
  byte tmp, In12, In13;

  if( analogRead(A1) > 3000 ) In12 = 1;
  else In12 = 0;
  if( analogRead(A2) > 3000 ) In13 = 1;
  else In13 = 0;

  if( In1 || In5 || In9 || In13 ) setOut1;
  else resOut1;

  if( In2 || In6 || In10 ) setOut2;
  else resOut2;

  if( In3 || In7 || In11) setOut3;
  else resOut3;

  if( In4 || In8 || In12) setOut4;
  else resOut4;


  if( Serial.available() )   SerialUSB.write(Serial.read());
  if( Serial1.available() )   SerialUSB.write(Serial1.read());
  if( SerialUSB.available()){
    tmp = SerialUSB.read();
    Serial.write(tmp);
    Serial1.write(tmp);
  }

  if( millis() - TimerForSec > 1000 ){
    TimerForSec = millis();
    if(CanOk){
      CAN.beginPacket(0x12);
      CAN.write('h');
      CAN.write('e');
      CAN.write('l');
      CAN.write('l');
      CAN.write('o');
      CAN.endPacket();    
    }
  }
}
