#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

// --- J2534 Definitions ---
typedef long (__stdcall *pfnPassThruOpen)(void* pName, unsigned long* pDeviceId);
typedef long (__stdcall *pfnPassThruClose)(unsigned long deviceId);
typedef long (__stdcall *pfnPassThruConnect)(unsigned long deviceId, unsigned long protocolId, unsigned long flags, unsigned long baudRate, unsigned long* pChannelId);
typedef long (__stdcall *pfnPassThruDisconnect)(unsigned long channelId);
typedef long (__stdcall *pfnPassThruStartMsgFilter)(unsigned long channelId, unsigned long filterType, void* pMaskMsg, void* pPatternMsg, void* pFlowControlMsg, unsigned long* pFilterId);
typedef long (__stdcall *pfnPassThruStopMsgFilter)(unsigned long channelId, unsigned long filterId);
typedef long (__stdcall *pfnPassThruWriteMsgs)(unsigned long channelId, void* pMsg, unsigned long* pNumMsgs, unsigned long timeout);
typedef long (__stdcall *pfnPassThruReadMsgs)(unsigned long channelId, void* pMsg, unsigned long* pNumMsgs, unsigned long timeout);
typedef long (__stdcall *pfnPassThruIoctl)(unsigned long id, unsigned long ioctlId, void* pInput, void* pOutput);
typedef long (__stdcall *pfnPassThruReadVersion)(unsigned long deviceId, char* pFirmware, char* pDll, char* pApi);
typedef long (__stdcall *pfnPassThruGetLastError)(char* pErr);

#define ISO15765            0x06
#define CAN                 0x05
#define ISO14230            0x04
#define FLOW_CONTROL_FILTER 0x03
#define PASS_FILTER         0x01
#define READ_VBATT          0x03
#define FIVE_BAUD_INIT      0x04
#define FAST_INIT           0x05

#pragma pack(push, 1)
struct PASSTHRU_MSG {
    unsigned long ProtocolID;
    unsigned long RxStatus;
    unsigned long TxFlags;
    unsigned long Timestamp;
    unsigned long DataSize;
    unsigned long ExtraDataIndex;
    unsigned char Data[4128];
};
#pragma pack(pop)

std::string JsonString(const std::string& value) {
    std::ostringstream out;
    out << char(34);
    for (unsigned char c : value) {
        if (c == 34 || c == 92) out << char(92) << c;
        else if (c < 32) out << char(92) << "u00" << std::hex << std::setw(2) << std::setfill(char(48)) << static_cast<int>(c) << std::dec;
        else out << c;
    }
    out << char(34);
    return out.str();
}

std::string JsonNumber(double value, int prec = 1) {
    if (!std::isfinite(value)) return "null";
    std::ostringstream out;
    out << std::fixed << std::setprecision(prec) << value;
    return out.str();
}

std::string HexBytes(const std::vector<unsigned char>& v, size_t from = 0) {
    std::ostringstream o;
    for (size_t i = from; i < v.size(); i++) {
        if (i > from) o << ' ';
        o << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)v[i];
    }
    return o.str();
}

std::string HexAddr(unsigned long val, int width = 3) {
    std::ostringstream o;
    o << "0x" << std::hex << std::uppercase << std::setw(width) << std::setfill('0') << val;
    return o.str();
}

bool ValidKLineStart(const PASSTHRU_MSG& msg, unsigned char target) {
    if (msg.ProtocolID != ISO14230 || (msg.RxStatus & 0x09) != 0 || msg.DataSize < 4 || msg.DataSize > sizeof(msg.Data)) return false;
    size_t end = msg.DataSize;
    if (msg.ExtraDataIndex >= 3 && msg.ExtraDataIndex < end) end = msg.ExtraDataIndex;
    if ((msg.Data[0] & 0xC0) != 0x80 || msg.Data[1] != 0xF1 || msg.Data[2] != target) return false;
    size_t length = msg.Data[0] & 0x3F;
    size_t start = 3;
    if (length == 0) {
        if (end < 5) return false;
        length = msg.Data[3];
        start = 4;
    }
    return length >= 1 && start + length <= end && msg.Data[start] == 0xC1;
}

bool ExtractKLinePayload(const PASSTHRU_MSG& msg, unsigned char target, std::vector<unsigned char>& payload) {
    payload.clear();
    if (msg.ProtocolID != ISO14230 || (msg.RxStatus & 0x09) != 0 || msg.DataSize < 4 || msg.DataSize > sizeof(msg.Data)) return false;
    size_t end = msg.DataSize;
    if (msg.ExtraDataIndex >= 3 && msg.ExtraDataIndex < end) end = msg.ExtraDataIndex;
    if ((msg.Data[0] & 0xC0) == 0x80) {
        if (msg.Data[1] != 0xF1 || msg.Data[2] != target) return false;
        size_t length = msg.Data[0] & 0x3F;
        size_t start = 3;
        if (length == 0) {
            if (end < 5) return false;
            length = msg.Data[3];
            start = 4;
        }
        if (start + length > end) return false;
        payload.assign(msg.Data + start, msg.Data + start + length);
        return !payload.empty();
    }
    return false;
}

bool MatchPositiveKwpResponse(const std::vector<unsigned char>& req, const std::vector<unsigned char>& payload) {
    if (req.empty() || payload.empty()) return false;
    if (payload[0] != static_cast<unsigned char>(req[0] + 0x40)) return false;
    if ((req[0] == 0x01 || req[0] == 0x09 || req[0] == 0x10 || req[0] == 0x12 || req[0] == 0x19 || req[0] == 0x1A || req[0] == 0x21 || req[0] == 0x30 || req[0] == 0x31 || req[0] == 0x3B || req[0] == 0xA5) && req.size() >= 2) {
        if (payload.size() < 2 || payload[1] != req[1]) return false;
    }
    if ((req[0] == 0x22 || req[0] == 0x2E) && req.size() >= 3) {
        if (payload.size() < 3 || payload[1] != req[1] || payload[2] != req[2]) return false;
    }
    return true;
}

struct J2534DeviceInfo {
    std::string id;
    std::string name;
    std::string vendor;
    std::string dllPath;
    bool exists = false;
};

static std::string ReadRegStringValue(HKEY hKey, const char* valueName) {
    char buf[1024] = {0};
    DWORD size = sizeof(buf) - 1;
    DWORD type = 0;
    if (RegQueryValueExA(hKey, valueName, NULL, &type, reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ)) {
        return std::string(buf);
    }
    return "";
}

std::vector<J2534DeviceInfo> EnumerateJ2534Devices() {
    std::vector<J2534DeviceInfo> list;
    const char* regPaths[] = {
        "SOFTWARE\\WOW6432Node\\PassThruSupport.04.04",
        "SOFTWARE\\PassThruSupport.04.04"
    };
    for (const char* basePath : regPaths) {
        HKEY hRoot = NULL;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, basePath, 0, KEY_READ, &hRoot) != ERROR_SUCCESS) continue;
        char subKeyName[512] = {0};
        for (DWORD idx = 0; ; idx++) {
            DWORD nameLen = sizeof(subKeyName);
            if (RegEnumKeyExA(hRoot, idx, subKeyName, &nameLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            HKEY hSub = NULL;
            if (RegOpenKeyExA(hRoot, subKeyName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                std::string dll = ReadRegStringValue(hSub, "FunctionLibrary");
                std::string name = ReadRegStringValue(hSub, "Name");
                std::string vendor = ReadRegStringValue(hSub, "Vendor");
                RegCloseKey(hSub);
                if (!dll.empty()) {
                    bool dup = false;
                    for (const auto& existing : list) {
                        if (_stricmp(existing.dllPath.c_str(), dll.c_str()) == 0 && existing.name == name) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) {
                        DWORD attr = GetFileAttributesA(dll.c_str());
                        bool fileOk = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
                        list.push_back({subKeyName, name.empty() ? subKeyName : name, vendor, dll, fileOk});
                    }
                }
            }
        }
        RegCloseKey(hRoot);
    }
    if (list.empty()) {
        std::string fallback = "C:\\Program Files (x86)\\Drew Technologies, Inc\\J2534\\MongoosePro JLR\\monpj432.dll";
        DWORD attr = GetFileAttributesA(fallback.c_str());
        bool fileOk = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
        list.push_back({"MongoosePro JLR", "MongoosePro JLR", "Drew Technologies Inc.", fallback, fileOk});
    }
    return list;
}

// --- J2534 Hardware Manager ---
class J2534Manager {
public:
    std::mutex mtx;
    HMODULE hMod = NULL;
    pfnPassThruOpen ptOpen = nullptr;
    pfnPassThruClose ptClose = nullptr;
    pfnPassThruConnect ptConnect = nullptr;
    pfnPassThruDisconnect ptDisconnect = nullptr;
    pfnPassThruStartMsgFilter ptStartFilter = nullptr;
    pfnPassThruStopMsgFilter ptStopFilter = nullptr;
    pfnPassThruWriteMsgs ptWrite = nullptr;
    pfnPassThruReadMsgs ptRead = nullptr;
    pfnPassThruIoctl ptIoctl = nullptr;
    pfnPassThruReadVersion ptReadVer = nullptr;
    pfnPassThruGetLastError ptGetErr = nullptr;

    unsigned long devId = 0;
    bool isConnected = false;
    std::string activeDllPath = "C:\\Program Files (x86)\\Drew Technologies, Inc\\J2534\\MongoosePro JLR\\monpj432.dll";
    std::string activeDeviceName = "MongoosePro JLR";
    std::string fwVer, dllVer, apiVer;

    void UnloadLocked() {
        if (isConnected && ptClose && devId != 0) {
            ptClose(devId);
        }
        devId = 0;
        isConnected = false;
        fwVer.clear();
        dllVer.clear();
        apiVer.clear();
        ptOpen = nullptr;
        ptClose = nullptr;
        ptConnect = nullptr;
        ptDisconnect = nullptr;
        ptStartFilter = nullptr;
        ptStopFilter = nullptr;
        ptWrite = nullptr;
        ptRead = nullptr;
        ptIoctl = nullptr;
        ptReadVer = nullptr;
        ptGetErr = nullptr;
        if (hMod) {
            FreeLibrary(hMod);
            hMod = NULL;
        }
    }

    bool InitLocked() {
        if (hMod) return ptOpen && ptClose && ptConnect && ptDisconnect && ptStartFilter && ptStopFilter && ptWrite && ptRead && ptIoctl;
        hMod = LoadLibraryA(activeDllPath.c_str());
        if (!hMod) return false;

        ptOpen = (pfnPassThruOpen)GetProcAddress(hMod, "PassThruOpen");
        ptClose = (pfnPassThruClose)GetProcAddress(hMod, "PassThruClose");
        ptConnect = (pfnPassThruConnect)GetProcAddress(hMod, "PassThruConnect");
        ptDisconnect = (pfnPassThruDisconnect)GetProcAddress(hMod, "PassThruDisconnect");
        ptStartFilter = (pfnPassThruStartMsgFilter)GetProcAddress(hMod, "PassThruStartMsgFilter");
        ptStopFilter = (pfnPassThruStopMsgFilter)GetProcAddress(hMod, "PassThruStopMsgFilter");
        ptWrite = (pfnPassThruWriteMsgs)GetProcAddress(hMod, "PassThruWriteMsgs");
        ptRead = (pfnPassThruReadMsgs)GetProcAddress(hMod, "PassThruReadMsgs");
        ptIoctl = (pfnPassThruIoctl)GetProcAddress(hMod, "PassThruIoctl");
        ptReadVer = (pfnPassThruReadVersion)GetProcAddress(hMod, "PassThruReadVersion");
        ptGetErr = (pfnPassThruGetLastError)GetProcAddress(hMod, "PassThruGetLastError");

        return ptOpen && ptClose && ptConnect && ptDisconnect && ptStartFilter && ptStopFilter && ptWrite && ptRead && ptIoctl;
    }

    bool Init() {
        std::lock_guard<std::mutex> lock(mtx);
        return InitLocked();
    }

    bool SelectDevice(const std::string& dllPath, const std::string& deviceName) {
        std::lock_guard<std::mutex> lock(mtx);
        UnloadLocked();
        activeDllPath = dllPath;
        activeDeviceName = deviceName.empty() ? dllPath : deviceName;
        if (!InitLocked()) return false;
        return ConnectDeviceLocked();
    }

    bool ConnectDeviceLocked() {
        if (isConnected) return true;
        if (!InitLocked()) return false;

        unsigned long newId = 0;
        long ret = ptOpen(NULL, &newId);
        if (ret != 0) return false;

        devId = newId;
        isConnected = true;

        char fw[256] = {}, dll[256] = {}, api[256] = {};
        if (ptReadVer && ptReadVer(devId, fw, dll, api) == 0) {
            fwVer = fw; dllVer = dll; apiVer = api;
        }
        return true;
    }

    bool ConnectDevice() {
        std::lock_guard<std::mutex> lock(mtx);
        return ConnectDeviceLocked();
    }

    double ReadBatteryVoltage() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!ConnectDeviceLocked()) return std::numeric_limits<double>::quiet_NaN();
        unsigned long vbattMv = 0;
        if (ptIoctl(devId, READ_VBATT, NULL, &vbattMv) == 0) {
            return vbattMv / 1000.0;
        }
        char firmware[256] = {}, dll[256] = {}, api[256] = {};
        if (ptReadVer && ptReadVer(devId, firmware, dll, api) != 0) {
            if (ptClose) ptClose(devId);
            devId = 0;
            isConnected = false;
            fwVer.clear(); dllVer.clear(); apiVer.clear();
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    long ProbeKLine(unsigned char target, std::vector<unsigned char>& response, bool& valid) {
        std::lock_guard<std::mutex> lock(mtx);
        response.clear();
        valid = false;
        if (!ConnectDeviceLocked()) return 8;
        unsigned long channel = 0;
        long result = ptConnect(devId, ISO14230, 0, 10400, &channel);
        if (result != 0) return result;
        PASSTHRU_MSG mask = {}, pattern = {}, tx = {}, rx = {};
        mask.ProtocolID = pattern.ProtocolID = tx.ProtocolID = ISO14230;
        mask.DataSize = pattern.DataSize = 1;
        unsigned long filter = 0;
        result = ptStartFilter(channel, PASS_FILTER, &mask, &pattern, NULL, &filter);
        if (result == 0) {
            tx.DataSize = 4;
            tx.Data[0] = 0x81; tx.Data[1] = target; tx.Data[2] = 0xF1; tx.Data[3] = 0x81;
            result = ptIoctl(channel, FAST_INIT, &tx, &rx);
            if (result == 0 && rx.DataSize <= sizeof(rx.Data)) {
                response.assign(rx.Data, rx.Data + rx.DataSize);
                valid = ValidKLineStart(rx, target);
            }
            // Send StopCommunication (82) cleanly if Fast Init succeeded
            if (valid) {
                PASSTHRU_MSG stopTx = {};
                stopTx.ProtocolID = ISO14230;
                stopTx.DataSize = 4;
                stopTx.Data[0] = 0x81; stopTx.Data[1] = target; stopTx.Data[2] = 0xF1; stopTx.Data[3] = 0x82;
                unsigned long n = 1;
                ptWrite(channel, &stopTx, &n, 150);
            }
            ptStopFilter(channel, filter);
        }
        ptDisconnect(channel);
        return result;
    }

    enum class KLineSeqMode { AllSteps, RequireSessionStopOnFailure, RequireSessionStopOnFirstPositive };

    bool OpenKLineChannelLocked(unsigned char target, unsigned long& outCh, unsigned long& outFilterId) {
        outCh = 0;
        outFilterId = 0;
        if (!ConnectDeviceLocked()) return false;

        // Guaranteed ISO 14230 Tidle / bus idle time before Fast Init
        std::this_thread::sleep_for(std::chrono::milliseconds(350));

        if (ptConnect(devId, ISO14230, 0, 10400, &outCh) != 0) return false;

        PASSTHRU_MSG mask = {}, pattern = {}, initTx = {}, initRx = {};
        mask.ProtocolID = pattern.ProtocolID = initTx.ProtocolID = ISO14230;
        mask.DataSize = pattern.DataSize = 1;
        if (ptStartFilter(outCh, PASS_FILTER, &mask, &pattern, NULL, &outFilterId) != 0) {
            ptDisconnect(outCh);
            outCh = 0;
            return false;
        }

        initTx.DataSize = 4;
        initTx.Data[0] = 0x81; initTx.Data[1] = target; initTx.Data[2] = 0xF1; initTx.Data[3] = 0x81;
        long ret = ptIoctl(outCh, FAST_INIT, &initTx, &initRx);
        if (ret != 0 || !ValidKLineStart(initRx, target)) {
            ptStopFilter(outCh, outFilterId);
            ptDisconnect(outCh);
            outCh = 0;
            outFilterId = 0;
            return false;
        }

        // Drain any leftover rx bytes after Fast Init
        for (int d = 0; d < 10; d++) {
            PASSTHRU_MSG drx = {}; unsigned long dn = 1;
            if (ptRead(outCh, &drx, &dn, 20) != 0 || dn == 0) break;
        }
        return true;
    }

    void CloseKLineChannelLocked(unsigned long ch, unsigned long filterId, unsigned char target) {
        if (ch != 0) {
            PASSTHRU_MSG stopTx = {};
            stopTx.ProtocolID = ISO14230;
            stopTx.DataSize = 4;
            stopTx.Data[0] = 0x81; stopTx.Data[1] = target; stopTx.Data[2] = 0xF1; stopTx.Data[3] = 0x82;
            unsigned long n = 1;
            ptWrite(ch, &stopTx, &n, 150);
            if (filterId != 0) ptStopFilter(ch, filterId);
            ptDisconnect(ch);
        }
    }

    bool TransactKLineLocked(unsigned long ch, unsigned char target,
                             const std::vector<unsigned char>& req, std::vector<unsigned char>& resp,
                             unsigned long timeoutMs, bool& outPositive) {
        resp.clear();
        outPositive = false;
        if (ch == 0 || req.empty() || req.size() > 63) return false;

        std::this_thread::sleep_for(std::chrono::milliseconds(40)); // P3_Min timing
        PASSTHRU_MSG tx = {};
        tx.ProtocolID = ISO14230;
        tx.DataSize = 4 + (unsigned long)req.size();
        tx.Data[0] = 0x80;
        tx.Data[1] = target;
        tx.Data[2] = 0xF1;
        tx.Data[3] = static_cast<unsigned char>(req.size() & 0xFF);
        std::copy(req.begin(), req.end(), tx.Data + 4);

        unsigned long n = 1;
        if (ptWrite(ch, &tx, &n, timeoutMs) != 0 || n == 0) return false;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            PASSTHRU_MSG rx = {};
            n = 1;
            long ret = ptRead(ch, &rx, &n, 30);
            if (ret != 0 && ret != 9 && ret != 16) break;
            if (ret != 0 || n == 0) continue;
            if ((rx.RxStatus & 0x01) != 0) continue; // Skip single-wire TX echo

            std::vector<unsigned char> payload;
            if (!ExtractKLinePayload(rx, target, payload)) continue;

            if (payload[0] == 0x7F) {
                if (payload.size() < 3 || payload[1] != req[0]) continue;
                if (payload[2] == 0x78) continue;
                resp = std::move(payload);
                return true;
            }
            if (MatchPositiveKwpResponse(req, payload)) {
                resp = std::move(payload);
                outPositive = true;
                return true;
            }
        }
        return false;
    }

    bool RunKLineSession(unsigned char target, const std::vector<std::vector<unsigned char>>& reqs, std::vector<std::vector<unsigned char>>& resps, unsigned long timeoutMs = 400, KLineSeqMode seqMode = KLineSeqMode::AllSteps) {
        std::lock_guard<std::mutex> lock(mtx);
        resps.assign(reqs.size(), {});
        unsigned long channel = 0, filter = 0;
        if (!OpenKLineChannelLocked(target, channel, filter)) return false;

        bool anyOk = false;
        for (size_t i = 0; i < reqs.size(); i++) {
            const auto& req = reqs[i];
            if (req.empty() || req.size() > 63) continue;
            bool gotPositive = false;
            if (TransactKLineLocked(channel, target, req, resps[i], timeoutMs, gotPositive) && gotPositive) {
                anyOk = true;
            }
            if (seqMode == KLineSeqMode::RequireSessionStopOnFailure && !gotPositive) {
                break;
            }
            if (seqMode == KLineSeqMode::RequireSessionStopOnFirstPositive) {
                if (i == 0 && !gotPositive) break; // Session open failed
                if (i >= 1 && gotPositive) break;  // Alternative succeeded, stop probing remaining variants
            }
        }

        CloseKLineChannelLocked(channel, filter, target);
        return anyOk;
    }

    bool OpenIso15765ChannelLocked(unsigned long txId, unsigned long rxId, unsigned long& outCh, unsigned long& outFilterId) {
        outCh = 0;
        outFilterId = 0;
        if (txId > 0x7FF || rxId > 0x7FF) return false;
        if (!ConnectDeviceLocked()) return false;

        if (ptConnect(devId, ISO15765, 0, 500000, &outCh) != 0) return false;

        PASSTHRU_MSG maskMsg = {0}, patternMsg = {0}, flowMsg = {0};
        maskMsg.ProtocolID = patternMsg.ProtocolID = flowMsg.ProtocolID = ISO15765;
        maskMsg.DataSize = patternMsg.DataSize = flowMsg.DataSize = 4;
        maskMsg.Data[0] = 0xFF; maskMsg.Data[1] = 0xFF; maskMsg.Data[2] = 0x07; maskMsg.Data[3] = 0xFF;

        patternMsg.Data[0] = (unsigned char)((rxId >> 24) & 0xFF);
        patternMsg.Data[1] = (unsigned char)((rxId >> 16) & 0xFF);
        patternMsg.Data[2] = (unsigned char)((rxId >> 8) & 0xFF);
        patternMsg.Data[3] = (unsigned char)(rxId & 0xFF);

        flowMsg.Data[0] = (unsigned char)((txId >> 24) & 0xFF);
        flowMsg.Data[1] = (unsigned char)((txId >> 16) & 0xFF);
        flowMsg.Data[2] = (unsigned char)((txId >> 8) & 0xFF);
        flowMsg.Data[3] = (unsigned char)(txId & 0xFF);

        if (ptStartFilter(outCh, FLOW_CONTROL_FILTER, &maskMsg, &patternMsg, &flowMsg, &outFilterId) != 0) {
            ptDisconnect(outCh);
            outCh = 0;
            return false;
        }
        return true;
    }

    void CloseIso15765ChannelLocked(unsigned long ch, unsigned long filterId) {
        if (ch != 0) {
            if (filterId != 0) ptStopFilter(ch, filterId);
            ptDisconnect(ch);
        }
    }

    bool TransactIso15765Locked(unsigned long channelId, unsigned long txId, unsigned long rxId,
                                const std::vector<unsigned char>& req, std::vector<unsigned char>& resp,
                                unsigned long timeoutMs) {
        resp.clear();
        if (channelId == 0 || req.empty() || req.size() > 4124 || txId > 0x7FF || rxId > 0x7FF) return false;

        PASSTHRU_MSG txMsg = {0};
        txMsg.ProtocolID = ISO15765;
        txMsg.TxFlags = 0x00000040; // FRAME_PAD
        txMsg.DataSize = 4 + (unsigned long)req.size();
        txMsg.Data[0] = (unsigned char)((txId >> 24) & 0xFF);
        txMsg.Data[1] = (unsigned char)((txId >> 16) & 0xFF);
        txMsg.Data[2] = (unsigned char)((txId >> 8) & 0xFF);
        txMsg.Data[3] = (unsigned char)(txId & 0xFF);
        std::copy(req.begin(), req.end(), txMsg.Data + 4);

        unsigned long numMsgs = 1;
        long ret = ptWrite(channelId, &txMsg, &numMsgs, timeoutMs);
        if (ret != 0 || numMsgs == 0) return false;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            PASSTHRU_MSG rxMsg = {};
            numMsgs = 1;
            ret = ptRead(channelId, &rxMsg, &numMsgs, 35);
            if (ret != 0 && ret != 9 && ret != 16) break;
            if (ret != 0 || numMsgs == 0 || rxMsg.ProtocolID != ISO15765 || (rxMsg.RxStatus & 0x0B) != 0 ||
                rxMsg.DataSize <= 4 || rxMsg.DataSize > sizeof(rxMsg.Data)) continue;

            const unsigned long actualId = (static_cast<unsigned long>(rxMsg.Data[0]) << 24) |
                (static_cast<unsigned long>(rxMsg.Data[1]) << 16) |
                (static_cast<unsigned long>(rxMsg.Data[2]) << 8) | rxMsg.Data[3];
            if (actualId != rxId) continue;

            unsigned long end = rxMsg.DataSize;
            if (rxMsg.ExtraDataIndex >= 4 && rxMsg.ExtraDataIndex < end) end = rxMsg.ExtraDataIndex;
            if (end <= 4) continue;

            std::vector<unsigned char> payload(rxMsg.Data + 4, rxMsg.Data + end);
            if (payload[0] == 0x7F) {
                if (payload.size() < 3 || payload[1] != req[0]) continue;
                if (payload[2] == 0x78) continue; // Response pending, keep waiting
                resp = std::move(payload);
                return true;
            } else {
                if (!MatchPositiveKwpResponse(req, payload)) continue;
                resp = std::move(payload);
                return true;
            }
        }
        return false;
    }

    bool SendAndReceive(unsigned long txId, unsigned long rxId, const std::vector<unsigned char>& req, std::vector<unsigned char>& resp, unsigned long timeoutMs = 600) {
        std::lock_guard<std::mutex> lock(mtx);
        resp.clear();
        if (req.empty() || req.size() > 4124) return false;
        unsigned long ch = 0, filterId = 0;
        if (!OpenIso15765ChannelLocked(txId, rxId, ch, filterId)) return false;
        bool received = TransactIso15765Locked(ch, txId, rxId, req, resp, timeoutMs);
        CloseIso15765ChannelLocked(ch, filterId);
        return received;
    }
};

struct CanChannelScope {
    J2534Manager& mgr;
    unsigned long txId = 0;
    unsigned long rxId = 0;
    unsigned long ch = 0;
    unsigned long filterId = 0;
    bool open = false;
    std::unique_lock<std::mutex> lock;

    CanChannelScope(J2534Manager& m, unsigned long tx, unsigned long rx)
        : mgr(m), txId(tx), rxId(rx), lock(m.mtx) {
        open = mgr.OpenIso15765ChannelLocked(txId, rxId, ch, filterId);
    }

    ~CanChannelScope() {
        if (open) {
            mgr.CloseIso15765ChannelLocked(ch, filterId);
        }
    }

    bool Transact(const std::vector<unsigned char>& req, std::vector<unsigned char>& resp, unsigned long timeoutMs = 600) {
        if (!open) return false;
        return mgr.TransactIso15765Locked(ch, txId, rxId, req, resp, timeoutMs);
    }
};

struct KLineChannelScope {
    J2534Manager& mgr;
    unsigned char target = 0;
    unsigned long ch = 0;
    unsigned long filterId = 0;
    bool open = false;
    std::unique_lock<std::mutex> lock;

    KLineChannelScope(J2534Manager& m, unsigned char tgt)
        : mgr(m), target(tgt), lock(m.mtx) {
        open = mgr.OpenKLineChannelLocked(target, ch, filterId);
    }

    ~KLineChannelScope() {
        if (open) {
            mgr.CloseKLineChannelLocked(ch, filterId, target);
        }
    }

    bool Transact(const std::vector<unsigned char>& req, std::vector<unsigned char>& resp, unsigned long timeoutMs = 450, bool* outPositive = nullptr) {
        if (!open) {
            resp.clear();
            if (outPositive) *outPositive = false;
            return false;
        }
        bool pos = false;
        bool rxOk = mgr.TransactKLineLocked(ch, target, req, resp, timeoutMs, pos);
        if (outPositive) *outPositive = pos;
        return rxOk;
    }
};

J2534Manager g_diag;

bool SendBatch(J2534Manager& m, unsigned long txId, unsigned long rxId,
               const std::vector<std::vector<unsigned char>>& reqs,
               std::vector<std::vector<unsigned char>>& resps, unsigned long timeoutMs) {
    resps.assign(reqs.size(), {});
    CanChannelScope scope(m, txId, rxId);
    if (!scope.open) return false;

    bool anyOk = false;
    for (size_t r = 0; r < reqs.size(); r++) {
        if (scope.Transact(reqs[r], resps[r], timeoutMs)) {
            anyOk = true;
        }
    }
    return anyOk;
}

// 21 80 decoder: Renault / Bosch EDC16C39 identification (SUZUKI CAN ECM BOSCH F9Q) & generic Suzuki KWP 21 80
std::string DecodeEcmIdentJson(const std::vector<unsigned char>& r) {
    if (r.size() < 3 || r[0] != 0x61 || r[1] != 0x80) return "null";
    if (r.size() < 20) {
        std::string ascii;
        for (size_t i = 2; i < r.size(); i++) {
            if (r[i] >= 32 && r[i] < 127) ascii += (char)r[i];
        }
        std::ostringstream o;
        o << "{\"ascii\":" << JsonString(ascii)
          << ",\"raw\":" << JsonString(HexBytes(r)) << "}";
        return o.str();
    }
    auto hexRange = [&](size_t a, size_t len) {
        std::vector<unsigned char> s(r.begin() + a, r.begin() + a + len);
        std::string h = HexBytes(s);
        h.erase(std::remove(h.begin(), h.end(), ' '), h.end());
        return h;
    };
    std::string supplier;
    for (size_t i = 7; i < 11; i++) supplier += (r[i] >= 32 && r[i] < 127) ? (char)r[i] : '?';
    bool isF9q = (supplier == "P037" || (r[2] == 0x82 && r[3] == 0x00));
    std::ostringstream o;
    o << "{\"renaultPartNumber\":" << JsonString(hexRange(2, 5))
      << ",\"supplier\":" << JsonString(supplier)
      << ",\"hardwareNumber\":" << JsonString(hexRange(11, 5))
      << ",\"softwareVersion\":" << JsonString(hexRange(16, 2))
      << ",\"calibration\":" << JsonString(hexRange(18, 2))
      << ",\"matchedModelId\":" << (isF9q ? JsonString("14") : "null")
      << ",\"matchedVariantId\":" << (isF9q ? JsonString("16330983") : "null")
      << ",\"raw\":" << JsonString(HexBytes(r)) << "}";
    return o.str();
}

std::vector<unsigned char> g_ecmSupportedIds;

// --- Communication profiles ---
struct ModuleDef {
    std::string name;
    std::string shortName;
    std::string ecuVariant;
    unsigned long txId;
    unsigned long rxId;
    unsigned char klineAddr;
    std::string systemGroup;
    unsigned long altTxId = 0;
    unsigned long altRxId = 0;
    unsigned char altKlineAddr = 0;
    int defaultBaseId = 0;
};

struct VehicleProfileInfo {
    std::string id;
    std::string title;
    std::string description;
};

std::string g_activeProfileId = "auto";
std::string g_detectedVehicleDesc = "ECU/model not identified";
std::string g_cachedVin = "";

std::vector<VehicleProfileInfo> g_vehicleProfiles = {
    {"auto", "Auto Detect - All Suzuki Models (Full 13 Systems: CAN + K-Line)", "Automatically scans all Suzuki CAN (0x7E0..0x799 / 0x261..0x277) and K-Line (0x11..0x99 / 0x7A) control units."},
    {"gv_jb419d", "Grand Vitara 1.9 DDiS (JB419D - F9Q Diesel) [Verified]", "CAN Engine (0x7E0 F9Q) + ABS/ESP (0x29/0x261) + 4WD (0x21/0x262) + BCM (0x5A/0x271) + SRS (0x59/0x273) + IPC (0x274) + KLS/IMM (0x275/0x5D) + TPMS (0x798/0x5C) + AC (0x99/0x272)."},
    {"gv_petrol", "Grand Vitara Petrol (1.6 / 2.0 / 2.4 / 2.7 / 3.2 - JB Series) [Beta]", "ECM (0x7E0/0x11) + TCM AT (0x7E1/0x19) + ABS/ESP + 4WD + BCM + SRS + OCM + TPMS + IPC + KLS/IMM + SKM + AC."},
    {"suzuki_can", "Suzuki Generic Full CAN (Swift, Vitara LY, SX4, S-Cross, Jimny, Baleno, Ignis, Splash, Celerio) [Beta]", "Full CAN 13 Modules: ECM, TCM, ABS/ESP, EPS, 4WD AllGrip, BCM, SRS, ACCM/OCM, TPMS, IPC, KLS/IMM, ALC, HVAC."},
    {"suzuki_kline", "Suzuki Classic K-Line (2000-2008 Swift, Jimny, Ignis, Liana, Wagon R+, Alto) [Beta]", "Pin 7 ISO14230 Fast Init 9 Modules: ECM (0x11/0x7A), TCM (0x19/0x13), ABS/ESP (0x29), 4WD (0x21), EPS (0x31), BCM (0x5A), SRS (0x59), IMM (0x5D/0x5B), HVAC (0x99)."}
};

std::vector<ModuleDef> GetActiveSuzukiModules() {
    if (g_activeProfileId == "gv_jb419d") {
        return {
            {"Motor Kontrol Modulu (ECM)", "ECM", "SUZUKI CAN ECM BOSCH F9Q (EDC16C39 0x7E0/0x7E8)", 0x7E0, 0x7E8, 0x7A, "POWERTRAIN", 0, 0, 0x11, 93},
            {"Fren ve ESP Kontrol Sistemi (ABS/ESP)", "ABS/ESP", "Suzuki ABS/ESP 56100-65J0/65J7 (K-Line 0x29 / CAN 0x261)", 0x261, 0x661, 0x29, "CHASSIS", 0x770, 0x780, 0, 34},
            {"Dort Ceker Transfer Kutusu (4WD)", "4WD", "Suzuki 4WD 38885-65J0 (K-Line 0x21 / CAN 0x262)", 0x262, 0x662, 0x21, "DRIVETRAIN", 0x26A, 0x66A, 0, 33},
            {"Govde Kontrol Modulu (BCM)", "BCM", "Suzuki BCM 36770-65J0 (K-Line 0x5A / CAN 0x271)", 0x271, 0x671, 0x5A, "BODY", 0x264, 0x664, 0, 39},
            {"Hava Yastigi Kontrol Modulu (SRS Airbag)", "SRS", "Suzuki SRS 38910-65J0 (K-Line 0x59 / CAN 0x273)", 0x273, 0x673, 0x59, "RESTRAINT", 0x263, 0x663, 0, 37},
            {"Anahtarsiz Calistirma ve Immobilizer (KLS / IMM)", "KLS/IMM", "Suzuki Keyless & Immo (K-Line 0x5D / CAN 0x275)", 0x275, 0x675, 0x5D, "BODY", 0x272, 0x672, 0x5B, 46},
            {"Gosterge Paneli (IPC Cluster)", "IPC", "Suzuki Combination Meter (CAN 0x274/0x674)", 0x274, 0x674, 0x00, "INSTRUMENT", 0x270, 0x670, 0, 47},
            {"Lastik Basinc Izleme Sistemi (TPMS)", "TPMS", "Suzuki TPMS (CAN 0x798/0x7A8 | K-Line 0x5C)", 0x798, 0x7A8, 0x5C, "CHASSIS", 0x26E, 0x66E, 0, 92},
            {"Otomatik Klima Kontrolu (HVAC / AC)", "HVAC", "Suzuki HVAC 39510-64J0 (K-Line 0x99 / CAN 0x272)", 0x272, 0x672, 0x99, "CLIMATE", 0x274, 0x674, 0x98, 35}
        };
    }
    if (g_activeProfileId == "suzuki_can") {
        return {
            {"Motor Kontrol Modulu (ECM)", "ECM", "SUZUKI CAN ECM (CAN 0x7E0/0x7E8)", 0x7E0, 0x7E8, 0x00, "POWERTRAIN", 0, 0, 0, 41},
            {"Otomatik Sanziman / CVT / AGS (TCM)", "TCM", "SUZUKI CAN TCM (CAN 0x7E1/0x7E9)", 0x7E1, 0x7E9, 0x00, "POWERTRAIN", 0x262, 0x662, 0, 49},
            {"Fren ve ESP Kontrol Sistemi (ABS/ESP)", "ABS/ESP", "SUZUKI CAN ABS/ESP (CAN 0x261/0x661 & UDS 0x770)", 0x261, 0x661, 0x00, "CHASSIS", 0x770, 0x780, 0, 34},
            {"Elektrikli Direksiyon Kontrolu (EPS)", "EPS", "SUZUKI CAN EPS (CAN 0x263/0x663)", 0x263, 0x663, 0x00, "STEERING", 0x265, 0x665, 0, 42},
            {"Dort Ceker / AllGrip Kontrolu (4WD)", "4WD", "SUZUKI CAN 4WD (CAN 0x262/0x662)", 0x262, 0x662, 0x00, "DRIVETRAIN", 0x26A, 0x66A, 0, 33},
            {"Govde Kontrol Modulu (BCM)", "BCM", "SUZUKI CAN BCM (CAN 0x271/0x671)", 0x271, 0x671, 0x00, "BODY", 0x264, 0x664, 0, 39},
            {"Hava Yastigi Kontrol Modulu (SRS Airbag)", "SRS", "SUZUKI CAN AIRBAG (CAN 0x273/0x673 & UDS 0x793)", 0x273, 0x673, 0x00, "RESTRAINT", 0x793, 0x7A3, 0, 37},
            {"Yardimci / Koltuk Kontrol Modulu (ACCM/OCM)", "OCM", "SUZUKI CAN ACCM (CAN 0x264/0x664)", 0x264, 0x664, 0x00, "RESTRAINT", 0x269, 0x669, 0, 36},
            {"Lastik Basinc Izleme Sistemi (TPMS)", "TPMS", "SUZUKI CAN TPMS (CAN 0x798/0x7A8)", 0x798, 0x7A8, 0x00, "CHASSIS", 0x26E, 0x66E, 0, 92},
            {"Gosterge Paneli (IPC Cluster)", "IPC", "SUZUKI CAN IPC (CAN 0x274/0x674 & UDS 0x795)", 0x274, 0x674, 0x00, "INSTRUMENT", 0x795, 0x7A5, 0, 47},
            {"Anahtarsiz Calistirma & Immobilizer (KLS/IMM)", "KLS/IMM", "SUZUKI CAN KLS (CAN 0x275/0x675)", 0x275, 0x675, 0x00, "BODY", 0x272, 0x672, 0, 46},
            {"Otomatik Far / Iletisim Modulu (ALC/TCU)", "SKM", "SUZUKI CAN ALC/TCU (CAN 0x276/0x676 & 0x799)", 0x276, 0x676, 0x00, "BODY", 0x799, 0x7A9, 0, 38},
            {"Otomatik Klima Kontrolu (HVAC / AC)", "HVAC", "SUZUKI CAN AC (CAN 0x272/0x672)", 0x272, 0x672, 0x00, "CLIMATE", 0x274, 0x674, 0, 35}
        };
    }
    if (g_activeProfileId == "suzuki_kline") {
        return {
            {"Motor Kontrol Modulu (ECM)", "ECM", "SUZUKI KWP ECM (K-Line 0x11 / 0x7A / 0x10)", 0x00, 0x00, 0x11, "POWERTRAIN", 0, 0, 0x7A, 64},
            {"Otomatik Sanziman Kontrolu (TCM)", "TCM", "SUZUKI KWP TCM (K-Line 0x19 / 0x13)", 0x00, 0x00, 0x19, "POWERTRAIN", 0, 0, 0x13, 75},
            {"Fren ve ESP Kontrol Sistemi (ABS/ESP)", "ABS/ESP", "SUZUKI KWP ABS/ESP (K-Line 0x29)", 0x00, 0x00, 0x29, "CHASSIS", 0, 0, 0, 58},
            {"Dort Ceker Transfer Kutusu (4WD)", "4WD", "SUZUKI KWP 4WD (K-Line 0x21)", 0x00, 0x00, 0x21, "DRIVETRAIN", 0, 0, 0, 56},
            {"Elektrikli Direksiyon Kontrolu (EPS)", "EPS", "SUZUKI KWP EPS (K-Line 0x31)", 0x00, 0x00, 0x31, "STEERING", 0, 0, 0, 67},
            {"Govde Kontrol Modulu (BCM)", "BCM", "SUZUKI KWP BCM (K-Line 0x5A)", 0x00, 0x00, 0x5A, "BODY", 0, 0, 0, 63},
            {"Hava Yastigi Kontrol Modulu (SRS Airbag)", "SRS", "SUZUKI KWP SRS (K-Line 0x59)", 0x00, 0x00, 0x59, "RESTRAINT", 0, 0, 0, 61},
            {"Immobilizer Kontrol Modulu (IMM)", "KLS/IMM", "SUZUKI KWP IMM (K-Line 0x5D / 0x5B)", 0x00, 0x00, 0x5D, "BODY", 0, 0, 0x5B, 70},
            {"Otomatik Klima Kontrolu (HVAC / AC)", "HVAC", "SUZUKI KWP AC (K-Line 0x99 / 0x98)", 0x00, 0x00, 0x99, "CLIMATE", 0, 0, 0x98, 59}
        };
    }
    return {
        {"Motor Kontrol Modulu (ECM)", "ECM", "Suzuki Hybrid ECM (CAN 0x7E0 / K-Line 0x11 & 0x7A)", 0x7E0, 0x7E8, 0x11, "POWERTRAIN", 0, 0, 0x7A, 41},
        {"Otomatik Sanziman Kontrolu (TCM)", "TCM", "Suzuki Hybrid TCM (CAN 0x7E1 & 0x262 / K-Line 0x19 & 0x13)", 0x7E1, 0x7E9, 0x19, "POWERTRAIN", 0x262, 0x662, 0x13, 49},
        {"Fren ve ESP Kontrol Sistemi (ABS/ESP)", "ABS/ESP", "Suzuki Hybrid ABS/ESP (CAN 0x261 & 0x770 / K-Line 0x29)", 0x261, 0x661, 0x29, "CHASSIS", 0x770, 0x780, 0, 34},
        {"Elektrikli Direksiyon Kontrolu (EPS)", "EPS", "Suzuki Hybrid EPS (CAN 0x263 / K-Line 0x31)", 0x263, 0x663, 0x31, "STEERING", 0x265, 0x665, 0, 42},
        {"Dort Ceker / AllGrip Kontrolu (4WD)", "4WD", "Suzuki Hybrid 4WD (CAN 0x262 / K-Line 0x21)", 0x262, 0x662, 0x21, "DRIVETRAIN", 0x26A, 0x66A, 0, 33},
        {"Govde Kontrol Modulu (BCM)", "BCM", "Suzuki Hybrid BCM (CAN 0x271 / K-Line 0x5A)", 0x271, 0x671, 0x5A, "BODY", 0x264, 0x664, 0, 39},
        {"Hava Yastigi Kontrol Modulu (SRS Airbag)", "SRS", "Suzuki Hybrid SRS (CAN 0x273 & 0x793 / K-Line 0x59)", 0x273, 0x673, 0x59, "RESTRAINT", 0x793, 0x7A3, 0, 37},
        {"Koltuk Doluluk Algilama Modulu (OCM)", "OCM", "Suzuki Hybrid OCM/ACCM (CAN 0x264 / K-Line 0x5B)", 0x264, 0x664, 0x5B, "RESTRAINT", 0x269, 0x669, 0, 36},
        {"Lastik Basinc Izleme Sistemi (TPMS)", "TPMS", "Suzuki Hybrid TPMS (CAN 0x798 / K-Line 0x5C)", 0x798, 0x7A8, 0x5C, "CHASSIS", 0x26E, 0x66E, 0, 92},
        {"Gosterge Paneli (IPC Cluster)", "IPC", "Suzuki CAN IPC (CAN 0x274 & 0x795)", 0x274, 0x674, 0x00, "INSTRUMENT", 0x795, 0x7A5, 0, 47},
        {"Anahtarsiz Calistirma & Immobilizer (KLS/IMM)", "KLS/IMM", "Suzuki Hybrid Keyless/Immo (CAN 0x275 / K-Line 0x5D)", 0x275, 0x675, 0x5D, "BODY", 0x272, 0x672, 0x5B, 46},
        {"Otomatik Far / Iletisim Modulu (ALC/TCU)", "SKM", "Suzuki CAN ALC/TCU (CAN 0x276 & 0x799)", 0x276, 0x676, 0x00, "BODY", 0x799, 0x7A9, 0, 38},
        {"Otomatik Klima Kontrolu (HVAC / AC)", "HVAC", "Suzuki Hybrid HVAC (CAN 0x272 / K-Line 0x99)", 0x272, 0x672, 0x99, "CLIMATE", 0x274, 0x674, 0x98, 35}
    };
}

struct DTCEntry {
    std::string code;
    std::string system;
    std::string title;
    std::string suggestion;
};

std::map<std::string, DTCEntry> g_dtcDb = {
    {"P0001", {"P0001", "ECM", "Yakit Hacim Regulator Kontrol Devresi Acik", "Yuksek basinc pompasi regulator soketi ve kablo demetini kontrol ediniz."}},
    {"P0016", {"P0016", "ECM", "Krank Mili ile Eksantrik Mili Pozisyon Korelasyon Hatasi", "Triger sente ayarini, krank ve eksantrik sensorlerini kontrol ediniz."}},
    {"P0045", {"P0045", "ECM", "Turbosarj Takviye Basinc Kontrol Devresi Acik", "Turbo VNT/wastegate selenoid valfi ve tesisatini kontrol ediniz."}},
    {"P0087", {"P0087", "ECM", "Yakit Dagitim Borusu (Common Rail) Basinci Cok Dusuk", "Yakit filtresi tikanikligi, dusuk basinc pompasi veya rail basinc sensorunu kontrol ediniz."}},
    {"P0088", {"P0088", "ECM", "Yakit Dagitim Borusu (Common Rail) Basinci Cok Yuksek", "Rail basinc regulatoru ve donus hattini kontrol ediniz."}},
    {"P0089", {"P0089", "ECM", "Yakit Basinc Regulatoru 1 Performans Hatasi", "Yakit basinc regulatoru asinmasi veya yakit hatti kirliligi."}},
    {"P0090", {"P0090", "ECM", "Yakit Basinc Regulatoru 1 Kontrol Devresi Acik", "Regulator soketi ve ECM surucu devresini kontrol ediniz."}},
    {"P0100", {"P0100", "ECM", "Hava Akis Olcer (MAF) Sensor Devresi Arizasi", "Hava akis metre soketi, besleme voltaji ve sasi hattini test ediniz."}},
    {"P0101", {"P0101", "ECM", "Hava Akis Olcer (MAF) Sinyal Aralik / Performans", "Hava filtresi, emme hortumu kacaklari veya kirlenmis MAF sensoru."}},
    {"P0102", {"P0102", "ECM", "Hava Akis Olcer (MAF) Dusuk Giris Sinyali", "MAF sensoru sinyal hattinda sasiye kisa devre kontrolu."}},
    {"P0110", {"P0110", "ECM", "Emme Havasi Sicaklik (IAT) Sensor Devresi", "Hava filtresi kutusundaki IAT sensor direncini olcunuz."}},
    {"P0115", {"P0115", "ECM", "Motor Sogutma Suyu Sicaklik (ECT) Devresi", "Termostat yuvasindaki hararet musuru ve kablo baglantilarini kontrol ediniz."}},
    {"P0120", {"P0120", "ECM", "Gaz Kelebegi / Pedal Konum Sensoru A Devresi", "Gaz pedali potansiyometresi soket baglantisini kontrol ediniz."}},
    {"P012B", {"P012B", "ECM", "Turbosarj Giris Basinc Sensoru Aralik / Performans", " Basinc sensoru ve emme hatti basinc kacaklarini kontrol ediniz."}},
    {"P0180", {"P0180", "ECM", "Yakit Sicaklik Sensoru A Devresi", "Yakit geri donus hattindaki sicaklik sensorunu kontrol ediniz."}},
    {"P0190", {"P0190", "ECM", "Rail Basinc Sensoru Devresi Arizasi", "Common rail uzerindeki basinc sensoru 5V referans voltajini olcunuz."}},
    {"P0191", {"P0191", "ECM", "Rail Basinc Sensoru Aralik / Performans", "Sensor ofset sapmasi veya pompa basinc dalgalanmasini kontrol ediniz."}},
    {"P0201", {"P0201", "ECM", "Silindir 1 Enjektor Devresi Acik / Kisa Devre", "1. silindir enjektor bobin direnci ve ECU tesisatini olcunuz."}},
    {"P0202", {"P0202", "ECM", "Silindir 2 Enjektor Devresi Acik / Kisa Devre", "2. silindir enjektor soketi ve kablo demetini test ediniz."}},
    {"P0203", {"P0203", "ECM", "Silindir 3 Enjektor Devresi Acik / Kisa Devre", "3. silindir enjektor devresini test ediniz."}},
    {"P0204", {"P0204", "ECM", "Silindir 4 Enjektor Devresi Acik / Kisa Devre", "4. silindir enjektor devresini test ediniz."}},
    {"P0217", {"P0217", "ECM", "Motor Sogutma Suyu Asiri Isinma Durumu", "Sogutma suyu seviyesini, termostati ve radyator fanlarini kontrol ediniz."}},
    {"P0225", {"P0225", "ECM", "Gaz Pedali Konum Sensoru C Devresi", "Gaz pedali ikincil potansiyometre hattini kontrol ediniz."}},
    {"P0234", {"P0234", "ECM", "Turbosarj Asiri Basinc Durumu (Overboost)", "Turbo wastegate/VNT aktuator hortumu ve N75 elektrovalfini test ediniz."}},
    {"P0235", {"P0235", "ECM", "Turbo Takviye Basinc (MAP) Sensor Devresi", "Intercooler cikisindaki MAP sensoru sinyalini ve boru kacaklarini kontrol ediniz."}},
    {"P0299", {"P0299", "ECM", "Turbosarj Dusuk Basinc Durumu (Underboost)", "Intercooler hortum yirtigi, turbo pervanesi veya vakum kacagini kontrol ediniz."}},
    {"P0335", {"P0335", "ECM", "Krank Mili Konum Sensoru (CKP) Devresi", "Volan uzerindeki krank sensoru temizligi ve soket baglantisini kontrol ediniz."}},
    {"P0340", {"P0340", "ECM", "Eksantrik Mili Konum Sensoru (CMP) Devresi", "Eksantrik sensoru ve triger kayisi senkronizasyonunu kontrol ediniz."}},
    {"P0380", {"P0380", "ECM", "Kizdirma Bujisi Devresi A (On Isitma Sistemi)", "Kizdirma bujilerinin direncini (0.6 - 1.0 Ohm) ve on isitma rolesini kontrol ediniz."}},
    {"P0400", {"P0400", "ECM", "EGR Sistemi Akis Arizasi", "EGR valfi kurum birikintisini ve vakum hattini temizleyiniz."}},
    {"P0403", {"P0403", "ECM", "EGR Valfi Kontrol Devresi Arizasi", "EGR solenoid motor soketi ve elektrik baglantisini kontrol ediniz."}},
    {"P0409", {"P0409", "ECM", "EGR Konum Sensoru A Devresi", "EGR potansiyometresi geri besleme voltajini olcunuz."}},
    {"P0487", {"P0487", "ECM", "EGR Gaz Kelebegi Kontrol Devresi Acik", "Emme kelebegi soketi ve motor sargilarini kontrol ediniz."}},
    {"P0488", {"P0488", "ECM", "EGR Gaz Kelebegi Kontrol Devresi Performans", "Giris kelebek valfi kurum temizligi ve DSL-03 adaptasyonu yapiniz."}},
    {"P0500", {"P0500", "ECM", "Arac Hiz Sensoru (VSS) Sinyali Yok", "ABS beyninden gelen hiz sinyali ve tekerlek sensor kablolarini kontrol ediniz."}},
    {"P0638", {"P0638", "ECM", "Gaz Kelebegi Aktuator Kontrolu (Hava Kapatma)", "Durdurma kelebegi (anti-shudder valve) motor dislilerini ve soketini kontrol ediniz."}},
    {"P0670", {"P0670", "ECM", "Kizdirma Bujisi Modulu Kontrol Devresi", "On isitma kontrol unitesi ana besleme sigortasini, rolesini ve tesisatini kontrol ediniz."}},
    {"P2002", {"P2002", "ECM", "Dizel Partikul Filtresi (DPF) Verimliligi Dusuk", "DPF kurum dolulugunu kontrol ediniz, rejenerasyon veya temizlik gerekebilir."}},
    {"P242A", {"P242A", "ECM", "Egzoz Gazi Sicaklik Sensoru 3 (EGT Bank 1 Sensor 3)", "DPF oncesi/sonrasi sicaklik sensoru ve soketini kontrol ediniz."}},
    {"P2452", {"P2452", "ECM", "DPF Diferansiyel Basinc Sensoru Devresi", "Sensor elektrik devresi ve 5V besleme voltajini olcunuz."}},
    {"P2453", {"P2453", "ECM", "DPF Diferansiyel Basinc Sensoru Performans", "Diferansiyel basinc sensorunun DPF'ye giden metal/silikon borularindaki tikanikligi kontrol ediniz."}},
    {"P2454", {"P2454", "ECM", "DPF Diferansiyel Basinc Sensoru Dusuk Voltaj", "Sasiye kisa devre veya sensor arizasi."}},
    {"C1015", {"C1015", "ABS/ESP", "Vakum Basinc Sensoru Devresi Arizasi", "Fren servosu (westinghouse) vakum sensoru devresini kontrol ediniz."}},
    {"C1021", {"C1021", "ABS/ESP", "On Sag Tekerlek Hiz Sensoru Sinyal Yok / Devre Acik", "Sag on tekerlek porya manyetik halkasi ve sensor kablosunu kontrol ediniz."}},
    {"C1022", {"C1022", "ABS/ESP", "On Sag Tekerlek Hiz Sensoru Aralik / Performans", "Porya rulmani boslugu veya manyetik disli kirlenmesini temizleyiniz."}},
    {"C1025", {"C1025", "ABS/ESP", "On Sol Tekerlek Hiz Sensoru Sinyal Yok / Devre Acik", "Sol on tekerlek hiz sensor soketi ve kablosunu kontrol ediniz."}},
    {"C1026", {"C1026", "ABS/ESP", "On Sol Tekerlek Hiz Sensoru Aralik / Performans", "Sol on porya ve hiz sensor araligini kontrol ediniz."}},
    {"C1028", {"C1028", "ABS/ESP", "Fren Ana Merkez Basinc Sensoru Devresi", "ESP hidrolik blok uzerindeki dahili basinc sensoru kalibrasyonunu kontrol ediniz."}},
    {"C1031", {"C1031", "ABS/ESP", "Arka Sag Tekerlek Hiz Sensoru Sinyal Yok", "Arka sag aks hiz sensoru baglantisini kontrol ediniz."}},
    {"C1032", {"C1032", "ABS/ESP", "Arka Sag Tekerlek Hiz Sensoru Aralik / Performans", "Arka sag porya rulmani manyetik seridini kontrol ediniz."}},
    {"C1035", {"C1035", "ABS/ESP", "Arka Sol Tekerlek Hiz Sensoru Sinyal Yok", "Arka sol aks hiz sensoru kablosunu test ediniz."}},
    {"C1036", {"C1036", "ABS/ESP", "Arka Sol Tekerlek Hiz Sensoru Aralik / Performans", "Arka sol porya manyetik halkasini kontrol ediniz."}},
    {"C1061", {"C1061", "ABS/ESP", "ABS Pompa Motoru Devresi Arizasi", "ABS ana motor sigortasi (40A) ve pompa motor fircalarini kontrol ediniz."}},
    {"C1063", {"C1063", "ABS/ESP", "ABS Solenoid Valf Guc Rolesi Devresi", "ESP modulu solenoid valf besleme rolesi devresini kontrol ediniz."}},
    {"C1071", {"C1071", "ABS/ESP", "ESP Kontrol Modulu Dahili Bellek Hatasi", "ABS/ESP elektronik kontrol unitesi besleme voltajini kontrol ediniz."}},
    {"C1074", {"C1074", "ABS/ESP", "G Sensoru Kalibrasyonu Tamamlanmadi", "Arac duz zemindeyken Sensor Kalibrasyonu (ABS-03) islemini calistiriniz."}},
    {"C1075", {"C1075", "ABS/ESP", "Direksiyon Aci Sensoru (SAS) Kalibrasyonu Eksik", "Direksiyon tam duz konumdayken SAS Kalibrasyonu (ABS-03 / VDC-101) yapiniz."}},
    {"C1076", {"C1076", "ABS/ESP", "Ana Silindir Basinc Sensoru Kalibrasyonu Eksik", "Fren pedalina basmadan Sensor Kalibrasyonu (ABS-03) islemini yapiniz."}},
    {"C1077", {"C1077", "ABS/ESP", "Boyuna G Sensoru / Savrulma Sensoru Kalibrasyonu Eksik", "Arac duz zemindeyken Sensor Kalibrasyonu (ABS-03) islemini yapiniz."}},
    {"C1078", {"C1078", "ABS/ESP", "Yanal G Sensoru / Savrulma Sensoru Kalibrasyonu Eksik", "Arac duz zemindeyken Sensor Kalibrasyonu (ABS-03) islemini yapiniz."}},
    {"C1079", {"C1079", "ABS/ESP", "Direksiyon Aci Sensoru (SAS) Kalibrasyon Hatasi", "Direksiyon simidi duz konumdayken SAS Kalibrasyonu (Sifirlama) islemi yapiniz."}},
    {"C107A", {"C107A", "ABS/ESP", "Savrulma Acisi (Yaw Rate) Sensoru Kalibrasyonu Eksik", "Arac duz zemindeyken Sensor Kalibrasyonu (ABS-03) islemini yapiniz."}},
    {"C1091", {"C1091", "ABS/ESP", "Direksiyon Aci Sensoru (SAS) Iletisim / Kalibrasyon Hatasi", "Direksiyon simidi duz konumdayken SAS Kalibrasyonu (Sifirlama) islemi yapiniz."}},
    {"C1093", {"C1093", "ABS/ESP", "Savrulma / Yanal Ivme Sensoru (Yaw / G-Sensor)", "Konsol altindaki savrulma sensoru montaji ve soketini kontrol ediniz."}},
    {"B1011", {"B1011", "SRS", "Surucu Hava Yastigi Atesleyici Devresi Acik", "Direksiyon zemberegi (clock spring / spiral kablo) baglantisini kontrol ediniz."}},
    {"B1012", {"B1012", "SRS", "Surucu Hava Yastigi Devresi Yuksek Direnc", "Direksiyon alti sari hava yastigi soket temasini kontrol ediniz."}},
    {"B1016", {"B1016", "SRS", "On Yolcu Hava Yastigi Devresi Acik", "Torpido ici yolcu hava yastigi baglanti soketini kontrol ediniz."}},
    {"B1021", {"B1021", "SRS", "Surucu Emniyet Kemeri On Gergi Devresi Acik", "Surucu koltugu altindaki sari emniyet kemeri fisini kontrol ediniz."}},
    {"B1026", {"B1026", "SRS", "Yolcu Emniyet Kemeri On Gergi Devresi Acik", "Yolcu koltugu altindaki sari soket baglantisini kontrol ediniz."}},
    {"B1031", {"B1031", "SRS", "Surucu Yan Hava Yastigi Devresi Acik", "Surucu koltugu sirtlik yan hava yastigi soketini kontrol ediniz."}},
    {"B1036", {"B1036", "SRS", "Yolcu Yan Hava Yastigi Devresi Acik", "Yolcu koltugu yan hava yastigi soketini kontrol ediniz."}},
    {"B1041", {"B1041", "SRS", "On Darbe Sensoru Arizasi", "On panel / radyator yanindaki darbe sensoru kablosunu kontrol ediniz."}},
    {"B1051", {"B1051", "SRS", "Hava Yastigi Kontrol Modulu Dahili Hata", "SRS beyni kaza kaydi veya dahili donanim arizasi."}},
    {"C1201", {"C1201", "4WD", "4WD Aktuator / Vites Motor Devresi Arizasi", "Transfer kutusu uzerindeki elektrikli 4WD motoru ve soketini kontrol ediniz."}},
    {"C1211", {"C1211", "4WD", "Transfer Kutusu Vites Pozisyon Salteri Arizasi", "4H, 4H-Lock ve 4L-Lock konum salterlerini kontrol ediniz."}},
    {"C1221", {"C1221", "4WD", "4WD Coklu Plakali Kavrama Asiri Isinma", "Arazi sartlarinda zorlanma sonucu asiri isinma korumasi, sogumasini bekleyiniz."}},
    {"C1222", {"C1222", "4WD", "4WD Diferansiyel Hiz Uyusmazligi", "Lastik ebatlari veya dis derinlikleri arasindaki farklari kontrol ediniz."}},
    {"U0100", {"U0100", "CAN", "Motor Kontrol Modulu (ECM) ile Iletisim Kaybi", "Motor beyni ana besleme rolesi ve CAN hatti direnclerini kontrol ediniz."}},
    {"U0121", {"U0121", "CAN", "ABS/ESP Kontrol Modulu ile Iletisim Kaybi", "Fren beyni sigortasi ve CAN haberlesme kablolarini kontrol ediniz."}},
    {"U0126", {"U0126", "CAN", "Direksiyon Aci Sensoru ile Iletisim Kaybi", "Direksiyon simidi altindaki aci sensoru baglantisini kontrol ediniz."}},
    {"U0155", {"U0155", "CAN", "Gosterge Paneli (IPC) ile Iletisim Kaybi", "Gosterge tablosu arkasindaki soket ve CAN sinyalini kontrol ediniz."}},
    {"U1073", {"U1073", "CAN", "CAN Veri Yolu Iletisim Kesintisi (Bus-Off)", "CAN-H (Pin 6) ve CAN-L (Pin 14) arasinda 60 Ohm hat direnci olcunuz."}},
    {"U1100", {"U1100", "BCM", "Govde Kontrol Unitesi (BCM) ile Iletisim Kaybi", "BCM sigortalari ve soket baglantilarini kontrol ediniz."}}
};

std::string FormatDTCCode(unsigned char b1, unsigned char b2) {
    char type = 'P';
    unsigned char t = (b1 >> 6) & 0x03;
    if (t == 0) type = 'P';
    else if (t == 1) type = 'C';
    else if (t == 2) type = 'B';
    else if (t == 3) type = 'U';

    char hexCode[16];
    sprintf_s(hexCode, "%c%01X%01X%02X", type, (b1 >> 4) & 3, b1 & 15, b2);
    return std::string(hexCode);
}

#include "suzuki_dtc.h"

DTCEntry LookupDTC(const std::string& code, const std::string& defaultMod, int variantId = 0) {
    auto it = g_dtcDb.find(code);
    if (it != g_dtcDb.end()) {
        return it->second;
    }
    std::string trText, enText;
    if (LookupDbDtcText(variantId, code, trText, enText) && !trText.empty()) {
        return {code, defaultMod, trText, !enText.empty() && enText != trText ? ("DTC DB: " + enText) : "Suzuki ariza tanimi (" + code + ")."};
    }
    return {code, defaultMod, "Diyagnostik Ariza Kodu (" + code + ")", "Servis el kilavuzundan bu kodun detayli test adimlarini inceleyiniz."};
}

// Signed 16-bit helper for diagnostic conversions
int ReadSigned16BE(const std::vector<unsigned char>& payload, size_t dataOffset) {
    size_t idx = 2 + dataOffset; // payload[0]=0x61, payload[1]=LID
    if (idx + 1 >= payload.size()) return 0;
    int raw = (static_cast<int>(payload[idx]) << 8) | static_cast<int>(payload[idx + 1]);
    return (raw >= 32768) ? (raw - 65536) : raw;
}

unsigned int ReadUnsigned16BE(const std::vector<unsigned char>& payload, size_t dataOffset) {
    size_t idx = 2 + dataOffset;
    if (idx + 1 >= payload.size()) return 0;
    return (static_cast<unsigned int>(payload[idx]) << 8) | static_cast<unsigned int>(payload[idx + 1]);
}

int ReadSigned32BE(const std::vector<unsigned char>& payload, size_t dataOffset) {
    size_t idx = 2 + dataOffset;
    if (idx + 3 >= payload.size()) return 0;
    uint32_t raw = (static_cast<uint32_t>(payload[idx]) << 24) |
                   (static_cast<uint32_t>(payload[idx + 1]) << 16) |
                   (static_cast<uint32_t>(payload[idx + 2]) << 8) |
                   static_cast<uint32_t>(payload[idx + 3]);
    return static_cast<int32_t>(raw);
}

struct CompleteLiveData {
    double vbatt = std::numeric_limits<double>::quiet_NaN();
    double ecmVbatt = std::numeric_limits<double>::quiet_NaN();
    double rpm = std::numeric_limits<double>::quiet_NaN();
    double desiredIdle = std::numeric_limits<double>::quiet_NaN();
    double speed = std::numeric_limits<double>::quiet_NaN();
    double coolantTemp = std::numeric_limits<double>::quiet_NaN();
    double intakeAirTemp = std::numeric_limits<double>::quiet_NaN();
    double fuelTemp = std::numeric_limits<double>::quiet_NaN();
    double baroPress = std::numeric_limits<double>::quiet_NaN();
    double boostPress = std::numeric_limits<double>::quiet_NaN();
    double pedalPos = std::numeric_limits<double>::quiet_NaN();
    double railPressBar = std::numeric_limits<double>::quiet_NaN();
    double dpfSootGram = std::numeric_limits<double>::quiet_NaN();
    double dpfTemp = std::numeric_limits<double>::quiet_NaN();
    double dpfDiffPress = std::numeric_limits<double>::quiet_NaN();
    double steeringAngle = std::numeric_limits<double>::quiet_NaN();
    double yawRate = std::numeric_limits<double>::quiet_NaN();
    double lateralG = std::numeric_limits<double>::quiet_NaN();
    bool sasValid = false;
    std::string sasStatus = "UNKNOWN";
    bool ecmOnline = false;
    bool absOnline = false;
    std::string absMode;
    bool connected = false;
};

CompleteLiveData QueryCompleteLiveData() {
    CompleteLiveData data;
    data.vbatt = g_diag.ReadBatteryVoltage();
    if (!std::isnan(data.vbatt)) {
        data.connected = true;
    }

    if (g_activeProfileId != "suzuki_kline") {
        bool f9qDataReceived = false;
        std::vector<std::vector<unsigned char>> ecmReqs = {
            {0x10, 0xC0},
            {0x21, 0xA0},
            {0x21, 0xA1},
            {0x21, 0xA6},
            {0x21, 0xA7},
            {0x21, 0xC0}
        };
        std::vector<std::vector<unsigned char>> ecmResps;
        if ((g_activeProfileId == "gv_jb419d" || g_activeProfileId == "auto") && SendBatch(g_diag, 0x7E0, 0x7E8, ecmReqs, ecmResps, 180)) {
            if (!ecmResps.empty() && !ecmResps[0].empty() && ecmResps[0][0] == 0x50) {
                data.ecmOnline = true;
                data.connected = true;
            }
            if (ecmResps.size() > 1 && ecmResps[1].size() >= 2 + 14 && ecmResps[1][0] == 0x61 && ecmResps[1][1] == 0xA0) {
                data.ecmOnline = true;
                data.connected = true;
                f9qDataReceived = true;
                if (g_activeProfileId == "auto") {
                    g_activeProfileId = "gv_jb419d";
                    g_detectedVehicleDesc = "Grand Vitara 1.9 DDiS (JB419D - Bosch EDC16C39 F9Q)";
                }
                data.desiredIdle = ReadSigned16BE(ecmResps[1], 8) * 1.0;
                data.rpm = ReadSigned16BE(ecmResps[1], 10) * 1.0;
                data.speed = ReadSigned16BE(ecmResps[1], 12) * 0.01;
            }
            if (ecmResps.size() > 2 && ecmResps[2].size() >= 2 + 49 && ecmResps[2][0] == 0x61 && ecmResps[2][1] == 0xA1) {
                data.ecmOnline = true;
                data.connected = true;
                f9qDataReceived = true;
                data.coolantTemp = ReadSigned16BE(ecmResps[2], 22) * 0.1 - 273.0;
                data.intakeAirTemp = ReadSigned16BE(ecmResps[2], 24) * 0.1 - 273.0;
                data.fuelTemp = ReadSigned16BE(ecmResps[2], 26) * 0.1 - 273.0;
                data.baroPress = ReadSigned16BE(ecmResps[2], 30) * 1.0;
                data.boostPress = ReadSigned16BE(ecmResps[2], 32) * 1.0;
                data.ecmVbatt = ReadSigned16BE(ecmResps[2], 38) * 0.0195503421309873;
                data.pedalPos = ReadSigned16BE(ecmResps[2], 47) * 0.01220703125;
            }
            if (ecmResps.size() > 3 && ecmResps[3].size() >= 2 + 36 && ecmResps[3][0] == 0x61 && ecmResps[3][1] == 0xA6) {
                data.railPressBar = ReadSigned16BE(ecmResps[3], 32) * 0.1;
            }
            if (ecmResps.size() > 4 && ecmResps[4].size() >= 2 + 51 && ecmResps[4][0] == 0x61 && ecmResps[4][1] == 0xA7) {
                data.dpfSootGram = ReadSigned32BE(ecmResps[4], 23) * 0.01;
                data.dpfTemp = ReadSigned16BE(ecmResps[4], 45) * 0.1 - 273.0;
            }
            if (ecmResps.size() > 5 && ecmResps[5].size() >= 2 + 14 && ecmResps[5][0] == 0x61 && ecmResps[5][1] == 0xC0) {
                data.dpfDiffPress = ReadSigned16BE(ecmResps[5], 12) * 1.0;
            }
        }

        // 1B. Universal Suzuki CAN KWP (21 00 / 21 01) & OBD-II Mode 01 Live Data Fallback
        // Runs on all non-F9Q Suzuki CAN vehicles (Swift, Vitara, SX4, S-Cross, Jimny, Petrol Grand Vitara, etc.)
        if (!f9qDataReceived && g_activeProfileId != "gv_jb419d") {
            std::vector<std::vector<unsigned char>> suzCanReqs = {
                {0x10, 0xC0},
                {0x21, 0x00},
                {0x21, 0x01}
            };
            std::vector<std::vector<unsigned char>> suzCanResps;
            if (SendBatch(g_diag, 0x7E0, 0x7E8, suzCanReqs, suzCanResps, 150)) {
                if (suzCanResps.size() > 1 && suzCanResps[1].size() >= 2 + 24 && suzCanResps[1][0] == 0x61 && suzCanResps[1][1] == 0x00) {
                    const auto& r0 = suzCanResps[1];
                    data.ecmOnline = true;
                    data.connected = true;
                    data.ecmVbatt = r0[2 + 12] * 0.0784313725490196;
                    data.coolantTemp = static_cast<int>(r0[2 + 16]) - 40.0;
                    data.rpm = ((static_cast<int>(r0[2 + 22]) << 8) | r0[2 + 23]) * 0.25;
                    if (r0.size() >= 2 + 44) {
                        data.intakeAirTemp = static_cast<int>(r0[2 + 43]) - 40.0;
                    }
                }
                if (suzCanResps.size() > 2 && suzCanResps[2].size() >= 2 + 39 && suzCanResps[2][0] == 0x61 && suzCanResps[2][1] == 0x01) {
                    const auto& r1 = suzCanResps[2];
                    data.ecmOnline = true;
                    data.connected = true;
                    if (std::isnan(data.rpm)) {
                        data.rpm = ((static_cast<int>(r1[2 + 37]) << 8) | r1[2 + 38]) * 0.25;
                    }
                }
            }

            std::vector<std::vector<unsigned char>> obdReqs = {
                {0x01, 0x0C}, // RPM
                {0x01, 0x0D}, // Speed
                {0x01, 0x05}, // Coolant Temp
                {0x01, 0x0F}, // Intake Air Temp
                {0x01, 0x0B}, // MAP / Boost (kPa)
                {0x01, 0x23}, // Fuel Rail Pressure (10 kPa)
                {0x01, 0x42}, // Control Module Voltage (mV)
                {0x01, 0x11}  // Throttle / Pedal Position (%)
            };
            std::vector<std::vector<unsigned char>> obdResps;
            if (SendBatch(g_diag, 0x7E0, 0x7E8, obdReqs, obdResps, 150)) {
                if (obdResps.size() > 0 && obdResps[0].size() >= 4 && obdResps[0][0] == 0x41 && obdResps[0][1] == 0x0C) {
                    data.ecmOnline = true;
                    data.connected = true;
                    data.rpm = ((static_cast<int>(obdResps[0][2]) << 8) | obdResps[0][3]) * 0.25;
                }
                if (obdResps.size() > 1 && obdResps[1].size() >= 3 && obdResps[1][0] == 0x41 && obdResps[1][1] == 0x0D) {
                    data.ecmOnline = true;
                    data.speed = obdResps[1][2] * 1.0;
                }
                if (obdResps.size() > 2 && obdResps[2].size() >= 3 && obdResps[2][0] == 0x41 && obdResps[2][1] == 0x05) {
                    data.coolantTemp = static_cast<int>(obdResps[2][2]) - 40.0;
                }
                if (obdResps.size() > 3 && obdResps[3].size() >= 3 && obdResps[3][0] == 0x41 && obdResps[3][1] == 0x0F) {
                    data.intakeAirTemp = static_cast<int>(obdResps[3][2]) - 40.0;
                }
                if (obdResps.size() > 4 && obdResps[4].size() >= 3 && obdResps[4][0] == 0x41 && obdResps[4][1] == 0x0B) {
                    data.boostPress = obdResps[4][2] * 10.0; // kPa -> hPa
                }
                if (obdResps.size() > 5 && obdResps[5].size() >= 4 && obdResps[5][0] == 0x41 && obdResps[5][1] == 0x23) {
                    data.railPressBar = ((static_cast<int>(obdResps[5][2]) << 8) | obdResps[5][3]) * 0.1; // 10 kPa = 0.1 bar
                }
                if (obdResps.size() > 6 && obdResps[6].size() >= 4 && obdResps[6][0] == 0x41 && obdResps[6][1] == 0x42) {
                    data.ecmVbatt = ((static_cast<int>(obdResps[6][2]) << 8) | obdResps[6][3]) * 0.001;
                }
                if (obdResps.size() > 7 && obdResps[7].size() >= 3 && obdResps[7][0] == 0x41 && obdResps[7][1] == 0x11) {
                    data.pedalPos = obdResps[7][2] * (100.0 / 255.0);
                }
            }
        }
    }

    // 1C. Classic Suzuki K-Line ECM (0x11) Fallback if CAN ECM did not respond and profile allows K-Line ECM
    if (!data.ecmOnline && (g_activeProfileId == "suzuki_kline" || g_activeProfileId == "gv_petrol" || g_activeProfileId == "auto")) {
        std::vector<std::vector<unsigned char>> kEcmReqs = {
            {0x01, 0x0C},
            {0x01, 0x0D},
            {0x01, 0x05},
            {0x01, 0x0F},
            {0x01, 0x0B},
            {0x01, 0x11},
            {0x21, 0x00},
            {0x21, 0x08}
        };
        std::vector<std::vector<unsigned char>> kEcmResps;
        if (g_diag.RunKLineSession(0x11, kEcmReqs, kEcmResps, 280)) {
            if (kEcmResps.size() > 0 && kEcmResps[0].size() >= 4 && kEcmResps[0][0] == 0x41 && kEcmResps[0][1] == 0x0C) {
                data.ecmOnline = true;
                data.connected = true;
                data.rpm = ((static_cast<int>(kEcmResps[0][2]) << 8) | kEcmResps[0][3]) * 0.25;
            }
            if (kEcmResps.size() > 1 && kEcmResps[1].size() >= 3 && kEcmResps[1][0] == 0x41 && kEcmResps[1][1] == 0x0D) {
                data.ecmOnline = true;
                data.speed = kEcmResps[1][2] * 1.0;
            }
            if (kEcmResps.size() > 2 && kEcmResps[2].size() >= 3 && kEcmResps[2][0] == 0x41 && kEcmResps[2][1] == 0x05) {
                data.coolantTemp = static_cast<int>(kEcmResps[2][2]) - 40.0;
            }
            if (kEcmResps.size() > 3 && kEcmResps[3].size() >= 3 && kEcmResps[3][0] == 0x41 && kEcmResps[3][1] == 0x0F) {
                data.intakeAirTemp = static_cast<int>(kEcmResps[3][2]) - 40.0;
            }
            if (kEcmResps.size() > 4 && kEcmResps[4].size() >= 3 && kEcmResps[4][0] == 0x41 && kEcmResps[4][1] == 0x0B) {
                data.boostPress = kEcmResps[4][2] * 10.0;
            }
            if (kEcmResps.size() > 5 && kEcmResps[5].size() >= 3 && kEcmResps[5][0] == 0x41 && kEcmResps[5][1] == 0x11) {
                data.pedalPos = kEcmResps[5][2] * (100.0 / 255.0);
            }
            if (kEcmResps.size() > 6 && kEcmResps[6].size() >= 2 + 24 && kEcmResps[6][0] == 0x61 && kEcmResps[6][1] == 0x00) {
                const auto& r0 = kEcmResps[6];
                data.ecmOnline = true;
                data.connected = true;
                if (std::isnan(data.ecmVbatt)) data.ecmVbatt = r0[2 + 12] * 0.0784313725490196;
                if (std::isnan(data.coolantTemp)) data.coolantTemp = static_cast<int>(r0[2 + 16]) - 40.0;
                if (std::isnan(data.rpm)) data.rpm = ((static_cast<int>(r0[2 + 22]) << 8) | r0[2 + 23]) * 0.25;
            }
            if (kEcmResps.size() > 7 && kEcmResps[7].size() >= 2 + 23 && kEcmResps[7][0] == 0x61 && kEcmResps[7][1] == 0x08) {
                const auto& r8 = kEcmResps[7];
                data.ecmOnline = true;
                data.connected = true;
                if (std::isnan(data.pedalPos)) data.pedalPos = r8[2 + 7] * 0.392156862745098;
                if (std::isnan(data.coolantTemp)) data.coolantTemp = static_cast<int>(r8[2 + 16]) - 40.0;
                if (std::isnan(data.rpm)) data.rpm = ((static_cast<int>(r8[2 + 21]) << 8) | r8[2 + 22]) * 0.25;
            }
        }
    }

    // 2. Query ABS/ESP: Try CAN (0x261/0x661) first if profile is not pure K-Line
    if (g_activeProfileId != "suzuki_kline") {
        std::vector<std::vector<unsigned char>> absCanReqs = {
            {0x10, 0xC0},
            {0x21, 0x00},
            {0x22, 0x12, 0x08}
        };
        std::vector<std::vector<unsigned char>> absCanResps;
        if (SendBatch(g_diag, 0x261, 0x661, absCanReqs, absCanResps, 150)) {
            if (absCanResps.size() > 1 && absCanResps[1].size() >= 2 + 26 && absCanResps[1][0] == 0x61 && absCanResps[1][1] == 0x00) {
                data.absOnline = true;
                data.absMode = "CAN 0x261 (21 00)";
                if (std::isnan(data.speed)) {
                    double rf = absCanResps[1][2 + 7];
                    double lf = absCanResps[1][2 + 8];
                    data.speed = (rf + lf) * 0.5;
                }
                data.lateralG = absCanResps[1][2 + 18] * 0.02 - 2.56;
                data.yawRate = ReadUnsigned16BE(absCanResps[1], 19) * 0.05 - 1638.4;
                data.steeringAngle = ReadSigned16BE(absCanResps[1], 24) * 1.0;
            } else if (absCanResps.size() > 2 && absCanResps[2].size() >= 5 && absCanResps[2][0] == 0x62 && absCanResps[2][1] == 0x12 && absCanResps[2][2] == 0x08) {
                data.absOnline = true;
                data.absMode = "CAN 0x261 (22 12 08)";
                int raw = (static_cast<int>(absCanResps[2][3]) << 8) | absCanResps[2][4];
                data.steeringAngle = (raw >= 32768) ? (raw - 65536) * 0.1 : raw * 0.1;
            }
        }
    }

    // 3. If ABS/ESP did not respond on CAN and profile supports K-Line ABS (0x29), try K-Line 0x29
    if (!data.absOnline && g_activeProfileId != "suzuki_can") {
        std::vector<std::vector<unsigned char>> kResps;
        if (g_diag.RunKLineSession(0x29, {{0x21, 0x00}}, kResps, 300) && !kResps.empty() && kResps[0].size() >= 2 + 22 && kResps[0][0] == 0x61 && kResps[0][1] == 0x00) {
            data.absOnline = true;
            data.absMode = "K-Line 0x29 (21 00)";
            const auto& r = kResps[0];
            if (std::isnan(data.speed)) {
                double rf = r[2 + 7];
                double lf = r[2 + 8];
                data.speed = (rf + lf) * 0.5;
            }
            data.lateralG = r[2 + 18] * 0.02 - 2.56;
            data.yawRate = static_cast<double>(r[2 + 19]) - 128.0;
            data.steeringAngle = ReadSigned16BE(r, 20) * 1.0; // offset 20, width 2, m=1
            if (r.size() >= 2 + 27) {
                data.sasValid = (r[2 + 26] & 0x40) != 0;
                data.sasStatus = data.sasValid ? "OK" : "UNSET_OR_UNCALIBRATED";
            }
        }
    }

    return data;
}

struct ModuleScanResult {
    std::string shortName;
    std::string name;
    std::string variant;
    std::string canId;
    std::string status;
    bool responsive = false;
    int dtcCount = -1;
};

void ParseKwpDtcList(const std::vector<unsigned char>& resp, const std::string& modShortName, std::vector<DTCEntry>& dtcList, ModuleScanResult& res) {
    // Handles both:
    // - 57 [count] [MSB LSB status] * count (Suzuki Service 17 / 18 with count byte)
    // - 58 [MSB LSB status] * count (ISO 14230 Service 18 without count byte if (size - 1) % 3 == 0)
    if (resp.size() < 2) {
        res.status = "Eksik DTC yaniti";
        res.dtcCount = -1;
        return;
    }

    size_t idx = 2;
    int count = resp[1];
    if (resp[0] == 0x58 && (resp.size() - 2) != static_cast<size_t>(count) * 3 && (resp.size() - 1) % 3 == 0) {
        count = static_cast<int>((resp.size() - 1) / 3);
        idx = 1;
    } else if (resp.size() < 2 + static_cast<size_t>(count) * 3) {
        if ((resp.size() - 1) % 3 == 0) {
            count = static_cast<int>((resp.size() - 1) / 3);
            idx = 1;
        } else {
            res.status = "Beklenmeyen DTC yanit uzunlugu (" + std::to_string(resp.size()) + " bayt)";
            res.dtcCount = -1;
            return;
        }
    }

    int validCount = 0;
    for (int i = 0; i < count && (idx + 1) < resp.size(); i++) {
        unsigned char b1 = resp[idx];
        unsigned char b2 = resp[idx + 1];
        if (b1 != 0x00 || b2 != 0x00) {
            std::string code = FormatDTCCode(b1, b2);
            dtcList.push_back(LookupDTC(code, modShortName));
            validCount++;
        }
        idx += 3;
    }

    res.dtcCount = validCount;
    if (validCount == 0) {
        res.status = "Normal (Ariza Kodu Yok)";
    } else {
        res.status = std::to_string(validCount) + " Adet Ariza Kodu Bulundu";
    }
}

void ParseUdsDtcList19(const std::vector<unsigned char>& resp, const std::string& modShortName, std::vector<DTCEntry>& dtcList, ModuleScanResult& res) {
    // Response to 19 02 <mask> (SUZUKI CAN ECM BOSCH F9Q):
    // 59 02 <statusAvailabilityMask> [DTC_HI DTC_MID DTC_LO STATUS] * N
    if (resp.size() < 3 || resp[0] != 0x59 || resp[1] != 0x02) {
        res.status = "Eksik 19 02 DTC yaniti";
        res.dtcCount = -1;
        return;
    }
    size_t payloadLen = resp.size() - 3;
    int validCount = 0;
    if (payloadLen % 4 != 0) {
        res.status = "Malformed 19 02 response: incomplete DTC/status record";
        res.dtcCount = -1;
        return;
    }
    size_t step = 4;
    for (size_t idx = 3; idx + 1 < resp.size(); idx += step) {
        unsigned char b1 = resp[idx];
        unsigned char b2 = resp[idx + 1];
        if (b1 != 0x00 || b2 != 0x00) {
            std::string code = FormatDTCCode(b1, b2);
            dtcList.push_back(LookupDTC(code, modShortName));
            validCount++;
        }
    }
    res.dtcCount = validCount;
    if (validCount == 0) {
        res.status = "Normal (Ariza Kodu Yok)";
    } else {
        res.status = std::to_string(validCount) + " Adet Ariza Kodu Bulundu";
    }
}

// Processes a completed K-Line scan batch ([0..3]=1A id, [4..5]=18 DTC queries, [6]=21 08).
// Returns true only when a genuine 0x58/0x57 DTC read response was parsed; never sets dtcCount=0 on rejection.
bool EvaluateKLineDtcScan(unsigned char kAddr, const std::string& modShortName,
                          const std::vector<std::vector<unsigned char>>& kResps,
                          std::vector<DTCEntry>& dtcList, ModuleScanResult& res) {
    res.responsive = true;
    std::string partNum;
    for (size_t i = 0; i < 4 && i < kResps.size(); ++i) {
        const auto& kr = kResps[i];
        if (kr.size() >= 5 && kr[0] == 0x5A) {
            std::string s;
            for (size_t b = 2; b < kr.size(); ++b) {
                char c = static_cast<char>(kr[b]);
                if (c >= 32 && c <= 126 && c != '*') s += c;
            }
            if (!s.empty() && partNum.empty()) partNum = s;
        }
    }
    if (!partNum.empty()) {
        res.variant += " [" + partNum + "]";
    }

    std::string dtcRejectDetail;
    for (size_t idx : {size_t(4), size_t(5)}) {
        if (idx >= kResps.size()) continue;
        const auto& kr = kResps[idx];
        if (kr.size() >= 2 && (kr[0] == 0x58 || kr[0] == 0x57)) {
            ParseKwpDtcList(kr, modShortName, dtcList, res);
            res.status += " (K-Line " + HexAddr(kAddr, 2) + ")";
            return res.dtcCount >= 0;
        }
        if (!kr.empty() && dtcRejectDetail.empty()) {
            dtcRejectDetail = HexBytes(kr);
        }
    }
    res.dtcCount = -1;
    res.status = "Modul K-Line " + HexAddr(kAddr, 2) + " uzerinden yanit verdi, ancak DTC okuma servisi basarisiz/reddedildi (" +
                 (dtcRejectDetail.empty() ? "yanit yok" : dtcRejectDetail) + ")";
    return false;
}

std::string ScanDTCsForModule(const ModuleDef& mod, std::vector<DTCEntry>& dtcList, ModuleScanResult& res) {
    res.shortName = mod.shortName;
    res.name = mod.name;
    res.variant = mod.ecuVariant;
    char idBuf[96];
    if (mod.txId != 0 && mod.klineAddr != 0) {
        sprintf_s(idBuf, "CAN 0x%03X/0x%03X | K-Line 0x%02X", mod.txId, mod.rxId, mod.klineAddr);
    } else if (mod.txId != 0) {
        sprintf_s(idBuf, "CAN 0x%03X/0x%03X", mod.txId, mod.rxId);
    } else {
        sprintf_s(idBuf, "K-Line 0x%02X (Pin 7)", mod.klineAddr);
    }
    res.canId = idBuf;
    res.dtcCount = -1;
    res.responsive = false;

    std::vector<unsigned char> resp;

    // 1. Try CAN Bus (ISO 15765, 500 kbps) if module has a CAN address (primary or alternate)
    bool sessionOk = false;
    unsigned long activeTx = mod.txId;
    unsigned long activeRx = mod.rxId;
    if (mod.txId != 0) {
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x10, 0xC0}, resp, 450) && resp.size() >= 2 && resp[0] == 0x50) {
            sessionOk = true;
        } else if (g_diag.SendAndReceive(activeTx, activeRx, {0x10, 0x81}, resp, 350) && resp.size() >= 2 && resp[0] == 0x50) {
            sessionOk = true;
        } else if (g_diag.SendAndReceive(activeTx, activeRx, {0x10, 0x03}, resp, 350) && resp.size() >= 2 && resp[0] == 0x50) {
            sessionOk = true;
        } else if (g_diag.SendAndReceive(activeTx, activeRx, {0x1A, 0x8E}, resp, 350) && resp.size() >= 2 && resp[0] == 0x5A) {
            sessionOk = true;
        } else if (g_diag.SendAndReceive(activeTx, activeRx, {0x18, 0x00, 0xFF, 0x00}, resp, 350) && resp.size() >= 2 && resp[0] == 0x58) {
            sessionOk = true;
        } else if (mod.altTxId != 0) {
            if (g_diag.SendAndReceive(mod.altTxId, mod.altRxId, {0x10, 0xC0}, resp, 450) && resp.size() >= 2 && resp[0] == 0x50) {
                sessionOk = true;
                activeTx = mod.altTxId;
                activeRx = mod.altRxId;
            } else if (g_diag.SendAndReceive(mod.altTxId, mod.altRxId, {0x10, 0x81}, resp, 350) && resp.size() >= 2 && resp[0] == 0x50) {
                sessionOk = true;
                activeTx = mod.altTxId;
                activeRx = mod.altRxId;
            } else if (g_diag.SendAndReceive(mod.altTxId, mod.altRxId, {0x10, 0x03}, resp, 350) && resp.size() >= 2 && resp[0] == 0x50) {
                sessionOk = true;
                activeTx = mod.altTxId;
                activeRx = mod.altRxId;
            } else if (g_diag.SendAndReceive(mod.altTxId, mod.altRxId, {0x1A, 0x8E}, resp, 350) && resp.size() >= 2 && resp[0] == 0x5A) {
                sessionOk = true;
                activeTx = mod.altTxId;
                activeRx = mod.altRxId;
            } else if (g_diag.SendAndReceive(mod.altTxId, mod.altRxId, {0x18, 0x00, 0xFF, 0x00}, resp, 350) && resp.size() >= 2 && resp[0] == 0x58) {
                sessionOk = true;
                activeTx = mod.altTxId;
                activeRx = mod.altRxId;
            }
        }
    }

    if (sessionOk) {
        res.responsive = true;
        std::vector<unsigned char> idResp;
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x1A, 0x8E}, idResp, 200) && idResp.size() >= 12 && idResp[0] == 0x5A) {
            std::string cid(idResp.begin() + 2, idResp.begin() + 12);
            res.variant += " [ID: " + cid + "]";
        } else if (g_diag.SendAndReceive(activeTx, activeRx, {0x22, 0xF1, 0x01}, idResp, 200) && idResp.size() >= 13 && idResp[0] == 0x62) {
            std::string cid(idResp.begin() + 3, idResp.begin() + 13);
            res.variant += " [UDS ID: " + cid + "]";
        }

        // Try diagnostic DTC read services in order:
        // - Bosch F9Q ECM (Base 93, dtc_read_info_set=93 '29'): 17 29 or 19 02 29
        // - UDS modules (Suzuki/UDS): 19 02 FF or 19 02 0D
        // - Suzuki/CAN modules: 18 00 FF 00 or 17 00 FF 00 or 17 FF 00
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x17, 0x29}, resp, 350) && resp.size() >= 2 && resp[0] == 0x57) {
            ParseKwpDtcList(resp, mod.shortName, dtcList, res);
            return res.status;
        }
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x19, 0x02, 0x29}, resp, 350) && resp.size() >= 3 && resp[0] == 0x59) {
            ParseUdsDtcList19(resp, mod.shortName, dtcList, res);
            return res.status;
        }
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x19, 0x02, 0xFF}, resp, 350) && resp.size() >= 3 && resp[0] == 0x59) {
            ParseUdsDtcList19(resp, mod.shortName, dtcList, res);
            return res.status;
        }
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x19, 0x02, 0x0D}, resp, 350) && resp.size() >= 3 && resp[0] == 0x59) {
            ParseUdsDtcList19(resp, mod.shortName, dtcList, res);
            return res.status;
        }
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x18, 0x00, 0xFF, 0x00}, resp, 350) && resp.size() >= 2 && resp[0] == 0x58) {
            ParseKwpDtcList(resp, mod.shortName, dtcList, res);
            return res.status;
        }
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x17, 0x00, 0xFF, 0x00}, resp, 350) && resp.size() >= 2 && resp[0] == 0x57) {
            ParseKwpDtcList(resp, mod.shortName, dtcList, res);
            return res.status;
        }
        if (g_diag.SendAndReceive(activeTx, activeRx, {0x17, 0xFF, 0x00}, resp, 350) && resp.size() >= 2 && resp[0] == 0x57) {
            ParseKwpDtcList(resp, mod.shortName, dtcList, res);
            return res.status;
        }

        res.dtcCount = -1;
        res.status = "Modul CAN uzerinden yanit verdi, ancak DTC okuma servisi reddedildi (" + HexBytes(resp) + ")";
        return res.status;
    }

    // 2. If CAN did not respond and module has a verified Suzuki K-Line address (Pin 7, 10400 baud), try primary and alternate K-Line Fast Init!
    unsigned char kTargets[2] = {mod.klineAddr, mod.altKlineAddr};
    for (unsigned char kAddr : kTargets) {
        if (kAddr == 0) continue;
        std::vector<std::vector<unsigned char>> kReqs = {
            {0x1A, 0x91},             // [0] Suzuki Part Number (ASCII)
            {0x1A, 0x9A},             // [1] Base Part Number
            {0x1A, 0x8B},             // [2] Hydraulic Unit / Extra ID
            {0x1A, 0x9E},             // [3] Calibration ID
            {0x18, 0x00, 0x40, 0x00}, // [4] Suzuki/Kline Read DTCs
            {0x18, 0x00, 0xFF, 0x00}, // [5] Suzuki/Kline Read DTCs
            {0x21, 0x08}              // [6] Local Data / Config
        };
        std::vector<std::vector<unsigned char>> kResps;
        if (g_diag.RunKLineSession(kAddr, kReqs, kResps, 350)) {
            EvaluateKLineDtcScan(kAddr, mod.shortName, kResps, dtcList, res);
            return res.status;
        }
    }

    res.status = (mod.txId != 0 && mod.klineAddr != 0)
        ? "CAN ve K-Line uzerinde yanit alinamadi"
        : (mod.txId != 0 ? "CAN uzerinde yanit alinamadi" : "K-Line (Pin 7) uzerinde yanit alinamadi");
    res.dtcCount = -1;
    res.responsive = false;
    return res.status;
}

// --- Official Steering Angle Sensor (SAS) & ESP Sensor Calibration Procedure ---
// Supports:
//   Mode 1: CAN ABS/ESP (0x261/0x661) TypeD/E (ABS-03.js: 30 01 08 80 80 00 00) & VDC-101.js (30 16 00 00 -> 22 11 01)
//   Mode 2: K-Line ABS/ESP (0x29, Pin 7, 56100-65J0 Grand Vitara JB) TypeB/C (ABS-03.js: A5 01 80 00 00 00) & TypeA (30 01 08 80 00 00 00)
struct ProcedureResult {
    bool success = false;
    std::string step;
    std::string message;
    std::vector<std::string> log;
};

struct SasPostCheck {
    bool statusReadOk = false;
    bool statusCalibrated = false;
    bool dtcReadOk = false;
    bool hasCalibOrWiringDtc = false;
};

// Confirms SAS calibration actually finished instead of trusting a start ack or absence of DTCs alone.
// Requires positive SAS status confirmation (21 00 / 22 11 01) AND clean post-calibration DTC readback.
inline bool VerifySasPostCalibration(const SasPostCheck& chk, std::string& outFailStep, std::string& outFailMsg) {
    if (!chk.statusReadOk) {
        outFailStep = "CALIBRATION_STATUS_UNVERIFIED";
        outFailMsg = "Kalibrasyon baslangic komutu kabul edildi ancak direksiyon aci sensoru kalibrasyon durum biti (21 00 / 22 11 01) okunamadi; sadece DTC yoklugu basari kaniti sayilmaz.";
        return false;
    }
    if (!chk.statusCalibrated) {
        outFailStep = "CALIBRATION_INCOMPLETE";
        outFailMsg = "Kalibrasyon baslangic komutu kabul edildi ancak tekrar okunan SAS durum biti kalibrasyonun henuz tamamlanmadigini/gecersiz oldugunu gosteriyor.";
        return false;
    }
    if (!chk.dtcReadOk) {
        outFailStep = "CALIBRATION_DTC_UNVERIFIED";
        outFailMsg = "SAS durum biti okundu ancak kalibrasyon sonrasi ABS/ESP ariza hafizasi (18 00 FF 00 / 19 02 FF) okunamadigi icin islem dogrulanamadi.";
        return false;
    }
    if (chk.hasCalibOrWiringDtc) {
        outFailStep = "CALIBRATION_DTC_STILL_ACTIVE";
        outFailMsg = "Kalibrasyon baslangic komutu kabul edildi ancak tekrar okunan ariza kodlarinda SAS kalibrasyon/sensor hatasi (C1075/C1037) hala aktif.";
        return false;
    }
    return true;
}

ProcedureResult ExecuteSASCalibration() {
    ProcedureResult res;
    unsigned long txId = 0x261;
    unsigned long rxId = 0x661;
    std::vector<unsigned char> resp;

    auto hasSasFailureDtc = [](const std::vector<DTCEntry>& dtcs) -> bool {
        for (const auto& d : dtcs) {
            if (d.code == "C1075" || d.code == "C1037") return true;
        }
        return false;
    };

    res.log.push_back("Adim 1: ABS/ESP modulu CAN hattinda (0x261 / 0x661) araniyor...");
    {
        CanChannelScope canScope(g_diag, txId, rxId);
        bool canSession = false;
        if (canScope.open) {
            if (canScope.Transact({0x10, 0xC0}, resp, 350) && resp.size() >= 2 && resp[0] == 0x50) {
                canSession = true;
            } else if (canScope.Transact({0x10, 0x81}, resp, 350) && resp.size() >= 2 && resp[0] == 0x50) {
                canSession = true;
            }
        }

        if (canSession) {
            res.log.push_back("CAN ABS/ESP oturumu acildi (" + HexBytes(resp) + ").");

            // Try Suzuki ABS-03 TypeD/E Sensor Calibration command first: 30 01 08 80 80 00 00 -> 70 01 08
            res.log.push_back("Adim 2A: Suzuki ABS-03 Sensor Kalibrasyon komutu gonderiliyor (30 01 08 80 80 00 00)...");
            if (canScope.Transact({0x30, 0x01, 0x08, 0x80, 0x80, 0x00, 0x00}, resp, 600) &&
                resp.size() >= 3 && resp[0] == 0x70 && resp[1] == 0x01 && resp[2] == 0x08) {
                res.log.push_back("ABS-03 baslangic komutu onaylandi (" + HexBytes(resp) + "). Kalibrasyon tamamlama dogrulamasi yapiliyor...");
                SasPostCheck chk;
                for (int poll = 0; poll < 5; ++poll) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    std::vector<unsigned char> stResp;
                    if (canScope.Transact({0x21, 0x00}, stResp, 350) && stResp.size() >= 2 + 27 && stResp[0] == 0x61 && stResp[1] == 0x00) {
                        chk.statusReadOk = true;
                        chk.statusCalibrated = ((stResp[2 + 26] & 0x40) != 0);
                        res.log.push_back("Post-check 21 00 (" + std::to_string(poll + 1) + "): sasBit=" + std::string(chk.statusCalibrated ? "1" : "0"));
                        if (chk.statusCalibrated) break;
                    } else if (canScope.Transact({0x22, 0x11, 0x01}, stResp, 350) && stResp.size() >= 4 && stResp[0] == 0x62 && stResp[1] == 0x11 && stResp[2] == 0x01) {
                        unsigned char v = stResp[3];
                        chk.statusReadOk = true;
                        chk.statusCalibrated = ((v & 0x10) == 0 && (v & 0x83) == 0x80);
                        res.log.push_back("Post-check 22 11 01 (" + std::to_string(poll + 1) + "): " + HexAddr(v, 2));
                        if (chk.statusCalibrated || (v & 0x10) != 0 || (v & 0x83) == 0x03) break;
                    }
                }
                std::vector<DTCEntry> postDtcs;
                ModuleScanResult dummyRes;
                if (canScope.Transact({0x18, 0x00, 0xFF, 0x00}, resp, 400) && resp.size() >= 2 && (resp[0] == 0x58 || resp[0] == 0x57)) {
                    ParseKwpDtcList(resp, "ABS/ESP", postDtcs, dummyRes);
                    chk.dtcReadOk = (dummyRes.dtcCount >= 0);
                    chk.hasCalibOrWiringDtc = hasSasFailureDtc(postDtcs);
                    res.log.push_back("Post-check 18 00 FF 00: dtcCount=" + std::to_string(dummyRes.dtcCount));
                } else if (canScope.Transact({0x19, 0x02, 0xFF}, resp, 400) && resp.size() >= 3 && resp[0] == 0x59) {
                    ParseUdsDtcList19(resp, "ABS/ESP", postDtcs, dummyRes);
                    chk.dtcReadOk = (dummyRes.dtcCount >= 0);
                    chk.hasCalibOrWiringDtc = hasSasFailureDtc(postDtcs);
                    res.log.push_back("Post-check 19 02 FF: dtcCount=" + std::to_string(dummyRes.dtcCount));
                }
                canScope.Transact({0x10, 0x81}, resp, 300);
                std::string failStep, failMsg;
                if (!VerifySasPostCalibration(chk, failStep, failMsg)) {
                    res.success = false;
                    res.step = failStep;
                    res.message = failMsg;
                    return res;
                }
                res.success = true;
                res.step = "DONE_CAN_ABS03";
                res.message = "ABS/ESP Direksiyon Aci ve G-Sensor Kalibrasyonu (ABS-03) Tamamlandi ve Dogrulandi!";
                return res;
            }

            // Try Suzuki VDC-101 Steering Angle Sensor adjustment: 30 16 00 00 -> 70 16 01 -> poll 22 11 01
            res.log.push_back("Adim 2B: VDC-101 Direksiyon Aci Sifirlama komutu gonderiliyor (30 16 00 00)...");
            if (canScope.Transact({0x30, 0x16, 0x00, 0x00}, resp, 500) &&
                resp.size() >= 3 && resp[0] == 0x70 && resp[1] == 0x16 && resp[2] == 0x01) {
                res.log.push_back("Sensor ayar modu kabul edildi (70 16 01). Durum sorgulaniyor (22 11 01)...");
                bool calibrated = false;
                for (int retry = 0; retry < 15; retry++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    if (canScope.Transact({0x22, 0x11, 0x01}, resp, 400) &&
                        resp.size() >= 4 && resp[0] == 0x62 && resp[1] == 0x11 && resp[2] == 0x01) {
                        unsigned char value = resp[3];
                        unsigned char v1 = value & 0x83;
                        unsigned char v2 = value & 0x10;
                        res.log.push_back("Durum: " + HexAddr(value, 2) + " (deneme " + std::to_string(retry + 1) + ")");
                        if (v2 == 0x10 || v1 == 0x03) {
                            res.step = "CALIBRATION_ERROR";
                            res.message = "Kalibrasyon sirasinda sensor hatasi algilandi. Direksiyonu tam duz tutunuz.";
                            canScope.Transact({0x10, 0x81}, resp, 300);
                            return res;
                        } else if (v1 == 0x80) {
                            calibrated = true;
                            break;
                        }
                    }
                }
                canScope.Transact({0x10, 0x81}, resp, 300);
                if (calibrated) {
                    res.success = true;
                    res.step = "DONE_CAN_VDC101";
                    res.message = "Direksiyon Aci Sensoru (SAS) Kalibrasyonu (VDC-101) Basariyla Tamamlandi!";
                    return res;
                }
            }
            canScope.Transact({0x10, 0x81}, resp, 300);
            res.step = "CAN_REJECTED";
            res.message = "CAN ABS/ESP oturumu acildi ancak kalibrasyon komutu reddedildi (" + HexBytes(resp) + ").";
            return res;
        }
    }
    res.log.push_back("CAN 0x261 uzerinde yanit alinamadi. Grand Vitara JB K-Line ABS/ESP (0x29, Pin 7) deneniyor...");

    // Try Grand Vitara JB (56100-65J0 / 56100-65J7) K-Line ABS/ESP (0x29) within a single KLineChannelScope:
    // Send TypeB/C (A5 01 80) first; only fall back to TypeA (30 01 08) if TypeB/C did NOT succeed or reject with NRC 0x23.
    KLineChannelScope kScope(g_diag, 0x29);
    if (kScope.open) {
        res.log.push_back("K-Line ABS/ESP (0x29) kalici oturum kuruldu.");
        std::vector<unsigned char> pre21, pre18, rA5, r30;
        bool pre21Ok = false, pre18Ok = false, rA5Ok = false, r30Ok = false;
        kScope.Transact({0x21, 0x00}, pre21, 450, &pre21Ok);
        kScope.Transact({0x18, 0x00, 0xFF, 0x00}, pre18, 450, &pre18Ok);
        if (pre21Ok && pre21[0] == 0x61) {
            res.log.push_back("ABS/ESP ilk durum paketi (21 00) okundu (" + std::to_string(pre21.size()) + " bayt).");
        }

        std::string acceptedMode;
        kScope.Transact({0xA5, 0x01, 0x80, 0x00, 0x00, 0x00}, rA5, 600, &rA5Ok);
        res.log.push_back("TX [A5 01 80 00 00 00] -> RX [" + (rA5.empty() ? "YOK" : HexBytes(rA5)) + "]");

        bool nrc23 = (!rA5.empty() && rA5[0] == 0x7F && rA5.size() >= 3 && rA5[2] == 0x23);
        if (rA5Ok && rA5.size() >= 2 && rA5[0] == 0xE5 && rA5[1] == 0x01) {
            acceptedMode = "TYPEB";
            res.log.push_back("Grand Vitara ABS-03 TypeB/C baslangic komutu onaylandi (" + HexBytes(rA5) + "). Yedek TypeA gonderilmedi.");
        } else if (!nrc23) {
            kScope.Transact({0x30, 0x01, 0x08, 0x80, 0x00, 0x00, 0x00}, r30, 600, &r30Ok);
            res.log.push_back("TX [30 01 08 80 00 00 00] -> RX [" + (r30.empty() ? "YOK" : HexBytes(r30)) + "]");
            if (r30Ok && r30.size() >= 3 && r30[0] == 0x70 && r30[1] == 0x01 && r30[2] == 0x08) {
                acceptedMode = "TYPEA";
                res.log.push_back("ABS-03 TypeA baslangic komutu onaylandi (" + HexBytes(r30) + ").");
            }
        }

        if (!acceptedMode.empty()) {
            SasPostCheck chk;
            std::vector<unsigned char> post21, post18;
            bool post21Ok = false, post18Ok = false;
            // Poll during the 2-3s stabilization window (keeps K-Line session alive and checks bit 0x40 completion)
            for (int poll = 0; poll < 5; ++poll) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (kScope.Transact({0x21, 0x00}, post21, 450, &post21Ok) && post21Ok &&
                    post21.size() >= 2 + 27 && post21[0] == 0x61 && post21[1] == 0x00) {
                    chk.statusReadOk = true;
                    chk.statusCalibrated = ((post21[2 + 26] & 0x40) != 0);
                    res.log.push_back("Post-check 21 00 (" + std::to_string(poll + 1) + "): sasBit=" +
                                      std::string(chk.statusCalibrated ? "1" : "0"));
                    if (chk.statusCalibrated) break;
                }
            }
            if (kScope.Transact({0x18, 0x00, 0xFF, 0x00}, post18, 450, &post18Ok) && post18Ok &&
                post18.size() >= 2 && (post18[0] == 0x58 || post18[0] == 0x57)) {
                std::vector<DTCEntry> postDtcs;
                ModuleScanResult dummyRes;
                ParseKwpDtcList(post18, "ABS/ESP", postDtcs, dummyRes);
                chk.dtcReadOk = (dummyRes.dtcCount >= 0);
                chk.hasCalibOrWiringDtc = hasSasFailureDtc(postDtcs);
                res.log.push_back("Post-check 18 00 FF 00: dtcCount=" + std::to_string(dummyRes.dtcCount));
            }
            std::string failStep, failMsg;
            if (!VerifySasPostCalibration(chk, failStep, failMsg)) {
                res.success = false;
                res.step = failStep;
                res.message = failMsg;
                return res;
            }
            res.success = true;
            res.step = (acceptedMode == "TYPEB") ? "DONE_KLINE_ABS03_TYPEB" : "DONE_KLINE_ABS03_TYPEA";
            res.message = (acceptedMode == "TYPEB")
                ? "Grand Vitara ABS/ESP Direksiyon Aci & G-Sensor Kalibrasyonu (K-Line 0x29 ABS-03) Tamamlandi ve Dogrulandi!"
                : "ABS/ESP Sensor Kalibrasyonu (K-Line 0x29) Tamamlandi ve Dogrulandi!";
            return res;
        }

        if (nrc23) {
            res.step = "KLINE_REJECTED_CONDITIONS";
            res.log.push_back("ABS Beyni Kalibrasyonu Reddetti: 7F A5 23 (NRC 0x23: Kosullar Uygun Degil).");

            std::vector<DTCEntry> absDtcs;
            ModuleScanResult dummyRes;
            if (pre18Ok && !pre18.empty() && (pre18[0] == 0x58 || pre18[0] == 0x57)) {
                ParseKwpDtcList(pre18, "ABS/ESP", absDtcs, dummyRes);
            }
            bool hasC1037 = false;
            for (const auto& d : absDtcs) {
                if (d.code == "C1037") { hasC1037 = true; break; }
            }

            double curAngle = 0.0;
            bool hasAngle = false;
            if (pre21Ok && pre21.size() >= 2 + 22 && pre21[0] == 0x61 && pre21[1] == 0x00) {
                curAngle = ReadSigned16BE(pre21, 20) * 1.0;
                hasAngle = true;
            }

            bool sasBitUnset = (pre21Ok && pre21.size() >= 2 + 27 && ((pre21[2 + 26] & 0x40) == 0));

            std::string reasonText;
            std::string actionText;

            if (!absDtcs.empty()) {
                reasonText += "1. SISTEMDE AKTIF ARIZA KODU VAR:\n";
                for (const auto& d : absDtcs) {
                    reasonText += "   - [" + d.code + "] " + d.title + "\n";
                }
                reasonText += "   (Bosch 8.0 ESP sistemi, tekerlek hiz sensoru veya donanim arizasi varken guvenlik geregi aci kalibrasyonunu otomatik KILITLER).\n\n";
                actionText += "- " + absDtcs[0].code + " nolu arizayi gideriniz.\n";
                actionText += "- 'Ariza Kodlarini Sil' butonu ile arıza hafizasini temizleyiniz.\n";
            }

            if (hasAngle && std::abs(curAngle) > 1.8) {
                char abuf[32]; sprintf_s(abuf, "%+.1f", curAngle);
                reasonText += "2. DIREKSIYON SIMIDI DUZ DEGIL:\n";
                reasonText += "   - Mevcut Aci: " + std::string(abuf) + " derece.\n";
                reasonText += "   (Kalibrasyonun kabul edilmesi icin simit kadrana gore tam duz 0.0 +- 1.5 derece olmalidir).\n\n";
                actionText += "- Direksiyon simidini kadrana gore tam 0 derece duz konuma getiriniz.\n";
            }

            if (sasBitUnset) {
                reasonText += "3. SAS DURUM BITI (Bayt 26 Bit 6 = 0):\n";
                if (hasC1037) {
                    reasonText += "   - C1037 ariza kodu ile birlikte SAS durum biti 0 gorunuyor; sensor besleme, sigorta veya zemberek baglantisi kontrol edilmelidir.\n\n";
                    actionText += "- C1037 icin direksiyon aci sensoru soketi, zemberek ve ilgili sigortayi kontrol ediniz.\n";
                } else {
                    reasonText += "   - 21 00 durum bayti 26 bit 6 sifir (hazir/kalibre degil). Tek basina donanim veya zemberek arizasi kaniti degildir; kalibrasyon tamamlanmadiginda da 0 olabilir.\n\n";
                }
            }

            if (reasonText.empty()) {
                reasonText = "Arac tamamen duz zeminde degil veya fren pedalina basiliyor olabilir.\n";
            }
            if (actionText.empty()) {
                actionText = "- Araci duz zemine alip tekerlekleri ve simidi tam duz tutunuz, fren pedalina basmayiniz.\n";
            }

            res.message = "ABS/ESP Beyni Kalibrasyonu Reddetti (Hata: NRC 0x23 - Kosullar Uygun Degil).\n\n"
                          "TESPIT EDILEN NEDENLER:\n" + reasonText +
                          "YAPILMASI GEREKENLER:\n" + actionText +
                          "- Ardindan kalibrasyonu tekrar baslatiniz.";
            return res;
        }
        res.step = "KLINE_REJECTED";
        res.message = "K-Line ABS/ESP baglandi ancak kalibrasyon komutu reddedildi. Aracin duz zeminde, hareketsiz ve fren pedalinin basili olmadigindan emin olunuz.";
        return res;
    }

    res.step = "NO_RESPONSE";
    res.message = "ABS/ESP modulune ne CAN (0x261) ne de K-Line (0x29) uzerinden ulasilabildi. Kontagin acik oldugunu kontrol ediniz.";
    return res;
}

// --- Official Diesel Particulate Filter (DPF) Reset Procedure (Suzuki DSL-04.js) ---
ProcedureResult ExecuteDPFReset() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM motor kontrol unitesi (0x7E0) oturumu aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.empty() || resp[0] != 0x50) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik mi?";
        res.log.push_back("HATA: 10 C0 yaniti alinamadi.");
        return res;
    }
    res.log.push_back("Adim 1 basarili: ECM ozel oturum acildi (" + HexBytes(resp) + ").");

    res.log.push_back("Adim 2: DPF Kurum ve Filtre Degisim Sifirlama komutu iletiliyor (31 0A 00)...");
    bool resetOk = false;
    for (int retry = 0; retry < 30; retry++) {
        if (canScope.Transact({0x31, 0x0A, 0x00}, resp, 500) && resp.size() >= 3 && resp[0] == 0x71 && resp[1] == 0x0A) {
            unsigned char code = resp[2];
            if (code == 0x02) {
                resetOk = true;
                res.log.push_back("DPF sifirlama onaylandi (71 0A 02).");
                break;
            } else if (code == 0x01) {
                res.log.push_back("DPF sifirlama islemi suruyor (71 0A 01)...");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            } else {
                res.log.push_back("DPF rutini durum kodu: " + HexAddr(code, 2));
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    if (resetOk) {
        res.log.push_back("Adim 3: ECM DTC hafizasi temizleniyor (14 FF FF FF)...");
        if (canScope.Transact({0x14, 0xFF, 0xFF, 0xFF}, resp, 500) && !resp.empty() && resp[0] == 0x54) {
            res.log.push_back("Adim 3 basarili: ECM DTC hafizasi temizlendi (" + HexBytes(resp) + ").");
        } else if (canScope.Transact({0x14, 0xFF, 0x00}, resp, 500) && !resp.empty() && resp[0] == 0x54) {
            res.log.push_back("Adim 3 basarili: ECM DTC hafizasi temizlendi (" + HexBytes(resp) + ").");
        } else {
            res.log.push_back("Uyari: DTC silme yaniti: " + HexBytes(resp));
        }
        res.success = true;
        res.step = "DONE";
        res.message = "Dizel Partikul Filtresi (DPF) ve Kurum Sayaci Basariyla Sifirlandi!";
    } else {
        res.step = "RESET_FAILED";
        res.message = "DPF sifirlama komutu (31 0A 00) ECU tarafindan onaylanmadi (" + HexBytes(resp) + "). Motorun calismadigindan ve kontagin acik oldugundan emin olunuz.";
    }

    canScope.Transact({0x10, 0x81}, resp, 300);
    res.log.push_back("Adim 4: Varsayilan oturuma donuldu (10 81).");
    return res;
}

// --- Official Oil Change Request Light Reset Procedure (Suzuki DSL-08.js) ---
ProcedureResult ExecuteOilReset() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM (0x7E0) ozel oturum aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.empty() || resp[0] != 0x50) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik mi?";
        return res;
    }

    res.log.push_back("Adim 2: Yag asinim konfigurasyon cercevesi okunuyor (21 6A)...");
    if (!canScope.Transact({0x21, 0x6A}, resp, 500) || resp.size() < 82 || resp[0] != 0x61 || resp[1] != 0x6A) {
        res.step = "READ_6A";
        res.message = "21 6A yag konfigurasyon cercevesi okunamadi (" + std::to_string(resp.size()) + " bayt).";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    // Per Suzuki DSL-08.js:
    // arr.response has 0x61 stripped (arr.response[0] == 0x6A), and reqData = arr.response.slice(1) (the data bytes after 0x6A).
    // reqData[45] |= 0x08; then write [0x3B, 0x6A].concat(reqData).
    std::vector<unsigned char> writeReq = {0x3B, 0x6A};
    writeReq.insert(writeReq.end(), resp.begin() + 2, resp.end());
    if (writeReq.size() <= 2 + 45) {
        res.step = "SHORT_6A";
        res.message = "21 6A yanit verisi beklenenden kisa.";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    writeReq[2 + 45] |= 0x08;

    res.log.push_back("Adim 3: Yag degisim sifirlama bayragi yaziliyor (3B 6A)...");
    if (!canScope.Transact(writeReq, resp, 600) || resp.size() < 2 || resp[0] != 0x7B || resp[1] != 0x6A) {
        res.step = "WRITE_6A_1";
        res.message = "3B 6A yazma komutu reddedildi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    res.log.push_back("Adim 4: Konfigurasyon cercevesi dogrulaniyor ve kaydediliyor (21 6A -> 3B 6A)...");
    if (!canScope.Transact({0x21, 0x6A}, resp, 500) || resp.size() < 82 || resp[0] != 0x61 || resp[1] != 0x6A) {
        res.step = "VERIFY_READ_6A";
        res.message = "3B 6A yazildi ancak dogrulama okuma (21 6A) basarisiz (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    std::vector<unsigned char> writeReq2 = {0x3B, 0x6A};
    writeReq2.insert(writeReq2.end(), resp.begin() + 2, resp.end());
    if (!canScope.Transact(writeReq2, resp, 600) || resp.size() < 2 || resp[0] != 0x7B || resp[1] != 0x6A) {
        res.step = "WRITE_6A_2";
        res.message = "Ikinci 3B 6A kayit komutu reddedildi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    canScope.Transact({0x10, 0x81}, resp, 300);
    res.success = true;
    res.step = "DONE";
    res.message = "Motor Yag Degisim Isigi ve Seyreltim Sayaci (DSL-08) Basariyla Sifirlandi!";
    res.log.push_back("Adim 5: Varsayilan oturuma donuldu (10 81).");
    return res;
}

// --- Official EGR Valve Data Initialization Procedure (Suzuki DSL-02.js, Func ID 20166) ---
ProcedureResult ExecuteEGRInit() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM (0x7E0) Assembly Line Test oturumu aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.size() < 2 || resp[0] != 0x50 || resp[1] != 0xC0) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik ve motor kapali mi?";
        return res;
    }

    res.log.push_back("Adim 2: F9Q hava sistemi ve EGR adaptasyon tablosu okunuyor (21 64)...");
    if (!canScope.Transact({0x21, 0x64}, resp, 500) || resp.size() < 46 || resp[0] != 0x61 || resp[1] != 0x64) {
        res.step = "READ_64";
        res.message = "21 64 F9Q EGR veri blogu okunamadi (" + std::to_string(resp.size()) + " bayt).";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    res.log.push_back("21 64 yaniti alindi (" + std::to_string(resp.size() - 2) + " veri bayti).");

    // Per Suzuki DSL-02.js:
    // readF9QData = arr.response.slice(1) (bytes after 0x64, i.e., resp.begin() + 2 .. resp.end())
    // readF9QData[39] = 0x7F; readF9QData[40] = 0xFF; readF9QData[41] = 0x7F; readF9QData[42] = 0xFF;
    // Write [0x3B, 0x64].concat(readF9QData) -> expects 7B 64.
    std::vector<unsigned char> writeReq = {0x3B, 0x64};
    writeReq.insert(writeReq.end(), resp.begin() + 2, resp.end());
    writeReq[2 + 39] = 0x7F;
    writeReq[2 + 40] = 0xFF;
    writeReq[2 + 41] = 0x7F;
    writeReq[2 + 42] = 0xFF;

    res.log.push_back("Adim 3: EGR valfi baslangic adaptasyon ofsetleri (7F FF 7F FF) yaziliyor (3B 64)...");
    if (!canScope.Transact(writeReq, resp, 600) || resp.size() < 2 || resp[0] != 0x7B || resp[1] != 0x64) {
        res.step = "WRITE_64";
        res.message = "3B 64 EGR sifirlama yazma komutu reddedildi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    res.log.push_back("Adim 3 basarili: 7B 64 pozitif yaniti alindi.");

    canScope.Transact({0x10, 0x81}, resp, 300);
    res.success = true;
    res.step = "DONE";
    res.message = "EGR Valfi Bilgisi Baslatma / Adaptasyon Sifirlama (DSL-02) Basariyla Tamamlandi!";
    res.log.push_back("Adim 4: Varsayilan oturuma donuldu (10 81).");
    return res;
}

// --- Official Inlet Throttle Valve Data Initialization Procedure (Suzuki DSL-03.js, Func ID 20167) ---
ProcedureResult ExecuteThrottleInit() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM (0x7E0) Assembly Line Test oturumu aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.size() < 2 || resp[0] != 0x50 || resp[1] != 0xC0) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik ve motor kapali mi?";
        return res;
    }

    const unsigned char flapDids[4][2] = {
        {0x2F, 0x1D}, // First offset opened flap data reset
        {0x2F, 0x1E}, // Last offset opened flap data reset
        {0x2F, 0x1F}, // First offset closed flap data reset
        {0x2F, 0x21}  // Last offset closed flap data reset
    };
    const char* flapLabels[4] = {
        "Ilk Acik Kelebek Ofseti (2E 2F 1D 7F FF)",
        "Son Acik Kelebek Ofseti (2E 2F 1E 7F FF)",
        "Ilk Kapali Kelebek Ofseti (2E 2F 1F 7F FF)",
        "Son Kapali Kelebek Ofseti (2E 2F 21 7F FF)"
    };

    for (int i = 0; i < 4; i++) {
        std::vector<unsigned char> req = {0x2E, flapDids[i][0], flapDids[i][1], 0x7F, 0xFF};
        res.log.push_back(std::string("Adim ") + std::to_string(i + 2) + ": " + flapLabels[i] + " sifirlaniyor...");
        if (!canScope.Transact(req, resp, 500) ||
            resp.size() < 3 || resp[0] != 0x6E || resp[1] != flapDids[i][0] || resp[2] != flapDids[i][1]) {
            res.step = "WRITE_2F";
            res.message = std::string(flapLabels[i]) + " komutu reddedildi (" + HexBytes(resp) + ").";
            canScope.Transact({0x10, 0x81}, resp, 300);
            return res;
        }
        res.log.push_back(std::string("Onaylandi: ") + HexBytes(resp));
    }

    canScope.Transact({0x10, 0x81}, resp, 300);
    res.success = true;
    res.step = "DONE";
    res.message = "Giris Kelebek Valfi (Bogaz Kelebegi) Bilgisi Baslatma (DSL-03) Basariyla Tamamlandi!";
    res.log.push_back("Adim 6: Varsayilan oturuma donuldu (10 81).");
    return res;
}

// --- Official Injector Calibration Code Read & ZFC Value Reset Procedure (Suzuki DSL-01.js, Func ID 20165) ---
ProcedureResult ExecuteInjectorZfcInit() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM (0x7E0) Assembly Line Test oturumu aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.size() < 2 || resp[0] != 0x50 || resp[1] != 0xC0) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik mi?";
        return res;
    }

    res.log.push_back("Adim 2: Enjektor IMA kalibrasyon kodlari okunuyor (21 AD)...");
    std::string imaSummary;
    if (canScope.Transact({0x21, 0xAD}, resp, 500) && resp.size() >= 31 && resp[0] == 0x61 && resp[1] == 0xAD) {
        auto extractIma = [&](size_t start) {
            std::string s;
            for (size_t i = start; i < start + 7 && i < resp.size(); i++) {
                s += (resp[i] >= 32 && resp[i] < 127) ? static_cast<char>(resp[i]) : '?';
            }
            return s;
        };
        std::string c1 = extractIma(3), c2 = extractIma(10), c3 = extractIma(17), c4 = extractIma(24);
        imaSummary = " [Silindir 1: " + c1 + ", Silindir 2: " + c2 + ", Silindir 3: " + c3 + ", Silindir 4: " + c4 + "]";
        res.log.push_back("Kayitli Enjektor IMA Kodlari:" + imaSummary);
    } else {
        res.log.push_back("Uyari: 21 AD yaniti: " + HexBytes(resp));
    }

    res.log.push_back("Adim 3: Tum silindirler icin ZFC (Sifir Yakit Kalibrasyonu) ogrenme degerleri sifirlaniyor (3B 7E FE)...");
    if (!canScope.Transact({0x3B, 0x7E, 0xFE}, resp, 600) || resp.size() < 2 || resp[0] != 0x7B || resp[1] != 0x7E) {
        res.step = "WRITE_7E";
        res.message = "3B 7E FE ZFC sifirlama komutu reddedildi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    res.log.push_back("Adim 3 basarili: 7B 7E pozitif yaniti alindi.");

    canScope.Transact({0x10, 0x81}, resp, 300);
    res.success = true;
    res.step = "DONE";
    res.message = "Enjektor IMA Kodlari Okundu ve ZFC Adaptasyonu Sifirlandi (DSL-01)!" + imaSummary;
    res.log.push_back("Adim 4: Varsayilan oturuma donuldu (10 81).");
    return res;
}

// --- Official DPF Service Regeneration & FAP Data Update Procedure (Suzuki DSL-06.js, Func ID 20170) ---
ProcedureResult ExecuteDPFRegen() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM (0x7E0) Assembly Line Test oturumu aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.size() < 2 || resp[0] != 0x50 || resp[1] != 0xC0) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik mi?";
        return res;
    }

    res.log.push_back("Adim 2: FAP servis rejenerasyon istegi gonderiliyor (31 0D 00)...");
    if (!canScope.Transact({0x31, 0x0D, 0x00}, resp, 600) || resp.size() < 3 || resp[0] != 0x71 || resp[1] != 0x0D) {
        res.step = "REGEN_REQ";
        res.message = "31 0D 00 servis rejenerasyon istegi reddedildi (" + HexBytes(resp) + "). Motor sicakligi ve rejenerasyon kosullarini kontrol ediniz.";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    unsigned char status = resp[2];
    res.log.push_back("31 0D 00 yaniti alindi (Durum: " + HexAddr(status, 2) + ").");

    res.log.push_back("Adim 3: FAP veri cercevesi okunuyor ve guncelleniyor (21 AC -> 3B AC)...");
    if (!canScope.Transact({0x21, 0xAC}, resp, 500) || resp.size() < 55 || resp[0] != 0x61 || resp[1] != 0xAC) {
        res.step = "READ_AC";
        res.message = "31 0D 00 yanitlandi ancak 21 AC FAP veri cercevesi okunamadi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    std::vector<unsigned char> writeReq = {0x3B, 0xAC};
    writeReq.insert(writeReq.end(), resp.begin() + 2, resp.end());
    writeReq[2 + 49] = 0x00;
    writeReq[2 + 50] = 0x00;
    writeReq[2 + 51] = 0x00;
    writeReq[2 + 52] = 0x00;
    if (!canScope.Transact(writeReq, resp, 600) || resp.size() < 2 || resp[0] != 0x7B || resp[1] != 0xAC) {
        res.step = "WRITE_AC";
        res.message = "3B AC FAP veri cercevesi guncelleme komutu reddedildi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    res.log.push_back("FAP veri cercevesi (3B AC) guncellendi.");

    canScope.Transact({0x14, 0xFF, 0xFF, 0xFF}, resp, 500);
    canScope.Transact({0x10, 0x81}, resp, 300);
    res.success = true;
    res.step = "DONE";
    res.message = "DPF Servis Rejenerasyon Komutu (31 0D 00) ve FAP Veri Guncellemesi (DSL-06) Basariyla Tamamlandi!";
    return res;
}

// --- Official High Pressure Rail Test Procedure (Suzuki DSL-10.js, Func ID 20182) ---
ProcedureResult ExecuteHighPressureTest() {
    ProcedureResult res;
    unsigned long txId = 0x7E0;
    unsigned long rxId = 0x7E8;
    std::vector<unsigned char> resp;

    res.log.push_back("Adim 1: ECM (0x7E0) Assembly Line Test oturumu aciliyor (10 C0)...");
    CanChannelScope canScope(g_diag, txId, rxId);
    if (!canScope.open || !canScope.Transact({0x10, 0xC0}, resp, 400) || resp.size() < 2 || resp[0] != 0x50 || resp[1] != 0xC0) {
        res.step = "SESSION";
        res.message = "ECM 10 C0 oturumu acilamadi. Kontak acik mi?";
        return res;
    }

    res.log.push_back("Adim 2: Yuksek basinc testi baslangic tablosu sifirlaniyor (3B 62 + 27x00)...");
    std::vector<unsigned char> writeReq(29, 0x00);
    writeReq[0] = 0x3B;
    writeReq[1] = 0x62;
    if (!canScope.Transact(writeReq, resp, 500) || resp.size() < 2 || resp[0] != 0x7B || resp[1] != 0x62) {
        res.step = "WRITE_62";
        res.message = "3B 62 baslangic komutu reddedildi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    res.log.push_back("Adim 3: Yuksek basinc (Common Rail) testi baslatiliyor (31 62 00)...");
    if (!canScope.Transact({0x31, 0x62, 0x00}, resp, 600) || resp.size() < 3 || resp[0] != 0x71 || resp[1] != 0x62) {
        res.step = "START_62";
        res.message = "31 62 00 komutu reddedildi (" + HexBytes(resp) + "). Motorun calisir ve rolantide oldugundan emin olunuz.";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    unsigned char st = resp[2];
    for (int i = 0; i < 15 && st == 0x01; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (canScope.Transact({0x31, 0x62, 0x01}, resp, 500) && resp.size() >= 3 && resp[0] == 0x71 && resp[1] == 0x62) {
            st = resp[2];
        }
    }
    if (st == 0x01 || (st != 0x00 && st != 0x02)) {
        res.step = (st == 0x01) ? "TEST_TIMEOUT" : "TEST_STATUS_FAILED";
        res.message = "Yuksek basinc testi tamamlanamadi (durum kodu: " + HexAddr(st, 2) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }

    std::string valSummary;
    if (!canScope.Transact({0x21, 0x62}, resp, 500) || resp.size() < 10 || resp[0] != 0x61 || resp[1] != 0x62) {
        res.step = "READ_62_FAILED";
        res.message = "21 62 yuksek basinc test sonucu okunamadi (" + HexBytes(resp) + ").";
        canScope.Transact({0x10, 0x81}, resp, 300);
        return res;
    }
    int t1 = ((resp[2] << 8) | resp[3]) * 10;
    int t2 = ((resp[4] << 8) | resp[5]) * 10;
    int t3 = ((resp[6] << 8) | resp[7]) * 10;
    int t4 = ((resp[8] << 8) | resp[9]) * 10;
    bool good = (t1 < 600 && t2 < 600 && t3 < 600 && t4 < 600);
    valSummary = " [Sapmalar: " + std::to_string(t1) + ", " + std::to_string(t2) + ", " + std::to_string(t3) + ", " + std::to_string(t4) + " -> " + (good ? "SAGLAM (<600)" : "SUPHELI (>=600)") + "]";
    res.log.push_back("21 62 Test Sonucu:" + valSummary);

    canScope.Transact({0x10, 0x81}, resp, 300);
    res.success = true;
    res.step = "DONE";
    res.message = "Yuksek Basinc (Common Rail) Testi (DSL-10) Tamamlandi!" + valSummary;
    return res;
}

// --- Official Multi-ECU DTC Clear Procedure ---
ProcedureResult ExecuteClearAllDTCs() {
    ProcedureResult res;
    std::vector<unsigned char> resp;
    int clearedCount = 0;

    // 1. Clear ECM (0x7E0): Try 14 FF FF FF (SUZUKI CAN ECM BOSCH F9Q) then 14 FF 00 (SUZUKI CAN ECM)
    res.log.push_back("ECM (0x7E0) DTC hafizasi siliniyor...");
    bool ecmCanResponded = false;
    {
        CanChannelScope ecmScope(g_diag, 0x7E0, 0x7E8);
        if (ecmScope.open && ecmScope.Transact({0x10, 0xC0}, resp, 300) && !resp.empty() && resp[0] == 0x50) {
            ecmCanResponded = true;
            bool ecmCleared = false;
            if (ecmScope.Transact({0x14, 0xFF, 0xFF, 0xFF}, resp, 500) && !resp.empty() && resp[0] == 0x54) {
                ecmCleared = true;
            } else if (ecmScope.Transact({0x14, 0xFF, 0x00}, resp, 500) && !resp.empty() && resp[0] == 0x54) {
                ecmCleared = true;
            }
            ecmScope.Transact({0x10, 0x81}, resp, 300);
            if (ecmCleared) {
                clearedCount++;
                res.log.push_back("ECM (0x7E0): Ariza hafizasi basariyla temizlendi (0x54 onaylandi).");
            } else {
                res.log.push_back("ECM (0x7E0): Oturum acildi ancak silme komutu onaylanmadi (" + HexBytes(resp) + ").");
            }
        }
    }
    if (!ecmCanResponded) {
        if (g_activeProfileId != "gv_jb419d" && g_activeProfileId != "suzuki_can") {
            std::vector<std::vector<unsigned char>> kResps;
            if (g_diag.RunKLineSession(0x11, {{0x14, 0xFF, 0x00}}, kResps, 350) &&
                !kResps.empty() && !kResps[0].empty() && kResps[0][0] == 0x54) {
                clearedCount++;
                res.log.push_back("ECM (K-Line 0x11): Ariza hafizasi basariyla temizlendi (0x54 onaylandi).");
            } else {
                res.log.push_back("ECM (0x7E0 / 0x11): Yanit alinamadi.");
            }
        } else {
            res.log.push_back("ECM (0x7E0): CAN uzerinden yanit alinamadi.");
        }
    }

    // 2. Clear other modules (14 FF 00) on CAN and fallback to K-Line if applicable
    auto activeModules = GetActiveSuzukiModules();
    for (const auto& mod : activeModules) {
        if (mod.shortName == "ECM") continue;
        bool modCleared = false;
        if (mod.txId != 0) {
            std::string addrStr = HexAddr(mod.txId, 3);
            CanChannelScope modScope(g_diag, mod.txId, mod.rxId);
            if (modScope.open &&
                ((modScope.Transact({0x10, 0xC0}, resp, 180) && !resp.empty() && resp[0] == 0x50) ||
                 (modScope.Transact({0x10, 0x81}, resp, 180) && !resp.empty() && resp[0] == 0x50))) {
                if (modScope.Transact({0x14, 0xFF, 0x00}, resp, 400) && !resp.empty() && resp[0] == 0x54) {
                    clearedCount++;
                    modCleared = true;
                    res.log.push_back(mod.shortName + " (" + addrStr + "): Ariza hafizasi temizlendi (0x54 onaylandi).");
                } else {
                    res.log.push_back(mod.shortName + " (" + addrStr + "): Silme komutu reddedildi (" + HexBytes(resp) + ").");
                }
                modScope.Transact({0x10, 0x81}, resp, 180);
            }
        }
        if (!modCleared && mod.klineAddr != 0) {
            std::vector<std::vector<unsigned char>> kClearReqs = {
                {0x14, 0xFF, 0x00},
                {0x14, 0x40, 0x00},
                {0x14, 0x80, 0x00},
                {0x14, 0x00, 0x00}
            };
            std::vector<std::vector<unsigned char>> kResps;
            if (g_diag.RunKLineSession(mod.klineAddr, kClearReqs, kResps, 350)) {
                for (const auto& kr : kResps) {
                    if (!kr.empty() && kr[0] == 0x54) {
                        clearedCount++;
                        res.log.push_back(mod.shortName + " (K-Line " + HexAddr(mod.klineAddr, 2) + "): Ariza hafizasi temizlendi (0x54 onaylandi).");
                        break;
                    }
                }
            }
        }
    }

    if (clearedCount > 0) {
        res.success = true;
        res.step = "DONE";
        res.message = std::to_string(clearedCount) + " adet kontrol unitesinin ariza hafizasi ECU onayi (0x54) ile temizlendi.";
    } else {
        res.success = false;
        res.step = "NO_ECU_CLEARED";
        res.message = "Hicbir kontrol unitesi DTC silme komutunu (0x14 -> 0x54) onaylamadi. Kontagin acik oldugundan emin olunuz.";
    }
    return res;
}

static std::string ExtractJsonField(const std::string& rawReq, const std::string& key) {
    size_t bodyPos = rawReq.find("\r\n\r\n");
    std::string body = (bodyPos != std::string::npos) ? rawReq.substr(bodyPos + 4) : rawReq;
    std::string pat = "\"" + key + "\"";
    size_t pos = body.find(pat);
    if (pos == std::string::npos) return "";
    size_t colon = body.find(':', pos + pat.size());
    if (colon == std::string::npos) return "";
    size_t q1 = body.find('"', colon + 1);
    if (q1 == std::string::npos) return "";
    std::string out;
    for (size_t i = q1 + 1; i < body.size(); i++) {
        if (body[i] == '\\' && i + 1 < body.size()) {
            out += body[i + 1];
            i++;
        } else if (body[i] == '"') {
            break;
        } else {
            out += body[i];
        }
    }
    return out;
}

#include "suzuki_identity.h"

IdentityMatch g_identityMatch;
#include "suzuki_live.h"
#include "suzuki_computed.h"
#include "suzuki_capture.h"
#include "suzuki_utilities.h"

ProcedureResult AutoDetectSuzukiVehicle() {
    ProcedureResult res;
    int prevSelected = g_identityMatch.selectedVariantId;
    g_identityMatch = IdentityMatch{};
    g_ecmSupportedIds.clear();
    g_activeProfileId = "auto";
    g_detectedVehicleDesc = "ECU/model not identified";
    struct Probe { int base; unsigned char target; std::vector<std::vector<unsigned char>> requests; };
    const std::vector<Probe> probes = {
        {41, 0, {{0x1A,0x8E},{0x1A,0x8A}}},
        {194, 0, {{0x22,0xF1,0x01},{0x22,0xF1,0x88}}},
        {93, 0, {{0x10,0xC0},{0x21,0x80}}},
        {64, 0x11, {{0x1A,0x8E},{0x1A,0x8A}}},
        {101, 0x7A, {{0x10,0xC0},{0x21,0x80}}},
        {98, 0x10, {{0x1A,0x94}}}
    };
    for (const auto& probe : probes) {
        std::vector<std::vector<unsigned char>> responses;
        if (probe.target) g_diag.RunKLineSession(probe.target, probe.requests, responses, 350);
        else SendBatch(g_diag, 0x7E0, 0x7E8, probe.requests, responses, 350);
        if (responses.empty()) continue;
        std::string cid, partNumber;
        for (const auto& response : responses) {
            res.log.push_back("Base " + std::to_string(probe.base) + " identity: " + HexBytes(response));
            IdentityMatch field = MatchDbIdentity(probe.base, response);
            if (!field.cid.empty()) cid = field.cid;
            if (!field.partNumber.empty()) partNumber = field.partNumber;
        }
        if (cid.empty() && partNumber.empty()) continue;
        IdentityMatch match = MatchDbIdentifiers(probe.base, cid, partNumber);
        for (const auto* c : match.candidates) {
            if (c->variantId == prevSelected) {
                match.selectedVariantId = prevSelected;
                break;
            }
        }
        if (match.selectedVariantId == 0 && !match.generic && !match.candidates.empty()) {
            match.selectedVariantId = match.candidates[0]->variantId;
        }
        g_identityMatch = match;
        g_activeProfileId = probe.target ? "suzuki_kline" : "suzuki_can";
        if (match.exact()) {
            const auto* record = match.candidates[0];
            g_detectedVehicleDesc = record->model[0] ? record->model : record->name;
            if (probe.base == 93) g_activeProfileId = "gv_jb419d";
            res.success = true;
            res.message = "One ECU DB record matched; vehicle profile identified: " + g_detectedVehicleDesc;
        } else if (match.activeCandidate() != nullptr) {
            const auto* record = match.activeCandidate();
            g_detectedVehicleDesc = std::string("User-selected candidate: ") + (record->model[0] ? record->model : record->name);
            if (probe.base == 93) g_activeProfileId = "gv_jb419d";
            res.success = true;
            res.message = "Active candidate selected: " + g_detectedVehicleDesc;
        } else if (match.generic) {
            g_detectedVehicleDesc = "Generic DB fallback; ECU/model unverified";
            res.message = "Only generic identity records matched. Specific vehicle identity is required.";
        } else if (match.candidates.empty()) {
            g_detectedVehicleDesc = "Identity read; no DB variant match";
            res.message = "Communication succeeded, but ECU/model is not identified.";
        } else {
            g_detectedVehicleDesc = "Multiple DB candidates; user selection required";
            res.message = "Select the matching vehicle attributes; no first-candidate or chassis-based model guess.";
        }
        res.step = g_activeProfileId;
        res.log.push_back(IdentityMatchJson(g_identityMatch));
        return res;
    }
    res.step = "auto";
    res.message = "No supported ECU identity read. SDL/GMLAN and special K-Line discovery remain unsupported; manual vehicle selection may be required.";
    return res;
}

std::string LoadHtmlPage() {
    std::ifstream f("index.html");
    if (f.is_open()) {
        std::stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }
    return "<html><body><h2>Suzuki Diagnostic Server Running</h2></body></html>";
}

void HandleClient(SOCKET clientSock) {
    char buf[8192] = {0};
    int bytesRead = recv(clientSock, buf, sizeof(buf) - 1, 0);
    if (bytesRead <= 0) {
        closesocket(clientSock);
        return;
    }

    std::string req(buf, bytesRead);
    while (req.find("\r\n\r\n") == std::string::npos && req.size() < 16384) {
        bytesRead = recv(clientSock, buf, sizeof(buf) - 1, 0);
        if (bytesRead <= 0) { closesocket(clientSock); return; }
        req.append(buf, bytesRead);
    }
    size_t headerEnd = req.find("\r\n\r\n");
    size_t contentLength = 0;
    std::istringstream headers(req.substr(0, headerEnd));
    std::string header;
    while (std::getline(headers, header)) {
        std::string lower = header;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.compare(0, 15, "content-length:") == 0) {
            try { contentLength = std::stoul(header.substr(15)); } catch (...) { closesocket(clientSock); return; }
        }
    }
    if (headerEnd == std::string::npos || contentLength > 16384) { closesocket(clientSock); return; }
    while (req.size() < headerEnd + 4 + contentLength) {
        bytesRead = recv(clientSock, buf, sizeof(buf) - 1, 0);
        if (bytesRead <= 0) { closesocket(clientSock); return; }
        req.append(buf, bytesRead);
    }
    std::string method, path;
    std::istringstream iss(req);
    iss >> method >> path;

    std::string contentType = "application/json; charset=utf-8";
    std::string body = "";
    int statusCode = 200;

    if (method == "GET" && (path == "/" || path == "/index.html")) {
        contentType = "text/html; charset=utf-8";
        body = LoadHtmlPage();
    }
    else if (method == "GET" && path == "/discovery-plan.js") {
        contentType = "application/javascript; charset=utf-8";
        std::ifstream file("discovery-plan.js", std::ios::binary);
        if (file) { std::ostringstream data; data << file.rdbuf(); body = data.str(); }
        else { statusCode = 404; body = ""; }
    }
    else if (method == "GET" && path.compare(0, 13, "/api/catalog/") == 0) {
        std::string name = path.substr(13);
        bool allowed = name == "index.json" || name == "conditions.json" || name == "discovery.json";
        if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
            std::string id = name.substr(0, name.size() - 5);
            if (!id.empty() && id.find_first_not_of("0123456789") == std::string::npos) allowed = true;
        }
        std::ifstream file;
        if (allowed) file.open("compatibility/catalog/" + name, std::ios::binary);
        if (file) {
            std::ostringstream data;
            data << file.rdbuf();
            body = data.str();
        } else {
            statusCode = 404;
            body = "{\"error\":\"Catalog entry not found\"}";
        }
    }
    else if (method == "POST" && path == "/api/db/decode-capture") {
        std::string param=ExtractJsonField(req,"paramId"), lid=ExtractJsonField(req,"lid"), raw=ExtractJsonField(req,"payload");
        std::vector<unsigned char> payload;
        std::map<std::string,std::vector<unsigned char>> payloads;
        std::string captures=ExtractJsonField(req,"captures");
        std::replace(captures.begin(),captures.end(),';','\n');
        bool parsed=!captures.empty()?ParseCapturedPayloads(captures,payloads):ParseSupportHex(raw,payload);
        if(captures.empty()&&parsed)payloads[lid]=payload;
        if(param.empty()||param.size()>9||param.find_first_not_of("0123456789")!=std::string::npos||raw.size()>8192||captures.size()>12000||!parsed){
            statusCode=400;body="{\"ok\":false,\"error\":\"Invalid captured payload or parameter\"}";
        }else{
            double value=0;
            bool ok=DecodeDbComputed(std::stoi(param),payloads,value);
            body=std::string("{\"ok\":")+(ok?"true":"false")+",\"offlineOnly\":true,\"vehicleVerified\":false,\"value\":"+(ok?JsonNumber(value,6):"null")+"}";
        }
    }
    else if (method == "POST" && path == "/api/dtc/decode-capture") {
        std::vector<unsigned char> response;
        std::string raw=ExtractJsonField(req,"payload");
        if(raw.size()>8192||!ParseSupportHex(raw,response)){statusCode=400;body="{\"ok\":false,\"error\":\"Invalid hex payload\"}";}
        else body=DecodeCapturedUdsDtc(response);
    }
    else if (method == "POST" && path == "/api/dtc/reference") {
        std::string variant = ExtractJsonField(req, "variantId"), code = ExtractJsonField(req, "code");
        if (variant.empty() || variant.size()>9 || variant.find_first_not_of("0123456789")!=std::string::npos || code.empty() || code.size()>32) {
            statusCode=400; body="{\"error\":\"Invalid variant or code\"}";
        } else body=DbDtcDescriptionJson(std::stoi(variant),code);
    }
    else if (method == "POST" && path == "/api/freeze-frame/decode-capture") {
        std::vector<unsigned char> response;
        std::string raw = ExtractJsonField(req, "payload");
        if (raw.size() > 8192 || !ParseSupportHex(raw, response)) {
            statusCode = 400;
            body = "{\"ok\":false,\"error\":\"Invalid freeze-frame hex payload\"}";
        } else {
            body = DecodeCapturedFreezeFrame(response);
        }
    }
    else if (method == "POST" && path == "/api/freeze-frame") {
        std::vector<unsigned char> resp;
        std::string dtcHex = ExtractJsonField(req, "dtcHex");
        std::vector<unsigned char> dtcBytes;
        ParseSupportHex(dtcHex, dtcBytes);
        if (dtcBytes.size() == 2) dtcBytes.push_back(0x00);
        if (dtcBytes.size() != 3) dtcBytes = {0xFF, 0xFF, 0xFF};
        std::vector<std::vector<unsigned char>> reqs = {
            {0x10, 0xC0},
            {0x19, 0x04, dtcBytes[0], dtcBytes[1], dtcBytes[2], 0xFF},
            {0x12, 0x00, 0x04, 0x00, 0x00},
            {0x12, 0x00, 0x00},
            {0x21, 0x87}
        };
        std::vector<std::vector<unsigned char>> resps;
        SendBatch(g_diag, 0x7E0, 0x7E8, reqs, resps, 350);
        bool found = false;
        for (size_t i = 1; i < resps.size(); ++i) {
            if (!resps[i].empty() && (resps[i][0] == 0x59 || resps[i][0] == 0x52 || resps[i][0] == 0x61)) {
                body = DecodeCapturedFreezeFrame(resps[i]);
                found = true;
                break;
            }
        }
        if (!found) {
            std::vector<std::vector<unsigned char>> kResps;
            if (g_diag.RunKLineSession(0x11, {{0x12, 0x00, 0x00}, {0x21, 0x87}}, kResps, 350)) {
                for (const auto& kr : kResps) {
                    if (!kr.empty() && (kr[0] == 0x52 || kr[0] == 0x61)) {
                        body = DecodeCapturedFreezeFrame(kr);
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            body = "{\"ok\":false,\"error\":\"No Freeze-Frame / Snapshot data returned by ECU (59 04 / 52 / 61 87)\"}";
        }
    }
    else if (method == "POST" && path == "/api/db/live") {
        std::string pageText = ExtractJsonField(req, "page");
        if (pageText.empty()) pageText = "0";
        if (pageText.size() > 4 || pageText.find_first_not_of("0123456789") != std::string::npos) {
            statusCode = 400;
            body = "{\"ok\":false,\"error\":\"Invalid page\"}";
        } else {
            if (g_identityMatch.activeCandidate() == nullptr) {
                AutoDetectSuzukiVehicle();
            }
            body = ReadDbLivePage(g_identityMatch, std::stoi(pageText));
        }
    }
    else if (method == "POST" && path == "/api/vehicle/candidate") {
        std::string id = ExtractJsonField(req, "variantId");
        const DbIdentityRecord* selected = nullptr;
        for (const auto* candidate : g_identityMatch.candidates) {
            if (std::to_string(candidate->variantId) == id) selected = candidate;
        }
        if (!selected) {
            statusCode = 400;
            body = "{\"ok\":false,\"error\":\"Candidate not present in last identity result\"}";
        } else {
            g_identityMatch.selectedVariantId = selected->variantId;
            g_detectedVehicleDesc = std::string("User-selected candidate: ") + (selected->model[0] ? selected->model : selected->name);
            body = "{\"ok\":true,\"ecuVerified\":false,\"variantId\":" + std::to_string(selected->variantId) +
                   ",\"message\":\"Candidate selected and activated for DB parameter & DTC lookup: " + std::string(selected->name) + "\"}";
        }
    }
    else if (method == "GET" && path == "/api/vehicle/identity") {
        body = IdentityMatchJson(g_identityMatch);
    }
    else if (method == "GET" && path == "/api/status") {
        double v = g_diag.ReadBatteryVoltage();
        if (!g_diag.isConnected) {
            g_identityMatch = IdentityMatch{};
            g_ecmSupportedIds.clear();
            g_detectedVehicleDesc = "ECU/model not identified";
        }
        std::stringstream ss;
        ss << "{\"connected\":" << (g_diag.isConnected ? "true" : "false")
           << ",\"voltage\":" << JsonNumber(v, 2)
           << ",\"fw\":" << JsonString(g_diag.fwVer)
           << ",\"dll\":" << JsonString(g_diag.dllVer)
           << ",\"activeDeviceName\":" << JsonString(g_diag.activeDeviceName)
           << ",\"activeDllPath\":" << JsonString(g_diag.activeDllPath)
           << ",\"activeProfileId\":" << JsonString(g_activeProfileId)
           << ",\"detectedVehicle\":" << JsonString(g_detectedVehicleDesc)
           << ",\"vin\":" << JsonString(g_cachedVin)
           << "}";
        body = ss.str();
    }
    else if (method == "GET" && path == "/api/j2534/devices") {
        auto devs = EnumerateJ2534Devices();
        std::ostringstream ss;
        ss << "{\"activeName\":" << JsonString(g_diag.activeDeviceName)
           << ",\"activeDll\":" << JsonString(g_diag.activeDllPath)
           << ",\"connected\":" << (g_diag.isConnected ? "true" : "false")
           << ",\"devices\":[";
        for (size_t i = 0; i < devs.size(); i++) {
            if (i > 0) ss << ",";
            ss << "{\"id\":" << JsonString(devs[i].id)
               << ",\"name\":" << JsonString(devs[i].name)
               << ",\"vendor\":" << JsonString(devs[i].vendor)
               << ",\"dllPath\":" << JsonString(devs[i].dllPath)
               << ",\"exists\":" << (devs[i].exists ? "true" : "false")
               << "}";
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && path == "/api/j2534/select") {
        std::string dllPath = ExtractJsonField(req, "dllPath");
        std::string devName = ExtractJsonField(req, "name");
        if (dllPath.empty()) {
            statusCode = 400;
            body = "{\"ok\":false,\"error\":\"DLL yolu bos olamaz.\"}";
        } else {
            g_identityMatch = IdentityMatch{};
            g_ecmSupportedIds.clear();
            g_detectedVehicleDesc = "ECU/model not identified";
            bool ok = g_diag.SelectDevice(dllPath, devName);
            double v = g_diag.ReadBatteryVoltage();
            std::ostringstream ss;
            ss << "{\"ok\":" << (ok ? "true" : "false")
               << ",\"connected\":" << (g_diag.isConnected ? "true" : "false")
               << ",\"activeName\":" << JsonString(g_diag.activeDeviceName)
               << ",\"activeDll\":" << JsonString(g_diag.activeDllPath)
               << ",\"fw\":" << JsonString(g_diag.fwVer)
               << ",\"dll\":" << JsonString(g_diag.dllVer)
               << ",\"voltage\":" << JsonNumber(v, 2)
               << ",\"message\":" << JsonString(ok
                   ? (g_diag.isConnected ? ("J2534 cihazi baglandi: " + g_diag.activeDeviceName) : ("J2534 DLL yuklendi (" + g_diag.activeDeviceName + "), ancak USB cihazi yanit vermedi."))
                   : ("J2534 DLL yuklenemedi: " + dllPath))
               << "}";
            body = ss.str();
        }
    }
    else if (method == "GET" && path == "/api/vehicle/profiles") {
        std::ostringstream ss;
        ss << "{\"activeProfileId\":" << JsonString(g_activeProfileId)
           << ",\"detectedVehicle\":" << JsonString(g_detectedVehicleDesc)
           << ",\"profiles\":[";
        for (size_t i = 0; i < g_vehicleProfiles.size(); i++) {
            if (i > 0) ss << ",";
            ss << "{\"id\":" << JsonString(g_vehicleProfiles[i].id)
               << ",\"title\":" << JsonString(g_vehicleProfiles[i].title)
               << ",\"description\":" << JsonString(g_vehicleProfiles[i].description)
               << "}";
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && path == "/api/vehicle/select") {
        std::string profId = ExtractJsonField(req, "profileId");
        bool found = false;
        for (const auto& p : g_vehicleProfiles) {
            if (p.id == profId) {
                g_activeProfileId = p.id;
                g_identityMatch = IdentityMatch{};
                g_ecmSupportedIds.clear();
                g_detectedVehicleDesc = "Manual communication profile; ECU/model not verified";
                found = true;
                break;
            }
        }
        if (!found) {
            statusCode = 400;
            body = "{\"ok\":false,\"error\":\"Gecersiz arac profili.\"}";
        } else {
            body = "{\"ok\":true,\"activeProfileId\":" + JsonString(g_activeProfileId) +
                   ",\"detectedVehicle\":" + JsonString(g_detectedVehicleDesc) + "}";
        }
    }
    else if (method == "POST" && path == "/api/vehicle/autodetect") {
        ProcedureResult pr = AutoDetectSuzukiVehicle();
        std::ostringstream ss;
        ss << "{\"success\":" << (pr.success ? "true" : "false")
           << ",\"activeProfileId\":" << JsonString(g_activeProfileId)
           << ",\"detectedVehicle\":" << JsonString(g_detectedVehicleDesc)
           << ",\"message\":" << JsonString(pr.message)
           << ",\"log\":[";
        for (size_t i = 0; i < pr.log.size(); i++) {
            if (i > 0) ss << ",";
            ss << JsonString(pr.log[i]);
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && (path == "/api/kline/abs" || path == "/api/kline/4wd" || path == "/api/kline/bcm" || path == "/api/kline/srs" || path == "/api/kline/imm" || path == "/api/kline/ac" || path == "/api/kline/ecm" || path == "/api/kline/tcm" || path == "/api/kline/eps")) {
        unsigned char target = 0x29;
        std::string modCode = "ABS/ESP";
        if (path == "/api/kline/4wd") { target = 0x21; modCode = "4WD"; }
        else if (path == "/api/kline/bcm") { target = 0x5A; modCode = "BCM"; }
        else if (path == "/api/kline/srs") { target = 0x59; modCode = "SRS"; }
        else if (path == "/api/kline/imm") { target = 0x5D; modCode = "KLS/IMM"; }
        else if (path == "/api/kline/ac")  { target = 0x99; modCode = "HVAC"; }
        else if (path == "/api/kline/ecm") { target = (g_activeProfileId == "gv_jb419d") ? 0x7A : 0x11; modCode = "ECM"; }
        else if (path == "/api/kline/tcm") { target = 0x13; modCode = "TCM"; }
        else if (path == "/api/kline/eps") { target = 0x31; modCode = "EPS"; }

        std::string targetHex = HexAddr(target, 2);
        std::vector<std::vector<unsigned char>> reqs = {
            {0x21, 0x80},
            {0x18, 0x00, 0xFF, 0x00},
            {0x21, 0x00}
        };
        if (target == 0x29) {
            reqs.push_back({0x18, 0x00, 0x40, 0x00});
        }
        if (target == 0x5D || target == 0x5B) {
            reqs.push_back({0x21, 0xA1});
            reqs.push_back({0x21, 0xA0});
        }

        std::vector<std::vector<unsigned char>> resps;
        bool sessionOk = g_diag.RunKLineSession(target, reqs, resps, 400);
        if (!sessionOk && target == 0x5D) {
            // Auto fallback to alternate K-Line target 0x5B (Classic models)
            target = 0x5B;
            targetHex = "0x5B";
            sessionOk = g_diag.RunKLineSession(target, reqs, resps, 400);
        }

        std::vector<unsigned char> initRaw;
        bool startAccepted = sessionOk;
        long driverCode = 0;
        if (!sessionOk) {
            driverCode = g_diag.ProbeKLine(target, initRaw, startAccepted);
        }

        std::string ecuIdRaw = (resps.size() > 0 && resps[0].size() >= 2 && resps[0][0] == 0x61) ? HexBytes(resps[0]) : "";
        std::vector<DTCEntry> dtcs;
        ModuleScanResult dummyRes;
        dummyRes.dtcCount = -1;
        if (resps.size() > 1 && !resps[1].empty() && (resps[1][0] == 0x58 || resps[1][0] == 0x57)) {
            ParseKwpDtcList(resps[1], modCode, dtcs, dummyRes);
        } else if (resps.size() > 3 && !resps[3].empty() && (resps[3][0] == 0x58 || resps[3][0] == 0x57)) {
            ParseKwpDtcList(resps[3], modCode, dtcs, dummyRes);
        } else if (sessionOk) {
            std::string rej = (resps.size() > 1 && !resps[1].empty()) ? HexBytes(resps[1]) : "yanit yok";
            dummyRes.status = "K-Line yanit verdi ancak DTC okuma servisi reddedildi (" + rej + ")";
        }

        double sasDeg = std::numeric_limits<double>::quiet_NaN();
        if (target == 0x29 && resps.size() > 2 && resps[2].size() >= 2 + 22 && resps[2][0] == 0x61 && resps[2][1] == 0x00) {
            sasDeg = ReadSigned16BE(resps[2], 20) * 1.0;
        }

        int regKeyCount = -1;
        for (const auto& r : resps) {
            if (r.size() >= 3 && r[0] == 0x61 && r[1] == 0xA1) {
                regKeyCount = static_cast<int>(r[2]);
            }
        }

        std::ostringstream ss;
        ss << "{\"protocol\":\"ISO14230 Fast Init / Pin 7 / 10400 baud\""
           << ",\"module\":" << JsonString(modCode)
           << ",\"targetHex\":" << JsonString(targetHex)
           << ",\"driverCode\":" << driverCode
           << ",\"startCommunicationAccepted\":" << (startAccepted ? "true" : "false")
           << ",\"ecuIdRaw\":" << (ecuIdRaw.empty() ? "null" : JsonString(ecuIdRaw))
           << ",\"steeringAngleDeg\":" << JsonNumber(sasDeg, 1)
           << ",\"registeredKeyCount\":" << (regKeyCount >= 0 ? std::to_string(regKeyCount) : "null")
           << ",\"dtcCount\":" << dummyRes.dtcCount
           << ",\"dtcReadStatus\":" << JsonString(dummyRes.status)
           << ",\"dtcs\":[";
        for (size_t i = 0; i < dtcs.size(); i++) {
            if (i > 0) ss << ",";
            ss << "{\"code\":" << JsonString(dtcs[i].code) << ",\"title\":" << JsonString(dtcs[i].title) << "}";
        }
        ss << "]"
           << ",\"rawInit\":" << JsonString(HexBytes(initRaw))
           << ",\"message\":" << JsonString(startAccepted
               ? (modCode + " (" + targetHex + ") K-Line baglantisi basarili.")
               : (modCode + " (" + targetHex + ") yanit vermedi (Kontak acik mi / Pin 7 bagli mi?)."))
           << "}";
        body = ss.str();
    }
    else if (method == "POST" && path == "/api/scan") {
        if (g_activeProfileId == "auto") {
            AutoDetectSuzukiVehicle();
        }
        std::vector<DTCEntry> allDTCs;
        std::vector<ModuleScanResult> scanResults;

        auto activeModules = GetActiveSuzukiModules();
        for (const auto& mod : activeModules) {
            ModuleScanResult res;
            ScanDTCsForModule(mod, allDTCs, res);
            scanResults.push_back(res);
        }

        std::stringstream ss;
        ss << "{\"status\":\"ok\",\"activeProfileId\":" << JsonString(g_activeProfileId)
           << ",\"detectedVehicle\":" << JsonString(g_detectedVehicleDesc)
           << ",\"modules\":[";
        for (size_t i = 0; i < scanResults.size(); i++) {
            if (i > 0) ss << ",";
            ss << "{"
               << "\"shortName\":" << JsonString(scanResults[i].shortName) << ","
               << "\"name\":" << JsonString(scanResults[i].name) << ","
               << "\"variant\":" << JsonString(scanResults[i].variant) << ","
               << "\"canId\":" << JsonString(scanResults[i].canId) << ","
               << "\"responsive\":" << (scanResults[i].responsive ? "true" : "false") << ","
               << "\"dtcCount\":" << scanResults[i].dtcCount << ","
               << "\"status\":" << JsonString(scanResults[i].status)
               << "}";
        }
        ss << "],\"dtcs\":[";
        for (size_t i = 0; i < allDTCs.size(); i++) {
            if (i > 0) ss << ",";
            ss << "{"
               << "\"code\":" << JsonString(allDTCs[i].code) << ","
               << "\"system\":" << JsonString(allDTCs[i].system) << ","
               << "\"title\":" << JsonString(allDTCs[i].title) << ","
               << "\"suggestion\":" << JsonString(allDTCs[i].suggestion)
               << "}";
        }
        if (g_cachedVin.empty()) {
            ExecuteVinActionJson("read", "");
        }
        ss << "],\"vin\":" << JsonString(g_cachedVin) << "}";
        body = ss.str();
    }
    else if (method == "GET" && path == "/api/ecm/ident") {
        std::vector<std::vector<unsigned char>> resps;
        SendBatch(g_diag, 0x7E0, 0x7E8, {{0x10, 0xC0}, {0x21, 0x80}}, resps, 400);
        bool session = resps.size() > 0 && resps[0].size() >= 2 && resps[0][0] == 0x50;
        body = std::string("{\"session\":") + (session ? "true" : "false") +
               ",\"ident\":" + DecodeEcmIdentJson(resps.size() > 1 ? resps[1] : std::vector<unsigned char>{}) + "}";
    }
    else if (method == "POST" && path == "/api/ecm/discover") {
        std::vector<std::vector<unsigned char>> reqs = {{0x10, 0xC0}};
        for (int id = 0x01; id <= 0xFE; id++) reqs.push_back({0x21, (unsigned char)id});
        std::vector<std::vector<unsigned char>> resps;
        SendBatch(g_diag, 0x7E0, 0x7E8, reqs, resps, 150);
        bool session = !resps.empty() && resps[0].size() >= 2 && resps[0][0] == 0x50;
        g_ecmSupportedIds.clear();
        std::ostringstream ss;
        ss << "{\"session\":" << (session ? "true" : "false") << ",\"ids\":[";
        bool first = true;
        for (size_t i = 1; i < resps.size(); i++) {
            const auto& r = resps[i];
            if (r.size() >= 2 && r[0] == 0x61) {
                g_ecmSupportedIds.push_back(r[1]);
                if (!first) ss << ",";
                first = false;
                char idb[8]; sprintf_s(idb, "%02X", r[1]);
                ss << "{\"id\":" << JsonString(idb) << ",\"length\":" << (r.size() - 2)
                   << ",\"data\":" << JsonString(HexBytes(r, 2)) << "}";
            }
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "GET" && path == "/api/ecm/rawlive") {
        std::vector<std::vector<unsigned char>> reqs = {{0x3E, 0x01}};
        size_t limit = std::min<size_t>(g_ecmSupportedIds.size(), 48);
        for (size_t i = 0; i < limit; i++) reqs.push_back({0x21, g_ecmSupportedIds[i]});
        std::vector<std::vector<unsigned char>> resps;
        bool ok = limit > 0 && SendBatch(g_diag, 0x7E0, 0x7E8, reqs, resps, 250);
        std::ostringstream ss;
        ss << "{\"ok\":" << (ok ? "true" : "false") << ",\"discovered\":" << g_ecmSupportedIds.size() << ",\"ids\":[";
        for (size_t i = 0; i < limit; i++) {
            if (i) ss << ",";
            char idb[8]; sprintf_s(idb, "%02X", g_ecmSupportedIds[i]);
            const auto& r = resps.size() > i + 1 ? resps[i + 1] : std::vector<unsigned char>{};
            bool pos = r.size() >= 2 && r[0] == 0x61;
            ss << "{\"id\":" << JsonString(idb) << ",\"data\":" << (pos ? JsonString(HexBytes(r, 2)) : "null") << "}";
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && (path.compare(0, 15, "/api/calibrate/") == 0 || path == "/api/dtc/clear")) {
        std::string sub = (path == "/api/dtc/clear") ? "clear" : path.substr(15);
        ProcedureResult pr;
        if (!IsServiceSupportedForProfile(sub, g_activeProfileId)) {
            bool isUnidentified = (ResolveEffectiveProfileFor(g_activeProfileId) == "UNIDENTIFIED");
            pr.step = isUnidentified ? "VEHICLE_UNIDENTIFIED" : "SERVICE_NOT_SUPPORTED";
            pr.success = false;
            pr.message = isUnidentified
                ? ("Arac/ECU henuz tanimlanmadi ('auto' modu). Yazma/kalibrasyon/DTC silme (" + sub + ") guvenlik geregi KAPALI. Lutfen once 'Arac Tara' ile kimlik dogrulayin veya arac modelini secin.")
                : ("Bu servis / kalibrasyon fonksiyonu (" + sub + ") secili arac modelinde (" + g_activeProfileId + ") desteklenmemektedir (KAPALI). Guvenlik icin komut gonderilmedi.");
            pr.log.push_back("Security Guard: Service rejected (" + ResolveEffectiveProfileFor(g_activeProfileId) + " / raw: " + g_activeProfileId + ").");
        } else {
            if (sub == "sas") pr = ExecuteSASCalibration();
            else if (sub == "dpf") pr = ExecuteDPFReset();
            else if (sub == "oil") pr = ExecuteOilReset();
            else if (sub == "egr") pr = ExecuteEGRInit();
            else if (sub == "throttle") pr = ExecuteThrottleInit();
            else if (sub == "injector") pr = ExecuteInjectorZfcInit();
            else if (sub == "dpf_regen") pr = ExecuteDPFRegen();
            else if (sub == "rail_test") pr = ExecuteHighPressureTest();
            else if (sub == "clear") pr = ExecuteClearAllDTCs();
            else if (sub == "f9q_ecm_replace") pr = ExecuteF9QEcmReplaceAndStarterTest(false);
            else if (sub == "f9q_starter_test") pr = ExecuteF9QEcmReplaceAndStarterTest(true);
            else if (sub == "can_dpf_regen") pr = ExecuteCanDieselCalibration("dpf_regen");
            else if (sub == "can_oil_reset") pr = ExecuteCanDieselCalibration("oil_reset");
            else if (sub == "can_egr_learn") pr = ExecuteCanDieselCalibration("egr_learn");
            else if (sub == "can_diesel_sensors") pr = ExecuteCanDieselCalibration("sensors");
            else if (sub == "petrol_ecm") pr = ExecutePetrolEcmCalibration();
            else if (sub == "tcm") pr = ExecuteTcmCalibration();
            else if (sub == "eps") pr = ExecuteEpsCalibration();
            else if (sub == "abs_bleed") pr = ExecuteAbsBleedAndEspFunc();
            else if (sub == "bcm_airbag") pr = ExecuteBcmAndTpmsService(false);
            else if (sub == "tpms") pr = ExecuteBcmAndTpmsService(true);
            else if (sub == "ipc") pr = ExecuteIpcServiceReset();
            else if (sub == "alc") pr = ExecuteAlcSensorInit();
            else if (sub == "epb") pr = RunEpbServiceProcedure();
            else if (sub == "hvac") pr = RunHvacCalibProcedure();
            else if (sub == "shvs_hybrid") pr = RunShvsHybridProcedure();
            else if (sub == "adas_calib") pr = RunAdasCalibProcedure();
            else {
                statusCode = 404;
                pr.message = "Bilinmeyen kalibrasyon komutu: " + sub;
            }
        }
        std::ostringstream ss;
        ss << "{\"success\":" << (pr.success ? "true" : "false")
           << ",\"step\":" << JsonString(pr.step)
           << ",\"message\":" << JsonString(pr.message)
           << ",\"log\":[";
        for (size_t i = 0; i < pr.log.size(); i++) {
            if (i > 0) ss << ",";
            ss << JsonString(pr.log[i]);
        }
        ss << "]}";
        body = ss.str();
    }
    else if ((method == "POST" || method == "GET") && path == "/api/db-catalog") {
        std::string baseStr = ExtractJsonField(req, "baseId");
        std::string query = ExtractJsonField(req, "query");
        int baseId = (!baseStr.empty() && baseStr.find_first_not_of("0123456789") == std::string::npos) ? std::atoi(baseStr.c_str()) : 0;
        body = BuildDbCatalogJson(baseId, query);
    }
    else if (method == "POST" && path == "/api/active-test/run") {
        std::string testName = ExtractJsonField(req, "testName");
        std::string mode = ExtractJsonField(req, "mode");
        std::string baseStr = ExtractJsonField(req, "baseId");
        int baseId = (!baseStr.empty() && baseStr.find_first_not_of("0123456789") == std::string::npos) ? std::atoi(baseStr.c_str()) : 0;
        ProcedureResult pr = ExecuteDbActiveTest(testName, mode.empty() ? "start" : mode, baseId);
        std::ostringstream ss;
        ss << "{\"success\":" << (pr.success ? "true" : "false")
           << ",\"step\":" << JsonString(pr.step)
           << ",\"message\":" << JsonString(pr.message)
           << ",\"log\":[";
        for (size_t i = 0; i < pr.log.size(); i++) {
            if (i > 0) ss << ",";
            ss << JsonString(pr.log[i]);
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && path == "/api/utility/run") {
        std::string code = ExtractJsonField(req, "code");
        std::string baseStr = ExtractJsonField(req, "baseId");
        int baseId = (!baseStr.empty() && baseStr.find_first_not_of("0123456789") == std::string::npos) ? std::atoi(baseStr.c_str()) : 0;
        ProcedureResult pr = ExecuteDbUtility(code, baseId);
        std::ostringstream ss;
        ss << "{\"success\":" << (pr.success ? "true" : "false")
           << ",\"step\":" << JsonString(pr.step)
           << ",\"message\":" << JsonString(pr.message)
           << ",\"log\":[";
        for (size_t i = 0; i < pr.log.size(); i++) {
            if (i > 0) ss << ",";
            ss << JsonString(pr.log[i]);
        }
        ss << "]}";
        body = ss.str();
    }
    else if ((method == "POST" || method == "GET") && path == "/api/coding/read") {
        body = ReadBcmCodingStateJson();
    }
    else if (method == "POST" && path == "/api/coding/write") {
        std::string featureId = ExtractJsonField(req, "featureId");
        std::string optStr = ExtractJsonField(req, "optionValue");
        if (optStr.empty()) optStr = ExtractJsonField(req, "option");
        int optVal = (!optStr.empty() && optStr.find_first_not_of("-0123456789") == std::string::npos) ? std::atoi(optStr.c_str()) : 0;
        ProcedureResult pr = ExecuteHiddenFeatureCoding(featureId, optVal);
        std::ostringstream ss;
        ss << "{\"success\":" << (pr.success ? "true" : "false")
           << ",\"step\":" << JsonString(pr.step)
           << ",\"message\":" << JsonString(pr.message)
           << ",\"log\":[";
        for (size_t i = 0; i < pr.log.size(); i++) {
            if (i > 0) ss << ",";
            ss << JsonString(pr.log[i]);
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && path == "/api/immo/action") {
        std::string action = ExtractJsonField(req, "action");
        std::string pin = ExtractJsonField(req, "pin");
        std::string seedKey = ExtractJsonField(req, "seedKey");
        std::string prof = ExtractJsonField(req, "profile");
        if (!prof.empty() && prof != g_activeProfileId && prof != "auto") {
            g_activeProfileId = prof;
        }
        ProcedureResult pr = ExecuteImmobilizerAction(action, pin, seedKey);
        std::ostringstream ss;
        ss << "{\"success\":" << (pr.success ? "true" : "false")
           << ",\"step\":" << JsonString(pr.step)
           << ",\"message\":" << JsonString(pr.message)
           << ",\"immoSystem\":" << JsonString(GetImmoSystemName(GetActiveImmoSystem(g_activeProfileId)))
           << ",\"activeProfileId\":" << JsonString(g_activeProfileId)
           << ",\"log\":[";
        for (size_t i = 0; i < pr.log.size(); i++) {
            if (i > 0) ss << ",";
            ss << JsonString(pr.log[i]);
        }
        ss << "]}";
        body = ss.str();
    }
    else if (method == "POST" && path == "/api/injector/ima") {
        if (!IsServiceSupportedForProfile("injector", g_activeProfileId)) {
            body = "{\"success\":false,\"message\":\"Enjektor IMA kalibrasyonu secili arac modelinde (" + g_activeProfileId + ") desteklenmemektedir (Benzinli motorda IMA bulunmaz).\",\"log\":[\"Security Guard: Injector IMA rejected by vehicle model compatibility matrix.\"]}";
        } else {
            std::string action = ExtractJsonField(req, "action");
            std::string c1 = ExtractJsonField(req, "cyl1");
            std::string c2 = ExtractJsonField(req, "cyl2");
            std::string c3 = ExtractJsonField(req, "cyl3");
            std::string c4 = ExtractJsonField(req, "cyl4");
            body = ExecuteInjectorImaJson(action.empty() ? "read" : action, c1, c2, c3, c4);
        }
    }
    else if (method == "POST" && path == "/api/vin") {
        std::string action = ExtractJsonField(req, "action");
        std::string vin = ExtractJsonField(req, "vin");
        body = ExecuteVinActionJson(action.empty() ? "read" : action, vin);
    }
    else if (method == "GET" && path == "/api/live") {
        CompleteLiveData d = QueryCompleteLiveData();
        std::stringstream ss;
        ss << "{"
           << "\"vbatt\":" << JsonNumber(d.vbatt, 2) << ","
           << "\"ecmVbatt\":" << JsonNumber(d.ecmVbatt, 2) << ","
           << "\"rpm\":" << JsonNumber(d.rpm, 0) << ","
           << "\"desiredIdle\":" << JsonNumber(d.desiredIdle, 0) << ","
           << "\"speed\":" << JsonNumber(d.speed, 1) << ","
           << "\"coolantTemp\":" << JsonNumber(d.coolantTemp, 1) << ","
           << "\"intakeAirTemp\":" << JsonNumber(d.intakeAirTemp, 1) << ","
           << "\"fuelTemp\":" << JsonNumber(d.fuelTemp, 1) << ","
           << "\"baroPress\":" << JsonNumber(d.baroPress, 0) << ","
           << "\"boostPress\":" << JsonNumber(d.boostPress, 0) << ","
           << "\"pedalPos\":" << JsonNumber(d.pedalPos, 1) << ","
           << "\"railPressBar\":" << JsonNumber(d.railPressBar, 1) << ","
           << "\"dpfSootGram\":" << JsonNumber(d.dpfSootGram, 2) << ","
           << "\"dpfTemp\":" << JsonNumber(d.dpfTemp, 1) << ","
           << "\"dpfDiffPress\":" << JsonNumber(d.dpfDiffPress, 0) << ","
           << "\"steeringAngle\":" << JsonNumber(d.steeringAngle, 1) << ","
           << "\"yawRate\":" << JsonNumber(d.yawRate, 1) << ","
           << "\"lateralG\":" << JsonNumber(d.lateralG, 2) << ","
           << "\"sasValid\":" << (d.sasValid ? "true" : "false") << ","
           << "\"sasStatus\":" << JsonString(d.sasStatus) << ","
           << "\"ecmOnline\":" << (d.ecmOnline ? "true" : "false") << ","
           << "\"absOnline\":" << (d.absOnline ? "true" : "false") << ","
           << "\"absMode\":" << JsonString(d.absMode) << ","
           << "\"activeProfileId\":" << JsonString(g_activeProfileId) << ","
           << "\"detectedVehicle\":" << JsonString(g_detectedVehicleDesc) << ","
           << "\"vin\":" << JsonString(g_cachedVin) << ","
           << "\"connected\":" << (d.connected ? "true" : "false")
           << "}";
        body = ss.str();
    }
    else {
        statusCode = 404;
        body = "{\"error\":\"Not Found\"}";
    }

    const char* statusText = "OK";
    if (statusCode == 400) statusText = "Bad Request";
    else if (statusCode == 403) statusText = "Forbidden";
    else if (statusCode == 404) statusText = "Not Found";
    else if (statusCode == 409) statusText = "Conflict";

    std::stringstream resp;
    resp << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    resp << "Content-Type: " << contentType << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    resp << "Connection: close\r\n\r\n";
    resp << body;

    std::string s = resp.str();
    size_t sent = 0;
    while (sent < s.size()) {
        int n = send(clientSock, s.data() + sent, static_cast<int>(s.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }
    closesocket(clientSock);
}

int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "--match-identity") {
        std::string cidArg = (argc >= 4 && std::string(argv[3]) != "-") ? argv[3] : "";
        std::string partArg = (argc >= 5 && std::string(argv[4]) != "-") ? argv[4] : "";
        std::cout << IdentityMatchJson(MatchDbIdentifiers(std::atoi(argv[2]), cidArg, partArg)) << std::endl;
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--check-identity") {
        for (const auto& record : kDbIdentities) {
            std::vector<unsigned char> response;
            if (IsF9QBase(record.baseId)) {
                response = {0x61, 0x80, 0x82, 0x00, 0x51, 0x86, 0x48, 'P', '0', '3', '7'};
                response.resize(26, 0);
            } else {
                std::string cidStr(record.cid);
                std::string partStr(record.partNumber);
                if (cidStr.size() == 10) {
                    response = IsUdsBase(record.baseId) ? std::vector<unsigned char>{0x62, 0xF1, 0x01} : std::vector<unsigned char>{0x5A, 0x8E};
                    response.insert(response.end(), cidStr.begin(), cidStr.end());
                } else if (partStr.size() == 10) {
                    response = IsUdsBase(record.baseId) ? std::vector<unsigned char>{0x62, 0xF1, 0x88} : std::vector<unsigned char>{0x5A, 0x8A};
                    response.insert(response.end(), partStr.begin(), partStr.end());
                } else {
                    continue;
                }
            }
            IdentityMatch match = MatchDbIdentifiers(record.baseId, record.cid, record.partNumber);
            if (IsF9QBase(record.baseId)) {
                match = MatchDbIdentity(record.baseId, response);
                if (record.baseId == 93 && !match.exact()) return 25;
            }
            bool found = false;
            for (const auto* candidate : match.candidates) if (candidate->variantId == record.variantId) found = true;
            if (!found) return 2;
            response.pop_back();
            if (!MatchDbIdentity(record.baseId, response).candidates.empty()) return 3;
        }
        if (!MatchDbIdentity(41, {0x7F, 0x1A, 0x11}).candidates.empty()) return 4;
        DbLiveSpec example = {1, 1, 0, 2, 65535, "test", "unit", 2, {{0,32767,0.1,0},{32768,65535,0.1,-6553.6}}};
        double decoded = 0;
        if (!DecodeDbNumeric(example, {0xFF,0xF6}, decoded) || std::abs(decoded + 1.0) > 0.00001) return 5;
        if (DecodeDbNumeric(example, {0x01}, decoded)) return 6;
        DbLiveSpec enumExample = {2, 1, 0, 1, 32, "flag", "", 2, {{0,0,0,0},{32,32,0,1}}, "enum", "[]"};
        if (!DecodeDbNumeric(enumExample, {0xA0}, decoded) || decoded != 1) return 8;
        if (!DecodeDbNumeric(enumExample, {0x80}, decoded) || decoded != 0) return 9;
        for (const auto& spec : kDbLiveSpecs) {
            if (spec.conversionCount < 1 || spec.conversionCount > 32 || !spec.outputs || !spec.dataType) return 10;
        }
        if (ReadDbLivePage(IdentityMatch{}, 0).find("Exact ECU identity required") == std::string::npos) return 7;
        DbSupportRule addRule={1,1,"ADD","221100","FFFFFF40000000","62110040000000"};
        DbSupportRule deleteRule={1,1,"DELETE","221100","FFFFFF40000000","62110040000000"};
        if(MatchSupportMask(addRule,{0x62,0x11,0,0xC0,0,0,0})!=DbSupport::Supported)return 11;
        if(MatchSupportMask(addRule,{0x62,0x11,0,0,0,0,0})!=DbSupport::Unsupported)return 12;
        if(MatchSupportMask(addRule,{0x7F,0x22,0x11})!=DbSupport::Unknown)return 13;
        if(EvaluateSupportRules({&addRule,&deleteRule},[](const DbSupportRule&){return DbSupport::Supported;})!=DbSupport::Unsupported)return 14;
        if(EvaluateSupportRules({&deleteRule},[](const DbSupportRule&){return DbSupport::Unknown;})!=DbSupport::Unknown)return 15;
        if(EvaluateSupportRules({},[](const DbSupportRule&){return DbSupport::Unknown;})!=DbSupport::Supported)return 16;
        double computed=0;
        if(!EvaluateDbExpression("$HB 256 * $LB +",{{"HB",1},{"LB",2}},computed)||computed!=258)return 17;
        if(EvaluateDbExpression("1 0 /",{},computed)||EvaluateDbExpression("$missing",{},computed)||EvaluateDbExpression("1 +",{},computed))return 18;
        std::vector<unsigned char> capture(149,0);capture[148]=1;capture[147]=2;
        if(!DecodeDbComputed(6819,{{"31001AE5FF",capture}},computed)||computed!=258)return 19;
        if(DecodeDbComputed(6819,{{"31001AE5FF",{1,2}}},computed))return 20;
        std::map<std::string,std::vector<unsigned char>> multi;
        if(!ParseCapturedPayloads("1123=01;1125=02",multi)) {
            if(!ParseCapturedPayloads("1123=01\n1125=02",multi))return 21;
        }
        if(!DecodeDbComputed(1330,multi,computed)||computed!=-97)return 22;
        if(ParseCapturedPayloads("1123=01\n1123=02",multi))return 23;
        std::vector<DTCEntry> entries;ModuleScanResult dtcResult;
        ParseUdsDtcList19({0x59,0x02,0xFF,1,2,3},"ECM",entries,dtcResult);
        if(dtcResult.dtcCount!=-1||!entries.empty())return 24;
        static_assert(sizeof(kDbUtilities) / sizeof(kDbUtilities[0]) >= 500 && sizeof(kDbActiveTests) / sizeof(kDbActiveTests[0]) >= 600, "DB utility or active test table incomplete");
        if (BuildDbCatalogJson(93, "").find("DSL-01") == std::string::npos) return 27;
        // Audit regression guards: unidentified mode gating, K-Line DTC rejection, SAS post-verify, Immo payload validators
        g_identityMatch = IdentityMatch{};
        if (IsServiceSupportedForProfile("sas", "auto") || IsServiceSupportedForProfile("clear", "auto")) return 28;
        std::vector<DTCEntry> klDtcs;
        ModuleScanResult klRejRes;
        std::vector<std::vector<unsigned char>> klRejResp = {{}, {}, {}, {}, {0x7F, 0x18, 0x11}, {0x7F, 0x18, 0x11}, {0x61, 0x08, 0x01}};
        if (EvaluateKLineDtcScan(0x29, "ABS/ESP", klRejResp, klDtcs, klRejRes) || klRejRes.dtcCount != -1) return 29;
        ModuleScanResult klOkRes;
        std::vector<std::vector<unsigned char>> klZeroResp = {{}, {}, {}, {}, {0x58, 0x00}, {}, {0x61, 0x08, 0x01}};
        if (!EvaluateKLineDtcScan(0x29, "ABS/ESP", klZeroResp, klDtcs, klOkRes) || klOkRes.dtcCount != 0) return 30;
        std::string sasStep, sasMsg;
        if (VerifySasPostCalibration(SasPostCheck{false, false, false, false}, sasStep, sasMsg)) return 31;
        if (VerifySasPostCalibration(SasPostCheck{false, false, true, false}, sasStep, sasMsg)) return 31;
        if (VerifySasPostCalibration(SasPostCheck{true, true, false, false}, sasStep, sasMsg)) return 31;
        if (VerifySasPostCalibration(SasPostCheck{true, false, true, false}, sasStep, sasMsg)) return 32;
        if (VerifySasPostCalibration(SasPostCheck{true, true, true, true}, sasStep, sasMsg)) return 33;
        if (!VerifySasPostCalibration(SasPostCheck{true, true, true, false}, sasStep, sasMsg)) return 34;
        if (IsValidImmoKeyStatusResp({0x61, 0x80, 0x01}) || IsValidImmoKeyStatusResp({0x61, 0xA0, 0x01}) ||
            IsValidImmoKeyStatusResp({0x61, 0x01, 0x01}) || !IsValidImmoKeyStatusResp({0x61, 0xA1, 0x02, 0x24, 0x05})) return 35;
        if (IsValidTransponderIdResp({0x61, 0xA0, 0x01, 0x02, 0x03, 0x04}) ||
            IsValidTransponderIdResp({0x61, 0xB0, 0x00, 0x00, 0x00, 0x00}) ||
            IsValidTransponderIdResp({0x61, 0xB0, 0x11}) ||
            !IsValidTransponderIdResp({0x61, 0xB0, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0, 0x01, 0x02})) return 36;
        std::string ic1, ic2, ic3, ic4;
        if (ParseInjectorIma21AdResponse({0x61, 0xAD}, ic1, ic2, ic3, ic4)) return 37;
        std::vector<unsigned char> imaGood = {0x61, 0xAD};
        for (const char* s : {"1111111", "2222222", "3333333", "4444444"}) {
            for (int k = 0; k < 7; ++k) imaGood.push_back(static_cast<unsigned char>(s[k]));
        }
        if (!ParseInjectorIma21AdResponse(imaGood, ic1, ic2, ic3, ic4) ||
            ic1 != "1111111" || ic2 != "2222222" || ic3 != "3333333" || ic4 != "4444444") return 38;
        std::string savedProf = g_activeProfileId;
        g_activeProfileId = "gv_jb419d";
        if (ExecuteInjectorImaJson("write", "12345678", "2222222", "3333333", "4444444").find("\"ok\":false") == std::string::npos) {
            g_activeProfileId = savedProf;
            return 39;
        }
        g_activeProfileId = "suzuki_can";
        if (ExecuteImmobilizerAction("register_remote", "0895", "").success ||
            ExecuteImmobilizerAction("ecu_immo_init", "0895", "").success ||
            ExecuteImmobilizerAction("steering_unlock", "0895", "").success) {
            g_activeProfileId = savedProf;
            return 40;
        }
        g_activeProfileId = "suzuki_kline";
        if (ExecuteImmobilizerAction("antenna_test", "", "").success) {
            g_activeProfileId = savedProf;
            return 41;
        }
        // Audit Round 3 guards: Base 169 address/session, auto-base platform guard, UDS seed, key count delta, strict 4-char PIN
        DbBaseComm b169 = GetDbBaseComm(169);
        DbBaseComm bUnknown = GetDbBaseComm(999999);
        if (b169.isKline || b169.canTx != 0x745 || b169.canRx != 0x765 || !IsUdsBase(169) ||
            bUnknown.isKline || bUnknown.canTx != 0 || bUnknown.canRx != 0) {
            g_activeProfileId = savedProf;
            return 42;
        }
        std::string targetErr;
        g_activeProfileId = "suzuki_can";
        g_identityMatch = MatchDbIdentifiers(41, "33920-65J0", "");
        if (ResolveDbTargetBase("169", 0, targetErr) != 0 ||
            ResolveDbTargetBase("41,169", 0, targetErr) != 41) {
            g_identityMatch = IdentityMatch{};
            g_activeProfileId = savedProf;
            return 43;
        }
        g_identityMatch = IdentityMatch{};
        if (IsValidUdsSeedResp({0x67, 0x03}) ||
            IsValidUdsSeedResp({0x67, 0x03, 0x00, 0x00, 0x00, 0x00}) ||
            IsValidUdsSeedResp({0x67, 0x03, 0xFF, 0xFF, 0xFF, 0xFF}) ||
            IsValidUdsSeedResp({0x67, 0x02, 0x11, 0x22, 0x33, 0x44}) ||
            !IsValidUdsSeedResp({0x67, 0x03, 0x12, 0x34, 0x56, 0x78})) {
            g_activeProfileId = savedProf;
            return 44;
        }
        std::string kcReason;
        if (VerifyImmoKeyCountTransition(false, -1, 2, kcReason) ||
            VerifyImmoKeyCountTransition(false, 2, 2, kcReason) ||
            !VerifyImmoKeyCountTransition(false, 2, 3, kcReason) ||
            VerifyImmoKeyCountTransition(true, 2, 1, kcReason) ||
            !VerifyImmoKeyCountTransition(true, 2, 0, kcReason)) {
            g_activeProfileId = savedProf;
            return 45;
        }
        unsigned char pHi = 0, pLo = 0, bHi = 0, bLo = 0;
        std::string pErr;
        if (ParseStrictImmoPin4("", pHi, pLo, bHi, bLo, pErr) ||
            ParseStrictImmoPin4("123", pHi, pLo, bHi, bLo, pErr) ||
            ParseStrictImmoPin4("12345", pHi, pLo, bHi, bLo, pErr) ||
            ParseStrictImmoPin4("089G", pHi, pLo, bHi, bLo, pErr) ||
            !ParseStrictImmoPin4("0895", pHi, pLo, bHi, bLo, pErr)) {
            g_activeProfileId = savedProf;
            return 46;
        }
        g_activeProfileId = "gv_jb419d";
        if (ExecuteImmobilizerAction("verify_pin", "", "").step != "INVALID_PIN" ||
            ExecuteImmobilizerAction("verify_pin", "12345", "").step != "INVALID_PIN" ||
            RunAdasCalibProcedure().step != "MODEL_NOT_SUPPORTED") {
            g_activeProfileId = savedProf;
            return 47;
        }
        g_activeProfileId = "suzuki_can";
        auto rHf = ExecuteHiddenFeatureCoding("HF-UDS-IKEY", 0);
        auto rSeed = ExecuteImmobilizerAction("uds_request_seed", "", "");
        if (rHf.success || (rHf.step != "FEATURE_NOT_SUPPORTED" && rHf.step != "PLATFORM_MISMATCH") ||
            rSeed.success || rSeed.step != "PLATFORM_MISMATCH") {
            g_activeProfileId = savedProf;
            return 48;
        }
        g_activeProfileId = savedProf;
        std::cout << "DB identity, numeric, support, computed, utility, active test, security and capture checks passed; no hardware opened" << std::endl;
        return 0;
    }
    std::cout << "=======================================================" << std::endl;
    std::cout << " Suzuki Multi-Vehicle Diagnostic Server (J2534) [BETA] " << std::endl;
    std::cout << " Lead Engineer: Ayem Kaan Yildirim                    " << std::endl;
    std::cout << " Website: https://kaanyildirim.com                     " << std::endl;
    std::cout << " Verified on Grand Vitara 1.9 DDiS. Other models exper." << std::endl;
    std::cout << " Professional CAN & K-Line Profiles + Device Selector  " << std::endl;
    std::cout << "=======================================================" << std::endl;

    if (!g_diag.Init()) {
        auto devs = EnumerateJ2534Devices();
        for (const auto& d : devs) {
            if (d.exists && g_diag.SelectDevice(d.dllPath, d.name)) {
                break;
            }
        }
    }
    std::cout << "[+] Active J2534: " << g_diag.activeDeviceName << " (" << g_diag.activeDllPath << ")" << std::endl;

    int serverPort = 8890;
    if (argc >= 2 && argv[1][0] != '-') {
        int customPort = std::atoi(argv[1]);
        if (customPort > 1024 && customPort < 65535) {
            serverPort = customPort;
        }
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    bool bound = false;
    for (int p = serverPort; p < serverPort + 10; ++p) {
        serverAddr.sin_port = htons((u_short)p);
        if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
            serverPort = p;
            bound = true;
            break;
        }
    }

    if (!bound) {
        std::cerr << "[-] Port " << serverPort << " (ve araligi) mesgul, lutfen acik olan diger sunuculari kapatin." << std::endl;
        closesocket(serverSock);
        WSACleanup();
        return 1;
    }

    listen(serverSock, 10);
    std::cout << "[+] Suzuki Professional Interface Ready -> http://127.0.0.1:" << serverPort << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
        if (clientSock != INVALID_SOCKET) {
            DWORD timeout = 4000;
            setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
            HandleClient(clientSock);
        }
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}
