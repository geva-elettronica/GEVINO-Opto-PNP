// Timer example for GEVINO Opto PNP
// Non-blocking software timer (class Timer) bundled in gevino_opto_pnp_io.h
//
// Timer behaviour:
//   - reset()      restarts the count; while it keeps being reset() it never fires.
//   - stato()      returns true once the configured time has elapsed since reset().
//   - fronte()     returns true only once (rising edge) when the time elapses.
//   - tempo(ms)    sets the period.
//
// This sketch toggles Out1 every 500 ms and Out2 every 1000 ms WITHOUT delay(),
// which is the recommended non-blocking, state-based programming style.

#include "gevino_opto_pnp_io.h"

Timer blinkFast(500);   // 500 ms period
Timer blinkSlow(1000);  // 1000 ms period

void setup() {
  gevino_io_setup();    // configures all the board I/O
  blinkFast.reset();    // start counting from now
  blinkSlow.reset();
}

void loop() {
  if (blinkFast.stato()) {            // 500 ms elapsed
    blinkFast.reset();                // restart the timer
    if (Out1) resOut1; else setOut1;  // toggle Out1
  }

  if (blinkSlow.stato()) {            // 1000 ms elapsed
    blinkSlow.reset();
    if (Out2) resOut2; else setOut2;  // toggle Out2
  }

  // ... add other non-blocking tasks here; never use delay() in loop().
}
