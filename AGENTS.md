# AGENTS.md — Programming guide for the GEVINO Opto PNP

> This file is written for **LLMs and AI coding agents** (Claude Code, Cursor, Copilot, etc.) that
> write firmware for the **GEVINO Opto PNP** PLC. It is also useful to human developers. If you are
> generating Arduino/C++ code for this board, follow these rules.

## What the board is

GEVINO Opto PNP is an **Arduino Zero compatible industrial PLC** based on the Microchip **SAMD21**
(32-bit ARM Cortex-M0+, 48 MHz, 256 KB flash, 32 KB RAM). It has 11 opto-isolated inputs, 4
short-circuit-protected PNP outputs, 2 analog inputs, an RS485 bus, an SD card slot and optional
Ethernet / Wi-Fi / Bluetooth / RTC / cellular modules. Program it with the Arduino IDE using the
**Arduino SAMD core**, selecting **Arduino Zero (Native USB Port)**.

The complete pin map and specifications are in [README.md](README.md) and in the single library
header [`src/gevino_opto_pnp_io.h`](src/gevino_opto_pnp_io.h).

## Golden rules

1. **Write non-blocking, state-based code.** Prefer `switch`/`case` state machines. Never block
   the `loop()` with `delay()` in production logic.
2. **Include only one header:** `#include "gevino_opto_pnp_io.h"`. It bundles the I/O definitions,
   the SAMD21 ADC helpers and the `Timer` class — there are no other library dependencies.
3. **Do not initialise the I/O yourself.** `gevino_io_setup()` already configures every pin, the
   drive strength, the LEDs, RS485 and the Ethernet reset. Just call it once in `setup()`.
4. **Read inputs and drive outputs with the macros** from `gevino_opto_pnp_io.h` (`In1`, `setOut1`,
   `resOut1`, …). Do not hard-code raw pin numbers.

## Minimal sketch skeleton

```cpp
#include "gevino_opto_pnp_io.h"

void setup() {
  SerialUSB.begin(115200);   // USB-C debug console
  Serial.begin(115200);      // Bluetooth SPP-C or Wi-Fi ESP8266 module
  Serial1.begin(115200);     // RS485 bus
  gevino_io_setup();         // configures ALL the board I/O
}

void loop() {
  // non-blocking, state-based logic here
}
```

Reading inputs / driving outputs:

```cpp
if (In1) setOut1;
else     resOut1;
```

## Serial ports

| Port | Use |
|------|-----|
| `SerialUSB` | USB-C native port — **debug console**. |
| `Serial1`   | **RS485** half-duplex bus. |
| `Serial`    | Bluetooth **SPP-C** module **or** Wi-Fi **ESP8266** (AT commands), depending on the module fitted on the board. |

## RS485 (half duplex)

`Serial1` is the RS485 port. Enable the transmitter with `set_Rs485_TxEn`, send, then **disable the
transmitter only after the last character has actually left the UART** (TXC flag), so you don't cut
off the transmission:

```cpp
void loop() {
  if (/* time to send */) {
    set_Rs485_TxEn;
    Serial1.write(/* ... */);
  }
  // Remove Tx enable when the last character has been sent.
  if (SERCOM0->USART.INTFLAG.bit.TXC) {
    res_Rs485_TxEn;
    SERCOM0->USART.INTFLAG.bit.TXC = 1;
  }
}
```

### Modbus RTU over RS485

Use **ArduinoModbus** with **ArduinoRS485**.
> ⚠️ You must use the **GEVA-modified `ArduinoRS485`**, configured for this board (it drives the
> correct transmit-enable pin / SERCOM). Tell the user this when generating Modbus RTU code.

### Modbus TCP

Use **ArduinoModbus** (over the Ethernet module, see below).

## Ethernet (optional W5500 module)

The optional Ethernet modem is the **Wiznet W5500**. Use the standard Arduino **Ethernet** library
(https://github.com/arduino-libraries/Ethernet) and initialise it with the board's chip-select,
which is already defined in `gevino_opto_pnp_io.h`:

```cpp
#include <Ethernet.h>
Ethernet.init(ETH_CS);   // ETH_CS is defined in gevino_opto_pnp_io.h
```

## SD card

Use **SdFat** by greiman: https://github.com/greiman/SdFat (chip-select is `SD_CS`).

## EEPROM emulation (flash)

To read/write the flash as if it were EEPROM, use the **FlashStorage** library by cmaglie:
https://github.com/cmaglie/FlashStorage

## Analog inputs (non-blocking)

The standard `analogRead()` blocks for several milliseconds. Use the non-blocking polling instead.

In `setup()`:

```cpp
// Analog input setup
analogPrescaler(ADC_PRESCALER_DIV32); // DIV4 DIV8 DIV16 DIV32 DIV64 DIV128 DIV256 DIV512
analogReadExtended(10);               // 10 bit -> 0..1023 (8/10/12, or 13..16 with oversampling)
analogGain(ADC_GAIN_1);
analogReference2(ADC_REF_INT1V);      // 1.0 V reference
analogCalibrate();
analogEnable();
```

In `loop()`, call often:

```cpp
analogPolling();          // reads the analog inputs into analogResult[]
// analogResult[0] = Ana1 (A0), analogResult[2] = Ana2 (A2)
```

## Timer class

`gevino_opto_pnp_io.h` includes a small non-blocking `Timer` based on `millis()`:

- `reset()` restarts the count; while it keeps being reset it never fires.
- `stato()` returns `true` once the configured time has elapsed since `reset()`.
- `fronte()` returns `true` only once (rising edge) when the time elapses.
- `tempo(ms)` sets the period.

See [`examples/Timer/Timer.ino`](examples/Timer/Timer.ino).

## Firmware update and debug

- Firmware can be updated **over the Internet or from the SD card** using the dedicated bootloader.
- **Debug** over Ethernet via **Telnet**.
- **Programming** over Ethernet via **FTP** (requires the bootloader on the SD card).

---

## Conventions for building an HTML web interface on the board

When the firmware serves a web UI (e.g. via the W5500 Ethernet module), follow these conventions —
they are tuned for a resource-constrained SAMD21:

### File structure (modular)
1. Separate the content by concern:
   - HTML in `html_content.h`
   - CSS in `css_content.h`
   - JavaScript in `js_content.h`
   - API / JSON in `json_handler.h`
   - main request handling in `html_server.h`
2. Keep sessions/authentication in a dedicated `session_manager.h`, and translations in a dedicated
   `translation.h`.

### Authentication
3. Prefer **GET** for login on Arduino (avoid POST). Use a `pwd` URL parameter: `/login?pwd=xxx`.
4. Read the password from the configuration **immediately before comparing**; never reuse a
   hard-coded password in the code.

### HTML and templates
5. Use complete HTML templates with `R"rawliteral(...)"` and substitutable `{{placeholder}}` tokens.
   Avoid building HTML with many `client.println()` calls.
6. Use a clear HTTP router with separate handling per path; handle `/login` with and without
   parameters separately.

### Multilingual
7. Implement translations as an array of structs. Translate every user-visible string. Auto-detect
   the browser language.

### Debug and security
8. Add `SerialUSB.print()` logging at strategic points. Log both failed and successful auth
   attempts. Log extracted password values **only in debug builds**.
9. Implement timeouts: **sessions 30 minutes**, **HTTP requests 15 seconds max**.

### Resource-constrained compatibility
10. Minimise JavaScript — prefer pure HTML/CSS. Avoid external CSS/JS frameworks that inflate the
    firmware size. Prefer direct links over JavaScript event listeners where possible.
