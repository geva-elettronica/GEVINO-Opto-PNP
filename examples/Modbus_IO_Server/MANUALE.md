# GEVINO Opto PNP — Modbus I/O Server

Manuale d'uso del firmware `Modbus_IO_Server` (v1.00).

Il firmware trasforma il GEVINO Opto PNP in un **modulo I/O remoto Modbus**: tutte le risorse della
scheda sono leggibili e comandabili **contemporaneamente** via:

- **Modbus TCP** sulla porta Ethernet (modulo W5500), fino a **4 client** collegati insieme;
- **Modbus RTU** sul bus **RS485** (morsetti RS-A / RS-B).

I master TCP e RTU vedono la **stessa immagine I/O**: se un master accende un'uscita, tutti gli
altri la leggono accesa.

---

## 1. Risorse disponibili

| Risorsa | Quantità | Note |
|---|---|---|
| Ingressi digitali optoisolati | 11 (In1…In11) | |
| Uscite digitali PNP | 4 (Out1…Out4) | 2,5 A, protette |
| Ingressi analogici | 2 (Ana1, Ana2) | 10 V, 1 V o 20 mA, 12 bit |
| Uscite NPN ausiliarie | 2 (Out5 = Ana1, Out6 = Ana2) | solo se l'ingresso analogico è messo in modo uscita |

Ana1 e Ana2 sono **commutabili**: ognuno può lavorare come ingresso analogico (10 V / 1 V / 20 mA)
oppure come uscita digitale NPN (Out5 / Out6). Il modo si sceglie da Modbus o da console, ma
**bisogna anche impostare i DIP switch** sulla scheda (vedi capitolo 2).

---

## 2. DIP switch — OBBLIGATORIO

Il firmware non può leggere i DIP switch: **la posizione dei DIP deve corrispondere al modo
impostato nel firmware** (registro `HR1` / `HR2` o comando `ana1` / `ana2`).
Se non corrispondono, la lettura analogica è sbagliata oppure l'uscita NPN non commuta.

> ⚠️ Cambiare i DIP **a scheda spenta** e con i carichi scollegati.

### Ana1 (A0) — DIP 1…5

| Modo firmware | Valore HR1 | DIP 1 | DIP 2 | DIP 3 | DIP 4 | DIP 5 |
|---|---|---|---|---|---|---|
| `10v` (default) | 0 | off | off | off | off | **on** |
| `1v` | 1 | off | off | **on** | off | **on** |
| `20ma` | 2 | **on** | off | **on** | off | **on** |
| `out` → **Out5 NPN** | 3 | off | **on** | off | off | off |

### Ana2 (A2) — DIP 6…8

| Modo firmware | Valore HR2 | DIP 6 | DIP 7 | DIP 8 |
|---|---|---|---|---|
| `10v` (default) | 0 | off | off | off |
| `1v` | 1 | off | off | **on** |
| `20ma` | 2 | **on** | off | **on** |
| `out` → **Out6 NPN** | 3 | off | **on** | off |

Le uscite Out5 / Out6 sono **NPN** (chiudono verso GND): il carico va collegato tra il +Vcc e il
morsetto Ana1 / Ana2.

---

## 3. Impostazioni di fabbrica

| Parametro | Valore |
|---|---|
| IP | **192.168.1.100** (statico) |
| Subnet mask | 255.255.255.0 |
| Gateway | 192.168.1.1 |
| Porta Modbus TCP | 502 |
| Indirizzo Modbus RTU | 1 |
| Baud rate RS485 | 19200 |
| Formato RS485 | 8N1 |
| Ana1 / Ana2 | ingresso 10 V |
| Watchdog comunicazione | disattivato |

Il MAC address è generato automaticamente dal numero di serie del microcontrollore (inizia con
`02:47:45`), quindi ogni scheda ha il suo.

> Le impostazioni sono salvate nella flash del processore. **Caricando un nuovo sketch con
> l'Arduino IDE la flash viene cancellata** e tornano i valori di fabbrica.

---

## 4. Mappa Modbus

Tutti gli indirizzi sono **a base 0** (come nel protocollo). Se il vostro master usa la notazione
"a base 1" (es. 40001), aggiungete 1.
In Modbus TCP lo Unit ID può essere qualsiasi valore da 1 a 255 (consigliato 1 o 255). **Non usare
Unit ID 0**: è l'indirizzo broadcast, il comando viene eseguito ma la scheda non risponde.

### Coils — FC 1 (lettura), FC 5 / 15 (scrittura)

| Indirizzo | Risorsa |
|---|---|
| 0 | Out1 |
| 1 | Out2 |
| 2 | Out3 |
| 3 | Out4 |
| 4 | Out5 (Ana1) — funziona solo se Ana1 è in modo `out`, altrimenti torna a 0 |
| 5 | Out6 (Ana2) — funziona solo se Ana2 è in modo `out`, altrimenti torna a 0 |

### Discrete inputs — FC 2

| Indirizzo | Risorsa |
|---|---|
| 0 … 10 | In1 … In11 (1 = ingresso attivo) |
| 11 … 16 | Stato reale delle uscite Out1 … Out6 |

### Input registers — FC 4

| Indirizzo | Contenuto | Unità |
|---|---|---|
| 0 | Ana1 grezzo | 0…4095 (0 se Ana1 è uscita) |
| 1 | Ana2 grezzo | 0…4095 (0 se Ana2 è uscita) |
| 2 | Ana1 scalato | mV (modi 10 V / 1 V) oppure µA (modo 20 mA) |
| 3 | Ana2 scalato | mV oppure µA |
| 4 | Ingressi In1…In11 a bit | bit0 = In1 … bit10 = In11 |
| 5 | Uscite reali Out1…Out6 a bit | bit0 = Out1 … bit5 = Out6 |
| 6 | Versione firmware | 100 = v1.00 |
| 7 | Uptime, parte bassa | secondi |
| 8 | Uptime, parte alta | secondi × 65536 |
| 9 | Indirizzo RTU attuale | |
| 10 | Baud rate RTU / 100 | es. 192 = 19200 |
| 11 | Client TCP collegati | 0…4 |

Esempio scalato: in modo 10 V, una tensione di 5 V restituisce circa `5000`; in modo 20 mA, una
corrente di 12 mA restituisce circa `12000`. I fondoscala sono nominali (vedi capitolo 8).

### Holding registers — FC 3 (lettura), FC 6 / 16 (scrittura)

| Indirizzo | Contenuto | Valori |
|---|---|---|
| 0 | Uscite Out1…Out6 a bit (alternativa alle coils) | bit0 = Out1 … bit5 = Out6 |
| 1 | Modo Ana1 | 0 = 10 V, 1 = 1 V, 2 = 20 mA, 3 = uscita Out5 |
| 2 | Modo Ana2 | 0 = 10 V, 1 = 1 V, 2 = 20 mA, 3 = uscita Out6 |
| 3 | Watchdog comunicazione | secondi, 0 = disattivato, max 3600 |

- I registri **1, 2 e 3 vengono salvati in flash** quando cambiano: non scriveteli ciclicamente con
  valori diversi (la flash ha un numero limitato di scritture). Scriverli con lo stesso valore non
  provoca scritture.
- Valori non validi vengono ignorati e il registro torna al valore precedente.

### Watchdog di comunicazione

Se `HR3` è diverso da 0 e **nessun master** (TCP o RTU) invia richieste per quel numero di secondi,
**tutte le uscite vengono spente**. Alla prima richiesta successiva il funzionamento riprende
normalmente (le uscite restano spente finché il master non le riaccende).

---

## 5. Console di configurazione (USB)

Collegate il GEVINO al PC con il cavo **USB-C** e aprite un terminale seriale (Monitor seriale
dell'Arduino IDE, PuTTY, TeraTerm…): **115200 baud**, fine riga "Newline" o "CR".
**Ogni volta che si apre la porta COM** la scheda stampa la pagina di stato (configurazione,
connessioni, I/O); scrivete `help` per l'elenco dei comandi. Se il terminale non mostra nulla,
verificate che attivi il segnale **DTR** (PuTTY, TeraTerm e il Monitor seriale dell'IDE lo fanno),
oppure scrivete `status`.

### Comandi

| Comando | Esempio | Effetto |
|---|---|---|
| `help` o `?` | | elenco comandi |
| `status` | | configurazione, connessioni e stato I/O |
| `ip <indirizzo>` | `ip 192.168.0.50` | indirizzo IP (*) |
| `mask <maschera>` | `mask 255.255.255.0` | subnet mask (*) |
| `gw <gateway>` | `gw 192.168.0.1` | gateway (*) |
| `port <n>` | `port 502` | porta Modbus TCP (*) |
| `id <1…247>` | `id 5` | indirizzo Modbus RTU (*) |
| `baud <velocità>` | `baud 9600` | 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 (*) |
| `format <fmt>` | `format 8E1` | 8N1, 8E1, 8O1, 8N2 (*) |
| `ana1 <modo>` | `ana1 out` | modo Ana1: `10v`, `1v`, `20ma`, `out` — **salvato subito** |
| `ana2 <modo>` | `ana2 20ma` | modo Ana2 — **salvato subito** |
| `wdt <secondi>` | `wdt 10` | watchdog comunicazione, 0 = off — **salvato subito** |
| `out <1…6> <0/1>` | `out 2 1` | forza un'uscita (per test) |
| `mon on` / `mon off` | | stampa lo stato I/O ogni secondo |
| `log on` / `log off` | | stampa i comandi uscite ricevuti via Modbus |
| `defaults` | | ricarica i valori di fabbrica (*) |
| `save` | | **salva** le impostazioni (*) e **riavvia** la scheda |
| `reboot` | | riavvia senza salvare |

(*) = la modifica ha effetto solo dopo `save` (che salva e riavvia).

### Esempio: cambiare IP, indirizzo RTU e baud rate

```
> ip 192.168.10.20
OK
> gw 192.168.10.1
OK
> id 3
OK
> baud 9600
OK
> save
Saved, rebooting...
```

Dopo il riavvio il terminale va richiuso e riaperto (la porta USB si riconnette).

### Esempio: Ana2 come uscita NPN

1. Spegnere la scheda e mettere i DIP 6 = off, 7 = on, 8 = off.
2. Riaccendere, da console: `ana2 out`
3. Test: `out 6 1` → l'uscita Out6 si accende; `out 6 0` → si spegne.

---

## 6. LED

| LED giallo frontale | Significato |
|---|---|
| lampeggio lento (1 Hz) | funzionamento normale, Ethernet collegata |
| lampeggio veloce (4 Hz) | cavo Ethernet scollegato / nessun link (RTU funziona comunque) |

---

## 7. Collegamenti e note

- **RS485**: morsetti RS-A / RS-B. Su bus lunghi terminare le estremità con 120 Ω.
  Il ponticello W9 deve essere chiuso (driver RS485 collegato).
- **Ethernet**: richiede il modulo opzionale W5500. Se il modulo non c'è, il firmware funziona
  solo in Modbus RTU e lo segnala in console.
- Alla accensione e dopo ogni riavvio **tutte le uscite partono spente**.
- Modbus TCP: massimo 4 connessioni insieme. Se si collega un quinto client, viene chiusa la
  connessione inattiva da più tempo. Una connessione senza richieste per 2 minuti viene chiusa.
- Software di test consigliati: **Modbus Poll**, **QModMaster**, **mbpoll** (riga di comando).

### Test rapido con mbpoll

```bash
mbpoll -m tcp -a 1 -t 0 -r 1 -c 6 192.168.1.100
```

(legge le 6 coils; con `-t 1` gli ingressi, `-t 3` gli input register, `-t 4` gli holding register.
mbpoll usa indirizzi a base 1, quindi `-r 1` = indirizzo 0.)

---

## 8. Precisione analogica

Il convertitore lavora a 12 bit con riferimento interno da 1,0 V; ogni portata (10 V, 1 V, 20 mA)
viene ridotta a circa 1 V dalla rete d'ingresso. Il valore scalato usa i fondoscala **nominali**
(10000 mV, 1000 mV, 20000 µA a 4095 punti). Per misure precise usare il valore grezzo e calibrare
nel master, oppure correggere le costanti `FS_10V_MV`, `FS_1V_MV`, `FS_20MA_UA` nel sorgente.
Il valore è filtrato (costante di tempo circa 30 ms).

---

## 9. Librerie necessarie (per ricompilare)

- **GEVINO** (questa libreria)
- **ArduinoModbus**
- **ArduinoRS485 — versione modificata GEVA** (pin di trasmissione RS485 su A1), **fornita in
  questa cartella** in `ArduinoRS485/`. Copiarla in `Documenti\Arduino\libraries\` sostituendo
  quella eventualmente già installata (istruzioni in `ArduinoRS485/LEGGIMI.txt`).
  ⚠️ Con la ArduinoRS485 originale il Modbus RTU **non funziona**.
- **Ethernet** (Arduino)
- **FlashStorage** (cmaglie)

Scheda: **Arduino Zero (Native USB Port)**, core Arduino SAMD.

---

GEVA Elettronica — https://www.gevaelettronica.it — info@gevaelettronica.it
