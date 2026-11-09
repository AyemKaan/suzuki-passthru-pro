# Suzuki OBD Professional (J2534 Diagnostic Suite) — BETA

<div align="center">

![Suzuki OBD Professional Banner](docs/screenshots/03_service_calibrations.png)

### Professional Multi-ECU Diagnostic, Calibration, Coding & Key Programming Software for Suzuki Vehicles
**Zero External Dependencies · Native C++ Server Engine · Standalone SAE J2534 Stack**

[![Release](https://img.shields.io/badge/Release-v2.28.01--BETA-blue.svg)](https://github.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20(x86%20%2F%20x64)-lightgrey.svg)](https://github.com)
[![Protocols](https://img.shields.io/badge/Protocols-ISO%2015765%20(CAN)%20%7C%20ISO%2014230%20(K--Line)%20%7C%20ISO%2014229%20(UDS)-orange.svg)](https://github.com)
[![Hardware](https://img.shields.io/badge/Hardware-SAE%20J2534%20PassThru-success.svg)](https://github.com)
[![Language](https://img.shields.io/badge/Language-English%20(Default)%20%7C%20T%C3%BCrk%C3%A7e-blueviolet.svg)](https://github.com)

</div>

---

> [!WARNING]
> **BETA RELEASE — HARDWARE VERIFICATION NOTICE**
> This software is currently in active **BETA** stage. Not 100% of Suzuki models have been physically field-tested yet.
> - **Physically Hardware-Verified Vehicle:** **Suzuki Grand Vitara 1.9 DDiS (JB419D / F9Q)** (Fully tested on CAN bus and Pin 7 K-Line).
> - **Other Suzuki Models:** Protocol definitions, ECU target addresses, service calibrations, and BCM coding routines are fully implemented and mapped for experimental/beta testing across CAN (`ISO 15765-4`), K-Line (`ISO 14230-4`), and UDS (`ISO 14229`).

---

## 📸 Interface Walkthrough & Screenshots

All 5 core functional modules have been captured directly from the live application running in high-resolution:

<div align="center">

### Tab 1: System Scan & Multi-ECU DTC Inspection
*Full vehicle diagnostic scan across Powertrain, ABS/ESP, BCM, Airbag, EPS, IPC, Climate, and Keyless with 1-click HTML report export.*
![Tab 1: System Scan & DTC](docs/screenshots/01_system_scan_dtc.png)

---

### Tab 2: Live Sensor Telemetry & Road-Test CSV Logger
*Real-time sensor monitoring (RPM, Rail Pressure, Boost, DPF Soot/Differential Pressure, SAS Angle, Yaw Rate) + timestamped CSV recording.*
![Tab 2: Live Data Telemetry](docs/screenshots/02_live_data_telemetry.png)

---

### Tab 3: Service, Calibrations & Actuator Tests
*27 vehicle-filtered quick calibration cards, Injector IMA (Cyl 1–4) coding, 17-digit VIN registration, 631 active tests & 570 utility routines.*
![Tab 3: Service & Calibrations](docs/screenshots/03_service_calibrations.png)

---

### Tab 4: Direct K-Line Terminal & ECU Identification
*Direct ISO 14230 Fast Init terminal for Suzuki Pin 7 K-Line controllers (0x29 ABS, 0x21 4WD, 0x5A BCM, 0x59 Airbag, 0x5B IMM, 0x99 AC, 0x11 ECM).*
![Tab 4: ECU Identity & K-Line Tools](docs/screenshots/04_ecu_kline_tools.png)

---

### Tab 5: Hidden Features (BCM Coding) & Immobilizer Key Programming
*21 model-specific personalization features + complete Immobilizer transponder chip registration, remote coding, and ESL unlock.*
![Tab 5: Hidden Features & Immobilizer Coding](docs/screenshots/05_hidden_features_immo.png)

</div>

---

## 🌟 Overview & Architecture

**Suzuki OBD Professional** is a standalone, high-performance diagnostic suite specifically engineered for Suzuki vehicles using any compliant **SAE J2534 PassThru** interface (MongoosePro JLR/Toyota, Scanmatik 2 Pro, Tactrix OpenPort 2.0, VCX SE, Chipsoft, etc.).

- **Standalone Native Core:** Compiled into a single 32-bit Windows executable (`suzuki_diag_server.exe`) with zero external runtime dependencies.
- **Embedded Web Dashboard:** Responsive single-screen interface (`index.html`) with English set as permanent default and instant one-click Turkish localization.
- **Multi-Protocol Engine:** Synchronous support for **CAN (500 kbps)**, **K-Line Pin 7 (ISO 14230-4 Fast Init @ 10,400 baud)**, and **UDS (ISO 14229)** with automatic bus arbitration and fallback.

---

## 🚀 Key Modules & Capabilities

### 1. System Scan & Full Vehicle DTC Inspection (Tab 1)
- **Deep Multi-ECU Topology:** Scans Powertrain (ECM/TCM), ABS/ESP/VDC, 4WD/AllGrip, BCM (Body Control), SRS Airbag, EPS (Electric Power Steering), IPC (Combination Meter), HVAC Climate Control, TPMS, and Keyless Entry/Immobilizer.
- **Intelligent ECU Recognition:** Matches against `4,058` ECU identity rules and `1,636` model variants to configure service scripts specifically for your chassis.
- **Extensive DTC Library:** `18,225` factory DTC descriptions with diagnostic check points.
- **Freeze-Frame Analysis:** Automatic frame decoding across Service `12`, Service `19 04`, and Service `21 87`.
- **1-Click HTML Report Export:** Generates clean, printable `.html` diagnostic health reports.

### 2. Live Data Telemetry & Road-Test CSV Logger (Tab 2)
- High-rate streaming of essential engine and chassis parameters (RPM, Common Rail Pressure, Boost Pressure, DPF Soot Load, Differential Pressure, EGT Temperatures, Steering Angle Sensor [SAS], Yaw Rate, Lateral Acceleration).
- Extended access to `6,337` parameters (Services `21` / `22`) and `165` computed formulas.
- **Road-Test CSV Recorder:** Timestamped `.csv` logging with one-click export for spreadsheet tuning and diagnosis.

### 3. OEM Service, Calibrations & Actuator Tests (Tab 3)
- **27 One-Click Quick Service & Calibration Cards:**
  - **Grand Vitara 1.9 DDiS (`F9Q`):** EGR adaptation reset, Inlet throttle flap calibration, Injector IMA read & ZFC reset, DPF soot initialization, DPF service regeneration, Oil dilution counter reset, Common rail high-pressure leak check, ECM memory transfer, Starter motor check.
  - **CAN Diesel (`D13A` / `D16AA` / `D20AA`):** DPF regeneration, Oil service reset, EGR learning value reset, Diff-pressure / MAF / Rail component replacement resets.
  - **Petrol Powertrain:** Electronic throttle learning, Idle Air Control (`IAC`) reset, Short/Long term fuel trim reset, Brake stroke learning, GPF adaptation.
  - **Chassis & Body Systems:** ABS/ESP Steering Angle (`SAS`) & G-Sensor calibration, ABS hydraulic air bleeding, TCM/CVT/AMT clutch learning reset, EPS zero-point calibration, Airbag crash history clear, TPMS sensor registration, IPC service reminder reset, Auto Headlight Leveling (`ALC`) sensor init.
  - **New Calibrations:** `EPB-01` Electronic Parking Brake pad replacement mode, `HVAC-01` Climate air mix flap & compressor init, `HVBMU-01` SHVS Mild-Hybrid / Strong Hybrid BMU reset, and `ADAS-01` Camera/Radar/BSM calibration.
- **Injector IMA Coding Box:** Cylinder 1–4 individual 7-character IMA code read (`21 AD` / `21 54`) and write (`3B AD FF`) with automated Zero Fuel Calibration (`3B 7E FE`) reset.
- **17-Digit VIN Read & Write Box:** ECM chassis number read (`1A 90`, `22 F1 90`, `21 90`, `09 02`) and registration/write (`3B 90` / `2E F1 90`) with security access unlock.
- **Database Catalog:** Searchable catalog of **631 Active Tests** and **570 Utility Procedures**.

### 4. ECU Identification & Direct K-Line Access (Tab 4)
- Direct ISO 14230 Fast Init terminal for Suzuki Pin 7 K-Line controllers (`0x29` ABS, `0x21` 4WD, `0x5A` BCM, `0x59` SRS, `0x5B` IMM, `0x99` AC, `0x11` ECM, `0x13` TCM, `0x31` EPS).
- Local ID scanner (`21 01` .. `21 FE`) and offline hex packet decoder.

### 5. Hidden Features, BCM Coding & Immobilizer Key Programming (Tab 5)
- **21 Model-Specific Hidden Features (BCM Coding):**
  - Read existing BCM state via `21 08` / `21 01` (CAN `0x7E4` / K-Line `0x5A`).
  - Auto door locking (speed-dependent @ 15 km/h or shift lever P-linked).
  - Auto door unlock (ignition OFF or shift lever P-linked).
  - Daytime Running Lights (`DRL`) enable/disable.
  - Comfort turn signals (lane-change 2, 3, or 4 flashes).
  - Auto folding mirrors on lock.
  - Cornering fog lamps.
  - Factory security alarm activation.
  - Acoustic buzzer / horn lock answerback.
  - Guide-me-home & welcome lighting timers.
  - ESP Dyno / Off-road full deactivation mode.
- **Immobilizer & Key Programming (`IMMO-01..207`):**
  - **Safe Read-Only Operations:** Key count & date (`21 A1`), Transponder chip ID (`21 B0`), Direct EEPROM PIN read (`10 C0 -> 21 21`), Keyless interior antenna check.
  - **4-Digit PIN Operations:** Presets (`0895`, `9502`, `5634`, `0325`), Safe PIN test, Transponder key registration (`30 A1 80` / `A5 A1 80`), Remote fob programming (`A5 A1 20` / `30 62 07`), ECM-Immobilizer sync, Electronic Steering Lock (`ESL`) unlock, and Double-confirmed erase all keys.
  - **2015+ UDS Seed-Key Routine:** Dynamic seed request (`27 03`) and calculated key unlock (`27 04 -> 31 01 01 0D`).

---

## 🛠️ Hardware Requirements & Tested Interfaces

Compatible with any SAE J2534-1 / J2534-2 compliant PassThru adapter running on 32-bit DLLs:
- **Drew Technologies:** MongoosePro JLR, MongoosePro Toyota, CarDAQ-Plus
- **Scanmatik:** SM2 / SM2 Pro (USB & Bluetooth)
- **Tactrix:** OpenPort 2.0
- **Allscanner:** VCX SE, VCX Nano
- **Chipsoft:** J2534 Pro / Lite
- **Generic:** Any PassThru device registered under Windows `Software\PassThruSupport.04.04`

---

## ⚡ Quick Start

### 1. Running the Pre-Compiled Binary
1. Connect your J2534 interface to your PC's USB port and the vehicle's OBD-II port.
2. Turn the vehicle's **Ignition ON** (engine stopped for programming; idling for live data).
3. Double-click `Suzuki_Diyagnostik_Baslat.bat` (or execute `suzuki_diag_server.exe`).
4. The web dashboard will automatically open in your browser at `http://127.0.0.1:8080`.
5. Select your J2534 interface from the dropdown list and click **Connect**.

### 2. Building from Source
Built with Microsoft Visual C++ (MSVC 2022 / 2019 x86 toolset):
```cmd
build_diag.cmd
suzuki_diag_server.exe --check-identity
```

---

## ⚖️ Disclaimer
This project is an independent diagnostic development for standard SAE J2534 PassThru hardware. It is not affiliated with, sponsored by, or endorsed by Suzuki Motor Corporation. All trademarks and brand names belong to their respective owners. Provided strictly "AS IS" under BETA testing terms.
