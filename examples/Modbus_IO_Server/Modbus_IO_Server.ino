// GEVINO Opto PNP - Modbus I/O server (TCP + RTU)
// GEVA Elettronica - https://www.gevaelettronica.it
//
// Exposes every resource of the board over Modbus TCP (W5500 Ethernet) and
// Modbus RTU (RS485 on Serial1) at the same time, with one shared I/O image:
//   - 11 opto-isolated inputs        -> discrete inputs + input registers
//   - 2 analog inputs Ana1 / Ana2    -> input registers (raw and scaled)
//   - 4 PNP outputs Out1..Out4       -> coils + holding register bitmap
//   - Ana1 / Ana2 as NPN Out5 / Out6 -> coils, when switched to output mode
// A command console on SerialUSB changes IP, RTU address, baud rate, etc.
// Register map, console commands and DIP switch settings: see MANUALE.md.
//
// Board: "Arduino Zero (Native USB Port)" - Arduino SAMD core.
// Libraries:
//   - GEVINO (this library)
//   - ArduinoModbus
//   - ArduinoRS485, GEVA-modified version: RS485Class RS485(Serial1, 1, A1, -1)
//     A copy is in the ArduinoRS485 subfolder: copy it into libraries\ (LEGGIMI.txt)
//   - Ethernet (W5500)
//   - FlashStorage by cmaglie
//
// Changelog
//   21/10/2026  v1.00  First release.

#include "gevino_opto_pnp_io.h"
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoRS485.h>   // GEVA-modified version (DE pin = A1)
#include <ArduinoModbus.h>
#include <FlashStorage.h>

#define FW_VERSION        100       // 1.00

// -----------------------------------------------------------------------------
// Factory defaults (used on first boot and after the "defaults" command)
// -----------------------------------------------------------------------------
#define DEF_IP            192, 168, 1, 100
#define DEF_MASK          255, 255, 255, 0
#define DEF_GW            192, 168, 1, 1
#define DEF_TCP_PORT      502
#define DEF_RTU_ID        1
#define DEF_RTU_BAUD      19200
#define DEF_RTU_FORMAT    FMT_8N1

#define MAX_TCP_CLIENTS   4         // simultaneous Modbus TCP connections
#define TCP_IDLE_MS       120000UL  // close a TCP client silent for this long

// Nominal full scale of the analog inputs at ADC raw = 4095 (1.0 V reference).
// Every range is reduced to ~1 V on the ADC pin by the input network.
#define FS_10V_MV         10000     // 10 V range  -> value in mV
#define FS_1V_MV          1000      // 1 V range   -> value in mV
#define FS_20MA_UA        20000     // 20 mA range -> value in uA

// -----------------------------------------------------------------------------
// Modbus map (all addresses 0-based)
// -----------------------------------------------------------------------------
// Coils (FC 1, 5, 15)
#define CO_OUT1           0         // 0..5 = Out1..Out6
#define CO_COUNT          6

// Discrete inputs (FC 2)
#define DI_IN1            0         // 0..10 = In1..In11
#define DI_OUT1_STATE     11        // 11..16 = real state of Out1..Out6
#define DI_COUNT          17

// Input registers (FC 4)
#define IR_ANA1_RAW       0
#define IR_ANA2_RAW       1
#define IR_ANA1_SCALED    2
#define IR_ANA2_SCALED    3
#define IR_IN_BITMAP      4
#define IR_OUT_BITMAP     5
#define IR_FW_VERSION     6
#define IR_UPTIME_LO      7
#define IR_UPTIME_HI      8
#define IR_RTU_ID         9
#define IR_RTU_BAUD_100   10
#define IR_TCP_CLIENTS    11
#define IR_COUNT          12

// Holding registers (FC 3, 6, 16)
#define HR_OUT_BITMAP     0         // bit0..5 = Out1..Out6
#define HR_ANA1_MODE      1         // ANA_xxx
#define HR_ANA2_MODE      2
#define HR_WDT_SEC        3         // comm watchdog, 0 = off
#define HR_COUNT          4

// Ana1 / Ana2 modes. The DIP switches must match (see MANUALE.md).
enum { ANA_10V = 0, ANA_1V = 1, ANA_20MA = 2, ANA_OUT = 3 };
const char* const anaModeName[] = { "10v", "1v", "20ma", "out" };

// RS485 frame formats
enum { FMT_8N1 = 0, FMT_8E1 = 1, FMT_8O1 = 2, FMT_8N2 = 3 };
const char* const fmtName[] = { "8N1", "8E1", "8O1", "8N2" };
const uint16_t fmtConfig[] = { SERIAL_8N1, SERIAL_8E1, SERIAL_8O1, SERIAL_8N2 };

const uint32_t validBauds[] = { 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 };

// -----------------------------------------------------------------------------
// Persistent configuration (flash). Erased when a new sketch is uploaded.
// -----------------------------------------------------------------------------
#define CFG_MAGIC         0x47564D31  // "GVM1", change it when Config changes

struct Config {
  uint32_t magic;
  uint8_t  ip[4];
  uint8_t  mask[4];
  uint8_t  gw[4];
  uint16_t tcpPort;
  uint8_t  rtuId;
  uint8_t  rtuFormat;
  uint32_t rtuBaud;
  uint8_t  anaMode[2];
  uint16_t wdtSec;
};

FlashStorage(cfgStore, Config);
Config cfg;

// -----------------------------------------------------------------------------
// Runtime state
// -----------------------------------------------------------------------------
EthernetServer* ethServer = nullptr;
EthernetClient  tcpClient[MAX_TCP_CLIENTS];
ModbusTCPServer tcpModbus[MAX_TCP_CLIENTS];
unsigned long   tcpLastMs[MAX_TCP_CLIENTS];

ModbusServer*   servers[1 + MAX_TCP_CLIENTS];  // [0] = RTU, [1..] = TCP
const uint8_t   serverCount = 1 + MAX_TCP_CLIENTS;

bool     ethPresent = false;
bool     ethLink    = false;
uint8_t  mac[6];

bool     cmdOut[CO_COUNT];      // commanded output state (the shared image)
uint32_t anaFilt[2];            // filtered ADC value x16
unsigned long lastRequestMs = 0;
bool     wdtTripped = false;

bool     monitorOn = false;
bool     logOn     = false;
bool     rebootPending = false;

Timer tAnalog(2);               // analog filter sample period
Timer tLed(500);                // heartbeat
Timer tMonitor(1000);           // console monitor / link check
Timer tReboot(300);
Timer tPortOpen(200);           // let the terminal settle after opening the port

// =============================================================================
// Configuration
// =============================================================================
void setDefaults() {
  const uint8_t ip[] = { DEF_IP }, mask[] = { DEF_MASK }, gw[] = { DEF_GW };
  cfg.magic = CFG_MAGIC;
  memcpy(cfg.ip, ip, 4);
  memcpy(cfg.mask, mask, 4);
  memcpy(cfg.gw, gw, 4);
  cfg.tcpPort    = DEF_TCP_PORT;
  cfg.rtuId      = DEF_RTU_ID;
  cfg.rtuFormat  = DEF_RTU_FORMAT;
  cfg.rtuBaud    = DEF_RTU_BAUD;
  cfg.anaMode[0] = ANA_10V;
  cfg.anaMode[1] = ANA_10V;
  cfg.wdtSec     = 0;
}

void loadConfig() {
  cfgStore.read(&cfg);
  if (cfg.magic != CFG_MAGIC) {
    setDefaults();
    SerialUSB.println(F("Config: flash empty, factory defaults loaded"));
  }
}

void saveConfig() {
  cfgStore.write(cfg);
}

bool validBaud(uint32_t b) {
  for (uint8_t i = 0; i < sizeof(validBauds) / sizeof(validBauds[0]); i++)
    if (validBauds[i] == b) return true;
  return false;
}

// Locally administered MAC built from the SAMD21 128-bit unique serial number.
void buildMac() {
  const uint32_t id = *(volatile uint32_t*)0x0080A00C ^ *(volatile uint32_t*)0x0080A040 ^
                      *(volatile uint32_t*)0x0080A044 ^ *(volatile uint32_t*)0x0080A048;
  mac[0] = 0x02; mac[1] = 0x47; mac[2] = 0x45;   // 02:"G":"E"
  mac[3] = id >> 16; mac[4] = id >> 8; mac[5] = id;
}

// =============================================================================
// I/O
// =============================================================================
uint8_t anaPin(uint8_t ch) { return ch == 0 ? Ana1 : Ana2; }

// Switch Ana1/Ana2 between analog input and NPN output.
void applyAnaMode(uint8_t ch) {
  const uint8_t pin = anaPin(ch);
  if (cfg.anaMode[ch] == ANA_OUT) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  } else {
    cmdOut[4 + ch] = false;
    pinMode(pin, INPUT);
    pinPeripheral(pin, PIO_ANALOG);
  }
}

// Drive the physical outputs from the commanded image.
void applyOutputs() {
  if (cmdOut[0]) setOut1; else resOut1;
  if (cmdOut[1]) setOut2; else resOut2;
  if (cmdOut[2]) setOut3; else resOut3;
  if (cmdOut[3]) setOut4; else resOut4;
  for (uint8_t ch = 0; ch < 2; ch++) {
    if (cfg.anaMode[ch] != ANA_OUT) cmdOut[4 + ch] = false;
    else digitalWrite(anaPin(ch), cmdOut[4 + ch] ? HIGH : LOW);
  }
}

uint16_t inBitmap() {
  const bool in[11] = { In1, In2, In3, In4, In5, In6, In7, In8, In9, In10, In11 };
  uint16_t bm = 0;
  for (uint8_t i = 0; i < 11; i++) if (in[i]) bm |= 1 << i;
  return bm;
}

uint16_t outBitmap() {
  uint16_t bm = 0;
  if (Out1) bm |= 1 << 0;
  if (Out2) bm |= 1 << 1;
  if (Out3) bm |= 1 << 2;
  if (Out4) bm |= 1 << 3;
  if (cfg.anaMode[0] == ANA_OUT && digitalRead(Ana1)) bm |= 1 << 4;
  if (cfg.anaMode[1] == ANA_OUT && digitalRead(Ana2)) bm |= 1 << 5;
  return bm;
}

uint16_t cmdBitmap() {
  uint16_t bm = 0;
  for (uint8_t i = 0; i < CO_COUNT; i++) if (cmdOut[i]) bm |= 1 << i;
  return bm;
}

uint16_t anaRaw(uint8_t ch) {
  return cfg.anaMode[ch] == ANA_OUT ? 0 : anaFilt[ch] >> 4;
}

uint16_t anaScaled(uint8_t ch) {
  const uint32_t raw = anaRaw(ch);
  switch (cfg.anaMode[ch]) {
    case ANA_10V:  return raw * FS_10V_MV  / 4095;
    case ANA_1V:   return raw * FS_1V_MV   / 4095;
    case ANA_20MA: return raw * FS_20MA_UA / 4095;
    default:       return 0;
  }
}

// Exponential filter on the non-blocking ADC results (time constant ~32 ms).
void analogFilter() {
  if (!tAnalog.stato()) return;
  tAnalog.reset();
  const uint32_t raw[2] = { analogResult[0], analogResult[2] };
  for (uint8_t ch = 0; ch < 2; ch++)
    anaFilt[ch] = anaFilt[ch] - (anaFilt[ch] >> 4) + raw[ch];
}

// =============================================================================
// Modbus image synchronisation
// Every server (RTU + each TCP slot) has its own register mapping. The
// commanded state lives in cmdOut[] / cfg and is copied into all of them, so a
// write from any master is seen by all the others.
// =============================================================================
void publishInputs() {
  const uint16_t inBm = inBitmap(), outBm = outBitmap();
  uint8_t di[DI_COUNT];
  for (uint8_t i = 0; i < 11; i++) di[DI_IN1 + i] = (inBm >> i) & 1;
  for (uint8_t i = 0; i < CO_COUNT; i++) di[DI_OUT1_STATE + i] = (outBm >> i) & 1;

  uint8_t clients = 0;
  for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) if (tcpClient[i]) clients++;

  const uint32_t up = millis() / 1000;
  uint16_t ir[IR_COUNT];
  ir[IR_ANA1_RAW]     = anaRaw(0);
  ir[IR_ANA2_RAW]     = anaRaw(1);
  ir[IR_ANA1_SCALED]  = anaScaled(0);
  ir[IR_ANA2_SCALED]  = anaScaled(1);
  ir[IR_IN_BITMAP]    = inBm;
  ir[IR_OUT_BITMAP]   = outBm;
  ir[IR_FW_VERSION]   = FW_VERSION;
  ir[IR_UPTIME_LO]    = up & 0xFFFF;
  ir[IR_UPTIME_HI]    = up >> 16;
  ir[IR_RTU_ID]       = cfg.rtuId;
  ir[IR_RTU_BAUD_100] = cfg.rtuBaud / 100;
  ir[IR_TCP_CLIENTS]  = clients;

  for (uint8_t s = 0; s < serverCount; s++) {
    servers[s]->writeDiscreteInputs(0, di, DI_COUNT);
    servers[s]->writeInputRegisters(0, ir, IR_COUNT);
  }
}

void publishWritable() {
  for (uint8_t s = 0; s < serverCount; s++) {
    for (uint8_t i = 0; i < CO_COUNT; i++) servers[s]->coilWrite(CO_OUT1 + i, cmdOut[i]);
    servers[s]->holdingRegisterWrite(HR_OUT_BITMAP, cmdBitmap());
    servers[s]->holdingRegisterWrite(HR_ANA1_MODE, cfg.anaMode[0]);
    servers[s]->holdingRegisterWrite(HR_ANA2_MODE, cfg.anaMode[1]);
    servers[s]->holdingRegisterWrite(HR_WDT_SEC, cfg.wdtSec);
  }
}

void logOutputs(const char* who) {
  if (!logOn) return;
  SerialUSB.print(who);
  SerialUSB.print(F(": outputs = 0b"));
  SerialUSB.println(cmdBitmap(), BIN);
}

// A request was served by server s: take over whatever the master wrote.
void collectWrites(ModbusServer* s, const char* who) {
  bool cfgChanged = false;
  const uint16_t oldBm = cmdBitmap();

  const uint16_t bm = s->holdingRegisterRead(HR_OUT_BITMAP);
  if (bm != oldBm) {
    for (uint8_t i = 0; i < CO_COUNT; i++) cmdOut[i] = (bm >> i) & 1;
  } else {
    for (uint8_t i = 0; i < CO_COUNT; i++) cmdOut[i] = s->coilRead(CO_OUT1 + i) != 0;
  }

  for (uint8_t ch = 0; ch < 2; ch++) {
    const long mode = s->holdingRegisterRead(HR_ANA1_MODE + ch);
    if (mode != cfg.anaMode[ch] && mode >= ANA_10V && mode <= ANA_OUT) {
      cfg.anaMode[ch] = mode;
      applyAnaMode(ch);
      cfgChanged = true;
      SerialUSB.print(who); SerialUSB.print(F(": Ana")); SerialUSB.print(ch + 1);
      SerialUSB.print(F(" mode = ")); SerialUSB.println(anaModeName[mode]);
    }
  }

  const long wdt = s->holdingRegisterRead(HR_WDT_SEC);
  if (wdt != cfg.wdtSec && wdt <= 3600) {
    cfg.wdtSec = wdt;
    cfgChanged = true;
    SerialUSB.print(who); SerialUSB.print(F(": watchdog = ")); SerialUSB.println(wdt);
  }

  if (cfgChanged) saveConfig();
  applyOutputs();
  publishWritable();               // also reverts invalid values
  if (cmdBitmap() != oldBm) logOutputs(who);

  lastRequestMs = millis();
  if (wdtTripped) {
    wdtTripped = false;
    SerialUSB.println(F("Watchdog: communication restored"));
  }
}

// Outputs off when no master has talked to us for cfg.wdtSec seconds.
void commWatchdog() {
  if (cfg.wdtSec == 0 || wdtTripped) return;
  if (millis() - lastRequestMs < cfg.wdtSec * 1000UL) return;
  wdtTripped = true;
  for (uint8_t i = 0; i < CO_COUNT; i++) cmdOut[i] = false;
  applyOutputs();
  publishWritable();
  SerialUSB.println(F("Watchdog: no Modbus traffic, outputs OFF"));
}

// =============================================================================
// Modbus RTU
// =============================================================================
void rtuBegin() {
  if (!ModbusRTUServer.begin(cfg.rtuId, cfg.rtuBaud, fmtConfig[cfg.rtuFormat])) {
    SerialUSB.println(F("Modbus RTU: start FAILED"));
  }
  ModbusRTUServer.configureCoils(0, CO_COUNT);
  ModbusRTUServer.configureDiscreteInputs(0, DI_COUNT);
  ModbusRTUServer.configureInputRegisters(0, IR_COUNT);
  ModbusRTUServer.configureHoldingRegisters(0, HR_COUNT);
}

void rtuPoll() {
  if (ModbusRTUServer.poll()) collectWrites(&ModbusRTUServer, "RTU");
}

// =============================================================================
// Modbus TCP (non-blocking, up to MAX_TCP_CLIENTS connections)
// =============================================================================
void tcpBegin() {
  for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
    tcpModbus[i].begin();
    tcpModbus[i].configureCoils(0, CO_COUNT);
    tcpModbus[i].configureDiscreteInputs(0, DI_COUNT);
    tcpModbus[i].configureInputRegisters(0, IR_COUNT);
    tcpModbus[i].configureHoldingRegisters(0, HR_COUNT);
  }

  Ethernet.init(ETH_CS);
  Ethernet.begin(mac, IPAddress(cfg.ip), IPAddress(cfg.gw), IPAddress(cfg.gw), IPAddress(cfg.mask));
  ethPresent = Ethernet.hardwareStatus() != EthernetNoHardware;
  if (!ethPresent) {
    SerialUSB.println(F("Ethernet: W5500 not found, Modbus TCP disabled"));
    return;
  }
  ethServer = new EthernetServer(cfg.tcpPort);
  ethServer->begin();
  ethLink = Ethernet.linkStatus() == LinkON;
  tLed.tempo(ethLink ? 500 : 125);
}

void tcpPoll() {
  if (!ethPresent) return;

  EthernetClient c = ethServer->accept();
  if (c) {
    // Free slot, otherwise drop the least recently active client.
    uint8_t slot = 0;
    for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
      if (!tcpClient[i]) { slot = i; break; }
      if (tcpLastMs[i] < tcpLastMs[slot]) slot = i;
    }
    if (tcpClient[slot]) tcpClient[slot].stop();
    tcpClient[slot] = c;
    tcpModbus[slot].accept(tcpClient[slot]);
    tcpLastMs[slot] = millis();
    SerialUSB.print(F("TCP: client ")); SerialUSB.print(c.remoteIP());
    SerialUSB.print(F(" connected, slot ")); SerialUSB.println(slot);
  }

  for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
    if (!tcpClient[i]) continue;
    if (!tcpClient[i].connected() || millis() - tcpLastMs[i] > TCP_IDLE_MS) {
      tcpClient[i].stop();
      SerialUSB.print(F("TCP: slot ")); SerialUSB.print(i); SerialUSB.println(F(" closed"));
      continue;
    }
    if (tcpModbus[i].poll()) {
      tcpLastMs[i] = millis();
      collectWrites(&tcpModbus[i], "TCP");
    }
  }
}

// =============================================================================
// SerialUSB console
// =============================================================================
void printIp(const uint8_t* a) {
  for (uint8_t i = 0; i < 4; i++) { SerialUSB.print(a[i]); if (i < 3) SerialUSB.print('.'); }
}

void printHelp() {
  SerialUSB.println(F(
    "\nCommands (end with Enter):\n"
    "  help | ?             this list\n"
    "  status               configuration and I/O state\n"
    "  ip <a.b.c.d>         IP address            (*)\n"
    "  mask <a.b.c.d>       subnet mask           (*)\n"
    "  gw <a.b.c.d>         gateway               (*)\n"
    "  port <1..65535>      Modbus TCP port       (*)\n"
    "  id <1..247>          Modbus RTU address    (*)\n"
    "  baud <rate>          1200..115200          (*)\n"
    "  format <8N1|8E1|8O1|8N2>  RS485 frame      (*)\n"
    "  ana1 <10v|1v|20ma|out>   Ana1 mode (saved at once, set the DIP switches!)\n"
    "  ana2 <10v|1v|20ma|out>   Ana2 mode (saved at once, set the DIP switches!)\n"
    "  wdt <0..3600>        comm watchdog seconds, 0 = off (saved at once)\n"
    "  out <1..6> <0|1>     force an output (test)\n"
    "  mon <on|off>         print I/O every second\n"
    "  log <on|off>         print output changes from Modbus\n"
    "  defaults             factory defaults        (*)\n"
    "  save                 save (*) settings to flash and reboot\n"
    "  reboot               reboot without saving\n"
    "(*) takes effect after 'save'"));
}

void printIo() {
  const uint16_t inBm = inBitmap(), outBm = outBitmap();
  SerialUSB.print(F("In 1..11: "));
  for (uint8_t i = 0; i < 11; i++) SerialUSB.print((inBm >> i) & 1);
  SerialUSB.print(F("  Out 1..6: "));
  for (uint8_t i = 0; i < CO_COUNT; i++) SerialUSB.print((outBm >> i) & 1);
  for (uint8_t ch = 0; ch < 2; ch++) {
    SerialUSB.print(F("  Ana")); SerialUSB.print(ch + 1); SerialUSB.print('(');
    SerialUSB.print(anaModeName[cfg.anaMode[ch]]); SerialUSB.print(F(")="));
    if (cfg.anaMode[ch] == ANA_OUT) { SerialUSB.print('-'); continue; }
    SerialUSB.print(anaRaw(ch)); SerialUSB.print('/');
    SerialUSB.print(anaScaled(ch));
    SerialUSB.print(cfg.anaMode[ch] == ANA_20MA ? F("uA") : F("mV"));
  }
  SerialUSB.println();
}

void printStatus() {
  SerialUSB.print(F("\nGEVINO Opto PNP Modbus I/O  fw "));
  SerialUSB.print(FW_VERSION / 100); SerialUSB.print('.');
  if (FW_VERSION % 100 < 10) SerialUSB.print('0');
  SerialUSB.println(FW_VERSION % 100);

  SerialUSB.print(F("Ethernet : "));
  if (!ethPresent) SerialUSB.println(F("not present"));
  else {
    SerialUSB.print(F("link ")); SerialUSB.print(ethLink ? F("UP") : F("DOWN"));
    SerialUSB.print(F("  MAC "));
    for (uint8_t i = 0; i < 6; i++) {
      if (mac[i] < 16) SerialUSB.print('0');
      SerialUSB.print(mac[i], HEX); if (i < 5) SerialUSB.print(':');
    }
    SerialUSB.println();
  }
  SerialUSB.print(F("IP       : ")); printIp(cfg.ip);
  SerialUSB.print(F("  mask ")); printIp(cfg.mask);
  SerialUSB.print(F("  gw ")); printIp(cfg.gw);
  SerialUSB.print(F("  port ")); SerialUSB.println(cfg.tcpPort);
  SerialUSB.print(F("RTU      : id ")); SerialUSB.print(cfg.rtuId);
  SerialUSB.print(F("  ")); SerialUSB.print(cfg.rtuBaud);
  SerialUSB.print(F(" ")); SerialUSB.println(fmtName[cfg.rtuFormat]);
  SerialUSB.print(F("Watchdog : "));
  if (cfg.wdtSec == 0) SerialUSB.println(F("off"));
  else { SerialUSB.print(cfg.wdtSec); SerialUSB.println(wdtTripped ? F(" s (TRIPPED)") : F(" s")); }
  SerialUSB.print(F("TCP slots: "));
  for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) {
    if (tcpClient[i]) SerialUSB.print(tcpClient[i].remoteIP()); else SerialUSB.print('-');
    SerialUSB.print(' ');
  }
  SerialUSB.println();
  printIo();
}

bool parseIp(const char* s, uint8_t* dst) {
  IPAddress a;
  if (!s || !a.fromString(s)) return false;
  for (uint8_t i = 0; i < 4; i++) dst[i] = a[i];
  return true;
}

bool parseOnOff(const char* s, bool& v) {
  if (!s) return false;
  if (!strcmp(s, "on") || !strcmp(s, "1"))  { v = true;  return true; }
  if (!strcmp(s, "off") || !strcmp(s, "0")) { v = false; return true; }
  return false;
}

void reply(bool ok) {
  SerialUSB.println(ok ? F("OK") : F("ERROR: invalid value, type 'help'"));
}

void execCommand(char* line) {
  for (char* p = line; *p; p++) *p = tolower(*p);
  const char* cmd = strtok(line, " \t");
  const char* arg = strtok(nullptr, " \t");
  const char* arg2 = strtok(nullptr, " \t");
  if (!cmd) return;

  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) printHelp();
  else if (!strcmp(cmd, "status")) printStatus();
  else if (!strcmp(cmd, "ip"))   reply(parseIp(arg, cfg.ip));
  else if (!strcmp(cmd, "mask")) reply(parseIp(arg, cfg.mask));
  else if (!strcmp(cmd, "gw"))   reply(parseIp(arg, cfg.gw));
  else if (!strcmp(cmd, "port")) {
    const long v = arg ? atol(arg) : 0;
    if (v >= 1 && v <= 65535) cfg.tcpPort = v;
    reply(v >= 1 && v <= 65535);
  }
  else if (!strcmp(cmd, "id")) {
    const long v = arg ? atol(arg) : 0;
    if (v >= 1 && v <= 247) cfg.rtuId = v;
    reply(v >= 1 && v <= 247);
  }
  else if (!strcmp(cmd, "baud")) {
    const uint32_t v = arg ? atol(arg) : 0;
    if (validBaud(v)) cfg.rtuBaud = v;
    reply(validBaud(v));
  }
  else if (!strcmp(cmd, "format")) {
    bool ok = false;
    for (uint8_t i = 0; i < 4 && arg; i++) {
      char name[4]; strcpy(name, fmtName[i]);
      for (char* p = name; *p; p++) *p = tolower(*p);
      if (!strcmp(arg, name)) { cfg.rtuFormat = i; ok = true; }
    }
    reply(ok);
  }
  else if (!strcmp(cmd, "ana1") || !strcmp(cmd, "ana2")) {
    const uint8_t ch = cmd[3] - '1';
    bool ok = false;
    for (uint8_t m = ANA_10V; m <= ANA_OUT && arg; m++) {
      if (strcmp(arg, anaModeName[m])) continue;
      cfg.anaMode[ch] = m;
      applyAnaMode(ch);
      applyOutputs();
      publishWritable();
      saveConfig();
      ok = true;
    }
    reply(ok);
    if (ok && cfg.anaMode[ch] == ANA_OUT)
      SerialUSB.println(F("Remember: DIP switches of this channel in 'Out Mos NPN' position"));
  }
  else if (!strcmp(cmd, "wdt")) {
    const long v = arg ? atol(arg) : -1;
    const bool ok = arg && v >= 0 && v <= 3600;
    if (ok) {
      cfg.wdtSec = v;
      lastRequestMs = millis();
      wdtTripped = false;
      publishWritable();
      saveConfig();
    }
    reply(ok);
  }
  else if (!strcmp(cmd, "out")) {
    const long n = arg ? atol(arg) : 0;
    bool v;
    bool ok = n >= 1 && n <= CO_COUNT && parseOnOff(arg2, v);
    if (ok && n >= 5 && cfg.anaMode[n - 5] != ANA_OUT) {
      SerialUSB.println(F("ERROR: set ana1/ana2 to 'out' first"));
      return;
    }
    if (ok) {
      cmdOut[n - 1] = v;
      applyOutputs();
      publishWritable();
    }
    reply(ok);
  }
  else if (!strcmp(cmd, "mon")) reply(parseOnOff(arg, monitorOn));
  else if (!strcmp(cmd, "log")) reply(parseOnOff(arg, logOn));
  else if (!strcmp(cmd, "defaults")) {
    setDefaults();
    SerialUSB.println(F("OK, factory defaults loaded: type 'save' to keep them"));
  }
  else if (!strcmp(cmd, "save")) {
    saveConfig();
    SerialUSB.println(F("Saved, rebooting..."));
    rebootPending = true;
    tReboot.reset();
  }
  else if (!strcmp(cmd, "reboot")) {
    SerialUSB.println(F("Rebooting..."));
    rebootPending = true;
    tReboot.reset();
  }
  else SerialUSB.println(F("Unknown command, type 'help'"));
}

void consolePoll() {
  static char buf[64];
  static uint8_t len = 0;
  static uint8_t portState = 0;     // 0 = closed, 1 = just opened, 2 = open

  // Status page every time a terminal opens the COM port (DTR set).
  if (!SerialUSB) portState = 0;
  else if (portState == 0) { portState = 1; tPortOpen.reset(); }
  else if (portState == 1 && tPortOpen.stato()) {
    portState = 2;
    len = 0;
    printStatus();
    SerialUSB.println(F("Type 'help' for the command list"));
    SerialUSB.print(F("> "));
  }

  while (SerialUSB.available()) {
    const char c = SerialUSB.read();
    if (c == '\r' || c == '\n') {
      if (len == 0) continue;
      buf[len] = 0;
      SerialUSB.println();
      execCommand(buf);
      len = 0;
      SerialUSB.print(F("> "));
    } else if (c == '\b' || c == 127) {
      if (len) { len--; SerialUSB.print(F("\b \b")); }
    } else if (len < sizeof(buf) - 1 && c >= ' ') {
      buf[len++] = c;
      SerialUSB.print(c);   // echo for terminals without local echo
    }
  }
}

// =============================================================================
// Housekeeping
// =============================================================================
void housekeeping() {
  if (tLed.stato()) {
    tLed.reset();
    if (digitalRead(LED_BUILTIN)) resLed; else setLed;
  }

  if (tMonitor.stato()) {
    tMonitor.reset();
    if (ethPresent) {
      const bool link = Ethernet.linkStatus() == LinkON;
      if (link != ethLink) {
        ethLink = link;
        SerialUSB.println(link ? F("Ethernet: link UP") : F("Ethernet: link DOWN"));
        tLed.tempo(ethLink ? 500 : 125);  // fast blink = no Ethernet link
      }
    }
    if (monitorOn) printIo();
  }

  if (rebootPending && tReboot.stato()) {
    SerialUSB.flush();
    NVIC_SystemReset();
  }
}

// =============================================================================
void setup() {
  SerialUSB.begin(115200);
  gevino_io_setup();

  // Non-blocking 12-bit analog acquisition, 1.0 V reference
  analogPrescaler(ADC_PRESCALER_DIV32);
  analogReadExtended(12);
  analogGain(ADC_GAIN_1);
  analogReference2(ADC_REF_INT1V);
  analogCalibrate();
  analogEnable();

  loadConfig();
  buildMac();
  applyAnaMode(0);
  applyAnaMode(1);
  applyOutputs();

  servers[0] = &ModbusRTUServer;
  for (uint8_t i = 0; i < MAX_TCP_CLIENTS; i++) servers[1 + i] = &tcpModbus[i];

  rtuBegin();
  tcpBegin();
  publishWritable();
  publishInputs();
  lastRequestMs = millis();

  // The status page is printed by consolePoll() when a terminal opens the port.
}

void loop() {
  analogPolling();
  analogFilter();
  publishInputs();
  rtuPoll();
  tcpPoll();
  commWatchdog();
  consolePoll();
  housekeeping();
}
