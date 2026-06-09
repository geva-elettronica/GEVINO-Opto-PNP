// Test for GEVINO opto PNP v1.5
// 26/05/2020   Giorgio Evangelista 
#include "gevino_opto_pnp_io.h"

void setup() {

  gevino_io_setup();
  Ana1_is_Output5;
  Ana2_is_Output6;

  SerialUSB.begin(115200);


}


void loop() {
  resOut6;
  setOut5;
  delay(200);
  resOut5;
  setOut6;
  delay(200);
}
