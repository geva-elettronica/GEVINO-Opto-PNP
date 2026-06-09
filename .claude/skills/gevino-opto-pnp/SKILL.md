---
name: gevino-opto-pnp
description: Use when writing Arduino/C++ firmware for the GEVA GEVINO Opto PNP industrial PLC (SAMD21, Arduino Zero compatible). Covers the single-header library, pin/I-O macros, non-blocking state-machine style, serial ports, RS485 + Modbus, Ethernet (W5500), SD (SdFat), flash-as-EEPROM (FlashStorage), the non-blocking ADC and Timer, and HTML web-UI conventions.
---

# GEVINO Opto PNP firmware

You are writing firmware for the **GEVINO Opto PNP** — an Arduino Zero compatible industrial PLC
based on the Microchip SAMD21 (ARM Cortex-M0+, 48 MHz, 256 KB flash, 32 KB RAM), made by GEVA
Elettronica. Build in the Arduino IDE / arduino-cli with the **Arduino SAMD core**, board
**Arduino Zero (Native USB Port)**.

## Read these first
- [`AGENTS.md`](../../../AGENTS.md) — the full programming guide (this skill is its short form).
- [`src/gevino_opto_pnp_io.h`](../../../src/gevino_opto_pnp_io.h) — the single library header with
  every pin macro and function. When unsure of a name or pin, read this file rather than guessing.

## Non-negotiable rules
1. **Include exactly one header:** `#include "gevino_opto_pnp_io.h"`. It bundles I/O, the SAMD21 ADC
   helpers and the `Timer` class. No other dependency is needed for the core library.
2. **Call `gevino_io_setup()` once in `setup()`** and do **not** initialise pins yourself — it
   already configures every pin, drive strength, LEDs, RS485 and the Ethernet reset.
3. **Use the macros**, never raw pin numbers: inputs `In1`…`In11` (true = active), outputs
   `setOut1`…`setOut4` / `resOut1`…`resOut4`, state `Out1`…`Out4`, LEDs `setLed`/`resLed`,
   `set_Rs485_TxEn`/`res_Rs485_TxEn`. Out1–Out4 are **PNP**; `Out5`/`Out6` are **NPN** auxiliary
   outputs, available only when enabled via DIP switch (`Ana1_is_Output5`, `Ana2_is_Output6`).
4. **Write non-blocking, state-based code.** Prefer `switch`/`case` state machines and `millis()` /
   the bundled `Timer`. Do **not** use `delay()` in production `loop()` logic.

## Skeleton

```cpp
#include "gevino_opto_pnp_io.h"

void setup() {
  SerialUSB.begin(115200);   // USB-C debug console
  Serial.begin(115200);      // Bluetooth SPP-C or Wi-Fi ESP8266
  Serial1.begin(115200);     // RS485 bus
  gevino_io_setup();
}

void loop() {
  if (In1) setOut1; else resOut1;
}
```

## Serial ports
- `SerialUSB` → USB-C, **debug console**.
- `Serial1` → **RS485** half-duplex. Enable with `set_Rs485_TxEn`, and only `res_Rs485_TxEn` after
  `SERCOM0->USART.INTFLAG.bit.TXC` confirms the last byte was sent.
- `Serial` → Bluetooth SPP-C or Wi-Fi ESP8266 (AT commands), depending on the fitted module.

## Recipes (see AGENTS.md for full code)
- **Modbus RTU:** ArduinoModbus + **GEVA-modified ArduinoRS485** (warn the user it must be the GEVA
  version configured for this board). **Modbus TCP:** ArduinoModbus over Ethernet.
- **Ethernet (W5500):** standard `Ethernet` library, `Ethernet.init(ETH_CS)` (`ETH_CS` is defined
  in the header).
- **SD card:** SdFat by greiman (chip-select `SD_CS`).
- **EEPROM emulation:** FlashStorage by cmaglie.
- **Analog (non-blocking):** in `setup()` configure with `analogPrescaler/analogReadExtended/
  analogGain/analogReference2/analogCalibrate/analogEnable`, then call `analogPolling()` often in
  `loop()`; results land in `analogResult[]` (`[0]`=Ana1/A0, `[2]`=Ana2/A2). Avoid the blocking
  `analogRead()`.

## HTML web UI
If you build a web interface, follow the resource-constrained conventions in **AGENTS.md**
(modular `*_content.h` files, GET-based login with a `pwd` param, `R"rawliteral(...)"` templates
with `{{placeholders}}`, struct-array translations, 30-min session / 15-s request timeouts, minimal
JavaScript).
