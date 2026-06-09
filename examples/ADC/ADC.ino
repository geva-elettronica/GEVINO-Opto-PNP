// Example for GEVINO PNP
// 30/08/2020   Giorgio Evangelista

// The standard Arduino analogRead() freezes the program for several milliseconds.
// This non-blocking polling method does not.

// Install "Arduino SAMD board"
// Use Arduino Zero Native Port

// install WiFi ESP library github.com/ekstrand/ESP8266wifi
// install eeprom library https://github.com/cmaglie/FlashStorage
// install Adafruit FRAM library https://github.com/adafruit/Adafruit_FRAM_I2C

// The ADC helpers are now bundled inside gevino_opto_pnp_io.h (single include).
#include "gevino_opto_pnp_io.h"

void setup() {

  SerialUSB.begin(115200);

  // Analog input Setup
  analogPrescaler(ADC_PRESCALER_DIV32);  // DIV4 DIV8 DIV16 DIV32 DIV64 DIV128 DIV256 DIV512 
  analogReadExtended(16); // 16 bit = 256 sample
  analogGain(ADC_GAIN_1);
  analogReference2(ADC_REF_INT1V); // 1.0V reference
  analogCalibrate();
  analogEnable();

}

void loop() {
  static unsigned long TimerForSec;
  
  analogPolling();  // Read Analog input and put value on analogResult[];

  if( millis() - TimerForSec > 200 ){
    TimerForSec = millis();
    SerialUSB.print(analogResult[0]);
    SerialUSB.print("\t");
    SerialUSB.print(analogResult[2]);
    SerialUSB.print("\t");
    SerialUSB.println(analogResult[5]);
  }


}
