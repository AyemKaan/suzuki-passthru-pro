#pragma once
#include "suzuki_utilities.generated.h"
#include "suzuki_active_tests.generated.h"

extern std::string g_cachedVin;
extern std::string g_activeProfileId;
extern IdentityMatch g_identityMatch;

enum class SequenceMode {
    RequireAll,
    TryFirstPositive
};

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

inline bool IsKnownVehicleProfile(const std::string& p) {
    return p == "gv_jb419d" || p == "gv_petrol" || p == "suzuki_kline" || p == "suzuki_can";
}

inline bool IsNissanPlatformBase(int baseId) {
    return (baseId >= 8 && baseId <= 32) ||
           (baseId >= 118 && baseId <= 123) ||
           baseId == 131 || baseId == 132 || baseId == 134 || baseId == 135 ||
           (baseId >= 156 && baseId <= 179);
}

inline bool IsToyotaTpBase(int baseId) {
    return baseId >= 201 && baseId <= 257;
}

inline int GetMatchedEcuBaseId() {
    if (const DbIdentityRecord* rec = g_identityMatch.activeCandidate()) {
        if (rec->baseId > 0) return rec->baseId;
    }
    if (!g_identityMatch.generic && g_identityMatch.baseId > 0) {
        return g_identityMatch.baseId;
    }
    return 0;
}

inline bool IsNissanLandyMatched() {
    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase <= 0) return false;
    if (IsNissanPlatformBase(matchedBase)) return true;
    if (const DbIdentityRecord* rec = g_identityMatch.activeCandidate()) {
        std::string m = ToLowerAscii(std::string(rec->model ? rec->model : "") + " " + (rec->name ? rec->name : ""));
        if (m.find("landy") != std::string::npos || m.find("y2k") != std::string::npos ||
            m.find("yn7") != std::string::npos || m.find("yk7") != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline bool IsBaseCompatibleWithEffectiveProfile(int baseId, const std::string& effProf) {
    if (baseId <= 0) return false;
    DbBaseComm comm = GetDbBaseComm(baseId);
    if (!comm.isKline && comm.canTx == 0) return false; // Unmapped / 29-bit unsupported base
    if (IsToyotaTpBase(baseId)) return false; // Toyota TP4/TP5 not handled by generic Suzuki CAN/KWP profiles
    if (IsNissanPlatformBase(baseId) && !IsNissanLandyMatched()) return false;
    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase > 0) {
        if (IsF9QBase(baseId) && !IsF9QBase(matchedBase)) return false;
        if (IsNissanPlatformBase(matchedBase) && !IsNissanPlatformBase(baseId) && baseId != 41 && baseId != 49) return false;
    }
    if (effProf == "suzuki_kline") return comm.isKline || baseId == 36 || baseId == 64 || baseId == 98 || baseId == 106;
    if (effProf == "gv_jb419d") return baseId != 169 && !IsNissanPlatformBase(baseId);
    if (effProf == "gv_petrol") return baseId != 169 && !IsNissanPlatformBase(baseId);
    if (effProf == "suzuki_can") return !comm.isKline && (!IsNissanPlatformBase(baseId) || IsNissanLandyMatched());
    return false;
}

inline std::string ResolveEffectiveProfileFor(const std::string& requestedProf) {
    if (requestedProf != "auto" && !requestedProf.empty()) {
        return IsKnownVehicleProfile(requestedProf) ? requestedProf : "UNKNOWN_PROFILE";
    }
    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase == 93 || matchedBase == 101) return "gv_jb419d";
    if (matchedBase == 64 || matchedBase == 98 || matchedBase == 106) return "suzuki_kline";
    if (matchedBase > 0) {
        if (const DbIdentityRecord* rec = g_identityMatch.activeCandidate()) {
            std::string m = ToLowerAscii(rec->model);
            if (m.find("grand vitara") != std::string::npos || m.find("jb4") != std::string::npos || m.find("jb6") != std::string::npos) {
                return "gv_petrol";
            }
        }
        return "suzuki_can";
    }
    return "UNIDENTIFIED";
}

inline std::string ResolveEffectiveProfile() {
    return ResolveEffectiveProfileFor(g_activeProfileId);
}

inline int ResolveDbTargetBase(const char* basesCsv, int overrideBaseId, std::string& outErr) {
    outErr.clear();
    if (overrideBaseId > 0) {
        if (!CsvContainsBase(basesCsv, overrideBaseId)) {
            outErr = "Secilen Base ID (" + std::to_string(overrideBaseId) + ") bu komut/testin destekledigi Base listesinde (" + (basesCsv ? basesCsv : "") + ") yok.";
            return 0;
        }
        DbBaseComm comm = GetDbBaseComm(overrideBaseId);
        if (!comm.isKline && comm.canTx == 0) {
            outErr = "Secilen Base ID (" + std::to_string(overrideBaseId) + ") icin CAN/K-Line haberlesme adresi tanimli degil.";
            return 0;
        }
        return overrideBaseId;
    }

    std::string effProf = ResolveEffectiveProfile();
    if (!IsKnownVehicleProfile(effProf)) {
        outErr = "Arac profili / ECU kimligi tanimlanmadan veritabanindan otomatik Base secimi yapilmaz. Once 'Arac Tara' ile kimlik dogrulayin veya manuel arac profili / Base ID secin.";
        return 0;
    }

    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase > 0 && CsvContainsBase(basesCsv, matchedBase) && IsBaseCompatibleWithEffectiveProfile(matchedBase, effProf)) {
        return matchedBase;
    }

    std::vector<int> preferredBases;
    if (effProf == "gv_jb419d") {
        preferredBases = {93, 101, 34, 39, 63, 70, 46, 33, 35, 37, 42, 47, 92, 38};
    } else if (effProf == "gv_petrol") {
        preferredBases = {41, 52, 64, 73, 106, 34, 39, 63, 70, 46, 49, 33, 35, 37, 42, 47, 92, 38};
    } else if (effProf == "suzuki_kline") {
        preferredBases = {64, 98, 106, 36, 63, 34, 35, 37, 42, 49, 92};
    } else { // suzuki_can: Suzuki native CAN/UDS bases only (excludes Nissan Base 169 unless Nissan/Landy matched)
        preferredBases = {194, 41, 52, 73, 182, 189, 185, 39, 44, 46, 139, 146, 192, 47, 197, 49, 147, 142, 186, 143, 191, 184, 187, 188, 190, 195, 198, 199, 200, 34, 35, 38, 42, 92};
    }
    for (int b : preferredBases) {
        if (CsvContainsBase(basesCsv, b) && IsBaseCompatibleWithEffectiveProfile(b, effProf)) {
            return b;
        }
    }

    outErr = "Bu veritabani komutu secili arac profili/platformu (" + effProf + ") ile uyumlu Base ID icermiyor (desteklenen: " + (basesCsv ? basesCsv : "") + ").";
    return 0;
}

inline unsigned char GetFallbackKlineTargetForBase(int baseId) {
    switch (baseId) {
        case 33: return 0x21; // 4WD
        case 34: case 185: return 0x29; // ABS/ESP
        case 35: return 0x99; // HVAC
        case 36: return 0x5B; // OCM / Standalone IMM (0x5B)
        case 37: case 90: return 0x59; // SRS Airbag
        case 39: case 44: return 0x5A; // BCM
        case 41: case 194: return 0x11; // ECM
        case 64: return 0x11; // KWP ECM (0x11)
        case 93: return 0x7A; // F9Q K-Line ECM
        case 42: return 0x31; // EPS
        case 46: case 48: case 70: case 111: return 0x5D; // KLS/IMM (0x5D)
        case 49: case 147: return 0x19; // TCM
        case 92: return 0x5C; // TPMS
        default: return 0;
    }
}

inline bool IsValidImmoKeyStatusResp(const std::vector<unsigned char>& r) {
    if (r.size() >= 3 && r[0] == 0x61 && r[1] == 0xA1) return true;
    if (r.size() >= 4 && r[0] == 0x62 && r[1] == 0x01 && r[2] == 0xA1) return true;
    return false;
}

inline int ExtractImmoKeyCount(const std::vector<unsigned char>& r) {
    if (!IsValidImmoKeyStatusResp(r)) return -1;
    return (r[0] == 0x62 && r.size() >= 4) ? static_cast<int>(r[3]) : static_cast<int>(r[2]);
}

inline bool VerifyImmoKeyCountTransition(bool isErase, int preCount, int postCount, std::string& outReason) {
    outReason.clear();
    if (preCount < 0 || postCount < 0) {
        outReason = "Islem oncesi/sonrasi anahtar sayisi okunamadi (21 A1 dogrulanamadi).";
        return false;
    }
    if (isErase) {
        if (postCount != 0) {
            outReason = "Tum anahtarlari silme komutu onaylandi ancak anahtar sayisi sifirlanmadi (Onceki: " +
                        std::to_string(preCount) + ", Sonraki: " + std::to_string(postCount) + ").";
            return false;
        }
        outReason = "Onceki anahtar sayisi (" + std::to_string(preCount) + ") basariyla 0'a sifirlandi.";
        return true;
    }
    if (postCount <= preCount) {
        outReason = "Anahtar kaydetme komutu yanit verdi ancak kayitli anahtar sayisi artmadi (Onceki: " +
                    std::to_string(preCount) + ", Sonraki: " + std::to_string(postCount) + ").";
        return false;
    }
    outReason = "Kayitli anahtar sayisi " + std::to_string(preCount) + " -> " + std::to_string(postCount) + " olarak artarak dogrulandi.";
    return true;
}

inline bool IsValidUdsSeedResp(const std::vector<unsigned char>& r) {
    // 27 01/03 -> 67 01/03 + >= 4 seed bytes (cannot be all 0x00 or all 0xFF)
    if (r.size() < 6 || r[0] != 0x67 || (r[1] != 0x01 && r[1] != 0x03)) return false;
    bool anyNonZero = false;
    bool allFf = true;
    for (size_t i = 2; i < r.size(); ++i) {
        if (r[i] != 0x00) anyNonZero = true;
        if (r[i] != 0xFF) allFf = false;
    }
    return anyNonZero && !allFf;
}

inline bool ParseStrictImmoPin4(const std::string& pinStr, unsigned char& hi, unsigned char& lo, unsigned char& bcdHi, unsigned char& bcdLo, std::string& outErr) {
    outErr.clear();
    size_t start = 0;
    while (start < pinStr.size() && std::isspace(static_cast<unsigned char>(pinStr[start]))) start++;
    size_t end = pinStr.size();
    while (end > start && std::isspace(static_cast<unsigned char>(pinStr[end - 1]))) end--;
    std::string trimmed = pinStr.substr(start, end - start);
    if (trimmed.empty()) {
        outErr = "Bos PIN kabul edilmez. Lutfen tam 4 haneli immobilizer PIN kodunu girin (otomatik varsayilan PIN kullanilmaz).";
        return false;
    }
    if (trimmed.size() != 4) {
        outErr = "Gecersiz PIN uzunlugu (" + std::to_string(trimmed.size()) + " karakter). Tam olarak 4 haneli PIN girilmelidir; fazla haneler kesilmez veya eksik haneler tamamlanmaz.";
        return false;
    }
    std::string clean;
    clean.reserve(4);
    bool allDec = true;
    for (char c : trimmed) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isxdigit(uc)) {
            outErr = "PIN yalnizca 4 adet 0-9 / A-F hex karakteri icermelidir.";
            return false;
        }
        if (!std::isdigit(uc)) allDec = false;
        clean.push_back(static_cast<char>(std::toupper(uc)));
    }
    bcdHi = static_cast<unsigned char>(std::strtoul(clean.substr(0, 2).c_str(), nullptr, 16));
    bcdLo = static_cast<unsigned char>(std::strtoul(clean.substr(2, 2).c_str(), nullptr, 16));
    unsigned long val = std::strtoul(clean.c_str(), nullptr, allDec ? 10 : 16);
    hi = static_cast<unsigned char>((val >> 8) & 0xFF);
    lo = static_cast<unsigned char>(val & 0xFF);
    return true;
}

inline bool IsValidTransponderIdResp(const std::vector<unsigned char>& r) {
    size_t idOffset = 0;
    if (r.size() >= 6 && r[0] == 0x61 && r[1] == 0xB0) {
        idOffset = 2;
    } else if (r.size() >= 7 && r[0] == 0x62 && r[1] == 0x01 && r[2] == 0xB0) {
        idOffset = 3;
    } else {
        return false;
    }
    bool anyNonZero = false;
    bool allFf = true;
    for (size_t i = idOffset; i < r.size(); ++i) {
        if (r[i] != 0x00) anyNonZero = true;
        if (r[i] != 0xFF) allFf = false;
    }
    return anyNonZero && !allFf;
}

inline ProcedureResult ExecuteHexSequenceOnBase(
    int baseId,
    const std::string& label,
    const std::vector<std::vector<unsigned char>>& cmds,
    SequenceMode mode = SequenceMode::RequireAll,
    bool (*customValidator)(const std::vector<unsigned char>&) = nullptr,
    std::vector<unsigned char>* outMatchedResp = nullptr)
{
    ProcedureResult res;
    if (outMatchedResp) outMatchedResp->clear();
    if (cmds.empty()) {
        res.step = "EMPTY_CMDS";
        res.message = label + ": Gonderilecek komut bulunamadi.";
        return res;
    }
    DbBaseComm comm = GetDbBaseComm(baseId);
    if (!comm.isKline && comm.canTx == 0) {
        res.step = "UNKNOWN_BASE_ADDRESS";
        res.message = label + ": Base " + std::to_string(baseId) + " icin CAN/K-Line adresi tanimli degil; islem guvenlik nedeniyle durduruldu.";
        return res;
    }
    bool isUds = IsUdsBase(baseId);
    std::vector<unsigned char> resp;

    auto runOnKline = [&](unsigned char kTarget) -> bool {
        res.log.push_back(label + " -> K-Line (Pin 7, Hedef " + HexAddr(kTarget, 2) + ", Base " + std::to_string(baseId) + ") oturumu aciliyor...");
        KLineChannelScope kScope(g_diag, kTarget);
        if (!kScope.open) {
            res.log.push_back("K-Line (" + HexAddr(kTarget, 2) + ") Fast Init yanit vermedi.");
            res.step = "KLINE_NO_RESP";
            res.message = label + ": K-Line (" + HexAddr(kTarget, 2) + ") uzerinden ECU yanit vermedi.";
            return false;
        }
        bool sessPos = false;
        std::vector<unsigned char> sessResp;
        kScope.Transact({0x10, 0xC0}, sessResp, 450, &sessPos);
        res.log.push_back("TX [10 C0] -> RX [" + (sessResp.empty() ? "YOK" : HexBytes(sessResp)) + "]");
        if (!sessPos || sessResp.empty() || sessResp[0] != 0x50) {
            res.success = false;
            res.step = sessResp.empty() ? "KLINE_NO_RESP" : "KLINE_SESSION_REJECTED";
            res.message = sessResp.empty()
                ? (label + ": K-Line (" + HexAddr(kTarget, 2) + ") uzerinden ECU yanit vermedi.")
                : (label + " (K-Line " + HexAddr(kTarget, 2) + "): Diagnostik oturum (10 C0) reddedildi; diger komutlar durduruldu.");
            return false;
        }
        size_t nonEmptyCmds = 0;
        size_t positiveCount = 0;
        for (const auto& c : cmds) {
            if (c.empty()) continue;
            nonEmptyCmds++;
            bool cmdPos = false;
            std::vector<unsigned char> rx;
            kScope.Transact(c, rx, 450, &cmdPos);
            res.log.push_back("TX [" + HexBytes(c) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
            bool stepPos = (cmdPos && !rx.empty() && rx[0] == static_cast<unsigned char>(c[0] + 0x40) &&
                            (!customValidator || customValidator(rx)));
            if (stepPos) {
                positiveCount++;
                if (outMatchedResp) *outMatchedResp = rx;
                if (mode == SequenceMode::TryFirstPositive) break;
            } else if (mode == SequenceMode::RequireAll) {
                break;
            }
        }
        bool seqOk = (mode == SequenceMode::TryFirstPositive) ? (positiveCount > 0) : (positiveCount == nonEmptyCmds && nonEmptyCmds > 0);
        res.success = seqOk;
        res.step = seqOk ? "DONE_KLINE" : "KLINE_REJECTED";
        res.message = seqOk
            ? (label + " (K-Line " + HexAddr(kTarget, 2) + ") ECU onayi ile tamamlandi!")
            : (label + " komutu K-Line uzerinden reddedildi veya beklenen veri alinamadi (Adimlar: " + std::to_string(positiveCount) + "/" + std::to_string(nonEmptyCmds) + ").");
        return seqOk;
    };

    if (comm.isKline) {
        runOnKline(comm.klineTarget);
        return res;
    }

    bool anyCanRx = false;
    {
        res.log.push_back(label + " -> CAN (" + HexAddr(comm.canTx, 3) + "/" + HexAddr(comm.canRx, 3) + ", Base " + std::to_string(baseId) + ") oturumu aciliyor...");
        CanChannelScope canScope(g_diag, comm.canTx, comm.canRx);
        if (canScope.open) {
            std::vector<unsigned char> sessCmd = isUds ? std::vector<unsigned char>{0x10, 0x03} : std::vector<unsigned char>{0x10, 0xC0};
            bool sessRx = canScope.Transact(sessCmd, resp, 350);
            if (!resp.empty()) anyCanRx = true;
            bool sessOk = sessRx && !resp.empty() && resp[0] == 0x50;
            if (!sessOk && !isUds) {
                std::vector<unsigned char> respAlt;
                bool altRx = canScope.Transact({0x10, 0x81}, respAlt, 350);
                if (!respAlt.empty()) { anyCanRx = true; resp = respAlt; }
                sessOk = altRx && !respAlt.empty() && respAlt[0] == 0x50;
            }
            if (sessOk) {
                res.log.push_back("CAN Oturumu acildi: " + HexBytes(resp));
                size_t totalValidCmds = 0;
                size_t positiveCount = 0;
                for (const auto& c : cmds) {
                    if (c.empty()) continue;
                    totalValidCmds++;
                    std::vector<unsigned char> rx;
                    bool ok = canScope.Transact(c, rx, 550);
                    if (!rx.empty()) anyCanRx = true;
                    res.log.push_back("TX [" + HexBytes(c) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
                    bool stepPos = (ok && !rx.empty() && rx[0] == static_cast<unsigned char>(c[0] + 0x40) &&
                                    (!customValidator || customValidator(rx)));
                    if (stepPos) {
                        positiveCount++;
                        if (outMatchedResp) *outMatchedResp = rx;
                        if (mode == SequenceMode::TryFirstPositive) break;
                    } else if (mode == SequenceMode::RequireAll) {
                        break; // Abort remaining commands immediately on first failure
                    }
                }
                canScope.Transact(isUds ? std::vector<unsigned char>{0x10, 0x01} : std::vector<unsigned char>{0x10, 0x81}, resp, 200);
                bool seqOk = (mode == SequenceMode::TryFirstPositive) ? (positiveCount > 0) : (positiveCount == totalValidCmds && totalValidCmds > 0);
                res.success = seqOk;
                res.step = seqOk ? "DONE_CAN" : "CAN_STEP_FAILED";
                res.message = seqOk
                    ? (label + " (" + HexAddr(comm.canTx, 3) + ") ECU pozitif yaniti ile basariyla tamamlandi!")
                    : (label + " adimi ECU tarafindan onaylanmadi (Onaylanan: " + std::to_string(positiveCount) + "/" + std::to_string(totalValidCmds) + ").");
                return res;
            }
            if (anyCanRx) {
                res.log.push_back("HATA: CAN Oturum acma (" + HexBytes(sessCmd) + ") reddedildi: " + HexBytes(resp));
                res.success = false;
                res.step = "CAN_SESSION_REJECTED";
                res.message = label + ": CAN diagnostik oturumu reddedildi (" + HexBytes(resp) + "); diger komutlar durduruldu.";
                return res;
            }
            res.log.push_back("CAN (" + HexAddr(comm.canTx, 3) + ") oturum acma yaniti alinamadi.");
        } else {
            res.log.push_back("CAN (" + HexAddr(comm.canTx, 3) + ") kanali acilamadi.");
        }
    }

    if (!anyCanRx) {
        unsigned char kFallback = GetFallbackKlineTargetForBase(baseId);
        if (kFallback != 0) {
            res.log.push_back("CAN (" + HexAddr(comm.canTx, 3) + ") yanit vermedi; otomatik K-Line (" + HexAddr(kFallback, 2) + ") yedek hatti deneniyor...");
            runOnKline(kFallback);
            return res;
        }
    }

    res.success = false;
    res.step = "CAN_NO_RESPONSE";
    res.message = label + ": ECU'dan yanit alinamadi (Kontagin acik oldugunu ve baglantiyi kontrol ediniz).";
    return res;
}

inline ProcedureResult ExecuteStagedSequenceOnBase(
    int baseId,
    const std::string& label,
    const std::vector<std::vector<unsigned char>>& stage1TryAny,
    const std::vector<std::vector<unsigned char>>& stage2TryAny,
    const std::vector<std::vector<unsigned char>>& stage3VerifyAll,
    bool (*verifyValidator)(const std::vector<unsigned char>&) = nullptr,
    std::vector<unsigned char>* outVerifyResp = nullptr,
    const std::vector<unsigned char>& preCheckCmd = {},
    bool (*preValidator)(const std::vector<unsigned char>&) = nullptr,
    std::vector<unsigned char>* outPreResp = nullptr)
{
    ProcedureResult res;
    if (outVerifyResp) outVerifyResp->clear();
    if (outPreResp) outPreResp->clear();
    DbBaseComm comm = GetDbBaseComm(baseId);
    if (!comm.isKline && comm.canTx == 0) {
        res.step = "UNKNOWN_BASE_ADDRESS";
        res.message = label + ": Base " + std::to_string(baseId) + " icin CAN/K-Line adresi tanimli degil; islem guvenlik nedeniyle durduruldu.";
        return res;
    }
    bool isUds = IsUdsBase(baseId);
    std::vector<unsigned char> resp;

    auto runKline = [&](unsigned char kTarget) -> bool {
        res.log.push_back(label + " -> K-Line (" + HexAddr(kTarget, 2) + ", Base " + std::to_string(baseId) + ") kalici oturum aciliyor...");
        KLineChannelScope kScope(g_diag, kTarget);
        if (!kScope.open) {
            res.log.push_back("K-Line (" + HexAddr(kTarget, 2) + ") Fast Init yanit vermedi.");
            res.step = "KLINE_NO_RESP";
            res.message = label + ": K-Line (" + HexAddr(kTarget, 2) + ") uzerinden baglanti kurulamadi.";
            return false;
        }
        bool pos = false;
        kScope.Transact({0x10, 0xC0}, resp, 450, &pos);
        res.log.push_back("TX [10 C0] -> RX [" + (resp.empty() ? "YOK" : HexBytes(resp)) + "]");
        if (!pos) {
            res.step = "KLINE_SESSION_REJECTED";
            res.message = label + ": K-Line oturumu (10 C0) reddedildi.";
            return false;
        }

        if (!preCheckCmd.empty()) {
            bool preOk = kScope.Transact(preCheckCmd, resp, 450, &pos);
            res.log.push_back("TX [PreCheck: " + HexBytes(preCheckCmd) + "] -> RX [" + (resp.empty() ? "YOK" : HexBytes(resp)) + "]");
            bool preValid = (preOk && pos && !resp.empty() &&
                             resp[0] == static_cast<unsigned char>(preCheckCmd[0] + 0x40) &&
                             (!preValidator || preValidator(resp)));
            if (!preValid) {
                res.step = "PRECHECK_FAILED";
                res.message = label + ": Islem oncesi durum okumasi basarisiz (" + (resp.empty() ? "YOK" : HexBytes(resp)) + "); guvenlik nedeniyle durduruldu.";
                return false;
            }
            if (outPreResp) *outPreResp = resp;
        }

        if (!stage1TryAny.empty()) {
            bool s1Ok = false;
            for (const auto& c : stage1TryAny) {
                kScope.Transact(c, resp, 500, &pos);
                res.log.push_back("TX [S1: " + HexBytes(c) + "] -> RX [" + (resp.empty() ? "YOK" : HexBytes(resp)) + "]");
                if (pos) { s1Ok = true; break; }
            }
            if (!s1Ok) {
                res.step = "STAGE1_AUTH_FAILED";
                res.message = label + ": Asama 1 (PIN/Guvenlik dogrulama) reddedildi; yazma/silme islemi durduruldu.";
                return false;
            }
        }

        bool s2Ok = false;
        for (const auto& c : stage2TryAny) {
            kScope.Transact(c, resp, 600, &pos);
            res.log.push_back("TX [S2: " + HexBytes(c) + "] -> RX [" + (resp.empty() ? "YOK" : HexBytes(resp)) + "]");
            if (pos) { s2Ok = true; break; }
        }
        if (!s2Ok) {
            res.step = "STAGE2_ACTION_FAILED";
            res.message = label + ": Guvenlik acildi ancak asil islem komutu ECU tarafindan reddedildi.";
            return false;
        }

        for (const auto& c : stage3VerifyAll) {
            bool vRxOk = kScope.Transact(c, resp, 450, &pos);
            res.log.push_back("TX [Verify: " + HexBytes(c) + "] -> RX [" + (resp.empty() ? "YOK" : HexBytes(resp)) + "]");
            bool vValid = (vRxOk && pos && !resp.empty() &&
                           resp[0] == static_cast<unsigned char>(c[0] + 0x40) &&
                           (!verifyValidator || verifyValidator(resp)));
            if (!vValid) {
                res.success = false;
                res.step = "STAGE3_VERIFY_FAILED";
                res.message = label + ": Asama 2 onaylandi ancak Asama 3 dogrulama okumasi basarisiz veya gecersiz (" +
                              (resp.empty() ? "YOK" : HexBytes(resp)) + ").";
                return false;
            }
            if (outVerifyResp) *outVerifyResp = resp;
        }
        res.success = true;
        res.step = "DONE_KLINE";
        res.message = label + " (K-Line " + HexAddr(kTarget, 2) + ") tum asamalar onaylanarak tamamlandi!";
        return true;
    };

    if (comm.isKline) {
        runKline(comm.klineTarget);
        return res;
    }

    bool anyCanRx = false;
    {
        res.log.push_back(label + " -> CAN (" + HexAddr(comm.canTx, 3) + "/" + HexAddr(comm.canRx, 3) + ", Base " + std::to_string(baseId) + ") kalici oturum aciliyor...");
        CanChannelScope canScope(g_diag, comm.canTx, comm.canRx);
        if (canScope.open) {
            std::vector<unsigned char> sessCmd = isUds ? std::vector<unsigned char>{0x10, 0x03} : std::vector<unsigned char>{0x10, 0xC0};
            bool rxOk = canScope.Transact(sessCmd, resp, 350);
            if (!resp.empty()) anyCanRx = true;
            bool sessOk = rxOk && !resp.empty() && resp[0] == 0x50;
            if (!sessOk && !isUds) {
                std::vector<unsigned char> respAlt;
                bool altRx = canScope.Transact({0x10, 0x81}, respAlt, 350);
                if (!respAlt.empty()) { anyCanRx = true; resp = respAlt; }
                sessOk = altRx && !respAlt.empty() && respAlt[0] == 0x50;
            }
            if (sessOk) {
                res.log.push_back("CAN Oturumu acildi: " + HexBytes(resp));
                if (!preCheckCmd.empty()) {
                    std::vector<unsigned char> rx;
                    bool preOk = canScope.Transact(preCheckCmd, rx, 450);
                    res.log.push_back("TX [PreCheck: " + HexBytes(preCheckCmd) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
                    bool preValid = (preOk && !rx.empty() &&
                                     rx[0] == static_cast<unsigned char>(preCheckCmd[0] + 0x40) &&
                                     (!preValidator || preValidator(rx)));
                    if (!preValid) {
                        canScope.Transact(isUds ? std::vector<unsigned char>{0x10, 0x01} : std::vector<unsigned char>{0x10, 0x81}, resp, 200);
                        res.step = "PRECHECK_FAILED";
                        res.message = label + ": Islem oncesi durum okumasi basarisiz (" + (rx.empty() ? "YOK" : HexBytes(rx)) + "); guvenlik nedeniyle durduruldu.";
                        return res;
                    }
                    if (outPreResp) *outPreResp = rx;
                }
                if (!stage1TryAny.empty()) {
                    bool s1Ok = false;
                    for (const auto& c : stage1TryAny) {
                        std::vector<unsigned char> rx;
                        bool ok = canScope.Transact(c, rx, 550);
                        res.log.push_back("TX [S1: " + HexBytes(c) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
                        if (ok && !rx.empty() && rx[0] == static_cast<unsigned char>(c[0] + 0x40)) {
                            s1Ok = true;
                            break;
                        }
                    }
                    if (!s1Ok) {
                        canScope.Transact(isUds ? std::vector<unsigned char>{0x10, 0x01} : std::vector<unsigned char>{0x10, 0x81}, resp, 200);
                        res.step = "STAGE1_AUTH_FAILED";
                        res.message = label + ": Asama 1 (PIN/Guvenlik dogrulama) reddedildi; yazma/silme islemi durduruldu.";
                        return res;
                    }
                }
                bool s2Ok = false;
                for (const auto& c : stage2TryAny) {
                    std::vector<unsigned char> rx;
                    bool ok = canScope.Transact(c, rx, 600);
                    res.log.push_back("TX [S2: " + HexBytes(c) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
                    if (ok && !rx.empty() && rx[0] == static_cast<unsigned char>(c[0] + 0x40)) {
                        s2Ok = true;
                        break;
                    }
                }
                if (!s2Ok) {
                    canScope.Transact(isUds ? std::vector<unsigned char>{0x10, 0x01} : std::vector<unsigned char>{0x10, 0x81}, resp, 200);
                    res.step = "STAGE2_ACTION_FAILED";
                    res.message = label + ": Asama 2 (Ana islem) ECU tarafindan reddedildi.";
                    return res;
                }
                for (const auto& c : stage3VerifyAll) {
                    std::vector<unsigned char> rx;
                    bool vOk = canScope.Transact(c, rx, 450);
                    res.log.push_back("TX [Verify: " + HexBytes(c) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
                    bool vValid = (vOk && !rx.empty() &&
                                   rx[0] == static_cast<unsigned char>(c[0] + 0x40) &&
                                   (!verifyValidator || verifyValidator(rx)));
                    if (!vValid) {
                        canScope.Transact(isUds ? std::vector<unsigned char>{0x10, 0x01} : std::vector<unsigned char>{0x10, 0x81}, resp, 200);
                        res.success = false;
                        res.step = "STAGE3_VERIFY_FAILED";
                        res.message = label + ": Asama 2 onaylandi ancak Asama 3 dogrulama okumasi basarisiz veya gecersiz (" +
                                      (rx.empty() ? "YOK" : HexBytes(rx)) + ").";
                        return res;
                    }
                    if (outVerifyResp) *outVerifyResp = rx;
                }
                canScope.Transact(isUds ? std::vector<unsigned char>{0x10, 0x01} : std::vector<unsigned char>{0x10, 0x81}, resp, 200);
                res.success = true;
                res.step = "DONE_CAN";
                res.message = label + " (CAN " + HexAddr(comm.canTx, 3) + ") tum asamalar onaylanarak tamamlandi!";
                return res;
            }
            if (anyCanRx) {
                res.step = "CAN_SESSION_REJECTED";
                res.message = label + ": CAN diagnostik oturumu reddedildi (" + HexBytes(resp) + ").";
                return res;
            }
        }
    }

    if (!anyCanRx) {
        unsigned char kFallback = GetFallbackKlineTargetForBase(baseId);
        if (kFallback != 0) {
            runKline(kFallback);
            return res;
        }
    }
    res.step = "CAN_NO_RESPONSE";
    res.message = label + ": ECU'dan yanit alinamadi.";
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
    std::string targetErr;
    int baseId = ResolveDbTargetBase(found->basesCsv, overrideBaseId, targetErr);
    if (baseId <= 0) {
        ProcedureResult r;
        r.step = "TARGET_BASE_BLOCKED";
        r.message = targetErr;
        return r;
    }
    return ExecuteHexSequenceOnBase(baseId, std::string("Aktif Test [") + found->testName + " - " + found->titleTr + " / " + modeLabel + "]", {bytes}, SequenceMode::RequireAll);
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
    std::string targetErr;
    int baseId = ResolveDbTargetBase(found->basesCsv, overrideBaseId, targetErr);
    if (baseId <= 0) {
        ProcedureResult r;
        r.step = "TARGET_BASE_BLOCKED";
        r.message = targetErr;
        return r;
    }
    // Note: DbUtilityRecord::allHex contains semicolon-separated candidate hex variants across ECU revisions.
    return ExecuteHexSequenceOnBase(baseId, std::string("Suzuki Service Utility [") + found->code + " - " + found->titleTr + "]", cmds, SequenceMode::TryFirstPositive);
}

// --- Additional Dedicated OEM Calibrations from Suzuki/Utilities/*.js ---

// DSL-05.js: F9Q ECM Replacement Initialization + DSL-11.js Starter / Assembly Line Test
inline ProcedureResult ExecuteF9QEcmReplaceAndStarterTest(bool starterTestOnly) {
    if (starterTestOnly) {
        return ExecuteHexSequenceOnBase(93, "F9Q Mars / Montaj Hatti Testi (DSL-11)", {
            {0x31, 0x61, 0x00},
            {0x21, 0x61},
            {0x31, 0x61, 0x01}
        }, SequenceMode::RequireAll);
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
    }, SequenceMode::RequireAll);
}

// DSL-16..DSL-39: CAN Diesel ECM (Base 41 - D13A / D16AA / D20AA / K9K / F9Q CAN) Reset & Calibrations
inline ProcedureResult ExecuteCanDieselCalibration(const std::string& subType) {
    int targetBase = (GetMatchedEcuBaseId() > 0 && IsUdsBase(GetMatchedEcuBaseId())) ? GetMatchedEcuBaseId() : 41;
    if (subType == "dpf_regen") {
        return ExecuteHexSequenceOnBase(targetBase, "CAN Dizel DPF Rejenerasyonu & DPF Sifirlama (DSL-16 / DSL-18)", {
            {0x30, 0x74, 0x08, 0x40, 0x40, 0x00, 0x00},
            {0x30, 0x72, 0x08, 0x80, 0x80, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    if (subType == "oil_reset") {
        return ExecuteHexSequenceOnBase(targetBase, "CAN Dizel Yag Degisim Isigi & Yakit Filtresi Sifirlama (DSL-22 / DSL-32)", {
            {0x30, 0x74, 0x08, 0x02, 0x02, 0x00, 0x00},
            {0x30, 0x79, 0x08, 0x40, 0x40, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    if (subType == "egr_learn") {
        return ExecuteHexSequenceOnBase(targetBase, "CAN Dizel EGR-1 / EGR-2 & Kelebek Ogrenme Sifirlama (DSL-25 / DSL-35 / DSL-39)", {
            {0x30, 0x70, 0x08, 0x08, 0x08, 0x00, 0x00},
            {0x30, 0x70, 0x08, 0x40, 0x40, 0x00, 0x00},
            {0x30, 0x71, 0x08, 0x80, 0x80, 0x00, 0x00},
            {0x30, 0x70, 0x08, 0x20, 0x20, 0x00, 0x00},
            {0x30, 0x7B, 0x08, 0x80, 0x80, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    // Default: CAN Diesel Sensors (Diff Pressure DSL-17, Flywheel DSL-19, A/F DSL-20, MAF DSL-21, PRV DSL-23, Rail Pressure DSL-24)
    return ExecuteHexSequenceOnBase(targetBase, "CAN Dizel Sensor Adaptasyonlari (Fark Basinc, MAF, A/F, Volan, Rail - DSL-17..24)", {
        {0x30, 0x74, 0x08, 0x80, 0x80, 0x00, 0x00}, // DSL-17 Diff Pressure
        {0x30, 0x74, 0x08, 0x10, 0x10, 0x00, 0x00}, // DSL-19 Flywheel
        {0x30, 0x74, 0x08, 0x08, 0x08, 0x00, 0x00}, // DSL-20 A/F Sensor
        {0x30, 0x74, 0x08, 0x04, 0x04, 0x00, 0x00}, // DSL-21 MAF Sensor
        {0x30, 0x74, 0x08, 0x01, 0x01, 0x00, 0x00}, // DSL-23 PRV
        {0x30, 0x78, 0x08, 0x80, 0x80, 0x00, 0x00}  // DSL-24 Rail Pressure Sensor
    }, SequenceMode::TryFirstPositive);
}

// Petrol & General ECM Learned Value / Throttle / Idle Air / Brake Stroke / GPF Reset (ECM-03, ECM-101, ECM-105, ECM-122, ECM-264)
inline ProcedureResult ExecutePetrolEcmCalibration() {
    std::string effProf = ResolveEffectiveProfile();
    int matchedBase = GetMatchedEcuBaseId();
    if (effProf == "suzuki_kline" || matchedBase == 64 || matchedBase == 106) {
        return ExecuteHexSequenceOnBase(64, "Klasik K-Line Benzinli ECM Adaptasyon Sifirlama (ECM-101/105/122)", {
            {0x30, 0x04, 0x00},
            {0x31, 0x04, 0x00},
            {0x31, 0x03, 0x00},
            {0x31, 0x06, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    if (matchedBase > 0 && IsUdsBase(matchedBase)) {
        return ExecuteHexSequenceOnBase(194, "UDS Benzinli ECM Fren Stroku / GPF Baslatma (ECM-03/264 UDS)", {
            {0x31, 0x01, 0xC0, 0x43, 0x00},
            {0x31, 0x01, 0xC0, 0x08, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    return ExecuteHexSequenceOnBase(41, "Benzinli CAN ECM Bogaz Kelebegi, Rolanti & Fren Stroku Sifirlama (ECM-03/101/105/122/264)", {
        {0x30, 0x04, 0x00},                         // ECM-122 Closed Throttle Pos Learn
        {0x31, 0x04, 0x00},                         // ECM-122 Routine
        {0x31, 0x03, 0x00},                         // ECM-101 Idle Air Vol Learn
        {0x31, 0x06, 0x00},                         // ECM-105 Self-Learning Control Clear
        {0x30, 0xD3, 0x08, 0x80, 0x80, 0x00, 0x00}, // ECM-03 Brake Stroke Sensor Zero Point
        {0x30, 0x15, 0x08, 0x80, 0x80, 0x00, 0x00}  // ECM-264 GPF Info Init
    }, SequenceMode::TryFirstPositive);
}

// Transmission (TCM: AT-06, AMT-01..03, CVT-01..03) Learning & Calibration
inline ProcedureResult ExecuteTcmCalibration() {
    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase > 0 && IsUdsBase(matchedBase)) {
        return ExecuteHexSequenceOnBase(147, "UDS Sanziman (CVT/AT) Ogrenme Degeri Sifirlama (CVT-02 UDS)", {
            {0x31, 0x01, 0xC1, 0x08, 0x00}
        }, SequenceMode::RequireAll);
    }
    return ExecuteHexSequenceOnBase(49, "Sanziman (AT / AMT / AGS / CVT) Kavrama, Vites ve Egim Sensoru Kalibrasyonu (AT-06 / AMT-01 / CVT-01..03)", {
        {0x30, 0x60, 0x08, 0x80, 0x80, 0x00, 0x00}, // AT-06 Neutral Position Calibration
        {0x30, 0x77, 0x08, 0x80, 0x80, 0x00, 0x00}, // AMT-01 All Learned Values Init
        {0x30, 0x76, 0x08, 0x80, 0x80, 0x00, 0x00}, // AMT-01 Clutch Init
        {0x30, 0x7A, 0x08, 0x80, 0x80, 0x00, 0x00}, // AMT-01 Gear Tower Init
        {0x30, 0x00, 0x08, 0x80, 0x80, 0x00, 0x00}, // CVT-01/02 Learned Value Reset
        {0x30, 0x20, 0x08, 0x80, 0x80, 0x00, 0x00}, // CVT-01
        {0x30, 0x51, 0x08, 0x80, 0x80, 0x00, 0x00}  // CVT-03 Inclination / G-Sensor Calibration
    }, SequenceMode::TryFirstPositive);
}

// EPS-01: Electric Power Steering Sensor Calibration
inline ProcedureResult ExecuteEpsCalibration() {
    return ExecuteHexSequenceOnBase(42, "EPS Elektrikli Direksiyon Tork / Aci Sensoru Kalibrasyonu (EPS-01)", {
        {0x30, 0x11, 0x08, 0x80, 0x80, 0x00, 0x00}
    }, SequenceMode::RequireAll);
}

// ABS-01 & ABS-04: ABS Hydraulic Bleed / Solenoid Test & ESP Function Setting
inline ProcedureResult ExecuteAbsBleedAndEspFunc() {
    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase > 0 && IsUdsBase(matchedBase)) {
        return ExecuteHexSequenceOnBase(185, "ABS/ESP UDS Fonksiyon Ayari (ABS-04 UDS)", {
            {0x31, 0x01, 0xC0, 0x20, 0x00}
        }, SequenceMode::RequireAll);
    }
    return ExecuteHexSequenceOnBase(34, "ABS Hidrolik Kontrol / Hava Alma Testi (ABS-01)", {
        {0x30, 0x00, 0x07, 0x01, 0x01, 0x00, 0x00}, // ABS-01 Hydraulic Control Test Start
        {0x30, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00}  // ABS-01 Hydraulic Control Test Stop
    }, SequenceMode::RequireAll);
}

// BCM-01 & TPMS-01: BCM Airbag Crash Record Clear & TPMS Sensor ID Registration
inline ProcedureResult ExecuteBcmAndTpmsService(bool tpmsMode) {
    if (tpmsMode) {
        ProcedureResult r = ExecuteHexSequenceOnBase(92, "TPMS Lastik Basinc Sensoru ID Kayit Modu (TPMS-01)", {
            {0x30, 0x08, 0x07, 0x80, 0x00, 0x00, 0x00},
            {0x30, 0x25, 0x07, 0x80, 0x00, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
        if (!r.success && r.step == "CAN_NO_RESPONSE") {
            return ExecuteHexSequenceOnBase(39, "TPMS / BCM Lastik Sensoru Kayit Modu (TPMS-01 BCM)", {
                {0x30, 0x08, 0x07, 0x80, 0x00, 0x00, 0x00},
                {0x30, 0x25, 0x07, 0x80, 0x00, 0x00, 0x00}
            }, SequenceMode::TryFirstPositive);
        }
        return r;
    }
    std::string effProf = ResolveEffectiveProfile();
    if (effProf == "suzuki_kline") {
        return ExecuteHexSequenceOnBase(63, "BCM Hava Yastigi Carpma Kaydi Silme (BCM-01 K-Line 0x5A)", {
            {0xA5, 0x07, 0x08, 0x00, 0x00, 0x00},
            {0x30, 0x07, 0x08, 0x08, 0x00, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    ProcedureResult r = ExecuteHexSequenceOnBase(39, "BCM Hava Yastigi (Airbag) Carpma Kaydi Silme (BCM-01 CAN)", {
        {0x30, 0x07, 0x08, 0x08, 0x00, 0x00, 0x00}
    }, SequenceMode::RequireAll);
    if (!r.success && (r.step == "CAN_NO_RESPONSE" || r.step == "KLINE_REJECTED")) {
        return ExecuteHexSequenceOnBase(63, "BCM Hava Yastigi Carpma Kaydi Silme (BCM-01 K-Line 0x5A)", {
            {0xA5, 0x07, 0x08, 0x00, 0x00, 0x00},
            {0x30, 0x07, 0x08, 0x08, 0x00, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    return r;
}

// IPC-03 & IPC-04: Instrument Cluster Oil Reminder & Navigation Reset
inline ProcedureResult ExecuteIpcServiceReset() {
    int matchedBase = GetMatchedEcuBaseId();
    if (matchedBase > 0 && IsUdsBase(matchedBase)) {
        return ExecuteHexSequenceOnBase(197, "Gosterge Paneli (IPC) UDS Navigasyon Sifirlama (IPC-04 UDS)", {
            {0x31, 0x01, 0xC0, 0x52, 0x00}
        }, SequenceMode::RequireAll);
    }
    return ExecuteHexSequenceOnBase(47, "Gosterge Paneli (IPC) Yag Bakim Isigi & Navigasyon Sifirlama (IPC-03 / IPC-04)", {
        {0x30, 0x35, 0x08, 0x80, 0x00, 0x00, 0x00},
        {0x30, 0x31, 0x08, 0x08, 0x00, 0x00, 0x00}
    }, SequenceMode::TryFirstPositive);
}

// ALC-01: Auto Headlamp Leveling Height Sensor Initialization
inline ProcedureResult ExecuteAlcSensorInit() {
    return ExecuteHexSequenceOnBase(38, "Otomatik Far Seviye (ALC) Yukseklik Sensoru Ilk Ayari (ALC-01)", {
        {0x30, 0x00, 0x08, 0x80, 0x80, 0x00, 0x00}
    }, SequenceMode::RequireAll);
}

// --- Hidden Features & BCM Personalization / Coding Engine (Model-by-Model) ---

inline std::string ReadBcmCodingStateJson() {
    std::vector<std::string> logLines;
    std::vector<unsigned char> resp;
    std::string source = "none";
    std::string rawHex = "";
    std::map<std::string, int> states;

    auto decode2108 = [&](const std::vector<unsigned char>& r) {
        size_t shiftOff = (!r.empty() && r[0] == 0x61) ? 1 : 0;
        auto byteAt = [&](size_t jsPos) -> int {
            size_t idx = jsPos + shiftOff;
            return (idx < r.size()) ? static_cast<int>(r[idx]) : -1;
        };
        if (byteAt(29) >= 0 && r.size() < 50) {
            // BCM-06 (Base 63 K-Line 21 08 layout - Grand Vitara JB / SX4 Pin 7 K-Line 0x5A)
            states["HF-ALARM"] = ((byteAt(29) & 0x04) >> 2);
            states["HF-ANSWERLIGHT"] = (((byteAt(29) & 0x80) >> 7) == 0) ? 1 : 0;
            if (byteAt(31) >= 0) {
                states["HF-AUTOLOCK"] = (byteAt(31) & 0x03);
                states["HF-AUTOUNLOCK"] = ((byteAt(31) & 0x0C) >> 2);
                states["HF-UNLOCKMODE"] = ((byteAt(31) & 0x10) >> 4);
            }
            if (byteAt(32) >= 0) {
                states["HF-BUZZER"] = (byteAt(32) & 0x01);
            }
            if (byteAt(33) >= 0) {
                states["HF-FOLLOWHOME"] = (byteAt(33) & 0x07);
                states["HF-ROOMLAMP"] = ((byteAt(33) & 0x18) >> 3);
            }
            if (byteAt(34) >= 0) {
                states["HF-RETAINEDPWR"] = (byteAt(34) & 0x03);
                states["HF-AUTOLIGHT-HW"] = ((byteAt(34) & 0x04) >> 2);
            }
            states["HF-ESP-DYNO"] = 0;
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

    // 1. Try CAN BCM candidates using persistent CAN channel scope per ECU
    struct CanBcmCandidate { unsigned long tx; unsigned long rx; const char* tag; };
    const CanBcmCandidate canBcmTargets[] = {
        {0x271, 0x671, "CAN_BCM_271"},
        {0x264, 0x664, "CAN_BCM_264"},
        {0x7E4, 0x7EC, "CAN_BCM_7E4"}
    };

    for (const auto& target : canBcmTargets) {
        logLines.push_back(std::string("Reading BCM Configuration on CAN (") + HexAddr(target.tx, 3) + "/" + HexAddr(target.rx, 3) + " -> 21 08)...");
        CanChannelScope canScope(g_diag, target.tx, target.rx);
        if (!canScope.open) continue;

        bool sessOk = (canScope.Transact({0x10, 0xC0}, resp, 200) && !resp.empty() && resp[0] == 0x50) ||
                      (canScope.Transact({0x10, 0x81}, resp, 200) && !resp.empty() && resp[0] == 0x50);
        if (!sessOk) continue;

        if (canScope.Transact({0x21, 0x08}, resp, 450) && resp.size() >= 10 && resp[0] == 0x61) {
            source = std::string(target.tag) + "_2108";
            rawHex = HexBytes(resp);
            logLines.push_back(std::string(target.tag) + " 21 08 Read OK: " + rawHex);
            decode2108(resp);
            canScope.Transact({0x10, 0x81}, resp, 150);
            break;
        } else if (canScope.Transact({0x21, 0x01}, resp, 400) && resp.size() >= 6 && resp[0] == 0x61) {
            source = std::string(target.tag) + "_2101";
            rawHex = HexBytes(resp);
            logLines.push_back(std::string(target.tag) + " 21 01 Read OK: " + rawHex);
            decode2101(resp);
            canScope.Transact({0x10, 0x81}, resp, 150);
            break;
        } else if (canScope.Transact({0x21, 0x80}, resp, 400) && resp.size() >= 6 && resp[0] == 0x61) {
            source = std::string(target.tag) + "_2180";
            rawHex = HexBytes(resp);
            logLines.push_back(std::string(target.tag) + " 21 80 Read OK: " + rawHex);
            decode2108(resp);
            canScope.Transact({0x10, 0x81}, resp, 150);
            break;
        }
        canScope.Transact({0x10, 0x81}, resp, 150);
    }

    if (source == "none") {
        // 2. Try K-Line BCM (0x5A on Pin 7 - Grand Vitara / Swift / SX4 / Jimny)
        logLines.push_back("CAN BCM no response; probing K-Line BCM (Pin 7, Target 0x5A -> 21 08 / 21 01)...");
        std::vector<std::vector<unsigned char>> kReqs = {{0x21, 0x08}, {0x21, 0x01}};
        std::vector<std::vector<unsigned char>> kResps;
        if (g_diag.RunKLineSession(0x5A, kReqs, kResps, 450)) {
            if (!kResps.empty() && kResps[0].size() >= 8 && kResps[0][0] == 0x61) {
                source = "KLINE_BCM_5A_2108";
                rawHex = HexBytes(kResps[0]);
                logLines.push_back("K-Line BCM (0x5A) 21 08 Read OK: " + rawHex);
                decode2108(kResps[0]);
            }
            if (kResps.size() >= 2 && kResps[1].size() >= 6 && kResps[1][0] == 0x61) {
                if (source == "none") source = "KLINE_BCM_5A_2101";
                rawHex = HexBytes(kResps[1]);
                logLines.push_back("K-Line BCM (0x5A) 21 01 Read OK: " + rawHex);
                decode2101(kResps[1]);
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

enum class SuzukiImmoSystem {
    GV_JB419D_DIESEL,
    GV_JB_PETROL,
    CLASSIC_KLINE,
    MODERN_CAN_UDS,
    UNIVERSAL_AUTO,
    UNKNOWN_PROFILE
};

inline SuzukiImmoSystem GetActiveImmoSystem(const std::string& profId) {
    std::string p = ResolveEffectiveProfileFor(profId);
    if (p == "gv_jb419d") return SuzukiImmoSystem::GV_JB419D_DIESEL;
    if (p == "gv_petrol") return SuzukiImmoSystem::GV_JB_PETROL;
    if (p == "suzuki_kline") return SuzukiImmoSystem::CLASSIC_KLINE;
    if (p == "suzuki_can") return SuzukiImmoSystem::MODERN_CAN_UDS;
    if (p == "UNIDENTIFIED") return SuzukiImmoSystem::UNIVERSAL_AUTO;
    return SuzukiImmoSystem::UNKNOWN_PROFILE;
}

inline std::string GetImmoSystemName(SuzukiImmoSystem sys) {
    switch (sys) {
        case SuzukiImmoSystem::GV_JB419D_DIESEL: return "Grand Vitara 1.9 DDiS (K-Line 0x5D KLS / 4-Digit PIN)";
        case SuzukiImmoSystem::GV_JB_PETROL: return "Grand Vitara Benzinli (CAN 0x7E0 ECM + K-Line 0x5D KLS / 4-Digit PIN)";
        case SuzukiImmoSystem::CLASSIC_KLINE: return "Klasik Suzuki K-Line (K-Line 0x5B IMM / 0x11 ECM / 4-Digit PIN)";
        case SuzukiImmoSystem::MODERN_CAN_UDS: return "Modern Suzuki CAN UDS (KLS 0x275 / BCM 0x271; Nissan/Landy Base 169: 0x745/0x765)";
        case SuzukiImmoSystem::UNKNOWN_PROFILE: return "Bilinmeyen Profil (Kilitli)";
        default: return "Tanimlanmamis Oto Mod (Yalnizca Salt-Okunur Tarama Acik)";
    }
}

inline bool IsHiddenFeatureSupportedForProfile(const std::string& featureId, const std::string& prof) {
    std::string p = ResolveEffectiveProfileFor(prof);
    if (p == "gv_jb419d" || p == "gv_petrol") {
        return (featureId == "HF-ALARM" ||
                featureId == "HF-ANSWERLIGHT" ||
                featureId == "HF-AUTOLOCK" ||
                featureId == "HF-AUTOUNLOCK" ||
                featureId == "HF-UNLOCKMODE" ||
                featureId == "HF-ROOMLAMP" ||
                featureId == "HF-RETAINEDPWR" ||
                featureId == "HF-AUTOLIGHT-HW" ||
                featureId == "HF-BUZZER" ||
                featureId == "HF-FOLLOWHOME" ||
                featureId == "HF-ESP-DYNO");
    }
    if (p == "suzuki_kline") {
        return (featureId == "HF-ALARM" ||
                featureId == "HF-ANSWERLIGHT" ||
                featureId == "HF-ROOMLAMP");
    }
    if (p == "suzuki_can") {
        if (featureId == "HF-UDS-IKEY") return IsNissanLandyMatched();
        return (featureId != "HF-AUTOLIGHT-HW");
    }
    // Fail-closed for UNIDENTIFIED ("auto" with no matched ECU) and UNKNOWN_PROFILE
    return false;
}

inline bool IsServiceSupportedForProfile(const std::string& svcKey, const std::string& prof) {
    std::string p = ResolveEffectiveProfileFor(prof);
    if (p == "gv_jb419d") {
        return (svcKey == "sas" || svcKey == "egr" || svcKey == "throttle" ||
                svcKey == "injector" || svcKey == "dpf" || svcKey == "dpf_regen" ||
                svcKey == "oil" || svcKey == "rail_test" || svcKey == "f9q_ecm_replace" ||
                svcKey == "f9q_starter_test" || svcKey == "abs_bleed" || svcKey == "bcm_airbag" ||
                svcKey == "tpms" || svcKey == "ipc" || svcKey == "alc" || svcKey == "hvac" ||
                svcKey == "clear");
    }
    if (p == "gv_petrol") {
        return (svcKey == "sas" || svcKey == "petrol_ecm" || svcKey == "tcm" ||
                svcKey == "abs_bleed" || svcKey == "bcm_airbag" || svcKey == "tpms" ||
                svcKey == "ipc" || svcKey == "alc" || svcKey == "hvac" || svcKey == "clear");
    }
    if (p == "suzuki_kline") {
        return (svcKey == "sas" || svcKey == "petrol_ecm" || svcKey == "tcm" ||
                svcKey == "eps" || svcKey == "abs_bleed" || svcKey == "bcm_airbag" ||
                svcKey == "tpms" || svcKey == "hvac" || svcKey == "clear");
    }
    if (p == "suzuki_can") {
        return (svcKey != "egr" && svcKey != "throttle" && svcKey != "injector" &&
                svcKey != "dpf" && svcKey != "rail_test" && svcKey != "f9q_ecm_replace" &&
                svcKey != "f9q_starter_test" && svcKey != "oil");
    }
    if (p == "UNIDENTIFIED") {
        // Fail-closed: block calibrations and DTC clearing until vehicle ECU identity is confirmed or manually selected
        return false;
    }
    return false;
}

inline ProcedureResult ExecuteHiddenFeatureCoding(const std::string& featureId, int optVal) {
    std::string effProf = ResolveEffectiveProfile();
    if (!IsHiddenFeatureSupportedForProfile(featureId, effProf)) {
        ProcedureResult r;
        r.step = (effProf == "UNIDENTIFIED") ? "VEHICLE_UNIDENTIFIED" : "FEATURE_NOT_SUPPORTED";
        r.success = false;
        r.message = (effProf == "UNIDENTIFIED")
            ? "Arac modeli/ECU henuz dogrulanmadi ('auto' modu). BCM kodlamasi gondermeden once 'Arac Tara' ile kimlik okuyun veya ustten arac modelini secin."
            : ("Bu ozellik (" + featureId + ") secili arac modelinde (" + effProf + ") donanimsal olarak bulunmamaktadir. BCM guvenligi icin komut gonderilmedi.");
        r.log.push_back("Security Check: Feature rejected (" + effProf + " / raw profile: " + g_activeProfileId + ").");
        return r;
    }

    auto tryCanThenClassic = [&](const std::string& label,
                                 const std::vector<std::vector<unsigned char>>& canCmds,
                                 const std::vector<std::vector<unsigned char>>& klineCmds,
                                 SequenceMode canMode = SequenceMode::RequireAll,
                                 SequenceMode klMode = SequenceMode::TryFirstPositive) -> ProcedureResult {
        if (effProf == "suzuki_can") {
            return canCmds.empty()
                ? ProcedureResult{false, "CAN_NOT_APPLICABLE", label + ": CAN UDS/BCM-03 komutu tanimli degil.", {}}
                : ExecuteHexSequenceOnBase(39, label + " [CAN BCM-03]", canCmds, canMode);
        }
        if (effProf == "suzuki_kline") {
            return klineCmds.empty()
                ? ProcedureResult{false, "KLINE_NOT_APPLICABLE", label + ": Klasik K-Line komutu tanimli degil.", {}}
                : ExecuteHexSequenceOnBase(63, label + " [K-Line 0x5A]", klineCmds, klMode);
        }
        // gv_jb419d / gv_petrol: CAN BCM (Base 39) vs K-Line BCM (Base 63) are probed SEPARATELY, never mixed in one batch.
        if (!canCmds.empty()) {
            ProcedureResult rCan = ExecuteHexSequenceOnBase(39, label + " [CAN BCM-03]", canCmds, canMode);
            if (rCan.success || klineCmds.empty()) return rCan;
            ProcedureResult rKl = ExecuteHexSequenceOnBase(63, label + " [K-Line 0x5A]", klineCmds, klMode);
            for (const auto& l : rCan.log) rKl.log.insert(rKl.log.begin(), l);
            return rKl;
        }
        return ExecuteHexSequenceOnBase(63, label + " [K-Line 0x5A]", klineCmds, klMode);
    };

    if (featureId == "HF-DRL") {
        unsigned char b = (optVal == 1) ? 0x80 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Daytime Running Lights (DRL) -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x36, 0x08, 0x80, b, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-AUTOLOCK") {
        unsigned char modeCan = (optVal == 2) ? 0x02 : (optVal == 1 ? 0x01 : 0x00);
        unsigned char modeKl  = (optVal == 2) ? 0x02 : (optVal == 1 ? 0x01 : 0x00);
        return tryCanThenClassic("Hidden Feature: Speed/Shift Auto Door Lock",
            {{0x30, 0x35, 0x08, 0x01, 0x00, modeCan, 0x00}},
            {{0x3B, 0x2F, modeKl}},
            SequenceMode::RequireAll, SequenceMode::RequireAll);
    }
    if (featureId == "HF-AUTOUNLOCK") {
        unsigned char modeCan = (optVal == 2) ? 0x02 : (optVal == 1 ? 0x01 : 0x00);
        unsigned char modeKl  = (optVal >= 1) ? 0x01 : 0x00;
        return tryCanThenClassic("Hidden Feature: Auto Door Unlock on IG-OFF / Shift-P",
            {{0x30, 0x35, 0x08, 0x02, 0x00, modeCan, 0x00}},
            {{0x3B, 0x02, modeKl}},
            SequenceMode::RequireAll, SequenceMode::RequireAll);
    }
    if (featureId == "HF-LANEFLASH") {
        unsigned char cs = (optVal >= 0 && optVal <= 3) ? static_cast<unsigned char>(optVal) : 0x03;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Comfort Lane-Change Turn Signal Flashes (BCM-03)", {
            {0x30, 0x35, 0x08, 0x20, 0x00, cs, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-MIRROR") {
        unsigned char b = (optVal == 1) ? 0x20 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Auto Folding Side Mirrors -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x33, 0x08, 0x20, b, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-CORNER") {
        unsigned char b = (optVal == 1) ? 0x04 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: Cornering Fog Lights -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x31, 0x08, 0x04, b, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-BUZZER") {
        if (optVal == 1) {
            return tryCanThenClassic("Hidden Feature: Lock/Unlock Outside Buzzer ON",
                {{0x30, 0x33, 0x08, 0x80, 0x80, 0x00, 0x00}},
                {{0x3B, 0x2B, 0x01}},
                SequenceMode::RequireAll, SequenceMode::RequireAll);
        }
        if (optVal == 2) {
            return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock/Unlock Horn Chime ON (BCM-03)", {
                {0x30, 0x31, 0x08, 0x08, 0x08, 0x00, 0x00}
            }, SequenceMode::RequireAll);
        }
        if (optVal == 3) {
            return ExecuteHexSequenceOnBase(39, "Hidden Feature: Lock/Unlock Siren Answerback ON (BCM-03)", {
                {0x30, 0x33, 0x08, 0x10, 0x10, 0x00, 0x00}
            }, SequenceMode::RequireAll);
        }
        return tryCanThenClassic("Hidden Feature: Lock/Unlock Acoustic Answerback OFF / Silent",
            {
                {0x30, 0x33, 0x08, 0x80, 0x00, 0x00, 0x00},
                {0x30, 0x31, 0x08, 0x08, 0x00, 0x00, 0x00},
                {0x30, 0x33, 0x08, 0x10, 0x00, 0x00, 0x00}
            },
            {{0x3B, 0x2B, 0x00}},
            SequenceMode::TryFirstPositive, SequenceMode::RequireAll);
    }
    if (featureId == "HF-ALARM") {
        unsigned char bCan = (optVal == 1) ? 0x20 : 0x00;
        unsigned char bKl  = (optVal == 1) ? 0x00 : 0x01; // BCM-110: 0x00 = WITH, 0x01 = WITHOUT
        return tryCanThenClassic(std::string("Hidden Feature: Factory Security Anti-Theft Alarm -> ") + (optVal ? "ACTIVE" : "DISABLED"),
            {{0x30, 0x34, 0x08, 0x20, bCan, 0x00, 0x00}},
            {
                {0x30, 0x09, 0x08, 0x20, bCan, 0x00, 0x00}, // BCM-06 K-Line 0x5A
                {0x3B, 0x21, bKl}                           // BCM-110 Classic
            },
            SequenceMode::RequireAll, SequenceMode::TryFirstPositive);
    }
    if (featureId == "HF-UNLOCKMODE") {
        unsigned char cs = (optVal == 1) ? 0x01 : 0x00;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Remote Door Unlock Action (1-Press All vs 2-Press)", {
            {0x30, 0x3B, 0x08, 0x10, 0x00, cs, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-FOLLOWHOME") {
        unsigned char cs = (optVal >= 0 && optVal <= 5) ? static_cast<unsigned char>(optVal) : 0x02;
        return tryCanThenClassic("Hidden Feature: Guide Me Home / Follow Me Home Headlight Timer",
            {{0x30, 0x3B, 0x08, 0x02, 0x00, cs, 0x00}},
            {{0x3B, 0x39, cs}},
            SequenceMode::RequireAll, SequenceMode::RequireAll);
    }
    if (featureId == "HF-GUIDECAR") {
        unsigned char cs = (optVal >= 0 && optVal <= 4) ? static_cast<unsigned char>(optVal) : 0x02;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Guide Me To Car / Welcome Lights Timer", {
            {0x30, 0x3B, 0x08, 0x04, 0x00, cs, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-FOOTLIGHT") {
        unsigned char cs = (optVal >= 0 && optVal <= 2) ? static_cast<unsigned char>(optVal) : 0x00;
        return ExecuteHexSequenceOnBase(39, "Hidden Feature: Footwell Ambient Lighting Mode", {
            {0x30, 0x3B, 0x08, 0x80, 0x00, cs, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-HBA") {
        unsigned char b = (optVal == 1) ? 0x40 : 0x00;
        return ExecuteHexSequenceOnBase(39, std::string("Hidden Feature: High Beam Assist (HBA) -> ") + (optVal ? "ACTIVE" : "DISABLED"), {
            {0x30, 0x36, 0x08, 0x40, b, 0x00, 0x00},
            {0x30, 0x01, 0x08, 0x40, b, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    if (featureId == "HF-ANSWERLIGHT") {
        unsigned char cs = (optVal == 1) ? 0x00 : 0x01;
        return tryCanThenClassic("Hidden Feature: Lock Answerback Flash Type (Hazard vs Interior Light)",
            {{0x30, 0x35, 0x08, 0x80, 0x00, cs, 0x00}},
            {{0x30, 0x10, 0x08, 0x80, 0x00, cs, 0x00}},
            SequenceMode::RequireAll, SequenceMode::RequireAll);
    }
    if (featureId == "HF-AUTOLIGHT-HW") {
        if (optVal == 0) return ExecuteHexSequenceOnBase(63, "BCM Hardware Coding: Auto Light Sensor -> WITH (BCM-103)", {{0x3B, 0x3A, 0x04}}, SequenceMode::RequireAll);
        if (optVal == 1) return ExecuteHexSequenceOnBase(63, "BCM Hardware Coding: Auto Light Sensor -> WITHOUT (BCM-103)", {{0x3B, 0x3A, 0x06}}, SequenceMode::RequireAll);
        if (optVal == 2) return ExecuteHexSequenceOnBase(63, "BCM Hardware Coding: Keyless Entry -> WITH (BCM-103)", {{0x3B, 0x33, 0x00}}, SequenceMode::RequireAll);
        return ExecuteHexSequenceOnBase(63, "BCM Hardware Coding: I-Key Smart Key -> WITH (BCM-103)", {{0x3B, 0x34, 0x01}}, SequenceMode::RequireAll);
    }
    if (featureId == "HF-ROOMLAMP") {
        unsigned char v = static_cast<unsigned char>(optVal);
        return tryCanThenClassic("Hidden Feature: Interior Dome Lamp Fade / Delay Timer (BCM-117 / BCM-118)",
            {},
            {{0x3B, 0x16, v}, {0x3B, 0x17, v}},
            SequenceMode::RequireAll, SequenceMode::TryFirstPositive);
    }
    if (featureId == "HF-RETAINEDPWR") {
        unsigned char v = (optVal >= 0 && optVal <= 2) ? static_cast<unsigned char>(optVal) : 0x00;
        return ExecuteHexSequenceOnBase(63, "Hidden Feature: Retained Power Window Time After IG-OFF (BCM-113)", {
            {0x3B, 0x27, v}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-UDS-IKEY") {
        if (!IsNissanLandyMatched()) {
            ProcedureResult r;
            r.step = "PLATFORM_MISMATCH";
            r.message = "HF-UDS-IKEY (BCM-168..190 / 2E 14 xx) veritabaninda yalnizca Nissan tabanli Landy C27 (Base 169: 0x745/0x765) icindir; standart Suzuki CAN araclara uygulanmaz.";
            return r;
        }
        if (optVal == 0) return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM: I-Key Lock/Unlock ON (BCM-168)", {{0x2E, 0x14, 0x01, 0x00}}, SequenceMode::RequireAll);
        if (optVal == 1) return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM: Door Handle Touch Sensor Unlock ON (BCM-190)", {{0x2E, 0x14, 0x07, 0x01}}, SequenceMode::RequireAll);
        if (optVal == 2) return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM: Answerback I-Key Buzzer ON (BCM-174)", {{0x2E, 0x14, 0x12, 0x00}}, SequenceMode::RequireAll);
        if (optVal == 3) return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM: Answerback I-Key Buzzer OFF (BCM-174)", {{0x2E, 0x14, 0x12, 0x01}}, SequenceMode::RequireAll);
        if (optVal == 4) return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM: Answerback Keyless Buzzer ON (BCM-175)", {{0x2E, 0x14, 0x14, 0x00}}, SequenceMode::RequireAll);
        return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM: Answerback Keyless Buzzer OFF (BCM-175)", {{0x2E, 0x14, 0x14, 0x01}}, SequenceMode::RequireAll);
    }
    if (featureId == "HF-SEATBELT") {
        unsigned char b = (optVal == 1) ? 0x60 : 0x00;
        return ExecuteHexSequenceOnBase(47, std::string("Hidden Feature: Seatbelt Warning Chime -> ") + (optVal ? "ON (Factory)" : "OFF (Muted)"), {
            {0x2E, 0x20, 0x41, 0x00, b}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-CLUSTER-THEME") {
        if (optVal == 1) {
            return ExecuteHexSequenceOnBase(142, "Hidden Feature: Cluster / Display Icon Color Theme (UDS ADM-04)", {
                {0x31, 0x01, 0xC0, 0x11, 0x00}
            }, SequenceMode::RequireAll);
        }
        return ExecuteHexSequenceOnBase(142, "Hidden Feature: Cluster / Display Icon Color Theme (CAN ADM-04)", {
            {0x30, 0x05, 0x07, 0x40, 0x40, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (featureId == "HF-ESP-DYNO") {
        unsigned char st = (optVal == 0) ? 0x04 : 0x00;
        return ExecuteHexSequenceOnBase(34, std::string("Hidden Feature: ESP Function Mode (ABS-04) -> ") + (optVal == 0 ? "DEACTIVATED (Off-Road/Dyno)" : "ACTIVATED (Normal)"), {
            {0x30, 0x80, 0x07, 0x04, st, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }

    ProcedureResult r;
    r.step = "UNKNOWN_FEATURE";
    r.message = "Unknown hidden feature ID: " + featureId;
    return r;
}

inline ProcedureResult ExecuteImmobilizerAction(const std::string& action, const std::string& pinStr, const std::string& seedKeyStr) {
    ProcedureResult res;
    res.step = "IMMO_" + action;

    std::string effProf = ResolveEffectiveProfile();
    SuzukiImmoSystem immoSys = GetActiveImmoSystem(g_activeProfileId);
    res.log.push_back("Aktif Model Mimarisi: " + GetImmoSystemName(immoSys) + " (Etkin profil: " + effProf + ")");

    if (immoSys == SuzukiImmoSystem::UNKNOWN_PROFILE) {
        res.success = false;
        res.step = "UNKNOWN_PROFILE_BLOCKED";
        res.message = "Bilinmeyen arac profili (" + g_activeProfileId + "). Immobilizer komutu guvenlik nedeniyle engellendi.";
        return res;
    }

    // Guvenlik Kilidi 0: Tanimlanmamis 'auto' modunda yazma/silme/PIN/guvenlik komutlari yasaktir
    bool isReadOnlyImmo = (action == "read_status" || action == "read_transponder");
    if (immoSys == SuzukiImmoSystem::UNIVERSAL_AUTO && !isReadOnlyImmo) {
        res.success = false;
        res.step = "UNIDENTIFIED_BLOCKED";
        res.message = "Guvenlik Kilidi: 'Otomatik' modda arac/ECU kimligi henuz dogrulanmadigi icin PIN deneme, anahtar kaydetme, anahtar silme ve UDS guvenlik komutlari engellendi. Once 'Arac Tara' ile kimlik dogrulayin veya manuel profil secin.";
        res.log.push_back("Security Lock: Unidentified 'auto' profile blocked write/security immo action: " + action);
        return res;
    }

    // Guvenlik Kilidi 1: UDS islemleri UDS olmayan araclarda calistirilamaz
    if ((action == "uds_request_seed" || action == "uds_unlock_register") &&
        immoSys != SuzukiImmoSystem::MODERN_CAN_UDS) {
        res.success = false;
        res.message = "Secili arac profili (" + effProf + ") UDS mimarisinde DEGILDIR. Bu aracta klasik 4 haneli PIN (Bolum 2) kullanilmaktadir. UDS Seed-Key yalnizca 2015+ modeller (Vitara LY, S-Cross JY, Swift AZ) icindir.";
        res.log.push_back("Model Compatibility Guard: UDS action rejected for non-UDS profile: " + effProf);
        return res;
    }

    // Guvenlik Kilidi 2: Klasik 4-haneli PIN ve KWP/KLS yazma/test islemleri modern UDS modellerinde calistirilamaz
    if ((action == "verify_pin" || action == "register_key" || action == "register_remote" ||
         action == "delete_all_keys" || action == "ecu_immo_init" || action == "steering_unlock") &&
        immoSys == SuzukiImmoSystem::MODERN_CAN_UDS) {
        res.success = false;
        res.step = "MODEL_NOT_SUPPORTED";
        res.message = "Secili arac profili (Modern CAN UDS) klasik 4 haneli PIN / KWP KLS-BCM rutini (" + action + ") kabul etmez. Lutfen Bolum 3'teki UDS Seed-Key (27 03 / 27 04) rutinini kullanin.";
        res.log.push_back("Model Compatibility Guard: Classic PIN/KLS action (" + action + ") rejected for MODERN_CAN_UDS profile.");
        return res;
    }

    // UDS Seed & Unlock actions do not use 4-digit classic PIN; route strictly to Nissan/Landy C27 Base 169 (0x745/0x765)
    if (action == "uds_request_seed") {
        if (!IsNissanLandyMatched()) {
            res.success = false;
            res.step = "PLATFORM_MISMATCH";
            res.message = "UDS Seed istegi (IMMO-110 / Base 169: 0x745/0x765) veritabaninda yalnizca Nissan tabanli Landy C27 BCM modulu icindir; standart Suzuki CAN araclara gonderilmez.";
            return res;
        }
        std::vector<unsigned char> seedResp;
        auto r1 = ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM Security Seed Request (10 03 -> 27 03 - Base 169)", {
            {0x27, 0x03},
            {0x27, 0x01}
        }, SequenceMode::TryFirstPositive, IsValidUdsSeedResp, &seedResp);
        if (r1.success && IsValidUdsSeedResp(seedResp)) {
            r1.message = "UDS Security Seed dogrulandi (" + HexAddr(seedResp[1], 2) + "): " + HexBytes(seedResp, 2);
        } else {
            r1.success = false;
            if (r1.step.empty() || r1.step == "DONE_CAN") r1.step = "INVALID_UDS_SEED";
            r1.message = "BCM'den gecerli uzunlukta UDS Seed yaniti (67 03 / 67 01 + en az 4 bayt sifirdan farkli seed) alinamadi.";
        }
        return r1;
    }

    if (action == "uds_unlock_register") {
        if (!IsNissanLandyMatched()) {
            res.success = false;
            res.step = "PLATFORM_MISMATCH";
            res.message = "UDS Key / Anahtar kaydi (IMMO-110 / Base 169: 0x745/0x765) veritabaninda yalnizca Nissan tabanli Landy C27 BCM modulu icindir; standart Suzuki CAN araclara gonderilmez.";
            return res;
        }
        std::string keyHex;
        for (char c : seedKeyStr) {
            if (!std::isspace(static_cast<unsigned char>(c))) keyHex.push_back(c);
        }
        std::vector<unsigned char> keyBytes;
        if (!ParseSupportHex(keyHex, keyBytes) || keyBytes.size() < 4) {
            res.step = "INVALID_UDS_KEY";
            res.message = "Gecerli UDS Security Key hex baytlarini (en az 4 bayt, orn: 11 22 33 44) girin.";
            return res;
        }
        std::vector<unsigned char> sendKeyCmd = {0x27, 0x04};
        sendKeyCmd.insert(sendKeyCmd.end(), keyBytes.begin(), keyBytes.end());
        return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS BCM Unlock (27 04) & Key Registration (31 01 01 0D - Base 169)", {
            sendKeyCmd,
            {0x31, 0x01, 0x01, 0x0D},
            {0x31, 0x03, 0x01, 0x0D}
        }, SequenceMode::RequireAll);
    }

    // Read Registered Key Count, Date & Immobilizer Status (21 A1 -> 61 A1 / 22 01 A1 -> 62 01 A1, No PIN required)
    if (action == "read_status") {
        ProcedureResult r1;
        r1.step = "IMMO_read_status";
        std::vector<unsigned char> matchedResp;
        auto tryStatusOnBase = [&](int baseId, const std::string& label, const std::vector<std::vector<unsigned char>>& cmds) {
            if (r1.success) return;
            std::vector<unsigned char> respBuf;
            auto rTry = ExecuteHexSequenceOnBase(baseId, label, cmds, SequenceMode::TryFirstPositive, IsValidImmoKeyStatusResp, &respBuf);
            for (const auto& l : rTry.log) r1.log.push_back(l);
            if (rTry.success && IsValidImmoKeyStatusResp(respBuf)) {
                r1.success = true;
                matchedResp = std::move(respBuf);
            }
        };

        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL) {
            tryStatusOnBase(70, "Grand Vitara 1.9 DDiS Keyless & Immobilizer Key Count (K-Line 0x5D - Base 70)", {{0x21, 0xA1}});
            tryStatusOnBase(46, "Grand Vitara KLS/IMM Key Count (CAN 0x275 / 0x675 - Base 46)", {{0x21, 0xA1}});
        } else if (immoSys == SuzukiImmoSystem::GV_JB_PETROL) {
            tryStatusOnBase(41, "Grand Vitara Petrol ECM Immobilizer Key Count (CAN 0x7E0 - Base 41)", {{0x21, 0xA1}});
            tryStatusOnBase(70, "Grand Vitara KLS Key Count (K-Line 0x5D - Base 70)", {{0x21, 0xA1}});
        } else if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            tryStatusOnBase(36, "Classic Suzuki Immobilizer Key Count (K-Line 0x5B - Base 36)", {{0x21, 0xA1}});
            tryStatusOnBase(64, "Classic Suzuki KWP ECM Immo Key Count (K-Line 0x11 - Base 64)", {{0x21, 0xA1}});
        } else if (immoSys == SuzukiImmoSystem::MODERN_CAN_UDS) {
            tryStatusOnBase(46, "Modern Suzuki KLS/IMM Key Count (CAN 0x275 / 0x675 - Base 46)", {{0x21, 0xA1}, {0x22, 0x01, 0xA1}});
            tryStatusOnBase(39, "Modern Suzuki BCM Key Count (CAN 0x271 / 0x671 - Base 39)", {{0x21, 0xA1}, {0x22, 0x01, 0xA1}});
            if (IsNissanLandyMatched()) {
                tryStatusOnBase(169, "Nissan/Landy UDS BCM Key Count (CAN 0x745 / 0x765 - Base 169)", {{0x22, 0x01, 0xA1}, {0x21, 0xA1}});
            }
        } else {
            tryStatusOnBase(70, "Keyless & Immobilizer Key Count Probe (K-Line 0x5D - Base 70)", {{0x21, 0xA1}});
            tryStatusOnBase(46, "Keyless & Immobilizer Key Count (CAN 0x275 - Base 46)", {{0x21, 0xA1}});
            tryStatusOnBase(41, "ECM Immobilizer Key Count (CAN 0x7E0 - Base 41)", {{0x21, 0xA1}});
            tryStatusOnBase(36, "Alternate Immobilizer Key Count (K-Line 0x5B - Base 36)", {{0x21, 0xA1}});
        }

        if (r1.success) {
            int keyCount = (matchedResp[0] == 0x62 && matchedResp.size() >= 4)
                ? static_cast<int>(matchedResp[3])
                : static_cast<int>(matchedResp[2]);
            r1.message = "Immobilizer key count read verified (" + HexBytes(matchedResp) +
                         ", Registered Keys: " + std::to_string(keyCount) + ").";
        } else {
            r1.message = "Key count response (21 A1 -> 61 A1 / 62 01 A1) not returned by Immobilizer / KLS. Ensure Ignition is ON.";
        }
        return r1;
    }

    // 2. Read Transponder Key ID Code (21 B0 -> validated 61 B0 / 62 01 B0 payload, No PIN required)
    if (action == "read_transponder") {
        ProcedureResult r1;
        r1.step = "IMMO_read_transponder";
        std::vector<unsigned char> matchedResp;
        auto tryTpOnBase = [&](int baseId, const std::string& label, const std::vector<std::vector<unsigned char>>& cmds) {
            if (r1.success) return;
            std::vector<unsigned char> respBuf;
            auto rTry = ExecuteHexSequenceOnBase(baseId, label, cmds, SequenceMode::TryFirstPositive, IsValidTransponderIdResp, &respBuf);
            for (const auto& l : rTry.log) r1.log.push_back(l);
            if (rTry.success && IsValidTransponderIdResp(respBuf)) {
                r1.success = true;
                matchedResp = std::move(respBuf);
            }
        };

        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL) {
            tryTpOnBase(70, "Grand Vitara 1.9 DDiS Transponder ID (K-Line 0x5D KLS - Base 70)", {{0x21, 0xB0}});
            tryTpOnBase(46, "Grand Vitara Transponder ID (CAN 0x275 KLS - Base 46)", {{0x21, 0xB0}});
            if (!r1.success) {
                r1.log.push_back("Mimari Bilgisi: Grand Vitara 1.9 DDiS modelinde transponder anten bobini motor beynine (Bosch EDC16C39) DEGIL, direksiyon altindaki KLS unitesine (K-Line 0x5D) baglidir.");
            }
        } else if (immoSys == SuzukiImmoSystem::GV_JB_PETROL) {
            tryTpOnBase(41, "Grand Vitara Petrol ECM Transponder ID (CAN 0x7E0 - Base 41)", {{0x21, 0xB0}});
            tryTpOnBase(70, "Grand Vitara KLS Transponder ID (K-Line 0x5D - Base 70)", {{0x21, 0xB0}});
        } else if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            tryTpOnBase(36, "Classic Suzuki IMM Transponder ID (K-Line 0x5B - Base 36)", {{0x21, 0xB0}});
            tryTpOnBase(64, "Classic Suzuki KWP ECM Transponder ID (K-Line 0x11 - Base 64)", {{0x21, 0xB0}});
        } else if (immoSys == SuzukiImmoSystem::MODERN_CAN_UDS) {
            tryTpOnBase(46, "Modern Suzuki KLS Transponder Key ID (CAN 0x275 / 0x675 - Base 46)", {{0x21, 0xB0}, {0x22, 0x01, 0xB0}});
            tryTpOnBase(39, "Modern Suzuki BCM Transponder Key ID (CAN 0x271 / 0x671 - Base 39)", {{0x21, 0xB0}, {0x22, 0x01, 0xB0}});
            if (IsNissanLandyMatched()) {
                tryTpOnBase(169, "Nissan/Landy UDS Transponder Key ID (CAN 0x745 / 0x765 - Base 169)", {{0x22, 0x01, 0xB0}, {0x21, 0xB0}});
            }
        } else {
            tryTpOnBase(70, "Transponder Key ID Probe (K-Line 0x5D - Base 70)", {{0x21, 0xB0}});
            tryTpOnBase(46, "Transponder Key ID (CAN 0x275 - Base 46)", {{0x21, 0xB0}});
            tryTpOnBase(41, "Transponder Key ID (Petrol ECM CAN 0x7E0 - Base 41)", {{0x21, 0xB0}});
            tryTpOnBase(36, "Transponder Key ID (K-Line 0x5B - Base 36)", {{0x21, 0xB0}});
        }

        if (r1.success) {
            size_t idOffset = (matchedResp[0] == 0x62) ? 3 : 2;
            size_t idBytes = matchedResp.size() - idOffset;
            r1.message = "Transponder Key ID verified (" + std::to_string(idBytes) + "-byte ID: " +
                         HexBytes(matchedResp, idOffset) + ").";
        } else {
            r1.message = "Valid Transponder Key ID (61 B0 / 62 01 B0) was not returned. Ensure a valid transponder key is inserted and Ignition is ON.";
        }
        return r1;
    }

    // 3. Direct Immobilizer PIN Read on supported ECUs (10 C0 -> 21 21 - IMMO-106)
    if (action == "read_pin") {
        ProcedureResult r1;
        r1.step = "IMMO_read_pin";

        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL || immoSys == SuzukiImmoSystem::GV_JB_PETROL) {
            r1 = ExecuteHexSequenceOnBase(70, "Grand Vitara KLS PIN Read (K-Line 0x5D - Base 70)", {
                {0x21, 0x21}
            }, SequenceMode::RequireAll);
            if (!r1.success && r1.step == "KLINE_NO_RESP") {
                auto r39 = ExecuteHexSequenceOnBase(39, "Grand Vitara BCM PIN Read (Base 39)", {
                    {0x21, 0x21}
                }, SequenceMode::RequireAll);
                for (const auto& l : r39.log) r1.log.push_back(l);
                if (r39.success) r1.success = true;
            }
        } else if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            r1 = ExecuteHexSequenceOnBase(36, "Classic Suzuki IMM PIN Read (K-Line 0x5B - Base 36)", {
                {0x21, 0x21}
            }, SequenceMode::RequireAll);
        } else {
            r1.success = false;
            r1.message = "Bu model mimarisinde dogrudan 21 21 PIN okuma desteklenmemektedir.";
            return r1;
        }

        r1.message = r1.success
            ? "PIN Read (21 21 -> 61 21) completed. Check response bytes in log."
            : "This ECU variant does not support direct unauthenticated 21 21 PIN reading (use known factory PIN 0895).";
        return r1;
    }

    // 4. Keyless Interior Antenna Self-Test (GV JB KLS Base 70 / Modern UDS BCM Base 169)
    if (action == "antenna_test") {
        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL || immoSys == SuzukiImmoSystem::GV_JB_PETROL) {
            return ExecuteHexSequenceOnBase(70, "Keyless Interior Antenna Diagnosis on KLS (K-Line 0x5D - Base 70)", {
                {0x2F, 0x01, 0x06, 0x03, 0x00, 0x05},
                {0x2F, 0x01, 0x06, 0x03, 0x01}
            }, SequenceMode::TryFirstPositive);
        }
        if (immoSys == SuzukiImmoSystem::MODERN_CAN_UDS) {
            if (IsNissanLandyMatched()) {
                return ExecuteHexSequenceOnBase(169, "Nissan/Landy UDS Keyless Antenna Diagnosis (0x745/0x765 BCM-167: 2F 01 06)", {
                    {0x2F, 0x01, 0x06, 0x03, 0x00, 0x05},
                    {0x2F, 0x01, 0x06, 0x03, 0x01}
                }, SequenceMode::TryFirstPositive);
            }
            return ExecuteHexSequenceOnBase(46, "Modern Suzuki CAN Keyless Antenna Diagnosis (0x275 KLS - Base 46)", {
                {0x2F, 0x01, 0x06, 0x03, 0x00, 0x05},
                {0x2F, 0x01, 0x06, 0x03, 0x01}
            }, SequenceMode::TryFirstPositive);
        }
        res.success = false;
        res.step = "MODEL_NOT_SUPPORTED";
        res.message = "Ic mekan keyless anten testi (2F 01 06) secili arac mimarisi (" + GetImmoSystemName(immoSys) + ") ile uyumlu degildir (Klasik K-Line araclar icin kapali).";
        res.log.push_back("Model Compatibility Guard: antenna_test rejected for architecture: " + effProf);
        return res;
    }

    unsigned char pinHi = 0, pinLo = 0, bcdHi = 0, bcdLo = 0;
    std::string pinErr;
    if (!ParseStrictImmoPin4(pinStr, pinHi, pinLo, bcdHi, bcdLo, pinErr)) {
        res.success = false;
        res.step = "INVALID_PIN";
        res.message = pinErr;
        res.log.push_back("PIN Validation Error: " + pinErr);
        return res;
    }

    std::vector<std::vector<unsigned char>> pinVariants = {
        {0x30, 0xA0, 0x07, 0x80, 0x80, pinHi, pinLo},
        {0x30, 0xA0, 0x07, 0x80, 0x80, bcdHi, bcdLo},
        {0xA5, 0xA0, 0x80, 0x00, pinHi, pinLo},
        {0xA5, 0xA0, 0x80, 0x00, bcdHi, bcdLo}
    };

    // 5. Verify 4-Digit PIN Only
    if (action == "verify_pin") {
        ProcedureResult r1;
        r1.step = "IMMO_verify_pin";

        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL) {
            r1 = ExecuteHexSequenceOnBase(70, "Grand Vitara 1.9 DDiS PIN Dogrulama (K-Line 0x5D KLS - Base 70)", pinVariants, SequenceMode::TryFirstPositive);
            if (!r1.success && r1.step == "KLINE_NO_RESP") {
                auto r46 = ExecuteHexSequenceOnBase(46, "Grand Vitara PIN Dogrulama (CAN 0x275 KLS - Base 46)", pinVariants, SequenceMode::TryFirstPositive);
                for (const auto& l : r46.log) r1.log.push_back(l);
                if (r46.success) r1.success = true;
            }
        } else if (immoSys == SuzukiImmoSystem::GV_JB_PETROL) {
            r1 = ExecuteHexSequenceOnBase(41, "Grand Vitara Petrol ECM PIN Dogrulama (CAN 0x7E0 - Base 41)", pinVariants, SequenceMode::TryFirstPositive);
            if (!r1.success && r1.step == "CAN_NO_RESPONSE") {
                auto r70 = ExecuteHexSequenceOnBase(70, "Grand Vitara KLS PIN Dogrulama (K-Line 0x5D - Base 70)", pinVariants, SequenceMode::TryFirstPositive);
                for (const auto& l : r70.log) r1.log.push_back(l);
                if (r70.success) r1.success = true;
            }
        } else if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            r1 = ExecuteHexSequenceOnBase(36, "Classic Suzuki PIN Dogrulama (K-Line 0x5B - Base 36)", pinVariants, SequenceMode::TryFirstPositive);
        }

        r1.message = r1.success
            ? "PIN ONAYLANDI! Beyin (" + pinStr + ") PIN kodunu kabul etti. Anahtar kodlama islemine guvenle gecebilirsiniz."
            : "PIN reddedildi veya secili arac beyni bu PIN'i tanimadi (Logdaki 7F negatif yaniti kontrol edin).";
        return r1;
    }

    auto runImmoKeyOpWithCountCheck = [&](int baseId, const std::string& opLabel, bool isErase,
                                          const std::vector<std::vector<unsigned char>>& stage2Cmds) -> ProcedureResult {
        std::vector<unsigned char> preResp, postResp;
        ProcedureResult r = ExecuteStagedSequenceOnBase(
            baseId, opLabel, pinVariants, stage2Cmds, {{0x21, 0xA1}},
            IsValidImmoKeyStatusResp, &postResp,
            {0x21, 0xA1}, IsValidImmoKeyStatusResp, &preResp);
        if (!r.success) return r;
        int preCount = ExtractImmoKeyCount(preResp);
        int postCount = ExtractImmoKeyCount(postResp);
        std::string countReason;
        if (!VerifyImmoKeyCountTransition(isErase, preCount, postCount, countReason)) {
            r.success = false;
            r.step = isErase ? "IMMO_ERASE_COUNT_NOT_ZERO" : "IMMO_REGISTER_COUNT_UNCHANGED";
            r.message = opLabel + ": " + countReason;
            r.log.push_back("Key Count Check FAILED: " + countReason);
            return r;
        }
        r.log.push_back("Key Count Check OK: " + countReason);
        r.message = opLabel + " basariyla tamamlandi (" + countReason + ").";
        return r;
    };

    // 6. Register Transponder Key (Pre 21 A1 count -> PIN Auth -> Register -> Post 21 A1 count > Pre)
    if (action == "register_key") {
        std::vector<std::vector<unsigned char>> regCmdsKls = {
            {0x30, 0xA1, 0x08, 0x01, 0x01, 0x00, 0x00},
            {0xA5, 0xA1, 0x01, 0x00, 0x00, 0x00},
            {0x31, 0xA1, 0x00}
        };
        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL) {
            return runImmoKeyOpWithCountCheck(70, "Grand Vitara 1.9 DDiS Anahtar Kodlama (K-Line 0x5D KLS - Base 70)", false, regCmdsKls);
        }
        if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            return runImmoKeyOpWithCountCheck(36, "Classic Suzuki Anahtar Kodlama (K-Line 0x5B - Base 36)", false, regCmdsKls);
        }
        // GV_JB_PETROL: KLS 0x5D or ECM Base 41
        auto r1 = runImmoKeyOpWithCountCheck(70, "Grand Vitara KLS Anahtar Kodlama (K-Line 0x5D - Base 70)", false, regCmdsKls);
        if (!r1.success && r1.step == "KLINE_NO_RESP") {
            std::vector<std::vector<unsigned char>> regCmdsEcm = {
                {0x30, 0xA1, 0x08, 0x40, 0x40, 0x1A, 0x0B},
                {0xA5, 0xA1, 0x40, 0x00, 0x1A, 0x0B},
                {0x31, 0xA1, 0x00}
            };
            auto r41 = runImmoKeyOpWithCountCheck(41, "Grand Vitara ECM Anahtar Kodlama (CAN 0x7E0 - Base 41)", false, regCmdsEcm);
            for (const auto& l : r1.log) r41.log.insert(r41.log.begin(), l);
            return r41;
        }
        return r1;
    }

    // 7. Register Remote Controller Fob (Stage 1 PIN Auth -> Stage 2 Remote Reg; GV JB & Classic K-Line only)
    if (action == "register_remote") {
        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL || immoSys == SuzukiImmoSystem::GV_JB_PETROL) {
            auto r1 = ExecuteStagedSequenceOnBase(70, "Grand Vitara Keyless Kumanda Kodlama (K-Line 0x5D KLS - Base 70)",
                pinVariants,
                {{0xA5, 0xA1, 0x20, 0x00, 0x00, 0x00}, {0x30, 0xA1, 0x08, 0x01, 0x01, 0x00, 0x00}, {0xA5, 0xA1, 0x01, 0x00, 0x00, 0x00}},
                {});
            if (!r1.success && r1.step == "KLINE_NO_RESP") {
                auto r63 = ExecuteStagedSequenceOnBase(63, "Grand Vitara BCM Kumanda Kodlama (K-Line 0x5A - Base 63)",
                    pinVariants,
                    {{0xA5, 0xA1, 0x20, 0x00, 0x00, 0x00}, {0x30, 0x62, 0x07}, {0x30, 0x04, 0x08}},
                    {});
                for (const auto& l : r1.log) r63.log.insert(r63.log.begin(), l);
                return r63;
            }
            return r1;
        }
        if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            return ExecuteStagedSequenceOnBase(63, "Classic Suzuki Kumanda Kodlama (K-Line 0x5A BCM - Base 63)",
                pinVariants,
                {{0x30, 0x62, 0x07}, {0x30, 0x04, 0x08}},
                {});
        }
        res.success = false;
        res.step = "MODEL_NOT_SUPPORTED";
        res.message = "Klasik PIN tabanli kumanda kodlama bu arac mimarisinde (" + GetImmoSystemName(immoSys) + ") desteklenmemektedir.";
        return res;
    }

    // 8. ECM <-> Immobilizer Control Unit Initialization / Pairing (IMMO-101; non-UDS Petrol / Classic ECM)
    if (action == "ecu_immo_init") {
        if (immoSys != SuzukiImmoSystem::GV_JB_PETROL && immoSys != SuzukiImmoSystem::CLASSIC_KLINE) {
            res.success = false;
            res.step = "MODEL_NOT_SUPPORTED";
            res.message = "ECM-IMMO eslestirme (Base 41) yalnizca Benzinli Grand Vitara veya Klasik K-Line mimarisinde desteklenir.";
            return res;
        }
        return ExecuteHexSequenceOnBase(41, "ECM / Immobilizer Control Unit Pairing & Initialization (IMMO-101: 31 01)", {
            {0x31, 0x01, 0x00, bcdHi, bcdLo},
            {0x31, 0x01, 0x01},
            {0x31, 0x01, bcdHi, bcdLo},
            {0x32, 0x01},
            {0x33, 0x01}
        }, SequenceMode::TryFirstPositive);
    }

    // 9. Electronic Steering Column Lock (ESL) Unlock (IMMO-109: 31 0D 00 <PIN>)
    if (action == "steering_unlock") {
        if (immoSys != SuzukiImmoSystem::GV_JB419D_DIESEL &&
            immoSys != SuzukiImmoSystem::GV_JB_PETROL &&
            immoSys != SuzukiImmoSystem::CLASSIC_KLINE) {
            res.success = false;
            res.step = "MODEL_NOT_SUPPORTED";
            res.message = "Klasik ESL direksiyon kilidi acma komutu bu arac mimarisinde desteklenmemektedir.";
            return res;
        }
        int base = (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) ? 36 : 70;
        return ExecuteHexSequenceOnBase(base, "Electronic Steering Lock (ESL) Unlock Request (IMMO-109: 31 0D 00)", {
            {0x31, 0x0D, 0x00, bcdHi, bcdLo},
            {0x31, 0x0D, 0x00, pinHi, pinLo},
            {0x31, 0x0D, 0x01}
        }, SequenceMode::TryFirstPositive);
    }

    // 12. Delete All Registered Key Codes (Pre 21 A1 count -> PIN Auth -> Erase -> Post 21 A1 count == 0)
    if (action == "delete_all_keys") {
        std::vector<std::vector<unsigned char>> eraseCmds = {
            {0x30, 0xA1, 0x08, 0x08, 0x08, 0x00, 0x00},
            {0xA5, 0xA1, 0x08, 0x00, 0x00, 0x00}
        };
        if (immoSys == SuzukiImmoSystem::GV_JB419D_DIESEL) {
            return runImmoKeyOpWithCountCheck(70, "Grand Vitara 1.9 DDiS Tum Anahtarlari Sil (K-Line 0x5D KLS - Base 70)", true, eraseCmds);
        }
        if (immoSys == SuzukiImmoSystem::CLASSIC_KLINE) {
            return runImmoKeyOpWithCountCheck(36, "Classic Suzuki Tum Anahtarlari Sil (K-Line 0x5B - Base 36)", true, eraseCmds);
        }
        // GV_JB_PETROL
        auto r1 = runImmoKeyOpWithCountCheck(70, "Grand Vitara KLS Tum Anahtarlari Sil (K-Line 0x5D - Base 70)", true, eraseCmds);
        if (!r1.success && r1.step == "KLINE_NO_RESP") {
            auto r41 = runImmoKeyOpWithCountCheck(41, "Erase All Registered Keys on ECM (CAN 0x7E0 - Base 41)", true, eraseCmds);
            for (const auto& l : r1.log) r41.log.insert(r41.log.begin(), l);
            return r41;
        }
        return r1;
    }

    res.message = "Unknown Immobilizer action: " + action;
    return res;
}

inline bool ParseInjectorIma21AdResponse(const std::vector<unsigned char>& resp,
                                         std::string& outC1, std::string& outC2,
                                         std::string& outC3, std::string& outC4) {
    if (resp.size() < 26 || resp[0] != 0x61 || resp[1] != 0xAD) return false;
    size_t startIdx = (resp.size() >= 27 && (resp[2] == 0x20 || resp[2] == 0xFF)) ? 3 : 2;
    size_t perCylLen = 0;
    if (resp.size() >= startIdx + 28) perCylLen = 7;
    else if (resp.size() >= startIdx + 24) perCylLen = 6;
    else return false;

    auto extractClean = [&](size_t off) {
        std::string s;
        for (size_t i = 0; i < perCylLen && (off + i) < resp.size(); ++i) {
            unsigned char b = resp[off + i];
            if (std::isalnum(b)) s.push_back(static_cast<char>(std::toupper(b)));
        }
        return s;
    };
    std::string c1 = extractClean(startIdx);
    std::string c2 = extractClean(startIdx + perCylLen);
    std::string c3 = extractClean(startIdx + perCylLen * 2);
    std::string c4 = extractClean(startIdx + perCylLen * 3);
    if (c1.size() < 6 || c2.size() < 6 || c3.size() < 6 || c4.size() < 6) return false;
    outC1 = std::move(c1);
    outC2 = std::move(c2);
    outC3 = std::move(c3);
    outC4 = std::move(c4);
    return true;
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
        log.push_back("Reading Injector IMA Calibration Codes on CAN ECM (0x7E0 -> 10 C0, 21 AD / 21 54)...");
        std::vector<std::vector<unsigned char>> canImaReqs = {{0x10, 0xC0}, {0x21, 0xAD}};
        std::vector<std::vector<unsigned char>> canImaResps;
        if (SendBatch(g_diag, 0x7E0, 0x7E8, canImaReqs, canImaResps, 600) && canImaResps.size() >= 2 &&
            ParseInjectorIma21AdResponse(canImaResps[1], cyl1, cyl2, cyl3, cyl4)) {
            ok = true;
            log.push_back("RX [61 AD]: " + HexBytes(canImaResps[1]));
            message = "Enjektör IMA kodları (21 AD) ECM üzerinden okundu ve doğrulandı (" + cyl1 + " / " + cyl2 + " / " + cyl3 + " / " + cyl4 + ").";
        } else {
            if (canImaResps.size() >= 2 && !canImaResps[1].empty()) {
                log.push_back("CAN 21 AD gecersiz/eksik yanit: " + HexBytes(canImaResps[1]));
            }
            std::vector<std::vector<unsigned char>> can54Reqs = {{0x10, 0xC0}, {0x21, 0x54}};
            std::vector<std::vector<unsigned char>> can54Resps;
            if (SendBatch(g_diag, 0x7E0, 0x7E8, can54Reqs, can54Resps, 600) && can54Resps.size() >= 2 &&
                can54Resps[1].size() >= 26 && can54Resps[1][0] == 0x61 && can54Resps[1][1] == 0x54) {
                std::vector<unsigned char> synthetic = can54Resps[1];
                synthetic[1] = 0xAD;
                if (ParseInjectorIma21AdResponse(synthetic, cyl1, cyl2, cyl3, cyl4)) {
                    ok = true;
                    log.push_back("RX [61 54]: " + HexBytes(can54Resps[1]));
                    message = "CAN Dizel Enjektör IMA kodları (21 54) okundu ve ayrıştırıldı.";
                }
            }
            if (!ok) {
                log.push_back("CAN 0x7E0 uzerinde gecerli IMA yaniti alinamadi; K-Line ECM (0x7A -> 21 AD) deneniyor...");
                std::vector<std::vector<unsigned char>> kReqs = {{0x10, 0xC0}, {0x21, 0xAD}};
                std::vector<std::vector<unsigned char>> kResps;
                if (g_diag.RunKLineSession(0x7A, kReqs, kResps, 500) && kResps.size() >= 2 &&
                    ParseInjectorIma21AdResponse(kResps[1], cyl1, cyl2, cyl3, cyl4)) {
                    ok = true;
                    log.push_back("K-Line 0x7A RX: " + HexBytes(kResps[1]));
                    message = "Enjektör IMA kodları (K-Line 0x7A -> 21 AD) okundu ve doğrulandı (" + cyl1 + " / " + cyl2 + " / " + cyl3 + " / " + cyl4 + ").";
                } else {
                    message = "Geçerli Enjektör IMA kodları (min. 24-28 bayt veri) okunamadı (21 AD / 21 54).";
                }
            }
        }
    } else if (action == "write") {
        std::string effProf = ResolveEffectiveProfile();
        if (effProf != "gv_jb419d") {
            message = "Guvenlik Kilidi: Enjektor IMA yazma (DSL-01 F9Q) yalnizca Grand Vitara 1.9 DDiS (F9Q Base 93) profili/kimligi dogrulandiginda aktiftir (Etkin profil: " + effProf + ").";
            log.push_back(message);
        } else {
            cyl1 = cleanCode(cyl1);
            cyl2 = cleanCode(cyl2);
            cyl3 = cleanCode(cyl3);
            cyl4 = cleanCode(cyl4);
            auto validLen = [](const std::string& s) { return s.size() >= 6 && s.size() <= 7; };
            if (!validLen(cyl1) || !validLen(cyl2) || !validLen(cyl3) || !validLen(cyl4)) {
                message = "Grand Vitara 1.9 DDiS (F9Q Base 93) her silindir IMA kodu kesinlikle 6 veya 7 alfanumerik karakter olmalidir (8+ karakterli kodlar sessizce kirpilmaz, reddedilir).";
            } else {
                auto pad7 = [](std::string s) {
                    while (s.size() < 7) s.push_back(' ');
                    return s;
                };
                std::string all28 = pad7(cyl1) + pad7(cyl2) + pad7(cyl3) + pad7(cyl4);
                std::vector<unsigned char> writeCmd = {0x3B, 0xAD, 0xFF};
                for (char ch : all28) writeCmd.push_back(static_cast<unsigned char>(ch));

                // Strict Stage 1: BOTH 3B AD FF write AND 3B 7E FE ZFC reset MUST succeed before any 21 AD read-back!
                auto prWrite = ExecuteHexSequenceOnBase(93, "Write Injector IMA Codes (3B AD FF) & Reset ZFC (3B 7E FE)", {
                    writeCmd,
                    {0x3B, 0x7E, 0xFE}
                }, SequenceMode::RequireAll);
                for (const auto& l : prWrite.log) log.push_back(l);
                if (!prWrite.success) {
                    ok = false;
                    message = "IMA yazma veya ZFC sifirlama basarisiz oldu (" + prWrite.message + "); okuma dogrulamasina gecilmedi.";
                } else {
                    std::vector<unsigned char> readBackResp;
                    auto prVerify = ExecuteHexSequenceOnBase(93, "Verify Injector IMA Codes Read-back (21 AD)", {
                        {0x21, 0xAD}
                    }, SequenceMode::RequireAll, nullptr, &readBackResp);
                    for (const auto& l : prVerify.log) log.push_back(l);
                    std::string rb1, rb2, rb3, rb4;
                    if (!prVerify.success || !ParseInjectorIma21AdResponse(readBackResp, rb1, rb2, rb3, rb4)) {
                        ok = false;
                        message = "IMA ve ZFC yazildi ancak 21 AD geri okuma verisi gecersiz/eksik; yazilan kodlar dogrulanamadi.";
                    } else if (rb1 != cyl1 || rb2 != cyl2 || rb3 != cyl3 || rb4 != cyl4) {
                        ok = false;
                        message = "21 AD geri okumasi yazilan IMA kodlariyla ESMESMEDI! Yazilan=[" +
                                  cyl1 + "," + cyl2 + "," + cyl3 + "," + cyl4 + "] | Okunan=[" +
                                  rb1 + "," + rb2 + "," + rb3 + "," + rb4 + "]";
                    } else {
                        ok = true;
                        message = "Enjektor IMA kodlari yazildi, ZFC sifirlandi ve 21 AD geri okumasi birebir dogrulandi (" +
                                  rb1 + " / " + rb2 + " / " + rb3 + " / " + rb4 + ")!";
                    }
                }
            }
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
        log.push_back("Reading VIN from ECM (CAN 0x7E0 -> 10 C0, then 1A 90 / 22 F1 90 / 21 90 / 09 02)...");
        std::vector<std::vector<unsigned char>> canVinReqs = {
            {0x10, 0xC0},
            {0x1A, 0x90},
            {0x21, 0x90},
            {0x22, 0xF1, 0x90}
        };
        std::vector<std::vector<unsigned char>> canVinResps;
        if (SendBatch(g_diag, 0x7E0, 0x7E8, canVinReqs, canVinResps, 600)) {
            for (size_t i = 1; i < canVinResps.size(); ++i) {
                const auto& kr = canVinResps[i];
                if (kr.size() >= 19 && (kr[0] == 0x5A || kr[0] == 0x61 || kr[0] == 0x62)) {
                    vin = extractAsciiVin(kr, (kr[0] == 0x62) ? 3 : 2);
                    if (vin.size() >= 17) {
                        ok = true;
                        log.push_back("CAN ECM RX: " + HexBytes(kr) + " -> VIN: " + vin);
                        break;
                    }
                }
            }
        }
        if (!ok) {
            const unsigned long obdTxIds[2] = {0x7DF, 0x7E0};
            for (unsigned long obdTx : obdTxIds) {
                CanChannelScope obdScope(g_diag, obdTx, 0x7E8);
                if (obdScope.open && obdScope.Transact({0x09, 0x02}, resp, 500) &&
                    resp.size() >= 19 && resp[0] == 0x49 && resp[1] == 0x02) {
                    vin = extractAsciiVin(resp, 3);
                    if (vin.size() >= 17) {
                        ok = true;
                        log.push_back("OBD-II Mode 09 PID 02 RX: " + HexBytes(resp) + " -> VIN: " + vin);
                        break;
                    }
                }
            }
        }
        if (!ok) {
            log.push_back("CAN 0x7E0 no VIN response; probing K-Line ECM (0x11 / 0x7A -> 10 C0, 1A 90 / 21 90)...");
            std::vector<std::vector<unsigned char>> kReqs = {{0x10, 0xC0}, {0x1A, 0x90}, {0x21, 0x90}};
            std::vector<std::vector<unsigned char>> kResps;
            if (g_diag.RunKLineSession(0x11, kReqs, kResps, 450) || g_diag.RunKLineSession(0x7A, kReqs, kResps, 450)) {
                for (const auto& kr : kResps) {
                    if (kr.size() >= 19 && (kr[0] == 0x5A || kr[0] == 0x61)) {
                        vin = extractAsciiVin(kr, 2);
                        if (vin.size() >= 17) { ok = true; log.push_back("K-Line ECM RX: " + HexBytes(kr) + " -> VIN: " + vin); break; }
                    }
                }
            }
        }
        if (!ok) {
            log.push_back("Probing K-Line BCM (0x5A) & IMM (0x5D)...");
            std::vector<std::vector<unsigned char>> bcmReqs = {{0x21, 0x80}, {0x1A, 0x90}};
            std::vector<std::vector<unsigned char>> bcmResps;
            if (g_diag.RunKLineSession(0x5A, bcmReqs, bcmResps, 400)) {
                for (const auto& kr : bcmResps) {
                    if (kr.size() >= 19 && (kr[0] == 0x5A || kr[0] == 0x61)) {
                        std::string v = extractAsciiVin(kr, 2);
                        if (v.size() >= 17) { vin = v; ok = true; log.push_back("BCM RX -> VIN: " + vin); break; }
                    }
                }
            }
            if (!ok && g_diag.RunKLineSession(0x5D, bcmReqs, bcmResps, 400)) {
                for (const auto& kr : bcmResps) {
                    if (kr.size() >= 19 && (kr[0] == 0x5A || kr[0] == 0x61)) {
                        std::string v = extractAsciiVin(kr, 2);
                        if (v.size() >= 17) { vin = v; ok = true; log.push_back("IMM RX -> VIN: " + vin); break; }
                    }
                }
            }
        }
        if (ok && !vin.empty()) {
            g_cachedVin = vin;
        }
        message = ok ? ("VIN Read Successful: " + vin) : "Could not read VIN (1A 90 / 22 F1 90 / 09 02). Ensure Ignition is ON.";
    } else if (action == "write") {
        std::string effProf = ResolveEffectiveProfile();
        std::string clean;
        for (char ch : vinInput) {
            if (std::isalnum(static_cast<unsigned char>(ch))) {
                clean.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            }
        }
        if (!IsKnownVehicleProfile(effProf)) {
            message = "Guvenlik Kilidi: Arac profili / ECU kimligi dogrulanmadan VIN yazilamaz (Etkin profil: " + effProf + "). Once 'Arac Tara' yapin veya model secin.";
            log.push_back(message);
        } else if (effProf == "gv_jb419d") {
            message = "Grand Vitara 1.9 DDiS (F9Q Base 93 Bosch EDC16C39) SDT-II PWTR-02 VIN yazma proseduru desteklememektedir (supports_vin_registration = false).";
            log.push_back(message);
        } else if (clean.size() != 17) {
            message = "VIN must be exactly 17 alphanumeric characters (entered: " + std::to_string(clean.size()) + ").";
        } else {
            vin = clean;
            std::vector<unsigned char> kwpWrite = {0x3B, 0x90};
            std::vector<unsigned char> udsWrite = {0x2E, 0xF1, 0x90};
            for (char ch : vin) {
                kwpWrite.push_back(static_cast<unsigned char>(ch));
                udsWrite.push_back(static_cast<unsigned char>(ch));
            }
            bool useUds = (GetMatchedEcuBaseId() > 0 && IsUdsBase(GetMatchedEcuBaseId()));
            std::vector<std::vector<unsigned char>> unlockCmds = {
                {0x30, 0x12, 0x08, 0x80, 0x80, 0x00, 0x00},
                {0x30, 0x76, 0x08, 0x80, 0x80, 0x00, 0x00},
                {0xA5, 0x0F, 0x40, 0x00, 0x00, 0x00}
            };
            if (effProf == "suzuki_kline" || useUds) {
                std::vector<unsigned char> verifyRx;
                ProcedureResult pr = effProf == "suzuki_kline"
                    ? ExecuteStagedSequenceOnBase(64, "K-Line ECM VIN Registration (PWTR-02: 3B 90)",
                        {}, {kwpWrite}, {{0x1A, 0x90}}, nullptr, &verifyRx)
                    : ExecuteStagedSequenceOnBase(194, "UDS ECM VIN Registration (2E F1 90 -> 22 F1 90)",
                        {}, {udsWrite}, {{0x22, 0xF1, 0x90}}, nullptr, &verifyRx);
                for (const auto& l : pr.log) log.push_back(l);
                std::string readBack;
                if (pr.success && verifyRx.size() >= 19) {
                    readBack = extractAsciiVin(verifyRx, (verifyRx[0] == 0x62) ? 3 : 2);
                }
                if (pr.success && readBack == vin) {
                    ok = true;
                    g_cachedVin = vin;
                    message = pr.message + " (Geri okuma dogrulandi: " + readBack + ")";
                } else {
                    ok = false;
                    message = pr.success
                        ? ("VIN yazildi ancak geri okunan VIN eslesmedi (Okunan: " + (readBack.empty() ? "EKSIK/YOK" : readBack) + ").")
                        : pr.message;
                }
            } else {
                // Non-UDS CAN Petrol ECM (Base 41): unlock -> write 3B 90 MUST succeed -> then read back 1A 90
                CanChannelScope canScope(g_diag, 0x7E0, 0x7E8);
                std::vector<unsigned char> resp;
                if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 350) || resp.empty() || resp[0] != 0x50) {
                    ok = false;
                    message = "CAN ECM 10 C0 oturumu acilamadi (" + (resp.empty() ? "YOK" : HexBytes(resp)) + "); VIN yazma durduruldu.";
                    log.push_back(message);
                } else {
                    log.push_back("CAN Oturum acildi: " + HexBytes(resp));
                    for (const auto& uc : unlockCmds) {
                        std::vector<unsigned char> rx;
                        bool uOk = canScope.Transact(uc, rx, 500);
                        log.push_back("TX [Unlock: " + HexBytes(uc) + "] -> RX [" + (rx.empty() ? "YOK" : HexBytes(rx)) + "]");
                        if (uOk && !rx.empty() && rx[0] == static_cast<unsigned char>(uc[0] + 0x40)) break;
                    }
                    std::vector<unsigned char> wRx;
                    bool wOk = canScope.Transact(kwpWrite, wRx, 600);
                    log.push_back("TX [Write 3B 90] -> RX [" + (wRx.empty() ? "YOK" : HexBytes(wRx)) + "]");
                    if (!wOk || wRx.size() < 2 || wRx[0] != 0x7B || wRx[1] != 0x90) {
                        ok = false;
                        message = "VIN yazma komutu (3B 90) ECU tarafindan reddedildi (" + (wRx.empty() ? "YOK" : HexBytes(wRx)) + ").";
                    } else {
                        std::vector<unsigned char> vRx;
                        bool vOk = canScope.Transact({0x1A, 0x90}, vRx, 500);
                        log.push_back("TX [Verify 1A 90] -> RX [" + (vRx.empty() ? "YOK" : HexBytes(vRx)) + "]");
                        std::string readBack = (vOk && vRx.size() >= 19 && vRx[0] == 0x5A) ? extractAsciiVin(vRx, 2) : "";
                        if (readBack == vin) {
                            ok = true;
                            g_cachedVin = vin;
                            message = "VIN yazildi (7B 90) ve 1A 90 geri okumasi ile dogrulandi: " + vin;
                        } else {
                            ok = false;
                            message = "3B 90 yanit verdi ancak 1A 90 dogrulamasi yazilan VIN ile eslesmedi (Okunan: " + (readBack.empty() ? "YOK" : readBack) + ").";
                        }
                    }
                    canScope.Transact({0x10, 0x81}, resp, 200);
                }
            }
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
    return ExecuteHexSequenceOnBase(34, "EPB Electronic Parking Brake Service Reset & Pad Replacement Mode (EPB-01)", {
        {0xA6, 0x00},
        {0x30, 0x01, 0x07, 0x80, 0x80, 0x00, 0x00},
        {0x31, 0x01, 0x02, 0x00}
    }, SequenceMode::TryFirstPositive);
}

inline ProcedureResult RunHvacCalibProcedure() {
    return ExecuteHexSequenceOnBase(35, "HVAC Climate Air Mix Flap Actuator & Compressor Initialization (HVAC-01)", {
        {0x30, 0x01, 0x08, 0x80, 0x80, 0x00, 0x00},
        {0x31, 0x01, 0x00}
    }, SequenceMode::TryFirstPositive);
}

inline ProcedureResult RunShvsHybridProcedure() {
    return ExecuteHexSequenceOnBase(41, "SHVS Mild-Hybrid / HVBMU Cell IC & ISG Starter Counter Reset (HVBMU-01)", {
        {0x30, 0x01, 0x07, 0x80, 0x80, 0x00, 0x00},
        {0x31, 0x01, 0xC0, 0x10, 0x00}
    }, SequenceMode::TryFirstPositive);
}

inline ProcedureResult RunAdasCalibProcedure() {
    std::string effProf = ResolveEffectiveProfile();
    if (!IsServiceSupportedForProfile("adas_calib", effProf)) {
        ProcedureResult r;
        r.step = (effProf == "UNIDENTIFIED") ? "VEHICLE_UNIDENTIFIED" : "MODEL_NOT_SUPPORTED";
        r.message = (effProf == "UNIDENTIFIED")
            ? "Arac kimligi dogrulanmadan ADAS/BSM/ADM kalibrasyonu baslatilamaz. Once 'Arac Tara' calistirin."
            : "Secili arac profili (" + effProf + ") CAN ADAS / BSM / ADM kalibrasyonunu desteklemiyor.";
        return r;
    }
    int matchedBase = GetMatchedEcuBaseId();
    if (IsNissanLandyMatched()) {
        if (matchedBase == 163 || matchedBase == 164) {
            return ExecuteHexSequenceOnBase(matchedBase, "Nissan/Landy UDS ADAS Control Unit Setup (ADAS-116)", {
                {0x31, 0x01, 0x05, 0x50},
                {0x31, 0x03, 0x05, 0x50}
            }, SequenceMode::RequireAll);
        }
        if (matchedBase == 132) {
            return ExecuteHexSequenceOnBase(132, "Nissan/Landy CAN ADAS Configuration (ADAS-106)", {
                {0x31, 0x01, 0x00},
                {0x31, 0x01, 0x01}
            }, SequenceMode::TryFirstPositive);
        }
        ProcedureResult r;
        r.step = "ADAS_ECU_NOT_MATCHED";
        r.message = "Landy platformunda ADAS/Camera modulu (Base 132/163/164) kimligi dogrulanmadi; BCM Base 169'a ADAS komutu gonderilmez.";
        return r;
    }
    if (matchedBase == 191) {
        return ExecuteHexSequenceOnBase(191, "Suzuki UDS BSM Blind Spot Aiming Calibration (0x77A/0x78A - Base 191)", {
            {0x31, 0x01, 0x03, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (matchedBase == 142 || matchedBase == 186) {
        return ExecuteHexSequenceOnBase(matchedBase, "Suzuki CAN/UDS ADM Display/Camera Calibration (0x79B/0x7AB)", {
            {0x30, 0x01, 0x07, 0x80, 0x80, 0x00, 0x00}
        }, SequenceMode::RequireAll);
    }
    if (matchedBase == 143) {
        return ExecuteHexSequenceOnBase(143, "Suzuki CAN SCAM Front Camera Calibration (0x777/0x787 - Base 143)", {
            {0x31, 0x01, 0xC0, 0x01, 0x00},
            {0x30, 0x01, 0x08, 0x80, 0x80, 0x00, 0x00}
        }, SequenceMode::TryFirstPositive);
    }
    ProcedureResult r = ExecuteHexSequenceOnBase(191, "Suzuki UDS BSM Aiming Check (0x77A/0x78A - Base 191)", {
        {0x31, 0x01, 0x03, 0x00, 0x00}
    }, SequenceMode::RequireAll);
    if (!r.success && r.step == "CAN_NO_RESPONSE") {
        auto rAdm = ExecuteHexSequenceOnBase(186, "Suzuki UDS ADM Camera Calibration (0x79B/0x7AB - Base 186)", {
            {0x30, 0x01, 0x07, 0x80, 0x80, 0x00, 0x00}
        }, SequenceMode::RequireAll);
        for (const auto& l : r.log) rAdm.log.insert(rAdm.log.begin(), l);
        return rAdm;
    }
    return r;
}
