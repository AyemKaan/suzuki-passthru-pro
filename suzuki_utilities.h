#pragma once
#include "suzuki_utilities.generated.h"
#include "suzuki_active_tests.generated.h"

inline bool CsvContainsBase(const char* csv, int targetBase) {
    if (targetBase <= 0) return true;
    if (!csv || !csv[0]) return false;
    std::string s(csv);
    size_t start = 0;
    while (start < s.size()) {
        size_t comma = s.find(',', start);
        std::string token = (comma == std::string::npos) ? s.substr(start) : s.substr(start, comma - start);
        if (!token.empty() && std::atoi(token.c_str()) == targetBase) return true;
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return false;
}

inline int FirstSupportedSuzukiBase(const char* csv) {
    if (!csv || !csv[0]) return 41;
    std::string s(csv);
    size_t start = 0;
    int firstAny = 41;
    bool hasFirst = false;
    while (start < s.size()) {
        size_t comma = s.find(',', start);
        std::string token = (comma == std::string::npos) ? s.substr(start) : s.substr(start, comma - start);
        if (!token.empty()) {
            int b = std::atoi(token.c_str());
            if (!hasFirst && b > 0) { firstAny = b; hasFirst = true; }
            // Prefer standard Suzuki CAN/K-Line/UDS bases (33..200)
            if ((b >= 33 && b <= 76) || (b >= 90 && b <= 116) || (b >= 133 && b <= 200)) {
                return b;
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return firstAny;
}

inline std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline std::string BuildDbCatalogJson(int filterBaseId, const std::string& query) {
    std::string q = ToLowerAscii(query);
    constexpr size_t totalUtils = sizeof(kDbUtilities) / sizeof(kDbUtilities[0]);
    constexpr size_t totalActs = sizeof(kDbActiveTests) / sizeof(kDbActiveTests[0]);

    std::ostringstream ss;
    ss << "{\"ok\":true"
       << ",\"filterBaseId\":" << filterBaseId
       << ",\"totalUtilities\":" << totalUtils
       << ",\"totalActiveTests\":" << totalActs
       << ",\"utilities\":[";
    bool firstU = true;
    int matchedU = 0;
    for (const auto& u : kDbUtilities) {
        if (filterBaseId > 0 && !CsvContainsBase(u.basesCsv, filterBaseId)) continue;
        if (!q.empty()) {
            std::string hay = ToLowerAscii(std::string(u.code) + " " + u.titleTr + " " + u.titleEn + " " + u.script + " " + u.primaryHex);
            if (hay.find(q) == std::string::npos) continue;
        }
        matchedU++;
        if (matchedU > 300) continue;
        if (!firstU) ss << ",";
        firstU = false;
        ss << "{\"script\":" << JsonString(u.script)
           << ",\"code\":" << JsonString(u.code)
           << ",\"titleTr\":" << JsonString(u.titleTr)
           << ",\"titleEn\":" << JsonString(u.titleEn)
           << ",\"bases\":" << JsonString(u.basesCsv)
           << ",\"primaryHex\":" << JsonString(u.primaryHex)
           << ",\"allHex\":" << JsonString(u.allHex)
           << "}";
    }
    ss << "],\"matchedUtilities\":" << matchedU << ",\"activeTests\":[";
    bool firstA = true;
    int matchedA = 0;
    for (const auto& a : kDbActiveTests) {
        if (filterBaseId > 0 && !CsvContainsBase(a.basesCsv, filterBaseId)) continue;
        if (!q.empty()) {
            std::string hay = ToLowerAscii(std::string(a.testName) + " " + a.titleTr + " " + a.titleEn + " " + a.startHex);
            if (hay.find(q) == std::string::npos) continue;
        }
        matchedA++;
        if (matchedA > 300) continue;
        if (!firstA) ss << ",";
        firstA = false;
        ss << "{\"testName\":" << JsonString(a.testName)
           << ",\"titleTr\":" << JsonString(a.titleTr)
           << ",\"titleEn\":" << JsonString(a.titleEn)
           << ",\"bases\":" << JsonString(a.basesCsv)
           << ",\"startHex\":" << JsonString(a.startHex)
           << ",\"altHex\":" << JsonString(a.altHex)
           << ",\"stopHex\":" << JsonString(a.stopHex)
           << "}";
    }
    ss << "],\"matchedActiveTests\":" << matchedA << "}";
    return ss.str();
}

inline unsigned char GetFallbackKlineTargetForBase(int baseId) {
    switch (baseId) {
        case 33: return 0x21; // 4WD
        case 34: case 185: return 0x29; // ABS/ESP
        case 35: return 0x99; // HVAC
        case 36: return 0x5B; // OCM
        case 37: case 90: return 0x59; // SRS Airbag
        case 39: case 44: return 0x5A; // BCM
        case 41: case 194: return 0x11; // ECM
        case 93: return 0x7A; // F9Q K-Line ECM
        case 42: return 0x31; // EPS
        case 46: case 48: return 0x5D; // KLS/IMM
        case 49: case 147: return 0x19; // TCM
        case 92: return 0x5C; // TPMS
        default: return 0;
    }
}

inline ProcedureResult ExecuteHexSequenceOnBase(int baseId, const std::string& label, const std::vector<std::vector<unsigned char>>& cmds) {
    ProcedureResult res;
    DbBaseComm comm = GetDbBaseComm(baseId);
    bool isUds = IsUdsBase(baseId);
    std::vector<unsigned char> resp;

    auto runOnKline = [&](unsigned char kTarget) -> bool {
        res.log.push_back(label + " -> K-Line (Pin 7, Hedef " + HexAddr(kTarget, 2) + ", Base " + std::to_string(baseId) + ") oturumu aciliyor...");
        std::vector<std::vector<unsigned char>> kReqs = {{0x10, 0xC0}};
        for (const auto& c : cmds) kReqs.push_back(c);
        std::vector<std::vector<unsigned char>> kResps;
        if (!g_diag.RunKLineSession(kTarget, kReqs, kResps, 450)) {
            res.log.push_back("K-Line (" + HexAddr(kTarget, 2) + ") uzerinden yanit alinamadi.");
            return false;
        }
        bool anyPositive = false;
        for (size_t i = 0; i < kResps.size(); ++i) {
            std::string txStr = HexBytes(kReqs[i]);
            std::string rxStr = kResps[i].empty() ? "YOK" : HexBytes(kResps[i]);
            res.log.push_back("TX [" + txStr + "] -> RX [" + rxStr + "]");
            if (i >= 1 && !kResps[i].empty() && kResps[i][0] == static_cast<unsigned char>(kReqs[i][0] + 0x40)) {
                anyPositive = true;
            }
        }
        res.success = anyPositive;
        res.step = anyPositive ? "DONE_KLINE" : "KLINE_REJECTED";
        res.message = anyPositive
            ? (label + " (K-Line " + HexAddr(kTarget, 2) + ") ECU onayi ile tamamlandi!")
            : (label + " komutu K-Line uzerinden gonderildi ancak pozitif ECU onayi alinamadi.");
        return anyPositive;
    };

    if (comm.isKline) {
        if (!runOnKline(comm.klineTarget) && res.step.empty()) {
            res.step = "KLINE_NO_RESP";
            res.message = label + ": K-Line (" + HexAddr(comm.klineTarget, 2) + ") uzerinden ECU yanit vermedi.";
        }
        return res;
    }

    res.log.push_back(label + " -> CAN (" + HexAddr(comm.canTx, 3) + "/" + HexAddr(comm.canRx, 3) + ", Base " + std::to_string(baseId) + ") oturumu aciliyor...");
    std::vector<unsigned char> sessCmd = isUds ? std::vector<unsigned char>{0x10, 0x03} : std::vector<unsigned char>{0x10, 0xC0};
    bool sessOk = g_diag.SendAndReceive(comm.canTx, comm.canRx, sessCmd, resp, 350) && !resp.empty() && resp[0] == 0x50;
    if (!sessOk && !isUds) {
        sessOk = g_diag.SendAndReceive(comm.canTx, comm.canRx, {0x10, 0x81}, resp, 350) && !resp.empty() && resp[0] == 0x50;
    }
    if (!sessOk) {
        res.log.push_back("Uyari: CAN Oturum acma (" + HexBytes(sessCmd) + ") yaniti: " + (resp.empty() ? "YOK" : HexBytes(resp)));
    } else {
        res.log.push_back("CAN Oturumu acildi: " + HexBytes(resp));
    }

    g_diag.SendAndReceive(comm.canTx, comm.canRx, {0x3E}, resp, 150);

    bool anyPositive = false;
    bool anyCanRx = sessOk;
    for (const auto& c : cmds) {
        if (c.empty()) continue;
        std::vector<unsigned char> rx;
        bool ok = g_diag.SendAndReceive(comm.canTx, comm.canRx, c, rx, 550);
        if (!rx.empty()) anyCanRx = true;
        res.log.push_back("TX [" + HexBytes(c) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
        if (ok && !rx.empty() && rx[0] == static_cast<unsigned char>(c[0] + 0x40)) {
            anyPositive = true;
        }
    }

    if (!anyPositive && !anyCanRx) {
        unsigned char kFallback = GetFallbackKlineTargetForBase(baseId);
        if (kFallback != 0) {
            res.log.push_back("CAN (" + HexAddr(comm.canTx, 3) + ") yanit vermedi; otomatik K-Line (" + HexAddr(kFallback, 2) + ") yedek hatti deneniyor...");
            if (runOnKline(kFallback)) {
                return res;
            }
        }
    }

    res.success = anyPositive;
    res.step = anyPositive ? "DONE_CAN" : "CAN_NO_POSITIVE";
    res.message = anyPositive
        ? (label + " (" + HexAddr(comm.canTx, 3) + ") ECU pozitif yaniti ile basariyla calistirildi!")
        : (label + " komutu gonderildi ancak ECU pozitif yanit donmedi (Log kutusunu kontrol ediniz).");
    return res;
}

inline ProcedureResult ExecuteDbActiveTest(const std::string& testName, const std::string& mode, int overrideBaseId) {
    const DbActiveTestRecord* found = nullptr;
    for (const auto& a : kDbActiveTests) {
        if (testName == a.testName) {
            found = &a;
            break;
        }
    }
    if (!found) {
        ProcedureResult r;
        r.step = "NOT_FOUND";
        r.message = "Aktif test veritabaninda bulunamadi: " + testName;
        return r;
    }
    std::string hexCmd = found->startHex;
    std::string modeLabel = "BASLAT (ON)";
    if (mode == "alt" && found->altHex[0]) {
        hexCmd = found->altHex;
        modeLabel = "DURUM 2 (ALT/OFF)";
    } else if (mode == "stop" && found->stopHex[0]) {
        hexCmd = found->stopHex;
        modeLabel = "DURDUR (EXIT)";
    }
    std::vector<unsigned char> bytes;
    if (!ParseSupportHex(hexCmd, bytes) || bytes.empty()) {
        ProcedureResult r;
        r.step = "EMPTY_CMD";
        r.message = std::string(found->titleTr) + " icin bu modda (" + mode + ") tanimli komut yok.";
        return r;
    }
    int baseId = (overrideBaseId > 0) ? overrideBaseId : FirstSupportedSuzukiBase(found->basesCsv);
    return ExecuteHexSequenceOnBase(baseId, std::string("Aktif Test [") + found->testName + " - " + found->titleTr + " / " + modeLabel + "]", {bytes});
}

inline ProcedureResult ExecuteDbUtility(const std::string& codeOrScript, int overrideBaseId) {
    const DbUtilityRecord* found = nullptr;
    for (const auto& u : kDbUtilities) {
        if (codeOrScript == u.code || codeOrScript == u.script) {
            found = &u;
            break;
        }
    }
    if (!found) {
        ProcedureResult r;
        r.step = "NOT_FOUND";
        r.message = "Servis fonksiyonu veritabaninda bulunamadi: " + codeOrScript;
        return r;
    }
    std::vector<std::vector<unsigned char>> cmds;
    std::string all(found->allHex);
    size_t start = 0;
    while (start < all.size()) {
        size_t semi = all.find(';', start);
        std::string token = (semi == std::string::npos) ? all.substr(start) : all.substr(start, semi - start);
        std::vector<unsigned char> b;
        if (!token.empty() && ParseSupportHex(token, b) && !b.empty()) {
            cmds.push_back(b);
        }
        if (semi == std::string::npos) break;
        start = semi + 1;
    }
    if (cmds.empty()) {
        ProcedureResult r;
        r.step = "INTERACTIVE_ONLY";
        r.message = std::string(found->code) + " (" + found->titleTr + ") parametrik/interaktif giris gerektiren bir aractir; dogrudan tek-tus hex komutu icermiyor.";
        return r;
    }
    int baseId = (overrideBaseId > 0) ? overrideBaseId : FirstSupportedSuzukiBase(found->basesCsv);
    return ExecuteHexSequenceOnBase(baseId, std::string("Suzuki Service Utility [") + found->code + " - " + found->titleTr + "]", cmds);
}

// --- Additional Dedicated OEM Calibrations from Suzuki/Utilities/*.js ---

// DSL-05.js: F9Q ECM Replacement Initialization + DSL-11.js Starter / Assembly Line Test
inline ProcedureResult ExecuteF9QEcmReplaceAndStarterTest(bool starterTestOnly) {
    if (starterTestOnly) {
        return ExecuteHexSequenceOnBase(93, "F9Q Mars / Montaj Hatti Testi (DSL-11)", {
            {0x31, 0x61, 0x00},
            {0x21, 0x61},
            {0x31, 0x61, 0x01}
        });
    }
    return ExecuteHexSequenceOnBase(93, "F9Q ECM Degisimi & Adaptasyon Baslatma (DSL-05)", {
        {0x21, 0xAD},
        {0x21, 0x64},
        {0x21, 0x7E},
        {0x21, 0xAC},
        {0x21, 0x66},
        {0x2E, 0x2F, 0x1D, 0x7F, 0xFF},
        {0x2E, 0x2F, 0x1E, 0x7F, 0xFF},
        {0x2E, 0x2F, 0x1F, 0x7F, 0xFF},
        {0x2E, 0x2F, 0x21, 0x7F, 0xFF},
        {0x3B, 0x7E, 0xFE}
    });
}

// DSL-16..DSL-39: CAN Diesel ECM (Base 41 - D13A / D16AA / D20AA / K9K / F9Q CAN) Reset & Calibrations
inline ProcedureResult ExecuteCanDieselCalibration(const std::string& subType) {
    if (subType == "dpf_regen") {
        return ExecuteHexSequenceOnBase(41, "CAN Dizel DPF Rejenerasyonu & DPF Sifirlama (DSL-16 / DSL-18)", {
            {0x30, 0x74, 0x08, 0x40, 0x40, 0x00, 0x00},
            {0x30, 0x72, 0x08, 0x80, 0x80, 0x00, 0x00}
        });
    }
    if (subType == "oil_reset") {
        return ExecuteHexSequenceOnBase(41, "CAN Dizel Yag Degisim Isigi & Yakit Filtresi Sifirlama (DSL-22 / DSL-32)", {
            {0x30, 0x74, 0x08, 0x02, 0x02, 0x00, 0x00},
            {0x30, 0x79, 0x08, 0x40, 0x40, 0x00, 0x00}
        });
    }
    if (subType == "egr_learn") {
        return ExecuteHexSequenceOnBase(41, "CAN Dizel EGR-1 / EGR-2 & Kelebek Ogrenme Sifirlama (DSL-25 / DSL-35 / DSL-39)", {
            {0x30, 0x70, 0x08, 0x08, 0x08, 0x00, 0x00},
            {0x30, 0x70, 0x08, 0x40, 0x40, 0x00, 0x00},
            {0x30, 0x71, 0x08, 0x80, 0x80, 0x00, 0x00},
            {0x30, 0x70, 0x08, 0x20, 0x20, 0x00, 0x00},
            {0x30, 0x7B, 0x08, 0x80, 0x80, 0x00, 0x00}
        });
    }
    // Default: All CAN Diesel Sensors (Diff Pressure DSL-17, Flywheel DSL-19, A/F DSL-20, MAF DSL-21, PRV DSL-23, Rail Pressure DSL-24)
    return ExecuteHexSequenceOnBase(41, "CAN Dizel Sensor Adaptasyonlari (Fark Basinc, MAF, A/F, Volan, Rail - DSL-17..24)", {
        {0x30, 0x74, 0x08, 0x80, 0x80, 0x00, 0x00}, // DSL-17 Diff Pressure
        {0x30, 0x74, 0x08, 0x10, 0x10, 0x00, 0x00}, // DSL-19 Flywheel
        {0x30, 0x74, 0x08, 0x08, 0x08, 0x00, 0x00}, // DSL-20 A/F Sensor
        {0x30, 0x74, 0x08, 0x04, 0x04, 0x00, 0x00}, // DSL-21 MAF Sensor
        {0x30, 0x74, 0x08, 0x01, 0x01, 0x00, 0x00}, // DSL-23 PRV
        {0x30, 0x78, 0x08, 0x80, 0x80, 0x00, 0x00}  // DSL-24 Rail Pressure Sensor
    });
}

// Petrol & General ECM Learned Value / Throttle / Idle Air / Brake Stroke / GPF Reset (ECM-03, ECM-101, ECM-105, ECM-122, ECM-264)
inline ProcedureResult ExecutePetrolEcmCalibration() {
    return ExecuteHexSequenceOnBase(41, "Benzinli / Genel ECM Bogaz Kelebegi, Rolanti, A/F, GPF ve Fren Stroku Ogrenme Sifirlama (ECM-03/101/105/122/264)", {
        {0x30, 0x04, 0x00},                         // ECM-122 Closed Throttle Pos Learn
        {0x31, 0x04, 0x00},                         // ECM-122 Routine
        {0x31, 0x03, 0x00},                         // ECM-101 Idle Air Vol Learn
        {0x31, 0x06, 0x00},                         // ECM-105 Self-Learning Control Clear
        {0x30, 0xD3, 0x08, 0x80, 0x80, 0x00, 0x00}, // ECM-03 Brake Stroke Sensor Zero Point
        {0x30, 0x15, 0x08, 0x80, 0x80, 0x00, 0x00}, // ECM-264 GPF Info Init
        {0x31, 0x01, 0xC0, 0x43, 0x00},             // ECM-03 UDS
        {0x31, 0x01, 0xC0, 0x08, 0x00}              // ECM-264 UDS
    });
}

// Transmission (TCM: AT-06, AMT-01..03, CVT-01..03) Learning & Calibration
inline ProcedureResult ExecuteTcmCalibration() {
    return ExecuteHexSequenceOnBase(49, "Sanziman (AT / AMT /AGS / CVT) Kavrama, Vites ve Egim Sensoru Kalibrasyonu (AT-06 / AMT-01 / CVT-01..03)", {
        {0x30, 0x60, 0x08, 0x80, 0x80, 0x00, 0x00}, // AT-06 Neutral Position Calibration
        {0x30, 0x77, 0x08, 0x80, 0x80, 0x00, 0x00}, // AMT-01 All Learned Values Init
        {0x30, 0x76, 0x08, 0x80, 0x80, 0x00, 0x00}, // AMT-01 Clutch Init
        {0x30, 0x7A, 0x08, 0x80, 0x80, 0x00, 0x00}, // AMT-01 Gear Tower Init
        {0x30, 0x00, 0x08, 0x80, 0x80, 0x00, 0x00}, // CVT-01/02 Learned Value Reset
        {0x30, 0x20, 0x08, 0x80, 0x80, 0x00, 0x00}, // CVT-01
        {0x30, 0x51, 0x08, 0x80, 0x80, 0x00, 0x00}, // CVT-03 Inclination / G-Sensor Calibration
        {0x31, 0x01, 0xC1, 0x08, 0x00}              // CVT-02 UDS
    });
}

// EPS-01: Electric Power Steering Sensor Calibration
inline ProcedureResult ExecuteEpsCalibration() {
    return ExecuteHexSequenceOnBase(42, "EPS Elektrikli Direksiyon Tork / Aci Sensoru Kalibrasyonu (EPS-01)", {
        {0x30, 0x11, 0x08, 0x80, 0x80, 0x00, 0x00}
    });
}

// ABS-01 & ABS-04: ABS Hydraulic Bleed / Solenoid Test & ESP Function Setting
inline ProcedureResult ExecuteAbsBleedAndEspFunc() {
    return ExecuteHexSequenceOnBase(34, "ABS Hidrolik Kontrol / Hava Alma Testi & ESP Fonksiyon Ayari (ABS-01 / ABS-04)", {
        {0x30, 0x00, 0x07, 0x01, 0x01, 0x00, 0x00}, // ABS-01 Hydraulic Control Test
        {0x30, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00}, // ABS-01 Stop
        {0x30, 0x80, 0x07, 0x04, 0x00, 0x00, 0x00}, // ABS-04 ESP Function
        {0x31, 0x01, 0xC0, 0x20, 0x00}              // ABS-04 UDS
    });
}

// BCM-01 & TPMS-01: BCM Airbag Crash Record Clear & TPMS Sensor ID Registration
inline ProcedureResult ExecuteBcmAndTpmsService(bool tpmsMode) {
    if (tpmsMode) {
        ProcedureResult r = ExecuteHexSequenceOnBase(92, "TPMS Lastik Basinc Sensoru ID Kayit Modu (TPMS-01)", {
            {0x30, 0x08, 0x07, 0x80, 0x00, 0x00, 0x00},
            {0x30, 0x25, 0x07, 0x80, 0x00, 0x00, 0x00}
        });
        if (!r.success) {
            return ExecuteHexSequenceOnBase(39, "TPMS / BCM Lastik Sensoru Kayit Modu (TPMS-01 BCM)", {
                {0x30, 0x08, 0x07, 0x80, 0x00, 0x00, 0x00},
                {0x30, 0x25, 0x07, 0x80, 0x00, 0x00, 0x00}
            });
        }
        return r;
    }
    ProcedureResult r = ExecuteHexSequenceOnBase(39, "BCM Hava Yastigi (Airbag) Carpma Kaydi Silme (BCM-01)", {
        {0x30, 0x07, 0x08, 0x08, 0x00, 0x00, 0x00}
    });
    if (!r.success) {
        return ExecuteHexSequenceOnBase(63, "BCM Hava Yastigi Carpma Kaydi Silme (BCM-01 K-Line 0x5A)", {
            {0xA5, 0x07, 0x08, 0x00, 0x00, 0x00},
            {0x30, 0x07, 0x08, 0x08, 0x00, 0x00, 0x00}
        });
    }
    return r;
}

// IPC-03 & IPC-04: Instrument Cluster Oil Reminder & Navigation Reset
inline ProcedureResult ExecuteIpcServiceReset() {
    return ExecuteHexSequenceOnBase(47, "Gosterge Paneli (IPC) Yag Bakim Isigi & Navigasyon Sifirlama (IPC-03 / IPC-04)", {
        {0x30, 0x35, 0x08, 0x80, 0x00, 0x00, 0x00},
        {0x30, 0x31, 0x08, 0x08, 0x00, 0x00, 0x00},
        {0x31, 0x01, 0xC0, 0x52, 0x00}
    });
}

// ALC-01: Auto Headlamp Leveling Height Sensor Initialization
inline ProcedureResult ExecuteAlcSensorInit() {
    return ExecuteHexSequenceOnBase(38, "Otomatik Far Seviye (ALC) Yukseklik Sensoru Ilk Ayari (ALC-01)", {
        {0x30, 0x00, 0x08, 0x80, 0x80, 0x00, 0x00}
    });
}

// --- Hidden Features & BCM Personalization / Coding Engine (Model-by-Model) ---

inline std::string ReadBcmCodingStateJson() {
    std::vector<std::string> logLines;
    std::vector<unsigned char> resp;
    std::string source = "none";
    std::string rawHex = "";
    std::map<std::string, int> states;

    auto decode2108 = [&](const std::vector<unsigned char>& r) {
        // r[0] == 0x61, r[1] == 0x08. In JS scripts res.response[0] is 0x08 (0x61 stripped),
        // so JS res.response[pos] corresponds to r[pos + 1] when 0x61 is present!
        size_t shiftOff = (!r.empty() && r[0] == 0x61) ? 1 : 0;
        auto byteAt = [&](size_t jsPos) -> int {
            size_t idx = jsPos + shiftOff;
            return (idx < r.size()) ? static_cast<int>(r[idx]) : -1;
        };
        if (byteAt(29) >= 0 && r.size() < 50) {
            // BCM-06 (Base 63 K-Line 21 08 layout)
            states["HF-ALARM"] = ((byteAt(29) & 0x04) >> 2);
            states["HF-ANSWERLIGHT"] = (((byteAt(29) & 0x80) >> 7) == 0) ? 1 : 0;
        }
        if (byteAt(60) >= 0) {
            // BCM-03 (Base 39 / 48 CAN 21 08 layout)
            states["HF-DRL"] = (byteAt(58) & 0x80) ? 1 : 0;
            states["HF-AUTOLOCK"] = (byteAt(30) & 0x03);
            states["HF-AUTOUNLOCK"] = ((byteAt(30) & 0x0C) >> 2);
            int turnRaw = ((byteAt(59) & 0x30) >> 4);
            states["HF-LANEFLASH"] = (turnRaw >= 0 && turnRaw <= 3) ? (3 - turnRaw) : 0;
            states["HF-MIRROR"] = (byteAt(53) & 0x40) ? 1 : 0;
            states["HF-CORNER"] = (byteAt(56) & 0x40) ? 1 : 0;
            if (byteAt(59) & 0x01) states["HF-BUZZER"] = 1;
            else if (byteAt(56) & 0x10) states["HF-BUZZER"] = 2;
            else if (byteAt(49) & 0x80) states["HF-BUZZER"] = 3;
            else states["HF-BUZZER"] = 0;
            states["HF-ALARM"] = (byteAt(58) & 0x20) ? 1 : 0;
            states["HF-UNLOCKMODE"] = (byteAt(60) & 0x04) ? 1 : 0;
            states["HF-FOLLOWHOME"] = ((byteAt(28) & 0x70) >> 4);
            states["HF-GUIDECAR"] = (byteAt(28) & 0x07);
            states["HF-FOOTLIGHT"] = ((byteAt(54) & 0xC0) >> 6);
            states["HF-HBA"] = (byteAt(53) & 0x10) ? 1 : 0;
            states["HF-ANSWERLIGHT"] = (byteAt(59) & 0x02) ? 1 : 0;
        }
    };

    auto decode2101 = [&](const std::vector<unsigned char>& r) {
        size_t shiftOff = (!r.empty() && r[0] == 0x61) ? 1 : 0;
        auto byteAt = [&](size_t jsPos) -> int {
            size_t idx = jsPos + shiftOff;
            return (idx < r.size()) ? static_cast<int>(r[idx]) : -1;
        };
        if (byteAt(6) >= 0) {
            states["HF-AUTOUNLOCK"] = (byteAt(1) & 0x80) ? 1 : 0;
            states["HF-AUTOLOCK"] = ((byteAt(5) & 0x30) >> 4);
            states["HF-ALARM"] = ((byteAt(4) & 0x80) == 0) ? 1 : 0;
            states["HF-BUZZER"] = (byteAt(4) & 0x07) ? 1 : 0;
            states["HF-RETAINEDPWR"] = ((byteAt(4) & 0x30) >> 4);
        }
        if (byteAt(14) >= 0) {
            states["HF-ROOMLAMP"] = ((byteAt(14) & 0x38) >> 3);
        }
    };

    // 1. Try CAN BCM (0x7E4 / 0x7EC - Base 39)
    logLines.push_back("Reading BCM Configuration via CAN (0x7E4 -> 21 08)...");
    g_diag.SendAndReceive(0x7E4, 0x7EC, {0x10, 0xC0}, resp, 300);
    if (g_diag.SendAndReceive(0x7E4, 0x7EC, {0x21, 0x08}, resp, 500) && resp.size() >= 10 && resp[0] == 0x61) {
        source = "CAN_BCM_7E4_2108";
        rawHex = HexBytes(resp);
        logLines.push_back("CAN BCM (0x7E4) 21 08 Read OK: " + rawHex);
        decode2108(resp);
    } else if (g_diag.SendAndReceive(0x7E4, 0x7EC, {0x21, 0x01}, resp, 450) && resp.size() >= 6 && resp[0] == 0x61) {
        source = "CAN_BCM_7E4_2101";
        rawHex = HexBytes(resp);
        logLines.push_back("CAN BCM (0x7E4) 21 01 Read OK: " + rawHex);
        decode2101(resp);
    } else {
        // 2. Try K-Line BCM (0x5A on Pin 7 - Grand Vitara / Swift / SX4 / Jimny)
        logLines.push_back("CAN 0x7E4 no response; probing K-Line BCM (Pin 7, Target 0x5A -> 21 08 / 21 01)...");
        std::vector<std::vector<unsigned char>> kReqs = {{0x10, 0xC0}, {0x21, 0x08}, {0x21, 0x01}};
        std::vector<std::vector<unsigned char>> kResps;
        if (g_diag.RunKLineSession(0x5A, kReqs, kResps, 450)) {
            if (kResps.size() >= 2 && kResps[1].size() >= 8 && kResps[1][0] == 0x61) {
                source = "KLINE_BCM_5A_2108";
                rawHex = HexBytes(kResps[1]);
                logLines.push_back("K-Line BCM (0x5A) 21 08 Read OK: " + rawHex);
                decode2108(kResps[1]);
            }
            if (kResps.size() >= 3 && kResps[2].size() >= 6 && kResps[2][0] == 0x61) {
                if (source == "none") source = "KLINE_BCM_5A_2101";
                rawHex = HexBytes(kResps[2]);
                logLines.push_back("K-Line BCM (0x5A) 21 01 Read OK: " + rawHex);
                decode2101(kResps[2]);
            }
        } else {
            logLines.push_back("K-Line BCM (0x5A) no response.");
        }
    }

    std::ostringstream ss;
    ss << "{\"ok\":" << (source != "none" ? "true" : "false")
       << ",\"source\":" << JsonString(source)
       << ",\"rawHex\":" << JsonString(rawHex)
       << ",\"states\":{";
    bool first = true;
    for (const auto& kv : states) {
        if (!first) ss << ",";
        first = false;
        ss << JsonString(kv.first) << ":" << kv.second;
    }
    ss << "},\"log\":[";
    for (size_t i = 0; i < logLines.size(); ++i) {
        if (i > 0) ss << ",";
        ss << JsonString(logLines[i]);
    }
    ss << "]}";
    return ss.str();
}

inline ProcedureResult ExecuteHiddenFeatureCoding(const std::string& featureId, int optVal) {
    // Runs on primary base (with automatic CAN -> K-Line 0x5A fallback in ExecuteHexSequenceOnBase)
    // and for multi-generation features sends both CAN (30 xx 08) and Classic/K-Line (3B xx) frames if needed
    if (featureId == "HF-DRL") {
        unsigned char b = (optVal == 1) ? 0x80 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Daytime Running Lights (DRL) -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x36, 0x08, 0x80, b, 0x00, 0x00}
        });
    }
    if (featureId == "HF-AUTOLOCK") {
        unsigned char modeCan = (optVal == 2) ? 0x02 : (optVal == 1 ? 0x01 : 0x00);
        unsigned char modeKl  = (optVal == 2) ? 0x02 : (optVal == 1 ? 0x01 : 0x00);
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Speed/Shift Auto Door Lock (BCM-03 / BCM-108)", {
            {0x30, 0x35, 0x08, 0x01, 0x00, modeCan, 0x00},
            {0x3B, 0x2F, modeKl}
        });
    }
    if (featureId == "HF-AUTOUNLOCK") {
        unsigned char modeCan = (optVal == 2) ? 0x02 : (optVal == 1 ? 0x01 : 0x00);
        unsigned char modeKl  = (optVal >= 1) ? 0x01 : 0x00;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Auto Door Unlock on IG-OFF / Shift-P (BCM-03 / BCM-104)", {
            {0x30, 0x35, 0x08, 0x02, 0x00, modeCan, 0x00},
            {0x3B, 0x02, modeKl}
        });
    }
    if (featureId == "HF-LANEFLASH") {
        // optVal: 3 = +3 flashes (CS: 0x03), 2 = +2 (CS: 0x02), 1 = +1 (CS: 0x01), 0 = OFF (CS: 0x00)
        unsigned char cs = (optVal >= 0 && optVal <= 3) ? static_cast<unsigned char>(optVal) : 0x03;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Comfort Lane-Change Turn Signal Flashes (BCM-03)", {
            {0x30, 0x35, 0x08, 0x20, 0x00, cs, 0x00}
        });
    }
    if (featureId == "HF-MIRROR") {
        unsigned char b = (optVal == 1) ? 0x20 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Auto Folding Side Mirrors -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x33, 0x08, 0x20, b, 0x00, 0x00}
        });
    }
    if (featureId == "HF-CORNER") {
        unsigned char b = (optVal == 1) ? 0x04 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Cornering Fog Lights -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x31, 0x08, 0x04, b, 0x00, 0x00}
        });
    }
    if (featureId == "HF-BUZZER") {
        if (optVal == 1) {
            return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock/Unlock Outside Buzzer ON (BCM-03 / BCM-107)", {
                {0x30, 0x33, 0x08, 0x80, 0x80, 0x00, 0x00},
                {0x3B, 0x2B, 0x01}
            });
        }
        if (optVal == 2) {
            return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock/Unlock Horn Chime ON (BCM-03)", {
                {0x30, 0x31, 0x08, 0x08, 0x08, 0x00, 0x00}
            });
        }
        if (optVal == 3) {
            return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock/Unlock Siren Answerback ON (BCM-03)", {
                {0x30, 0x33, 0x08, 0x10, 0x10, 0x00, 0x00}
            });
        }
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock/Unlock Acoustic Answerback OFF / Silent (BCM-03 / BCM-107)", {
            {0x30, 0x33, 0x08, 0x80, 0x00, 0x00, 0x00},
            {0x30, 0x31, 0x08, 0x08, 0x00, 0x00, 0x00},
            {0x30, 0x33, 0x08, 0x10, 0x00, 0x00, 0x00},
            {0x3B, 0x2B, 0x00}
        });
    }
    if (featureId == "HF-ALARM") {
        unsigned char bCan = (optVal == 1) ? 0x20 : 0x00;
        unsigned char bKl  = (optVal == 1) ? 0x00 : 0x01; // BCM-110: 0x00 = WITH, 0x01 = WITHOUT
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Factory Security Anti-Theft Alarm -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x34, 0x08, 0x20, bCan, 0x00, 0x00}, // BCM-03 CAN
            {0x30, 0x09, 0x08, 0x20, bCan, 0x00, 0x00}, // BCM-06 K-Line 0x5A (Grand Vitara / SX4)
            {0x3B, 0x21, bKl}                           // BCM-110 Classic
        });
    }
    if (featureId == "HF-UNLOCKMODE") {
        unsigned char cs = (optVal == 1) ? 0x01 : 0x00; // 0 = 1-Action (All Doors), 1 = 2-Action (Driver First)
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Remote Door Unlock Action (1-Press All vs 2-Press)", {
            {0x30, 0x3B, 0x08, 0x10, 0x00, cs, 0x00}
        });
    }
    if (featureId == "HF-FOLLOWHOME") {
        unsigned char cs = (optVal >= 0 && optVal <= 5) ? static_cast<unsigned char>(optVal) : 0x02;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Guide Me Home / Follow Me Home Headlight Timer", {
            {0x30, 0x3B, 0x08, 0x02, 0x00, cs, 0x00},
            {0x3B, 0x39, cs}
        });
    }
    if (featureId == "HF-GUIDECAR") {
        unsigned char cs = (optVal >= 0 && optVal <= 4) ? static_cast<unsigned char>(optVal) : 0x02;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Guide Me To Car / Welcome Lights Timer", {
            {0x30, 0x3B, 0x08, 0x04, 0x00, cs, 0x00}
        });
    }
    if (featureId == "HF-FOOTLIGHT") {
        unsigned char cs = (optVal >= 0 && optVal <= 2) ? static_cast<unsigned char>(optVal) : 0x00;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Footwell Ambient Lighting Mode", {
            {0x30, 0x3B, 0x08, 0x80, 0x00, cs, 0x00}
        });
    }
    if (featureId == "HF-HBA") {
        unsigned char b = (optVal == 1) ? 0x40 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: High Beam Assist (HBA) -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x36, 0x08, 0x40, b, 0x00, 0x00},
            {0x30, 0x01, 0x08, 0x40, b, 0x00, 0x00}
        });
    }
    if (featureId == "HF-ANSWERLIGHT") {
        // optVal 1 = Hazard Warning Lights (CS: 0x00), 0 = Interior Dome Light (CS: 0x01)
        unsigned char cs = (optVal == 1) ? 0x00 : 0x01;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock Answerback Flash Type (Hazard vs Interior Light)", {
            {0x30, 0x35, 0x08, 0x80, 0x00, cs, 0x00}, // BCM-03 CAN
            {0x30, 0x10, 0x08, 0x80, 0x00, cs, 0x00}  // BCM-06 K-Line 0x5A (Grand Vitara / SX4)
        });
    }
    if (featureId == "HF-AUTOLIGHT-HW") {
        if (optVal == 0) return ExecuteHexSequenceOnBase(39, "BCM Hardware Coding: Auto Light Sensor -> WITH (BCM-103)", {{0x3B, 0x3A, 0x04}});
        if (optVal == 1) return ExecuteHexSequenceOnBase(39, "BCM Hardware Coding: Auto Light Sensor -> WITHOUT (BCM-103)", {{0x3B, 0x3A, 0x06}});
        if (optVal == 2) return ExecuteHexSequenceOnBase(39, "BCM Hardware Coding: Keyless Entry -> WITH (BCM-103)", {{0x3B, 0x33, 0x00}});
        return ExecuteHexSequenceOnBase(39, "BCM Hardware Coding: I-Key Smart Key -> WITH (BCM-103)", {{0x3B, 0x34, 0x01}});
    }
    if (featureId == "HF-ROOMLAMP") {
        unsigned char v = static_cast<unsigned char>(optVal);
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Interior Dome Lamp Fade / Delay Timer (BCM-117 / BCM-118)", {
            {0x3B, 0x16, v},
            {0x3B, 0x17, v}
        });
    }
    if (featureId == "HF-RETAINEDPWR") {
        unsigned char v = (optVal >= 0 && optVal <= 2) ? static_cast<unsigned char>(optVal) : 0x00;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Retained Power Window Time After IG-OFF (BCM-113)", {
            {0x3B, 0x27, v}
        });
    }
    if (featureId == "HF-UDS-IKEY") {
        if (optVal == 0) return ExecuteHexSequenceOnBase(169, "UDS BCM: I-Key Lock/Unlock ON (BCM-168)", {{0x2E, 0x14, 0x01, 0x00}});
        if (optVal == 1) return ExecuteHexSequenceOnBase(169, "UDS BCM: Door Handle Touch Sensor Unlock ON (BCM-190)", {{0x2E, 0x14, 0x07, 0x01}});
        if (optVal == 2) return ExecuteHexSequenceOnBase(169, "UDS BCM: Answerback I-Key Buzzer ON (BCM-174)", {{0x2E, 0x14, 0x12, 0x00}});
        if (optVal == 3) return ExecuteHexSequenceOnBase(169, "UDS BCM: Answerback I-Key Buzzer OFF (BCM-174)", {{0x2E, 0x14, 0x12, 0x01}});
        if (optVal == 4) return ExecuteHexSequenceOnBase(169, "UDS BCM: Answerback Keyless Buzzer ON (BCM-175)", {{0x2E, 0x14, 0x14, 0x00}});
        return ExecuteHexSequenceOnBase(169, "UDS BCM: Answerback Keyless Buzzer OFF (BCM-175)", {{0x2E, 0x14, 0x14, 0x01}});
    }
    if (featureId == "HF-SEATBELT") {
        unsigned char b = (optVal == 1) ? 0x60 : 0x00;
        return ExecuteHexSequenceOnBase(47, std::string("Hidden Feature: Seatbelt Warning Chime -> ") + (optVal ? "ON (Factory)" : "OFF (Muted)"), {
            {0x2E, 0x20, 0x41, 0x00, b}
        });
    }
    if (featureId == "HF-CLUSTER-THEME") {
        if (optVal == 1) {
            return ExecuteHexSequenceOnBase(142, "Hidden Feature: Cluster / Display Icon Color Theme (UDS ADM-04)", {
                {0x31, 0x01, 0xC0, 0x11, 0x00}
            });
        }
        return ExecuteHexSequenceOnBase(142, "Hidden Feature: Cluster / Display Icon Color Theme (CAN ADM-04)", {
            {0x30, 0x05, 0x07, 0x40, 0x40, 0x00, 0x00}
        });
    }
    if (featureId == "HF-ESP-DYNO") {
        // ABS-04: 0x04 = DEACTIVATION (Off-Road / Dyno Mode), 0x00 = ACTIVATION (Normal)
        unsigned char st = (optVal == 0) ? 0x04 : 0x00;
        return ExecuteHexSequenceOnBase(34, std::string("Hidden Feature: ESP Function Mode (ABS-04) -> ") + (optVal == 0 ? "DEACTIVATED (Off-Road/Dyno)" : "ACTIVATED (Normal)"), {
            {0x30, 0x80, 0x07, 0x04, st, 0x00, 0x00}
        });
    }

    ProcedureResult r;
    r.step = "UNKNOWN_FEATURE";
    r.message = "Unknown hidden feature ID: " + featureId;
    return r;
}

inline ProcedureResult ExecuteImmobilizerAction(const std::string& action, const std::string& pinStr, const std::string& seedKeyStr) {
    ProcedureResult res;
    res.step = "IMMO_" + action;

    auto parsePinBytes = [&](unsigned char& hi, unsigned char& lo, unsigned char& bcdHi, unsigned char& bcdLo) -> bool {
        std::string clean;
        for (char c : pinStr) {
            if (std::isxdigit(static_cast<unsigned char>(c))) clean.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        if (clean.empty()) clean = "0895"; // Suzuki classic default fallback if empty
        while (clean.size() < 4) clean = "0" + clean;
        if (clean.size() > 4) clean = clean.substr(clean.size() - 4);

        // BCD representation (used by IMMO-101: parseInt(str.substring(0,2), 16), parseInt(str.substring(2,4), 16))
        bcdHi = static_cast<unsigned char>(std::strtoul(clean.substr(0, 2).c_str(), nullptr, 16));
        bcdLo = static_cast<unsigned char>(std::strtoul(clean.substr(2, 2).c_str(), nullptr, 16));

        // Integer representation (used by Immobilizer.js passwordBase 10 / 16)
        bool allDec = true;
        for (char c : clean) if (!std::isdigit(static_cast<unsigned char>(c))) allDec = false;
        unsigned long val = std::strtoul(clean.c_str(), nullptr, allDec ? 10 : 16);
        hi = static_cast<unsigned char>((val >> 8) & 0xFF);
        lo = static_cast<unsigned char>(val & 0xFF);
        return true;
    };

    // 1. Read Registered Key Count, Date & Immobilizer Status (No PIN required)
    if (action == "read_status") {
        auto r1 = ExecuteHexSequenceOnBase(41, "Immobilizer Status & Registered Key Count Read (21 A1 / 21 A0 / 21 01)", {
            {0x21, 0xA1},
            {0x21, 0xA0},
            {0x21, 0x01}
        });
        auto r2 = ExecuteHexSequenceOnBase(39, "BCM / Immobilizer Status Read (CAN 0x7E4 / K-Line 0x5A & 0x5B)", {
            {0x21, 0xA1},
            {0x21, 0x01}
        });
        for (const auto& l : r2.log) r1.log.push_back(l);
        if (r2.success) r1.success = true;

        // Also probe K-Line 0x5B (Dedicated Immobilizer / KLS module on Pin 7 via Base 36)
        auto k5b = ExecuteHexSequenceOnBase(36, "K-Line Immobilizer (0x5B) Status Read (21 A1 / 21 01)", {
            {0x21, 0xA1},
            {0x21, 0x01}
        });
        for (const auto& l : k5b.log) r1.log.push_back(l);
        if (k5b.success) r1.success = true;

        r1.message = r1.success
            ? "Immobilizer status & registered key count read completed. Check log for 61 A1 [KeyCount, Year, Month] and 61 01 details."
            : "No positive response from Immobilizer / ECM on 21 A1 / 21 01. Ensure Ignition is ON.";
        return r1;
    }

    // 2. Read Transponder Key ID Code (21 B0 - IMMO-05 / IMMO-06 / IMMO-201, No PIN required)
    if (action == "read_transponder") {
        auto r1 = ExecuteHexSequenceOnBase(41, "Read Transponder Key ID Code on ECM (21 B0 - IMMO-05 / IMMO-06)", {
            {0x21, 0xB0}
        });
        auto r2 = ExecuteHexSequenceOnBase(39, "Read Transponder Key ID Code on BCM/IMM (21 B0 - IMMO-201)", {
            {0x21, 0xB0}
        });
        for (const auto& l : r2.log) r1.log.push_back(l);
        if (r2.success) r1.success = true;

        auto k5b = ExecuteHexSequenceOnBase(36, "K-Line Immobilizer (0x5B) Transponder ID Read (21 B0)", {
            {0x21, 0xB0}
        });
        for (const auto& l : k5b.log) r1.log.push_back(l);
        if (k5b.success) r1.success = true;

        r1.message = r1.success
            ? "Transponder Key ID (21 B0 -> 61 B0 [10-byte ID]) read successfully."
            : "Transponder ID (21 B0) not returned by ECU. Ensure a valid transponder key is in the ignition.";
        return r1;
    }

    // 3. Direct Immobilizer PIN Read on supported ECUs (10 C0 -> 21 21 - IMMO-106)
    if (action == "read_pin") {
        auto r1 = ExecuteHexSequenceOnBase(39, "Immobilizer Direct PIN Read (10 C0 -> 21 21 - IMMO-106)", {
            {0x21, 0x21}
        });
        auto k5b = ExecuteHexSequenceOnBase(36, "K-Line Immobilizer (0x5B) PIN Read (10 C0 -> 21 21)", {
            {0x21, 0x21},
            {0x10, 0x81}
        });
        for (const auto& l : k5b.log) r1.log.push_back(l);
        if (k5b.success) r1.success = true;

        r1.message = r1.success
            ? "PIN Read (21 21 -> 61 21) completed. Check response bytes in log."
            : "This ECU variant does not support direct unauthenticated 21 21 PIN reading.";
        return r1;
    }

    // 4. Keyless Interior Antenna Self-Test (BCM-167: 2F 01 06 03 00 05)
    if (action == "antenna_test") {
        return ExecuteHexSequenceOnBase(169, "Keyless Interior Antenna Diagnosis (BCM-167: 2F 01 06)", {
            {0x2F, 0x01, 0x06, 0x03, 0x00, 0x05},
            {0x2F, 0x01, 0x06, 0x03, 0x01},
            {0x2F, 0x01, 0x06, 0x00}
        });
    }

    unsigned char pinHi = 0, pinLo = 0, bcdHi = 0, bcdLo = 0;
    parsePinBytes(pinHi, pinLo, bcdHi, bcdLo);

    // 5. Verify 4-Digit PIN Only (Safe Test: 30 A0 07 80 80 <PIN> / A5 A0 80 00 <PIN>)
    if (action == "verify_pin") {
        auto r1 = ExecuteHexSequenceOnBase(41, "Verify Immobilizer 4-Digit PIN (CAN 30 A0 07 80 80 / K-Line A5 A0 80 00)", {
            {0x30, 0xA0, 0x07, 0x80, 0x80, pinHi, pinLo},
            {0x30, 0xA0, 0x07, 0x80, 0x80, bcdHi, bcdLo},
            {0xA5, 0xA0, 0x80, 0x00, pinHi, pinLo},
            {0xA5, 0xA0, 0x80, 0x00, bcdHi, bcdLo}
        });
        r1.message = r1.success
            ? "PIN ACCEPTED by Immobilizer / ECM! You can now proceed with key registration."
            : "PIN rejected or unsupported on this ECU (check 7F negative response in log).";
        return r1;
    }

    // 6. Register Transponder Key (Immobilizer.js + IMMO-202 + IMMO-203)
    if (action == "register_key") {
        auto r1 = ExecuteHexSequenceOnBase(41, "Immobilizer Key Registration (PIN Unlock -> Set Date -> Register Key -> Verify 21 A1)", {
            {0x30, 0xA0, 0x07, 0x80, 0x80, pinHi, pinLo},
            {0x30, 0xA0, 0x07, 0x80, 0x80, bcdHi, bcdLo},
            {0xA5, 0xA0, 0x80, 0x00, pinHi, pinLo},
            {0xA5, 0xA0, 0x80, 0x00, bcdHi, bcdLo},
            {0x30, 0xA1, 0x08, 0x40, 0x40, 0x1A, 0x0B},
            {0xA5, 0xA1, 0x40, 0x00, 0x1A, 0x0B},
            {0x30, 0xA1, 0x08, 0x80, 0x80, 0x00, 0x00},
            {0xA5, 0xA1, 0x80, 0x00, 0x00, 0x00},
            {0x31, 0xA1, 0x00},
            {0x3B, 0xD2, 0x00, 0x01},
            {0x21, 0xA1}
        });
        return r1;
    }

    // 7. Register Remote Controller Fob (K-Line A5 A1 20 00 00 00 / CAN IMMO-204 30 62 07)
    if (action == "register_remote") {
        auto r1 = ExecuteHexSequenceOnBase(39, "Remote Controller (Keyless Fob) Registration (A5 A1 20 / 30 62 07 / 30 04 08)", {
            {0xA5, 0xA0, 0x80, 0x00, pinHi, pinLo},
            {0xA5, 0xA1, 0x20, 0x00, 0x00, 0x00},
            {0x30, 0x62, 0x07},
            {0x30, 0x04, 0x08},
            {0x30, 0x05, 0x08, 0x00}
        });
        auto r2 = ExecuteHexSequenceOnBase(41, "ECM / K-Line Remote Controller Registration (A5 A1 20 00 00 00)", {
            {0xA5, 0xA0, 0x80, 0x00, pinHi, pinLo},
            {0xA5, 0xA1, 0x20, 0x00, 0x00, 0x00}
        });
        for (const auto& l : r2.log) r1.log.push_back(l);
        if (r2.success) r1.success = true;
        return r1;
    }

    // 8. ECM <-> Immobilizer Control Unit Initialization / Pairing (IMMO-101)
    if (action == "ecu_immo_init") {
        return ExecuteHexSequenceOnBase(41, "ECM / Immobilizer Control Unit Pairing & Initialization (IMMO-101: 31 01)", {
            {0x31, 0x01, 0x00, bcdHi, bcdLo},
            {0x31, 0x01, 0x01},
            {0x31, 0x01, bcdHi, bcdLo},
            {0x32, 0x01},
            {0x33, 0x01}
        });
    }

    // 9. Electronic Steering Column Lock (ESL) Unlock (IMMO-109: 31 0D 00 <PIN>)
    if (action == "steering_unlock") {
        return ExecuteHexSequenceOnBase(39, "Electronic Steering Lock (ESL) Unlock Request (IMMO-109: 31 0D 00)", {
            {0x31, 0x0D, 0x00, bcdHi, bcdLo},
            {0x31, 0x0D, 0x00, pinHi, pinLo},
            {0x31, 0x0D, 0x01}
        });
    }

    // 10. UDS Immobilizer Security Seed Request (2015+ Models: IMMO-110..113 -> 10 03, 27 03)
    if (action == "uds_request_seed") {
        auto r1 = ExecuteHexSequenceOnBase(169, "UDS Immobilizer Security Access Seed Request (10 03 -> 27 03 - IMMO-110)", {
            {0x27, 0x03},
            {0x27, 0x01}
        });
        r1.message = r1.success
            ? "UDS Security Seed (67 03 / 67 01) received! Copy the seed bytes from the log, compute the Key, and enter it in the Seed-Key field."
            : "No 67 03 / 67 01 Seed response from BCM.";
        return r1;
    }

    // 11. UDS Immobilizer Send Key & Start Registration (IMMO-110..113 -> 27 04 <KEY> -> 31 01 01 0D)
    if (action == "uds_unlock_register") {
        std::vector<unsigned char> keyBytes;
        if (!ParseSupportHex(seedKeyStr, keyBytes) || keyBytes.empty()) {
            res.message = "Please enter the calculated UDS Security Key hex bytes (e.g. 11 22 33 44) in the UDS Key field.";
            return res;
        }
        std::vector<unsigned char> sendKeyCmd = {0x27, 0x04};
        sendKeyCmd.insert(sendKeyCmd.end(), keyBytes.begin(), keyBytes.end());
        return ExecuteHexSequenceOnBase(169, "UDS Immobilizer Security Unlock (27 04) & Key Registration (31 01 01 0D - IMMO-110)", {
            sendKeyCmd,
            {0x31, 0x01, 0x01, 0x0D},
            {0x31, 0x03, 0x01, 0x0D}
        });
    }

    // 12. Delete All Registered Key Codes (30 A1 08 08 08 00 00 / A5 A1 08 00 00 00)
    if (action == "delete_all_keys") {
        return ExecuteHexSequenceOnBase(41, "Erase All Registered Immobilizer Keys (PIN Unlock -> 30 A1 08 08 / A5 A1 08)", {
            {0x30, 0xA0, 0x07, 0x80, 0x80, pinHi, pinLo},
            {0x30, 0xA0, 0x07, 0x80, 0x80, bcdHi, bcdLo},
            {0xA5, 0xA0, 0x80, 0x00, pinHi, pinLo},
            {0xA5, 0xA0, 0x80, 0x00, bcdHi, bcdLo},
            {0x30, 0xA1, 0x08, 0x08, 0x08, 0x00, 0x00},
            {0xA5, 0xA1, 0x08, 0x00, 0x00, 0x00},
            {0x21, 0xA1}
        });
    }

    res.message = "Unknown Immobilizer action: " + action;
    return res;
}

inline std::string ExecuteInjectorImaJson(const std::string& action, const std::string& c1, const std::string& c2, const std::string& c3, const std::string& c4) {
    std::vector<std::string> log;
    bool ok = false;
    std::string cyl1 = c1, cyl2 = c2, cyl3 = c3, cyl4 = c4;
    std::string message;

    auto cleanCode = [](std::string s) {
        std::string r;
        for (char ch : s) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                r.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            }
        }
        return r;
    };

    if (action == "read") {
        std::vector<unsigned char> resp;
        log.push_back("Reading Injector IMA Calibration Codes on CAN ECM (0x7E0 -> 10 C0, 21 AD / 21 54)...");
        g_diag.SendAndReceive(0x7E0, 0x7E8, {0x10, 0xC0}, resp, 350);
        resp.clear();
        if (g_diag.SendAndReceive(0x7E0, 0x7E8, {0x21, 0xAD}, resp, 600) && resp.size() >= 2 && resp[0] == 0x61 && resp[1] == 0xAD) {
            ok = true;
            log.push_back("RX [61 AD]: " + HexBytes(resp));
            // DSL-01 / DSL-05 F9Q format: 61 AD [FF] + 28 ASCII bytes (4 cylinders x 7 chars)
            size_t startIdx = (resp.size() >= 31 && resp[2] == 0xFF) ? 3 : 2;
            auto extractAscii = [&](size_t off, size_t len) {
                std::string s;
                for (size_t i = 0; i < len && (off + i) < resp.size(); ++i) {
                    unsigned char b = resp[off + i];
                    if (b >= 0x20 && b <= 0x7E) s.push_back(static_cast<char>(b));
                }
                return s;
            };
            if (resp.size() >= startIdx + 28) {
                cyl1 = extractAscii(startIdx, 7);
                cyl2 = extractAscii(startIdx + 7, 7);
                cyl3 = extractAscii(startIdx + 14, 7);
                cyl4 = extractAscii(startIdx + 21, 7);
            }
            message = "Injector IMA codes (21 AD) read successfully from ECM.";
        } else {
            resp.clear();
            if (g_diag.SendAndReceive(0x7E0, 0x7E8, {0x21, 0x54}, resp, 600) && resp.size() >= 18 && resp[0] == 0x61 && resp[1] == 0x54) {
                ok = true;
                log.push_back("RX [61 54]: " + HexBytes(resp));
                message = "CAN Diesel Injector IMA data (21 54) read successfully.";
            } else {
                // Fallback to K-Line F9Q (0x7A) or Classic ECM (0x11)
                log.push_back("CAN 0x7E0 no response; probing K-Line ECM (0x7A / 0x11 -> 21 AD)...");
                std::vector<std::vector<unsigned char>> kReqs = {{0x10, 0xC0}, {0x21, 0xAD}};
                std::vector<std::vector<unsigned char>> kResps;
                if (g_diag.RunKLineSession(0x7A, kReqs, kResps, 500) && kResps.size() >= 2 && kResps[1].size() >= 30 && kResps[1][0] == 0x61) {
                    ok = true;
                    const auto& kr = kResps[1];
                    log.push_back("K-Line 0x7A RX: " + HexBytes(kr));
                    size_t startIdx = (kr.size() >= 31 && kr[2] == 0xFF) ? 3 : 2;
                    auto extractK = [&](size_t off) {
                        std::string s;
                        for (size_t i = 0; i < 7 && (off + i) < kr.size(); ++i) {
                            if (kr[off + i] >= 0x20 && kr[off + i] <= 0x7E) s.push_back(static_cast<char>(kr[off + i]));
                        }
                        return s;
                    };
                    cyl1 = extractK(startIdx);
                    cyl2 = extractK(startIdx + 7);
                    cyl3 = extractK(startIdx + 14);
                    cyl4 = extractK(startIdx + 21);
                    message = "Injector IMA codes (K-Line 0x7A -> 21 AD) read successfully.";
                } else {
                    message = "Could not read Injector IMA codes (21 AD / 21 54). Ensure Ignition is ON.";
                }
            }
        }
    } else if (action == "write") {
        cyl1 = cleanCode(cyl1);
        cyl2 = cleanCode(cyl2);
        cyl3 = cleanCode(cyl3);
        cyl4 = cleanCode(cyl4);
        if (cyl1.size() < 6 || cyl2.size() < 6 || cyl3.size() < 6 || cyl4.size() < 6) {
            message = "Each cylinder Injector IMA code must be 6 to 8 alphanumeric characters (7 chars standard on Grand Vitara 1.9 DDiS F9Q).";
        } else {
            auto pad7 = [](std::string s) {
                while (s.size() < 7) s.push_back(' ');
                if (s.size() > 7) s = s.substr(0, 7);
                return s;
            };
            std::string all28 = pad7(cyl1) + pad7(cyl2) + pad7(cyl3) + pad7(cyl4);
            // DSL-01 / DSL-05: [0x3B, 0xAD, 0xFF] + 28 ASCII bytes, followed by ZFC reset [0x3B, 0x7E, 0xFE]
            std::vector<unsigned char> writeCmd = {0x3B, 0xAD, 0xFF};
            for (char ch : all28) writeCmd.push_back(static_cast<unsigned char>(ch));

            auto pr = ExecuteHexSequenceOnBase(93, "Write Injector IMA Codes (3B AD FF) & Reset ZFC (3B 7E FE)", {
                writeCmd,
                {0x3B, 0x7E, 0xFE},
                {0x21, 0xAD}
            });
            for (const auto& l : pr.log) log.push_back(l);
            ok = pr.success;
            message = pr.message;
        }
    }

    std::ostringstream ss;
    ss << "{\"ok\":" << (ok ? "true" : "false")
       << ",\"success\":" << (ok ? "true" : "false")
       << ",\"cyl1\":" << JsonString(cyl1)
       << ",\"cyl2\":" << JsonString(cyl2)
       << ",\"cyl3\":" << JsonString(cyl3)
       << ",\"cyl4\":" << JsonString(cyl4)
       << ",\"message\":" << JsonString(message)
       << ",\"log\":[";
    for (size_t i = 0; i < log.size(); ++i) {
        if (i > 0) ss << ",";
        ss << JsonString(log[i]);
    }
    ss << "]}";
    return ss.str();
}

inline std::string ExecuteVinActionJson(const std::string& action, const std::string& vinInput) {
    std::vector<std::string> log;
    bool ok = false;
    std::string vin;
    std::string message;

    auto extractAsciiVin = [](const std::vector<unsigned char>& r, size_t startOffset) -> std::string {
        std::string s;
        for (size_t i = startOffset; i < r.size() && s.size() < 17; ++i) {
            unsigned char b = r[i];
            if ((b >= '0' && b <= '9') || (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z')) {
                s.push_back(static_cast<char>(std::toupper(b)));
            }
        }
        return s;
    };

    if (action == "read") {
        std::vector<unsigned char> resp;
        log.push_back("Reading VIN from ECM (CAN 0x7E0 -> 1A 90 / 22 F1 90 / 21 90 / 09 02)...");
        if (g_diag.SendAndReceive(0x7E0, 0x7E8, {0x1A, 0x90}, resp, 500) && resp.size() >= 19 && resp[0] == 0x5A && resp[1] == 0x90) {
            vin = extractAsciiVin(resp, 2);
            ok = !vin.empty();
            log.push_back("RX [5A 90]: " + HexBytes(resp) + " -> VIN: " + vin);
        }
        if (!ok) {
            resp.clear();
            if (g_diag.SendAndReceive(0x7E0, 0x7E8, {0x22, 0xF1, 0x90}, resp, 500) && resp.size() >= 20 && resp[0] == 0x62 && resp[1] == 0xF1 && resp[2] == 0x90) {
                vin = extractAsciiVin(resp, 3);
                ok = !vin.empty();
                log.push_back("RX [62 F1 90]: " + HexBytes(resp) + " -> VIN: " + vin);
            }
        }
        if (!ok) {
            resp.clear();
            if (g_diag.SendAndReceive(0x7E0, 0x7E8, {0x21, 0x90}, resp, 500) && resp.size() >= 19 && resp[0] == 0x61 && resp[1] == 0x90) {
                vin = extractAsciiVin(resp, 2);
                ok = !vin.empty();
                log.push_back("RX [61 90]: " + HexBytes(resp) + " -> VIN: " + vin);
            }
        }
        if (!ok) {
            resp.clear();
            if (g_diag.SendAndReceive(0x7E0, 0x7E8, {0x09, 0x02}, resp, 500) && resp.size() >= 19 && resp[0] == 0x49 && resp[1] == 0x02) {
                vin = extractAsciiVin(resp, 3);
                ok = !vin.empty();
                log.push_back("RX [49 02]: " + HexBytes(resp) + " -> VIN: " + vin);
            }
        }
        if (!ok) {
            log.push_back("CAN 0x7E0 no VIN response; probing K-Line ECM (0x11 / 0x7A -> 1A 90 / 21 90)...");
            std::vector<std::vector<unsigned char>> kReqs = {{0x1A, 0x90}, {0x21, 0x90}};
            std::vector<std::vector<unsigned char>> kResps;
            if (g_diag.RunKLineSession(0x11, kReqs, kResps, 450)) {
                for (const auto& kr : kResps) {
                    if (kr.size() >= 19 && (kr[0] == 0x5A || kr[0] == 0x61)) {
                        vin = extractAsciiVin(kr, 2);
                        if (!vin.empty()) { ok = true; log.push_back("K-Line RX: " + HexBytes(kr) + " -> VIN: " + vin); break; }
                    }
                }
            }
        }
        message = ok ? ("VIN Read Successful: " + vin) : "Could not read VIN (1A 90 / 22 F1 90 / 09 02). Ensure Ignition is ON.";
    } else if (action == "write") {
        std::string clean;
        for (char ch : vinInput) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                clean.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            }
        }
        if (clean.size() != 17) {
            message = "VIN must be exactly 17 alphanumeric characters (entered: " + std::to_string(clean.size()) + ").";
        } else {
            vin = clean;
            std::vector<unsigned char> kwpWrite = {0x3B, 0x90};
            std::vector<unsigned char> udsWrite = {0x2E, 0xF1, 0x90};
            for (char ch : vin) {
                kwpWrite.push_back(static_cast<unsigned char>(ch));
                udsWrite.push_back(static_cast<unsigned char>(ch));
            }
            // PWTR-02.js & DSL-15 sequence: Unlock (30 12 08 80 80 / 30 76 08 80 80 / A5 0F 40) -> Write VIN (3B 90 / 2E F1 90) -> Verify (1A 90)
            auto pr = ExecuteHexSequenceOnBase(41, "ECM VIN Registration (PWTR-02 / DSL-15: 3B 90 & UDS 2E F1 90)", {
                {0x30, 0x12, 0x08, 0x80, 0x80, 0x00, 0x00},
                {0x30, 0x76, 0x08, 0x80, 0x80, 0x00, 0x00},
                {0xA5, 0x0F, 0x40, 0x00, 0x00, 0x00},
                kwpWrite,
                udsWrite,
                {0x1A, 0x90}
            });
            for (const auto& l : pr.log) log.push_back(l);
            ok = pr.success;
            message = pr.message;
        }
    }

    std::ostringstream ss;
    ss << "{\"ok\":" << (ok ? "true" : "false")
       << ",\"success\":" << (ok ? "true" : "false")
       << ",\"vin\":" << JsonString(vin)
       << ",\"message\":" << JsonString(message)
       << ",\"log\":[";
    for (size_t i = 0; i < log.size(); ++i) {
        if (i > 0) ss << ",";
        ss << JsonString(log[i]);
    }
    ss << "]}";
    return ss.str();
}

inline ProcedureResult RunEpbServiceProcedure() {
    // EPB-01: Electronic Parking Brake ECU Reset & Actuator Service Mode
    return ExecuteHexSequenceOnBase(34, "EPB Electronic Parking Brake Service Reset & Pad Replacement Mode (EPB-01)", {
        {0xA6, 0x00},
        {0x30, 0x01, 0x07, 0x80, 0x80, 0x00, 0x00},
        {0x31, 0x01, 0x02, 0x00}
    });
}

inline ProcedureResult RunHvacCalibProcedure() {
    // HVAC-01..16 / AC-01..09 (Base 35: CAN 0x7A2 / K-Line 0x99)
    return ExecuteHexSequenceOnBase(35, "HVAC Climate Air Mix Flap Actuator & Compressor Initialization (HVAC-01)", {
        {0x30, 0x01, 0x08, 0x80, 0x80, 0x00, 0x00},
        {0x31, 0x01, 0x00}
    });
}

inline ProcedureResult RunShvsHybridProcedure() {
    // HVBMU-01..03 / HV-01..05 (SHVS Mild-Hybrid / Strong Hybrid Battery & ISG Reset)
    return ExecuteHexSequenceOnBase(41, "SHVS Mild-Hybrid / HVBMU Cell IC & ISG Starter Counter Reset (HVBMU-01)", {
        {0x30, 0x01, 0x07, 0x80, 0x80, 0x00, 0x00},
        {0x31, 0x01, 0xC0, 0x10, 0x00}
    });
}

inline ProcedureResult RunAdasCalibProcedure() {
    // ADAS / Forward Camera / Radar / Blind Spot (BSM) / 360 AVM Initialization
    return ExecuteHexSequenceOnBase(169, "ADAS Camera / Radar / BSM / 360 AVM Calibration Initialization", {
        {0x31, 0x01, 0xC0, 0x01, 0x00},
        {0x30, 0x01, 0x08, 0x80, 0x80, 0x00, 0x00}
    });
}
