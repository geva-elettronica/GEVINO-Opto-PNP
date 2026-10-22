# CLAUDE.md — Modbus_IO_Server

Guida per chi **testa** o **modifica** questo firmware (persone o agenti AI come Claude Code).
Il manuale utente è [MANUALE.md](MANUALE.md); le regole generali per programmare la scheda sono in
[../../AGENTS.md](../../AGENTS.md) e nell'header [../../src/gevino_opto_pnp_io.h](../../src/gevino_opto_pnp_io.h).

## Cosa fa

Sketch unico [Modbus_IO_Server.ino](Modbus_IO_Server.ino) per GEVINO Opto PNP (SAMD21, Arduino Zero):
espone 11 ingressi, 4 uscite PNP, 2 analogici (commutabili in uscite NPN Out5/Out6) via
**Modbus TCP** (W5500, fino a 4 client) e **Modbus RTU** (RS485 su `Serial1`) contemporaneamente,
più una console di configurazione su `SerialUSB`. Configurazione persistente in flash (FlashStorage).

## Compilare

- Scheda: **Arduino Zero (Native USB Port)** — FQBN `arduino:samd:arduino_zero_native` (core SAMD 1.8.x).
- Librerie: GEVINO, ArduinoModbus, **ArduinoRS485 modificata GEVA**, Ethernet, FlashStorage.
- Da riga di comando (arduino-cli incluso nell'Arduino IDE 2):

```bash
"/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe" compile --config-file ~/.arduinoIDE/arduino-cli.yaml -b arduino:samd:arduino_zero_native .
```

I warning `ADC_CTRLB_RESSEL_16BIT_Val redefined` e `inline variables` vengono dall'header della
libreria e sono innocui. Occupazione v1.00: ~59 KB flash, ~5,4 KB RAM.

### ⚠️ ArduinoRS485 deve essere la versione GEVA

Una copia è inclusa in [ArduinoRS485/](ArduinoRS485/): va copiata in `libraries/` al posto
dell'originale (vedi `ArduinoRS485/LEGGIMI.txt`). Non spostarla in `src/` dello sketch: verrebbe
compilata insieme a quella in `libraries/` dando errori di simboli duplicati.
La libreria è modificata: ultima riga di `RS485.cpp`
`RS485Class RS485(Serial1, 1, A1, -1);` (DE su A1). Con l'originale Arduino il pin DE è sbagliato
e l'RTU non trasmette. Inoltre `RS485Class::end()` di quella versione chiama `pinMode(-1, ...)`
quando `rePin = -1`: **non chiamare `ModbusRTUServer.end()` / `RS485.end()`**. Per questo i
cambi di baud/indirizzo si applicano con salvataggio + riavvio, non a caldo.

## Architettura (da conoscere prima di modificare)

- **Immagine condivisa**: ogni server Modbus (RTU + 4 slot TCP) ha la sua mappa di registri
  interna ad ArduinoModbus. La verità è in `cmdOut[]` (uscite comandate) e `cfg` (modi Ana,
  watchdog). Array `servers[]`: `[0]` = RTU, `[1..4]` = TCP.
  - `publishInputs()` — ogni giro di `loop()` copia ingressi/analogici/diagnostica in tutti i server.
  - `collectWrites(server)` — dopo ogni richiesta servita (`poll()` == 1) legge coils e holding
    register da **quel** server, aggiorna `cmdOut[]`/`cfg`, poi `publishWritable()` riallinea tutti.
  - Se cambia `HR0` (bitmap) vince la bitmap, altrimenti si leggono le coils.
- **Non bloccante**: nessun `delay()` nel `loop()`. Temporizzazioni con la classe `Timer` della
  libreria (`stato()` + `reset()`; attenzione: `tempo()` riavvia il conteggio, non chiamarlo a
  ogni giro).
- **Analogici**: `analogPolling()` della libreria (12 bit, rif. 1 V, prescaler DIV32) +
  filtro esponenziale in `analogFilter()` (`anaFilt[]` = valore × 16). `analogResult[0]` = Ana1,
  `analogResult[2]` = Ana2.
- **Ana1/Ana2 ↔ Out5/Out6**: `applyAnaMode()` mette il pin in `OUTPUT` oppure `INPUT` +
  `pinPeripheral(pin, PIO_ANALOG)`. In modo analogico la coil corrispondente è forzata a 0.
  Il firmware **non legge i DIP switch**: l'utente deve impostarli (tabelle in MANUALE.md).
- **TCP**: `EthernetServer::accept()` non bloccante; gli `EthernetClient` sono globali perché
  `ModbusTCPServer::accept()` ne memorizza il puntatore. Slot pieni → si chiude il client meno
  recente. Inattività > `TCP_IDLE_MS` → chiusura.
- **Flash**: `saveConfig()` cancella e riscrive una pagina. Va chiamato solo quando un valore
  cambia davvero. Cambiando la struct `Config` **cambiare `CFG_MAGIC`**, altrimenti si leggono
  dati vecchi come validi. Il caricamento di un nuovo sketch cancella la flash → valori di fabbrica.
- **TCP Unit ID 0** = broadcast in libmodbus: esegue ma non risponde. È un comportamento della
  libreria, documentato nel manuale.

## Mappa Modbus (sintesi, dettagli in MANUALE.md)

| Tipo | Indirizzi (base 0) |
|---|---|
| Coils | 0–5 = Out1–Out6 |
| Discrete inputs | 0–10 = In1–In11, 11–16 = stato reale Out1–Out6 |
| Input registers | 0–1 Ana grezzi, 2–3 Ana scalati (mV/µA), 4 bitmap ingressi, 5 bitmap uscite, 6 versione fw, 7–8 uptime, 9 id RTU, 10 baud/100, 11 client TCP |
| Holding registers | 0 bitmap uscite, 1–2 modo Ana (0=10V 1=1V 2=20mA 3=out), 3 watchdog s |

Aggiungendo registri: aggiornare le `#define` `*_COUNT`, `publishInputs()` / `publishWritable()` /
`collectWrites()`, **MANUALE.md** e questa tabella. Non spostare gli indirizzi esistenti
(romperebbe gli impianti già configurati): aggiungere in coda.

## Checklist di test (al banco)

Strumenti: terminale seriale 115200 su USB-C, Modbus Poll / QModMaster / `mbpoll`, adattatore
USB-RS485, alimentatore 12/24 V, carichi di prova (lampadine o LED con resistenza).

1. **Avvio**: aprendo la porta COM (DTR, rilevato con `if (SerialUSB)` in `consolePoll()`) la
   console stampa lo stato; chiudendo e riaprendo il terminale la pagina viene ristampata; IP 192.168.1.100, RTU id 1, 19200 8N1. LED giallo 1 Hz
   con cavo Ethernet, 4 Hz senza.
2. **Ingressi**: `mon on`, attivare In1…In11 uno alla volta → bit corretto in console, in DI 0–10
   e in IR4.
3. **Uscite TCP**: scrivere coil 0–3 → Out1–Out4 si accendono; DI 11–14 e IR5 riflettono lo stato.
   Scrivere HR0 = 15 → tutte accese; HR0 = 0 → spente.
4. **Uscite RTU**: stesse prove via RS485; verificare che il master TCP veda le modifiche fatte
   dall'RTU e viceversa.
5. **Analogici**: con 5 V su Ana1 in modo 10 V → IR2 ≈ 5000. Provare 1 V e 20 mA (DIP adeguati).
6. **Ana come uscita**: DIP in "Out Mos NPN", `ana1 out` (oppure HR1 = 3) → coil 4 comanda Out5;
   IR0 = 0. Tornare con `ana1 10v` → coil 4 forzata a 0. Ripetere per Ana2/Out6/HR2.
7. **Console**: `ip`, `id`, `baud`, `format`, `save` → dopo il riavvio i nuovi parametri sono
   attivi (verificare con `status` e col master). `defaults` + `save` → ritorno di fabbrica.
8. **Watchdog**: HR3 = 5, fermare il polling → dopo 5 s uscite spente e messaggio in console;
   riprendere il polling → messaggio "restored".
9. **TCP multi-client**: aprire 4 connessioni insieme; la 5ª chiude la meno recente. Staccare il
   cavo di rete → `link DOWN` in console, RTU continua a funzionare.
10. **Senza modulo W5500**: il firmware deve partire e funzionare solo in RTU.
11. **Persistenza**: modo Ana e watchdog devono sopravvivere a un riavvio (tasto Reset).

## Possibili sviluppi

- Watchdog hardware del SAMD21 (es. libreria Adafruit_SleepyDog) per ripartire in caso di blocco.
- Configurazione di rete anche via Modbus (registri protetti da password).
- Lettura del LED/segnale di Fault delle uscite PNP.
- DHCP opzionale.

## Convenzioni

- Codice e commenti in inglese (come la libreria), documentazione utente in italiano.
- Usare le macro della libreria (`In1`, `setOut1`, `Ana1`, …), mai numeri di pin.
- Chiamare `gevino_io_setup()` una sola volta in `setup()`.
- Aggiornare `FW_VERSION` e il changelog in testa al `.ino` a ogni modifica rilasciata.
