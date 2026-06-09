# GEVINO Opto PNP

**GEVINO Opto PNP** is an affordable, Arduino-compatible **industrial PLC** built around the
Microchip **SAMD21** microcontroller (32-bit ARM Cortex-M0+). It is **Arduino Zero compatible**,
so you program it with the standard Arduino IDE / Arduino SAMD core, while getting industrial-grade
I/O: opto-isolated inputs, short-circuit-protected PNP outputs, analog inputs, an RS485 bus, an SD
card slot and optional Ethernet / Wi-Fi / Bluetooth / RTC / cellular modem modules.

Designed and manufactured by **[GEVA Elettronica](https://www.gevaelettronica.it)** (Italy).

> Looking to program the board? Jump to **[Quick start](#quick-start)**.
> Writing firmware with an AI coding assistant? Read **[AGENTS.md](AGENTS.md)** — the LLM/agent
> programming guide for this board.

---

## Highlights

- **Arduino Zero compatible** — program it with the Arduino IDE and the Arduino SAMD core.
- **11 dual-polarity opto-isolated inputs** (positive or negative common).
- **4 short-circuit-protected PNP outputs**, 40 V / 2.5 A, for resistive, inductive and capacitive loads.
- **2 analog inputs**, 20 mA / 1 V / 10 V, 12-bit resolution.
- **RS485 bus** for parallel connection of up to 255 GEVINO devices, M-Bus or other.
- **SD card** slot and **USB-C** programming/debug port.
- Robust by design: protected against **inductive loads, electrostatic discharge (ESD) and
  electromagnetic interference (EMI)**.
- Optional **Ethernet, Wi-Fi, Bluetooth, RTC, RS232, RS485, cellular modem** and **CE certification**.

---

## Technical specifications

### General

| | |
|---|---|
| Type | Industrial PLC, Arduino Zero compatible |
| Power supply, inputs and outputs | 7 V – 40 V |
| Protection | Inductive loads, electrostatic discharge (ESD), electromagnetic interference (EMI) |

### Inputs / Outputs

- **11 opto-isolated inputs**, positive or negative common (dual polarity).
- **4 short-circuit-protected PNP outputs**, 40 V 2.5 A, for resistive, inductive and capacitive loads.
- **2 analog inputs**: 20 mA / 1 V / 10 V, 12-bit resolution.
  (The two analog inputs can optionally be configured as digital **NPN** auxiliary outputs 5 and 6 via DIP switch.)
- **1 RS485 port** for parallel connection of up to **255** GEVINO devices, M-Bus or other.

### Front panel (behind the door)

- SD card slot
- Micro USB-C port
- LEDs for inputs, outputs, serial and power
- Reset button

### Processor

- 32-bit **ARM Cortex-M0+**, 48 MHz (Microchip SAMD21)
- Flash memory **256 KB**, RAM **32 KB**

### Optional modules

- Ethernet RJ45 module (Wiznet **W5500**)
- Wi-Fi **ESP** module (ESP8266)
- Bluetooth (SPP-C), RTC, RS232, cellular modem (SIM800/SIM808 class), CE certification

### Update and debug

- Update GEVINO firmware **over the Internet or from the SD card** using a dedicated bootloader.
- **Debug via Ethernet Telnet.**
- **Programming via Ethernet FTP** (requires the bootloader on SD).

---

## Pinout / pin mapping

The board exposes its functions through named macros defined in
[`src/gevino_opto_pnp_io.h`](src/gevino_opto_pnp_io.h). Pin numbers refer to the Arduino SAMD
(Arduino Zero) numbering.

### Opto-isolated inputs (active level)

| Macro | Arduino pin | | Macro | Arduino pin |
|-------|-------------|---|-------|-------------|
| `In1` | 19 (A5) | | `In7`  | 6  |
| `In2` | 38       | | `In8`  | 7  |
| `In3` | 2        | | `In9`  | 8  |
| `In4` | 3        | | `In10` | 9  |
| `In5` | 4        | | `In11` | 45 |
| `In6` | 5        | |        |    |

`In1` … `In11` return `true` when the input is active (the macro already negates `digitalRead`).

### Outputs

The 4 main outputs are **PNP**; the 2 optional auxiliary outputs (Out5/Out6, on the analog pins)
are **NPN**.

| Macro | Arduino pin | Type | Notes |
|-------|-------------|------|-------|
| `Out1` | A3 | PNP | |
| `Out2` | A4 | PNP | |
| `Out3` | 42 | PNP | |
| `Out4` | 27 | PNP | |
| `Out5` | A0 (Ana1) | NPN | Aux; only if enabled via DIP switch (`Ana1_is_Output5`) |
| `Out6` | A2 (Ana2) | NPN | Aux; only if enabled via DIP switch (`Ana2_is_Output6`) |

### Analog inputs

| Macro | Arduino pin | `analogPolling()` result |
|-------|-------------|--------------------------|
| `Ana1` | A0 | `analogResult[0]` |
| `Ana2` | A2 | `analogResult[2]` |

### System / bus pins

| Macro | Arduino pin | Function |
|-------|-------------|----------|
| `Rs485_TxEn` | A1 | RS485 transmit enable (half duplex) |
| `SD_CS` | 12 | SD card chip-select |
| `ETH_CS` | 10 | W5500 Ethernet chip-select |
| `ETH_RES` | 11 | W5500 Ethernet reset |
| `LedRx_name` | 25 | RX LED |
| `LedTx_name` | 26 | TX LED |
| `LED_BUILTIN` | — | yellow front LED |

### Serial ports

| Port | Use |
|------|-----|
| `SerialUSB` | USB-C native port — **debug / programming console** |
| `Serial1`   | **RS485** half-duplex bus (use `set_Rs485_TxEn` / `res_Rs485_TxEn`) |
| `Serial`    | Bluetooth **SPP-C** module **or** Wi-Fi **ESP8266** (AT commands), depending on the fitted module |

---

## Repository contents

| Path | Description |
|------|-------------|
| [`src/`](src/) | The single-header support library (`gevino_opto_pnp_io.h`). |
| [`examples/`](examples/) | Ready-to-flash Arduino sketches. |
| [`docs/`](docs/) | Hardware manual (PDF) and CE declaration. |
| [`AGENTS.md`](AGENTS.md) | Programming guide for LLMs / AI coding agents. |
| [`llms.txt`](llms.txt) | LLM-friendly map of the repository (llmstxt.org convention). |
| [`keywords.txt`](keywords.txt) | Arduino IDE syntax highlighting. |
| [`library.properties`](library.properties) | Arduino library metadata. |

---

## Requirements

- **Arduino IDE** (or arduino-cli).
- **Arduino SAMD core** ("Arduino SAMD Boards" / a core compatible with the GEVINO board).
- Select the board as **Arduino Zero (Native USB Port)**.

The library is **self-contained** — it requires no external dependencies. It needs **C++17**
(the default on recent Arduino SAMD cores) because of the `inline` variables and functions in the
header.

## Installation

1. Download the ZIP from GitHub, or clone the repository.
2. In the Arduino IDE: **Sketch → Include Library → Add .ZIP Library…**
3. The examples then appear under **File → Examples → GEVINO**.

## Quick start

```cpp
#include "gevino_opto_pnp_io.h"   // the only include you need

void setup() {
  SerialUSB.begin(115200);        // USB-C debug console
  gevino_io_setup();              // configures all the board I/O (do NOT init pins yourself)
}

void loop() {
  if (In1) setOut1;               // mirror input 1 onto output 1
  else     resOut1;
}
```

Write **non-blocking, state-based** code (prefer `switch`/`case`, avoid `delay()` in `loop()`).
See [AGENTS.md](AGENTS.md) for the full programming method, RS485/Modbus, Ethernet, SD and analog
recipes.

## API reference

### Inputs
`In1` … `In11` — `true` when the input is active.

### Digital outputs
- Read state: `Out1` … `Out4`
- Set: `setOut1` … `setOut4`
- Reset: `resOut1` … `resOut4`
- `Out5` / `Out6` (`setOut5`, `resOut5`, `setOut6`, `resOut6`) are **NPN** auxiliary outputs,
  available only when enabled via DIP switch on Ana1/Ana2 (`Ana1_is_Output5`, `Ana2_is_Output6`).

### LEDs and RS485
`setLed` / `resLed` (yellow front LED), `setLedRx` / `resLedRx`, `setLedTx` / `resLedTx`,
`set_Rs485_TxEn` / `res_Rs485_TxEn`.

### Functions
| Function | Description |
|----------|-------------|
| `gevino_io_setup()` | Configures pins, drive strength, LEDs, RS485 and resets the Ethernet module. Call it in `setup()`. |
| `testLeds()` | LED / output power-up sequence for bench testing and photos. |
| `analogEnable()` | Starts the first non-blocking ADC conversion. |
| `analogPolling()` | Non-blocking analog read; updates `analogResult[]`. Call it often in `loop()`. |

### Non-blocking ADC
`analogPolling()` avoids the multi-millisecond freeze of the standard `analogRead()`. Results land
in `analogResult[]` (`analogResult[0]` = Ana1/A0, `analogResult[2]` = Ana2/A2). Advanced ADC
configuration helpers (`analogReadExtended`, `analogGain`, `analogReference2`, `analogCalibrate`,
`analogPrescaler`, `analogReset`, `analogDifferential`, …) are bundled in the same header.

### Timer
A lightweight non-blocking software timer class (`Timer`) based on `millis()` is included — see the
[`Timer`](examples/Timer/Timer.ino) example.

## Examples

| Example | What it shows |
|---------|---------------|
| [`ADC`](examples/ADC/ADC.ino) | Non-blocking analog reading with `analogPolling()`. |
| [`Timer`](examples/Timer/Timer.ino) | Non-blocking blink using the `Timer` class. |
| [`Aux_As_Output`](examples/Aux_As_Output/Aux_As_Output.ino) | Using the analog pins as NPN auxiliary outputs 5/6. |
| [`GEVINO_opto_PNP_Test`](examples/GEVINO_opto_PNP_Test/GEVINO_opto_PNP_Test.ino) | Full factory test: SD, RTC, FRAM, Ethernet, CAN, LoRa, ESP, SIM, inputs/outputs. |

## Documentation

The hardware manual and the CE declaration (PDF) are in [`docs/`](docs/).

## Buy / contact

GEVINO PLCs are designed and produced by **GEVA Elettronica**.

- Website: https://www.gevaelettronica.it
- Email: info@gevaelettronica.it

## License

MIT — see [LICENSE](LICENSE). The bundled SAMD21 ADC helpers are derived from Blake Felt's
`ATSAMD21_ADC` and retain their original attribution in the source header.
