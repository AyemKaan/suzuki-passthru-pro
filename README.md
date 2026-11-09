# Suzuki OBD Professional (J2534 Diagnostic Suite) — BETA

> [!WARNING]
> **BETA RELEASE — HARDWARE VERIFICATION NOTICE**
> This software is currently in **BETA** stage. Not 100% of Suzuki models have been physically tested yet.
> - **Physically Hardware-Verified Vehicle:** **Suzuki Grand Vitara 1.9 DDiS (JB419D / F9Q)** (CAN + K-Line Pin 7).
> - **Other Suzuki Models:** Protocol definitions, ECU addresses, calibrations, and BCM coding routines are included for experimental / beta testing across CAN (`ISO 15765-4`) and K-Line (`ISO 14230-4`). Use at your own discretion.

A standalone, zero-dependency C++ diagnostic server and bilingual (English default / Turkish optional) web interface for Suzuki vehicles using any standard SAE J2534 PassThru adapter (MongoosePro JLR, Scanmatik 2 Pro, Tactrix OpenPort 2.0, VCX SE, Chipsoft, etc.).

Supports **CAN (`ISO 15765-4` / 500 kbps)**, **K-Line (`ISO 14230-4` KWP2000 Fast Init / Pin 7 / 10400 baud)**, and **UDS (`ISO 14229`)**.

---

## Key Features

1. **System Scan & DTC Diagnostics (Tab 1):**
   - Full multi-ECU scanning across Powertrain (ECM/TCM), ABS/ESP, 4WD/AllGrip, BCM, Airbag/SRS, EPS, Combination Meter (IPC), HVAC Climate, TPMS, and Keyless/Immobilizer.
   - Automatic vehicle identification (`4,058` ECU identity rules & `1,636` model definitions) that dynamically filters the interface to match the connected vehicle.
   - `18,225` bilingual (English / Turkish) DTC descriptions and Freeze-Frame snapshot decoding (`12`, `19 04`, `21 87`).

2. **Live Data / Data List (Tab 2):**
   - Real-time engine & chassis telemetry (RPM, rail pressure, boost, DPF soot mass, differential pressure, temperatures, steering angle, yaw rate, lateral G).
   - `6,337` extended Service `21` / `22` parameters and `165` computed parameters.

3. **Service, Calibrations & Actuator Tests (Tab 3):**
   - **23 One-Click Quick Service Cards** dynamically filtered by detected vehicle profile:
     - Grand Vitara 1.9 DDiS (`F9Q`): EGR adaptation reset, Inlet throttle learning, Injector IMA read & ZFC reset, DPF soot reset & service regeneration, Oil dilution reset, High-pressure rail leak test, ECM replacement memory transfer, Starter check.
     - CAN Diesel (`D13A` / `D16AA` / `D20AA`): DPF regeneration, Oil reset, EGR learning, Diff-pressure / MAF / A-F / Rail sensor resets.
     - Petrol ECM: Throttle learning, IAC reset, Fuel trim reset, Brake stroke & GPF reset.
     - Chassis & Body: SAS & G-Sensor calibration, ABS air bleeding, TCM/AMT/CVT learning, EPS zero-point calibration, BCM crash history clear, TPMS registration, IPC service reset, Auto Headlight Leveling (`ALC`) init.
   - **Full Searchable Catalog:** `631` Active Tests (ON / ALT / EXIT) and `570` Utility procedures.

4. **ECU Identity & Direct K-Line Tools (Tab 4):**
   - Direct `ISO 14230` Fast-Init K-Line access on Pin 7 (`0x29` ABS, `0x21` 4WD, `0x5A` BCM, `0x59` SRS, `0x5B` IMM, `0x99` AC, `0x11` ECM, `0x13` TCM, `0x31` EPS).
   - Local ID scanner (`21 01..FE`) and offline packet decoder.

5. **Model-by-Model Hidden Features, BCM Coding & Immobilizer / Key Programming (Tab 5):**
   - **Immobilizer, Transponder & Keyless Key Programming (`IMMO-01..207` — `POST /api/immo/action`):**
     - **Safe Read-Only Diagnostics (No PIN Required):**
       - Read Registered Key Count & Registration Year/Month (`21 A1` / `21 A0` / `21 01` across CAN `0x7E0`/`0x7E4` & K-Line `0x5A`/`0x5B`/`0x11`).
       - Read Transponder Key Chip 10-Byte ID (`21 B0` — `IMMO-05`, `IMMO-06`, `IMMO-201`).
       - Direct EEPROM PIN Read (`10 C0 -> 21 21` — `IMMO-106`).
       - Keyless Interior Antenna Diagnosis (`2F 01 06 03 00 05` — `BCM-167`).
     - **4-Digit PIN Key & Remote Registration (Grand Vitara JB, Swift, SX4, Jimny — CAN & K-Line):**
       - Built-in Suzuki Factory PIN presets (`0895`, `9502`, `5634`, `0325`) + custom 4-digit PIN entry.
       - Safe **Verify PIN Only** check (`30 A0 07 80 80 <PIN>` CAN / `A5 A0 80 00 <PIN>` K-Line).
       - **Register Transponder Key** (`30 A1 08 80 80` CAN / `A5 A1 80 00` K-Line / `31 A1 00` `IMMO-202` / `3B D2 00 01` `IMMO-203`).
       - **Register Remote Controller Fob** (`A5 A1 20 00` K-Line / `30 62 07` CAN `IMMO-204`).
       - **ECM <-> Immobilizer Control Unit Pairing / Initialization** (`31 01 <PIN>` — `IMMO-101`).
       - **Electronic Steering Lock (ESL) Unlock** (`31 0D 00 <PIN>` — `IMMO-109`).
       - **Erase All Registered Keys** (`30 A1 08 08` CAN / `A5 A1 08` K-Line, protected by double confirmation).
     - **2015+ UDS Seed-Key Immobilizer (`IMMO-110..113` — Vitara LY, S-Cross, Swift A2L):**
       - Request Security Seed (`10 03 -> 27 03`) and Send Calculated Key (`27 04 <KEY> -> 31 01 01 0D`).
   - **21 Model-Specific Hidden Features & BCM Personalizations:**
     - Reads current BCM configuration state (`21 08` / `21 01` across CAN `0x7E4` and K-Line `0x5A`) and automatically filters **21 hidden features / personalizations** according to the selected or auto-detected vehicle profile:
     - `HF-DRL`: Daytime Running Lights (DRL) ON / OFF (`30 36 08 80`)
     - `HF-AUTOLOCK`: Speed (15 km/h) or Shift-linked Auto Door Lock (`30 35 08 01` / `3B 2F`)
     - `HF-AUTOUNLOCK`: Ignition OFF or Shift-P Auto Door Unlock (`30 35 08 02` / `3B 02`)
     - `HF-LANEFLASH`: Comfort Turn Signal Lane-Change Flash Count (`OFF` / `2` / `3` / `4` Flashes `30 35 08 20`)
     - `HF-MIRROR`: Auto Folding Side Mirrors on Lock (`30 33 08 20`)
     - `HF-CORNER`: Cornering Fog Lights (`30 31 08 04`)
     - `HF-BUZZER`: Lock / Unlock Buzzer, Horn, or Siren Answerback (`30 33 08 80` / `3B 2B`)
     - `HF-ALARM`: Factory Security Anti-Theft Alarm ON / OFF (`30 34 08 20` / `30 09 08 20` / `3B 21`)
     - `HF-UNLOCKMODE`: Remote Door Unlock Behavior (`1-Press All Doors` vs `2-Press Driver First` `30 3B 08 10`)
     - `HF-FOLLOWHOME`: Guide Me Home / Follow Me Home Headlight Timer (`0s..30s` `30 3B 08 02` / `3B 39`)
     - `HF-GUIDECAR`: Guide Me To Car Welcome Lighting Timer (`0s..25s` `30 3B 08 04`)
     - `HF-FOOTLIGHT`: Footwell Ambient Lighting Mode (`Headlights+Doors` / `Doors Only` / `OFF` `30 3B 08 80`)
     - `HF-HBA`: High Beam Assist Activation (`30 36 08 40`)
     - `HF-ANSWERLIGHT`: Lock Flash Mode (`Hazard Lights` vs `Interior Dome Lamp Only` `30 35 08 80` / `30 10 08 80`)
     - `HF-AUTOLIGHT-HW`: Auto Light Sensor / Keyless / I-Key Retrofit Variant Coding (`3B 3A` / `3B 33` / `3B 34`)
     - `HF-ROOMLAMP`: Interior Dome Lamp Fade-Out Delay Timer (`3B 16` / `3B 17`)
     - `HF-RETAINEDPWR`: Retained Power Window Operation After IG-OFF (`3B 27`)
     - `HF-UDS-IKEY`: UDS Smart Key & Door Handle Request Switch Customization (`2E 14 01..14`)
     - `HF-SEATBELT`: Seatbelt Reminder Acoustic Chime ON / OFF (`2E 20 41`)
     - `HF-CLUSTER-THEME`: Cluster / Center Display Icon Color Theme (`30 05 07 40` / `31 01 C0 11`)
     - `HF-ESP-DYNO`: ESP Off-Road / Dyno Full Deactivation Mode (`30 80 07 04`)
8. **Immobilizer, Transponder & Keyless Key Programming (`IMMO-01..207` — 12 Actions)**
   - **Safe Read-Only Diagnostics (No PIN Required):** Registered Key Count & Date (`21 A1` / `21 01`), Transponder Chip ID (`21 B0`), Direct EEPROM PIN Read (`10 C0 -> 21 21`), Keyless Interior Antenna Test (`2F 01 06`).
   - **4-Digit PIN Key & Remote Coding (CAN `0x7E4` & K-Line `0x5B`/`0x5D`):** Factory Suzuki PIN presets (`0895`, `9502`, `5634`, `0325`) + custom PIN, Safe PIN Verification (`30 A0 07 80` / `A5 A0 80`), Transponder Key Registration (`30 A1 08 80` / `A5 A1 80`), Remote Fob Registration (`A5 A1 20` / `30 62 07`), ECM–Immobilizer Pairing (`31 01 + PIN`), Electronic Steering Lock (ESL) Unlock (`31 0D 00 + PIN`), and Double-Confirmed Erase All Keys (`30 A1 08 08` / `A5 A1 08`).
   - **2015+ UDS Seed-Key Immobilizer (`IMMO-110..113`):** Dynamic Seed Request (`10 03 -> 27 03`) and Key Unlock + Key Registration (`27 04 -> 31 01 01 0D`).
9. **Injector IMA Calibration Code Read/Write (Cylinders 1–4) & 17-Digit VIN Read/Write**
   - **Injector IMA Coding (`POST /api/injector/ima`):** Reads and writes individual 7-character Injector IMA codes for Cylinders 1–4 (`21 AD` -> `3B AD FF <28 ASCII>` + `3B 7E FE` ZFC Reset on F9Q 1.9 DDiS, plus `21 54` / `3B 54` on CAN Diesel).
   - **VIN Read & Registration (`POST /api/vin`):** Reads 17-character VIN via `1A 90`, `22 F1 90`, `21 90`, `09 02` (CAN & K-Line) and writes 17-character ASCII VIN to replacement ECMs (`30 12/76 08 80` / `A5 0F 40` unlock -> `3B 90` / `2E F1 90`).
10. **EPB, HVAC, SHVS Hybrid & ADAS Calibrations + Road-Test CSV Logger & HTML Report Export**
    - **4 Additional Quick Service Cards:** `EPB-01` Electronic Parking Brake Pad Replacement Mode, `HVAC-01` Climate Air Mix Flap & Compressor Init, `HVBMU-01` SHVS Mild/Strong Hybrid BMU Reset, and `ADAS-01` Camera/Radar/BSM/360 AVM Calibration Init.
    - **Road-Test CSV Data Logger & HTML Diagnostic Report:** Record live sensor streams to `.csv` during road tests (Tab 2) and export a full vehicle ECU & DTC scan report as `.html` in one click (Tab 1).

---

## Quick Start

### Prerequisites
1. Windows 10 or 11 (x86/x64).
2. Installed 32-bit SAE J2534 PassThru driver (MongoosePro, Scanmatik 2, OpenPort 2.0, etc.).
3. Visual Studio 2022 C++ x86 build tools (only required if compiling from source).

### Running
1. Launch `Suzuki_Diyagnostik_Baslat.bat` or run `suzuki_diag_server.exe`.
2. Open your browser at `http://127.0.0.1:8080`.
3. Default UI language is **English (`EN`)**; you can switch to **Turkish (`TR`)** anytime from the top-right selector.
4. Connect your J2534 interface to the vehicle's OBD-II port, turn Ignition ON, and click **Connect**.

### Compiling from Source
```cmd
build_diag.cmd
suzuki_diag_server.exe --check-identity
```

---

## Disclaimer
This project is an independent diagnostic tool developed for standard SAE J2534 PassThru interfaces. It is not affiliated with, sponsored by, or endorsed by Suzuki Motor Corporation. Provided "AS IS" in BETA status.
