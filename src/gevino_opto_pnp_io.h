#ifndef _GEVINO_OPTO_PNP_IO_H_
#define _GEVINO_OPTO_PNP_IO_H_

/*
  GEVINO Opto PNP - single-header support library
  GEVA Elettronica - https://www.gevaelettronica.it

  Industrial PLC based on the Microchip SAMD21 (ARM Cortex-M0+), Arduino Zero
  compatible. This single header bundles everything the board needs:

    1. Low-level SAMD21 ADC helpers        (originally ATSAMD21_ADC.h, Blake Felt)
    2. Pin definitions and I/O macros      (GEVA Elettronica)
    3. Board setup and self-test routines  (GEVA Elettronica)
    4. Non-blocking analog polling         (GEVA Elettronica)
    5. A lightweight Timer class           (GEVA Elettronica)

  Usage:
    #include "gevino_opto_pnp_io.h"   // this is the only include you need

  Notes:
    - Requires C++17 (default on recent Arduino SAMD cores) because of the
      `inline` variables and functions used to make this header safe to include
      from multiple translation units.
    - Do NOT initialise the I/O yourself: call gevino_io_setup() in setup().

  Changelog (board defines, from v1.3)
    22/11/2022  More output current on some pins (DRVSTR).
    26/01/2024  Exchanged A0 for A1.
    06/06/2026  Functions and variables marked `inline` for use as a library
                (no multiple-definition). Fixed pinMode(Out4) -> pinMode(Out4_name)
                in testLeds().
    08/06/2026  Merged ATSAMD21_ADC.h and Timer.h into this single header.
*/

#include <Arduino.h>

// =============================================================================
// 1. Low-level SAMD21 ADC helpers
//    Originally ATSAMD21_ADC.h by Blake Felt.
//    Adds extra ADC features such as 13..16 bit oversampling and differential
//    mode. Used by the non-blocking analog polling below and exposed for
//    advanced configuration.
// =============================================================================

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef ADC_CTRLB_RESSEL_12BIT_Val
#define ADC_CTRLB_RESSEL_8BIT_Val   0x03
#define ADC_CTRLB_RESSEL_10BIT_Val  0x02 // default by Arduino
#define ADC_CTRLB_RESSEL_12BIT_Val  0x00
#endif
#define ADC_CTRLB_RESSEL_16BIT_Val  0x01 // used for averaging mode output

#define ADC_PIN_TEMP                0x18 // positive mux, pg 870
#define ADC_PIN_BANDGAP             0x19
#define ADC_PIN_SCALEDCOREVCC       0x1A
#define ADC_PIN_SCALEDIOVCC         0x1B
#define ADC_PIN_DAC                 0x1C

#define ADC_PIN_GND                 0x18 // negative mux, pg 869
#define ADC_PIN_IOGND               0x19

#define ADC_GAIN_1                  0x00 // pg 868
#define ADC_GAIN_2                  0x01
#define ADC_GAIN_4                  0x02
#define ADC_GAIN_8                  0x03
#define ADC_GAIN_16                 0x04
#define ADC_GAIN1_DIV2              0x0F // default by Arduino

#define ADC_REF_INT1V               0x00 // 1.0V reference, pg 861
#define ADC_REF_INTVCC0             0x01 // 1/1.48 VDDANA
#define ADC_REF_INTVCC1             0x02 // 1/2 VDDANA (only for VDDANA > 2.0V) // default
#define ADC_REF_VREFA               0x03 // external reference
#define ADC_REF_VREFB               0x04 // external reference

#define ADC_PRESCALER_DIV4          0x00 // pg 864
#define ADC_PRESCALER_DIV8          0x01
#define ADC_PRESCALER_DIV16         0x02
#define ADC_PRESCALER_DIV32         0x03
#define ADC_PRESCALER_DIV64         0x04
#define ADC_PRESCALER_DIV128        0x05
#define ADC_PRESCALER_DIV256        0x06
#define ADC_PRESCALER_DIV512        0x07 // Arduino default

// NVM Software Calibration Area Mapping, pg 32. Address starting at NVMCTRL_OTP4.
// NVM register access code modified from https://github.com/arduino/ArduinoCore-samd/blob/master/cores/arduino/USB/samd21_host.c
// ADC Linearity Calibration value. Should be written to the CALIB register.
#define NVM_ADC_LINEARITY_POS         27
#define NVM_ADC_LINEARITY_SIZE         8
// ADC Bias Calibration value. Should be written to the CALIB register.
#define NVM_ADC_BIASCAL_POS           35
#define NVM_ADC_BIASCAL_SIZE           3

// Taken from Arduino IDE:
// Wait for synchronization of registers between the clock domains
static __inline__ void syncADC() __attribute__((always_inline, unused));
static void syncADC() {
  while(ADC->STATUS.bit.SYNCBUSY == 1);
}

// Taken from Arduino IDE:
// Wait for synchronization of registers between the clock domains
static __inline__ void syncDAC() __attribute__((always_inline, unused));
static void syncDAC() {
  while (DAC->STATUS.bit.SYNCBUSY == 1);
}

// taken from Arduino IDE, changes the pin to an input:
int pinPeripheral( uint32_t ulPin, EPioType ulPeripheral );

inline uint8_t analogReadExtended(uint8_t bits) {
/*
 * Allows for adc to read 8, 10, or 12 bits normally or 13-16 bits using oversampling and decimation.
 * See pages 853 & 862
 * 8,10,12 bit = 1 sample ~ 436 microseconds
 * 13 bit = 4 samples ~ 1668 microseconds
 * 14 bit = 16 samples ~ 6595 microseconds
 * 15 bit = 64 samples ~ 26308 microseconds
 * 16 bit = 256 samples ~ 105156 microseconds
 */
  switch(bits) {
    case 8:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_8BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x0;
      return 0;
    break;

    case 10:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_10BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x0;
      return 0;
    break;

    case 12:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_12BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x0;
      return 0;
    break;

    case 13:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x1;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x2;
      return 0;
    break;

    case 14:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x2;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x4;
      return 0;
    break;

    case 15:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x1;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x6;
      return 0;
    break;

    case 16:
      ADC->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
      ADC->AVGCTRL.bit.ADJRES = 0x0;
      ADC->AVGCTRL.bit.SAMPLENUM = 0x8;
      return 0;
    break;

    default:
      return -1;
    break;
   }
}

// returns the internal pin value of the specified pin, useful
// for analogDifferentialRaw function
inline uint8_t internalPinValue(uint8_t pin) {
  return g_APinDescription[pin].ulADCChannelNumber;
}

// modified from Arduino analogRead, can be used in conjunction with analogRead:
inline int16_t analogDifferential(uint8_t pin_pos,uint8_t pin_neg) {
  if(pin_pos<A0) pin_pos += A0;
  if(pin_neg<A0) pin_neg += A0;

  if((g_APinDescription[pin_neg].ulADCChannelNumber>0x07) && (pin_neg<ADC_PIN_GND)) { // if the negative pin is out of bounds
    return 0;
  }

  uint32_t value_read = 0;

  pinPeripheral(pin_pos,PIO_ANALOG); // set pins to analog mode
  pinPeripheral(pin_neg,PIO_ANALOG);

  if((pin_pos == A0) || (pin_neg == A0)) { // Disable DAC
    syncDAC();
    DAC->CTRLA.bit.ENABLE = 0x00; // Disable DAC
    syncDAC();
  }

  syncADC();
  ADC->INPUTCTRL.bit.MUXPOS = g_APinDescription[pin_pos].ulADCChannelNumber; // Selection for the positive ADC input
  ADC->INPUTCTRL.bit.MUXNEG = g_APinDescription[pin_neg].ulADCChannelNumber; // negative ADC input

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x01; // enable adc
  ADC->CTRLB.bit.DIFFMODE = 1; // set to differential mode

  syncADC();
  ADC->SWTRIG.bit.START = 1; // start conversion

  ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY; // clear the data ready flag
  syncADC();

  ADC->SWTRIG.bit.START = 1; // restart conversion, as changing inputs messes up first conversion

  while(ADC->INTFLAG.bit.RESRDY == 0);   // Wait for conversion to complete
  value_read = ADC->RESULT.reg; // read the value

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x00; // disable adc
  ADC->CTRLB.bit.DIFFMODE = 0; // put back into single-ended mode
  ADC->INPUTCTRL.bit.MUXNEG = ADC_PIN_GND; // set back muxneg to internal ground
  syncADC();

  return value_read;
}

// same as the above function, but no error checking, no pin types are changed, and the positive and negative
// inputs are the raw values being input. The DAC is not automatically shut off either. See datasheet page
inline int16_t analogDifferentialRaw(uint8_t mux_pos,uint8_t mux_neg) {

  uint32_t value_read = 0;

  syncADC();
  ADC->INPUTCTRL.bit.MUXPOS = mux_pos; // Selection for the positive ADC input
  ADC->INPUTCTRL.bit.MUXNEG = mux_neg; // negative ADC input

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x01; // enable adc
  ADC->CTRLB.bit.DIFFMODE = 1; // set to differential mode

  syncADC();
  ADC->SWTRIG.bit.START = 1; // start conversion

  ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY; // clear the data ready flag
  syncADC();

  ADC->SWTRIG.bit.START = 1; // restart conversion, as changing inputs messes up first conversion

  while(ADC->INTFLAG.bit.RESRDY == 0);   // Wait for conversion to complete
  value_read = ADC->RESULT.reg; // read the value

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x00; // disable adc
  ADC->CTRLB.bit.DIFFMODE = 0; // put back into single-ended mode
  ADC->INPUTCTRL.bit.MUXNEG = ADC_PIN_GND; // set back muxneg to internal ground
  syncADC();

  return value_read;
}

// sets the gain of the ADC. See page 868. All values defined above.
inline void analogGain(uint8_t gain) {
  syncADC();
  ADC->INPUTCTRL.bit.GAIN = gain;
  syncADC();
}

// calibrates the bias and linearity based on the nvm register.
// NVM register access code modified from https://github.com/arduino/ArduinoCore-samd/blob/master/cores/arduino/USB/samd21_host.c
// datasheet pages 32 and 882
inline void analogCalibrate() {
  syncADC();
  // read NVM register
  uint32_t adc_linearity = (*((uint32_t *)(NVMCTRL_OTP4) // original position
          + (NVM_ADC_LINEARITY_POS / 32)) // move to the correct 32 bit window, read value
          >> (NVM_ADC_LINEARITY_POS % 32)) // shift value to match the desired position
          & ((1 << NVM_ADC_LINEARITY_SIZE) - 1); // apply a bitmask for the desired size

  uint32_t adc_biascal = (*((uint32_t *)(NVMCTRL_OTP4)
          + (NVM_ADC_BIASCAL_POS / 32))
          >> (NVM_ADC_BIASCAL_POS % 32))
          & ((1 << NVM_ADC_BIASCAL_SIZE) - 1);

  // write values to CALIB register
  ADC->CALIB.bit.LINEARITY_CAL = adc_linearity;
  ADC->CALIB.bit.BIAS_CAL = adc_biascal;
  syncADC();
}

// set the analog reference voltage, but with all available options
// (the Arduino IDE neglects some). The Arduino IDE also changes
// the gain when analogReference() is used, but this won't. pg 861
inline void analogReference2(uint8_t ref) {
  syncADC();
  ADC->REFCTRL.bit.REFSEL = ref;
  syncADC();
}

// increases accuracy of gain stage by enabling the reference buffer
// offset compensation. Takes longer to start. pg 861
inline void analogReferenceCompensation(uint8_t val) {
  if(val>0) val = 1;
  syncADC();
  ADC->REFCTRL.bit.REFCOMP = val;
  syncADC();
}

// sets the ADC clock relative to the peripheral clock. pg 864
inline void analogPrescaler(uint8_t val) {
  syncADC();
  ADC->CTRLB.bit.PRESCALER = val;
  syncADC();
}

// resets the ADC. pg 860
// note that this doesn't put back the default values set by the
// Arduino IDE.
inline void analogReset() {
  syncADC();
  ADC->CTRLA.bit.SWRST = 1; // set reset bit
  while(ADC->CTRLA.bit.SWRST==1); // wait until it's finished
  syncADC();
}

#ifdef __cplusplus
  }
#endif

// =============================================================================
// 2. Pin definitions and I/O macros (GEVA Elettronica)
//    11 opto-isolated inputs, 4 protected PNP outputs, 2 analog inputs that can
//    optionally act as outputs 5/6, RS485 transmit-enable, on-board LEDs and the
//    chip-selects for the SD card and the optional W5500 Ethernet module.
// =============================================================================

#define In1_name  19   // A5
#define In2_name  38
#define In3_name  2
#define In4_name  3
#define In5_name  4
#define In6_name  5
#define In7_name  6
#define In8_name  7
#define In9_name  8
#define In10_name 9
#define In11_name 45
#define Rs485_TxEn  A1
#define SD_CS 12
#define ETH_RES 11
#define ETH_CS 10
#define Ana1  A0
#define Ana2  A2

#define Out1_name A3
#define Out2_name A4
#define Out3_name 42
#define Out4_name 27
#define LedRx_name  25
#define LedTx_name  26

#define In1  !digitalRead(In1_name)
#define In2  !digitalRead(In2_name)
#define In3  !digitalRead(In3_name)
#define In4  !digitalRead(In4_name)
#define In5  !digitalRead(In5_name)
#define In6  !digitalRead(In6_name)
#define In7  !digitalRead(In7_name)
#define In8  !digitalRead(In8_name)
#define In9  !digitalRead(In9_name)
#define In10 !digitalRead(In10_name)
#define In11 !digitalRead(In11_name)
#define Out1 digitalRead(Out1_name)
#define Out2 digitalRead(Out2_name)
#define Out3 digitalRead(Out3_name)
#define Out4 digitalRead(Out4_name)
#define setOut1 digitalWrite(Out1_name, HIGH)
#define resOut1 digitalWrite(Out1_name, LOW)
#define setOut2 digitalWrite(Out2_name, HIGH)
#define resOut2 digitalWrite(Out2_name, LOW)
#define setOut3 digitalWrite(Out3_name, HIGH)
#define resOut3 digitalWrite(Out3_name, LOW)
#define setOut4 digitalWrite(Out4_name, HIGH)
#define resOut4 digitalWrite(Out4_name, LOW)
#define setLed  digitalWrite(LED_BUILTIN, HIGH) // Yellow front Led
#define resLed  digitalWrite(LED_BUILTIN, LOW)  // Yellow front Led
#define setLedRx  digitalWrite(LedRx_name, LOW)
#define resLedRx  digitalWrite(LedRx_name, HIGH)
#define setLedTx  digitalWrite(LedTx_name, LOW)
#define resLedTx  digitalWrite(LedTx_name, HIGH)
#define set_Rs485_TxEn  digitalWrite(Rs485_TxEn, HIGH)
#define res_Rs485_TxEn  digitalWrite(Rs485_TxEn, LOW)

// if Enabled with Dip Switch
#define Ana1_is_Output5 pinMode(Ana1, OUTPUT);
#define Ana2_is_Output6 pinMode(Ana2, OUTPUT);
#define setOut5 digitalWrite(Ana1, HIGH)
#define resOut5 digitalWrite(Ana1, LOW)
#define setOut6 digitalWrite(Ana2, HIGH)
#define resOut6 digitalWrite(Ana2, LOW)

// =============================================================================
// 3. Board setup and self-test (GEVA Elettronica)
// =============================================================================

inline void gevino_io_setup( void ){
  PORT->Group[PORTA].PINCFG[22].bit.DRVSTR = 1;  // More current on output pin SDA
  PORT->Group[PORTA].PINCFG[23].bit.DRVSTR = 1;  // More current on output pin SCL
  PORT->Group[PORTB].PINCFG[10].bit.DRVSTR = 1;  // More current on output pin MOSI
  PORT->Group[PORTB].PINCFG[11].bit.DRVSTR = 1;  // More current on output pin SCK
  PORT->Group[PORTB].PINCFG[37].bit.DRVSTR = 1;  // More current on output pin Serial-Tx
  PORT->Group[PORTA].PINCFG[10].bit.DRVSTR = 1;  // More current on output pin Serial1-Tx
  PORT->Group[PORTA].PINCFG[19].bit.DRVSTR = 1;  // More current on output pin D12 cs-SD
  PORT->Group[PORTA].PINCFG[16].bit.DRVSTR = 1;  // More current on output pin D11 cs-Eth

// Define Output
  pinMode(Out1_name, OUTPUT);       // Set Out1_name digital pin as Output
  pinMode(Out2_name, OUTPUT);
  pinMode(Out3_name, OUTPUT);
  pinMode(Out4_name, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // Yellow front Led
  pinMode(ETH_RES, OUTPUT);

// Define Input Pullup
  pinMode(In1_name, INPUT_PULLUP); // In01
  pinMode(In2_name, INPUT_PULLUP); // In02
  pinMode(In3_name, INPUT_PULLUP); // In03
  pinMode(In4_name, INPUT_PULLUP); // In04
  pinMode(In5_name, INPUT_PULLUP); // In05
  pinMode(In6_name, INPUT_PULLUP); // In06
  pinMode(In7_name, INPUT_PULLUP); // In07
  pinMode(In8_name, INPUT_PULLUP); // In08
  pinMode(In9_name, INPUT_PULLUP); // In09
  pinMode(In10_name, INPUT_PULLUP);// In10
  pinMode(In11_name, INPUT_PULLUP);// In11
  pinMode(Rs485_TxEn, OUTPUT);
  res_Rs485_TxEn;

  resOut1;
  resOut2;
  resOut3;
  resOut4;

  digitalWrite(ETH_RES, LOW);
  delay(300);
  digitalWrite(ETH_RES, HIGH);
  delay(300);
}

inline void testLeds( void ){       // Turns on most of the LEDs, useful for photos and bench testing
// Define Output
  pinMode(Out1_name, OUTPUT);
  pinMode(Out2_name, OUTPUT);
  pinMode(Out3_name, OUTPUT);
  pinMode(Out4_name, OUTPUT);
  pinMode(In1_name,  OUTPUT);
  pinMode(In2_name,  OUTPUT);
  pinMode(In3_name,  OUTPUT);
  pinMode(In4_name,  OUTPUT);
  pinMode(In5_name,  OUTPUT);
  pinMode(In6_name,  OUTPUT);
  pinMode(In7_name,  OUTPUT);
  pinMode(In8_name,  OUTPUT);
  pinMode(In9_name,  OUTPUT);
  pinMode(In10_name, OUTPUT);
  pinMode(In11_name, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); // Yellow front Led
  pinMode(Rs485_TxEn, OUTPUT);      // Set Tx Enable
  pinMode(1, OUTPUT);        		// Set Tx Output
  pinMode(0, OUTPUT);        		// Set Rx Output

// All Off
  digitalWrite(Out1_name, LOW);
  digitalWrite(Out2_name, LOW);
  digitalWrite(Out3_name, LOW);
  digitalWrite(Out4_name, LOW);
  digitalWrite(In1_name,  HIGH);
  digitalWrite(In2_name,  HIGH);
  digitalWrite(In3_name,  HIGH);
  digitalWrite(In4_name,  HIGH);
  digitalWrite(In5_name,  HIGH);
  digitalWrite(In6_name,  HIGH);
  digitalWrite(In7_name,  HIGH);
  digitalWrite(In8_name,  HIGH);
  digitalWrite(In9_name,  HIGH);
  digitalWrite(In10_name, HIGH);
  digitalWrite(In11_name, HIGH);
  digitalWrite(LED_BUILTIN, LOW);   // Yellow front Led
  digitalWrite(Rs485_TxEn, HIGH);	// Tx Enable High
  digitalWrite(1, HIGH);	// Tx
  digitalWrite(0, HIGH);	// Rx

  pinMode(26, OUTPUT); // Tx Led
  digitalWrite(26, LOW);
  delay(200);
  digitalWrite(26, HIGH);

  pinMode(25, OUTPUT); // Rx Led
  digitalWrite(25, LOW);
  delay(200);
  digitalWrite(25, HIGH);

  pinMode(LED_BUILTIN, OUTPUT); // Yellow Led
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(In1_name, OUTPUT); // In01
  digitalWrite(In1_name, LOW);
  delay(200);
  digitalWrite(In1_name, HIGH);

  pinMode(In2_name, OUTPUT); // In02
  digitalWrite(In2_name, LOW);
  delay(200);
  digitalWrite(In2_name, HIGH);

  pinMode(In3_name, OUTPUT);  // In03
  digitalWrite(In3_name, LOW);
  delay(200);
  digitalWrite(In3_name, HIGH);

  pinMode(In4_name, OUTPUT);  // In04
  digitalWrite(In4_name, LOW);
  delay(200);
  digitalWrite(In4_name, HIGH);

  pinMode(In5_name, OUTPUT);  // In05
  digitalWrite(In5_name, LOW);
  delay(200);
  digitalWrite(In5_name, HIGH);

  pinMode(In6_name, OUTPUT);  // In06
  digitalWrite(In6_name, LOW);
  delay(200);
  digitalWrite(In6_name, HIGH);

  pinMode(In7_name, OUTPUT);  // In07
  digitalWrite(In7_name, LOW);
  delay(200);
  digitalWrite(In7_name, HIGH);

  pinMode(In8_name, OUTPUT);  // In08
  digitalWrite(In8_name, LOW);
  delay(200);
  digitalWrite(In8_name, HIGH);

  pinMode(In9_name, OUTPUT);  // In09
  digitalWrite(In9_name, LOW);
  delay(200);
  digitalWrite(In9_name, HIGH);

  pinMode(In10_name, OUTPUT);  // In10
  digitalWrite(In10_name, LOW);
  delay(200);
  digitalWrite(In10_name, HIGH);

  pinMode(In11_name, OUTPUT);  // In11
  digitalWrite(In11_name, LOW);
  delay(200);
  digitalWrite(In11_name, HIGH);

// RS485 Tx
  digitalWrite(1, LOW);
  delay(200);
  digitalWrite(1, HIGH);

// RS485 Rx
//  digitalWrite(Rs485_TxEn, LOW);
  digitalWrite(0, LOW);
  delay(200);
//  digitalWrite(Rs485_TxEn, HIGH);
  digitalWrite(0, HIGH);

  pinMode(Out1_name, OUTPUT);       // Set Out1_name digital pin as Output
  setOut1;
  delay(200);
  resOut1;

  pinMode(Out2_name, OUTPUT);
  setOut2;
  delay(200);
  resOut2;

  pinMode(Out3_name, OUTPUT);
  setOut3;
  delay(200);
  resOut3;

  pinMode(Out4_name, OUTPUT);
  setOut4;
  delay(200);
  resOut4;


  delay(200);
// All On
  digitalWrite(0, LOW);
  digitalWrite(1, LOW);
  digitalWrite(26, LOW); // Tx Led
  digitalWrite(25, LOW); // Rx Led
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(In1_name, LOW);
  digitalWrite(In2_name, LOW);
  digitalWrite(In3_name, LOW);
  digitalWrite(In4_name, LOW);
  digitalWrite(In5_name, LOW);
  digitalWrite(In6_name, LOW);
  digitalWrite(In7_name, LOW);
  digitalWrite(In8_name, LOW);
  digitalWrite(In9_name, LOW);
  digitalWrite(In10_name, LOW);
  digitalWrite(In11_name, LOW);
  setOut1;
  setOut2;
  setOut3;
  setOut4;

  delay(1000);
// All off
  digitalWrite(0, HIGH);
  digitalWrite(1, HIGH);
  digitalWrite(26, HIGH);  // Tx
  digitalWrite(25, HIGH);  // Rx
  digitalWrite(LED_BUILTIN, LOW);  // Led
  digitalWrite(In1_name, HIGH);
  digitalWrite(In2_name, HIGH);
  digitalWrite(In3_name, HIGH);
  digitalWrite(In4_name, HIGH);
  digitalWrite(In5_name, HIGH);
  digitalWrite(In6_name, HIGH);
  digitalWrite(In7_name, HIGH);
  digitalWrite(In8_name, HIGH);
  digitalWrite(In9_name, HIGH);
  digitalWrite(In10_name, HIGH);
  digitalWrite(In11_name, HIGH);
  resOut1;
  resOut2;
  resOut3;
  resOut4;

}

// =============================================================================
// 4. Non-blocking analog polling (GEVA Elettronica)
//    The standard Arduino analogRead() freezes the program for several
//    milliseconds. analogPolling() cycles through the channels without blocking
//    and stores the latest conversions in analogResult[].
// =============================================================================
inline uint32_t analogResult[6];

inline void analogPolling(void){
//A0 = PA2 = AIN0
//A1 = PB8 = AIN2
//A2 = PB9 = AIN3
//A3 = PA4 = AIN4
//A4 = PA5 = AIN5
//A5 = PB2 = AIN10
  static byte pin = 0;


  if( ADC->INTFLAG.bit.RESRDY ){				// if conversion is done
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;
    switch(pin){
	  case 0:
	    analogResult[0] = ADC->RESULT.reg;	// save conversion value
	    pin = 3;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	    break;
	  case 3:
	    analogResult[2] = ADC->RESULT.reg;	// save conversion value
	    pin = 10;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	    break;
	  case 10:
	    analogResult[5] = ADC->RESULT.reg;	// save conversion value
	    pin = 0;
        ADC->INPUTCTRL.bit.MUXPOS = pin; // Selection new conversion pin
        ADC->SWTRIG.bit.START = 1;            // start new conversion
	  }
   }
}


inline void analogEnable( void ){
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->CTRLA.bit.ENABLE = 0x01;             // Enable ADC
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->INPUTCTRL.bit.MUXNEG = ADC_PIN_IOGND; // set back muxneg to internal ground  ADC_PIN_GND / ADC_PIN_IOGND
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->INPUTCTRL.bit.MUXPOS = 2;             // Selection new conversion pin
  while (ADC->STATUS.bit.SYNCBUSY == 1);
  ADC->SWTRIG.bit.START = 1;                 // start new conversion
}

// =============================================================================
// 5. Timer class (GEVA Elettronica - Giorgio Evangelista)
//    Simple non-blocking software timer based on millis().
//
//    Behaviour:
//      - While it keeps being reset(), it never fires.
//      - Once the set time elapses without being reset, it fires.
//
//    See the Timer example.
// =============================================================================
class Timer
{
  public:
    Timer( unsigned long _t ){ _Time = _t; _Timer = millis()+_Time; };
    Timer() {_Timer = millis();}
    void reset(void){ _Timer = millis(); }
    void set(void){ _Timer = millis() + _Time; }
    bool stato(void){ return ( millis() - _Timer >= _Time ); }
    bool fronte(void){
	  if ( !ff && ( millis() - _Timer >= _Time )){
		  ff = 1;
		  return 1;
	  }else{
		  if ( millis() - _Timer <= _Time ) ff=0;
		  return 0;
	  }
	}
    void tempo(unsigned long _t){ _Time = _t; _Timer = millis()+_t;}

  private:
    unsigned long _Time;
    unsigned long _Timer;
	bool ff = 0;
};

#endif // _GEVINO_OPTO_PNP_IO_H_
