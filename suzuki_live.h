#pragma once
#include "suzuki_live.generated.h"
#include "suzuki_support.h"
#include "suzuki_computed.h"

struct DbBaseComm {
    bool isKline = false;
    unsigned char klineTarget = 0x11;
    unsigned long canTx = 0x7E0;
    unsigned long canRx = 0x7E8;
};

inline DbBaseComm GetDbBaseComm(int baseId) {
    switch (baseId) {
        case 8: case 162: return {false, 0, 0x740, 0x760}; // NISSAN CAN-1 / UDS-CAN ABS/ESP
        case 9: case 174: return {false, 0, 0x744, 0x764}; // NISSAN CAN-1 / UDS-CAN AC/HVAC
        case 10: case 166: return {false, 0, 0x752, 0x772}; // NISSAN CAN-1 / UDS-CAN AIRBAG
        case 11: case 18: case 120: case 121: case 169: case 176: return {false, 0, 0x745, 0x765}; // NISSAN CAN / UDS-CAN BCM & IMMO (Landy C26/C27)
        case 12: case 19: case 119: case 160: return {false, 0, 0x7E0, 0x7E8}; // NISSAN CAN ECM
        case 13: case 22: case 177: return {false, 0, 0x743, 0x763}; // NISSAN CAN / UDS-CAN IPC
        case 14: case 23: case 159: return {false, 0, 0x74D, 0x76D}; // NISSAN CAN IPDM-ER / USM
        case 15: return {false, 0, 0x75C, 0x77C}; // NISSAN CAN-N AFS
        case 16: case 167: return {false, 0, 0x759, 0x779}; // NISSAN ASD-L
        case 17: case 168: return {false, 0, 0x709, 0x729}; // NISSAN ASD-R
        case 20: case 161: return {false, 0, 0x742, 0x762}; // NISSAN CAN EPS
        case 21: case 122: return {false, 0, 0x74C, 0x76C}; // NISSAN CAN-N I-KEY / IMMO
        case 24: case 123: return {false, 0, 0x7E1, 0x7E9}; // NISSAN CAN TCM
        case 30: case 118: return {true, 0x10, 0, 0}; // NISSAN DDL2 ECM
        case 33: return {false, 0, 0x262, 0x662}; // SUZUKI CAN 4WD
        case 34: return {false, 0, 0x261, 0x661}; // SUZUKI CAN ABS/ESP
        case 35: return {false, 0, 0x272, 0x672}; // SUZUKI CAN AC
        case 36: return {false, 0, 0x264, 0x664}; // SUZUKI CAN ACCM
        case 37: return {false, 0, 0x273, 0x673}; // SUZUKI CAN AIRBAG
        case 38: return {false, 0, 0x276, 0x676}; // SUZUKI CAN ALC
        case 39: case 44: case 139: return {false, 0, 0x271, 0x671}; // SUZUKI CAN BCM
        case 40: return {false, 0, 0x7E6, 0x7EE}; // SUZUKI CAN CNG
        case 41: case 45: case 52: case 53: case 93: case 124: case 136: case 182: case 189: case 194: case 196: return {false, 0, 0x7E0, 0x7E8}; // SUZUKI CAN / UDS ECM
        case 42: return {false, 0, 0x263, 0x663}; // SUZUKI CAN EPS
        case 43: return {false, 0, 0x277, 0x677}; // SUZUKI CAN ICM
        case 46: case 48: return {false, 0, 0x275, 0x675}; // SUZUKI CAN KLS
        case 47: return {false, 0, 0x274, 0x674}; // SUZUKI CAN IPC
        case 49: case 147: return {false, 0, 0x7E1, 0x7E9}; // SUZUKI CAN / UDS TCM
        case 56: return {true, 0x21, 0, 0}; // SUZUKI KWP 4WD
        case 58: case 96: case 97: return {true, 0x29, 0, 0}; // SUZUKI KWP ABS/ESP
        case 59: case 68: return {true, 0x99, 0, 0}; // SUZUKI KWP AC
        case 60: return {true, 0x98, 0, 0}; // SUZUKI KWP AC2
        case 61: case 62: return {true, 0x59, 0, 0}; // SUZUKI KWP AIRBAG
        case 63: case 140: return {true, 0x5A, 0, 0}; // SUZUKI KWP BCM
        case 64: case 71: case 99: case 102: case 106: case 137: return {true, 0x11, 0, 0}; // SUZUKI KWP ECM (0x11)
        case 66: return {true, 0x0A, 0, 0}; // SUZUKI KWP EMCD
        case 67: case 108: case 114: return {true, 0x31, 0, 0}; // SUZUKI KWP EPS / DELPHI IMMO
        case 69: return {true, 0x09, 0, 0}; // SUZUKI KWP EV_MOTOR
        case 70: case 111: return {true, 0x5D, 0, 0}; // SUZUKI KWP IMMOBILIZER
        case 72: return {true, 0x5B, 0, 0}; // SUZUKI KWP OCM
        case 73: case 138: return {true, 0x01, 0, 0}; // SUZUKI KWP PCM
        case 74: return {true, 0x27, 0, 0}; // SUZUKI KWP SCC
        case 75: return {true, 0x19, 0, 0}; // SUZUKI KWP TCM
        case 76: return {true, 0x5C, 0, 0}; // SUZUKI KWP TPMS
        case 90: return {false, 0, 0x793, 0x7A3}; // SUZUKI UDS-CAN AIRBAG
        case 91: return {false, 0, 0x776, 0x786}; // SUZUKI CAN RBS
        case 92: return {false, 0, 0x798, 0x7A8}; // SUZUKI CAN TPMS
        case 95: case 98: case 100: case 103: case 107: case 183: return {true, 0x10, 0, 0}; // SUZUKI KWP ECM (0x10)
        case 101: case 104: case 105: return {true, 0x7A, 0, 0}; // SUZUKI KWP ECM F9Q/K9K/F8Q (0x7A)
        case 109: case 115: return {true, 0x26, 0, 0}; // SUZUKI KWP IMMOBILIZER JOHNSON
        case 110: case 116: return {true, 0xC0, 0, 0}; // SUZUKI KWP IMMOBILIZER SIEMENS
        case 126: return {true, 0x33, 0, 0}; // SUZUKI OBD-II-KWP
        case 131: return {false, 0, 0x7B7, 0x7BA}; // NISSAN CAN-1 AVM
        case 132: case 164: return {false, 0, 0x75D, 0x77D}; // NISSAN CAN-1 / UDS-CAN ADAS4
        case 133: case 190: return {false, 0, 0x7E4, 0x7EC}; // SUZUKI CAN / UDS BMU
        case 134: case 170: return {false, 0, 0x707, 0x727}; // NISSAN CAN-1 / UDS-CAN FR-CAMERA
        case 135: case 178: return {false, 0, 0x74E, 0x76E}; // NISSAN CAN-1 / UDS-CAN SONAR
        case 141: case 200: return {false, 0, 0x799, 0x7A9}; // SUZUKI CAN / UDS TCU
        case 142: case 186: return {false, 0, 0x79B, 0x7AB}; // SUZUKI CAN / UDS ADM
        case 143: return {false, 0, 0x777, 0x787}; // SUZUKI CAN SCAM
        case 144: case 198: return {false, 0, 0x715, 0x725}; // SUZUKI CAN / UDS ISG
        case 145: return {false, 0, 0x79A, 0x7AA}; // SUZUKI CAN CSW
        case 146: return {false, 0, 0x750, 0x760}; // SUZUKI CAN GW
        case 149: return {false, 0, 0x7E5, 0x7ED}; // SUZUKI CAN HCU
        case 150: case 195: return {false, 0, 0x716, 0x726}; // SUZUKI CAN / UDS HVBMU
        case 151: case 153: return {false, 0, 0x7E3, 0x7EB}; // SUZUKI CAN INV
        case 152: return {true, 0xD1, 0, 0}; // SUZUKI KWP AVAS
        case 154: return {false, 0, 0x778, 0x788}; // SUZUKI CAN MFL
        case 155: return {false, 0, 0x79C, 0x7AC}; // SUZUKI UDS-CAN HUD
        case 156: return {false, 0, 0x755, 0x775}; // NISSAN CAN-1 APS
        case 157: return {false, 0, 0x73D, 0x73E}; // NISSAN CAN-1 SOWL
        case 158: return {false, 0, 0x7D4, 0x7D5}; // NISSAN CAN-1 SOWR
        case 163: return {false, 0, 0x7C3, 0x7C9}; // NISSAN UDS-CAN ADAS
        case 171: return {false, 0, 0x723, 0x735}; // NISSAN UDS-CAN FR-RADAR
        case 172: return {false, 0, 0x710, 0x730}; // NISSAN UDS-CAN GW2
        case 175: return {false, 0, 0x70C, 0x700}; // NISSAN UDS-CAN IDM
        case 181: return {false, 0, 0x79F, 0x7AF}; // SUZUKI CAN AHB
        case 184: return {false, 0, 0x717, 0x727}; // SUZUKI UDS-CAN 48V-BMU
        case 185: return {false, 0, 0x770, 0x780}; // SUZUKI UDS-CAN ABS/ESP
        case 187: return {false, 0, 0x772, 0x782}; // SUZUKI UDS-CAN ALH
        case 188: return {false, 0, 0x79E, 0x7AE}; // SUZUKI UDS-CAN AVAS
        case 191: return {false, 0, 0x77A, 0x78A}; // SUZUKI UDS-CAN BSM
        case 192: return {false, 0, 0x751, 0x761}; // SUZUKI UDS-CAN CGW
        case 193: return {false, 0, 0x710, 0x720}; // SUZUKI UDS-CAN DDC
        case 197: return {false, 0, 0x795, 0x7A5}; // SUZUKI UDS-CAN IPC
        case 199: return {false, 0, 0x779, 0x789}; // SUZUKI UDS-CAN REBS
        default: return {false, 0, 0, 0}; // Unmapped/unknown base must fail closed instead of falling back to 0x7E0/0x7E8
    }
}

bool DecodeDbNumeric(const DbLiveSpec& spec, const std::vector<unsigned char>& payload, double& value) {
    if (spec.offset < 0 || spec.width < 1 || spec.width > 4 || static_cast<size_t>(spec.offset + spec.width) > payload.size()) return false;
    unsigned long raw = 0;
    for (int i = 0; i < spec.width; ++i) raw = (raw << 8) | payload[spec.offset + i];
    raw &= spec.mask;
    int matches = 0;
    for (int i = 0; i < spec.conversionCount; ++i) {
        const auto& rule = spec.conversions[i];
        if (raw >= rule.minimum && raw <= rule.maximum) {
            value = raw * rule.multiplier + rule.constant;
            ++matches;
        }
    }
    return matches == 1 && std::isfinite(value);
}

std::string ReadDbLivePage(const IdentityMatch& identity, int page) {
    const DbIdentityRecord* candidate = identity.activeCandidate();
    if (!candidate) return "{\"ok\":false,\"error\":\"Exact ECU identity required\",\"values\":[]}";
    struct CombinedItem {
        const DbLiveSpec* live = nullptr;
        const DbComputedSpec* comp = nullptr;
    };
    std::vector<CombinedItem> items;
    for (const auto& spec : kDbLiveSpecs) {
        for (const auto& link : kDbLiveLinks) {
            if (link.variantId == candidate->variantId && link.paramId == spec.id) { items.push_back({&spec, nullptr}); break; }
        }
    }
    for (const auto& cspec : kDbComputedSpecs) {
        std::string sname = cspec.serviceName ? cspec.serviceName : "";
        if (sname == "READ_FREEZE_FRAME_DATA") continue;
        for (const auto& clink : kDbComputedLinks) {
            if (clink.variantId == candidate->variantId && clink.paramId == cspec.id) { items.push_back({nullptr, &cspec}); break; }
        }
    }
    size_t begin = static_cast<size_t>(page) * 16;
    if (begin >= items.size()) return "{\"ok\":false,\"error\":\"No readable parameters on page\",\"values\":[]}";
    size_t end = (std::min)(begin + 16, items.size());
    DbBaseComm comm = GetDbBaseComm(candidate->baseId);
    if (!comm.isKline && comm.canTx == 0) {
        return "{\"ok\":false,\"error\":\"Unmapped ECU Base communication address\",\"values\":[]}";
    }
    bool isUds = IsUdsBase(candidate->baseId);
    std::map<std::string,std::vector<unsigned char>> supportResponses;
    std::vector<DbSupport> support(end-begin,DbSupport::Unknown);
    for(size_t i=begin;i<end;++i){
        int pid = items[i].live ? items[i].live->id : items[i].comp->id;
        std::vector<const DbSupportRule*> rules;
        for(const auto& rule:kDbSupportRules)if(rule.variantId==candidate->variantId&&rule.paramId==pid)rules.push_back(&rule);
        support[i-begin]=EvaluateSupportRules(rules,[&](const DbSupportRule& rule){
            std::string key=rule.tx;
            if(!supportResponses.count(key)){
                std::vector<unsigned char> request;
                if(!ParseSupportHex(key,request)||request.size()>5||(request[0]!=0x21&&request[0]!=0x22&&request[0]!=0x1A))return DbSupport::Unknown;
                std::vector<std::vector<unsigned char>> replies;
                if(comm.isKline)g_diag.RunKLineSession(comm.klineTarget,{request},replies,350);
                else SendBatch(g_diag,comm.canTx,comm.canRx,{request},replies,350);
                supportResponses[key]=replies.empty()?std::vector<unsigned char>{}:replies[0];
            }
            return MatchSupportMask(rule,supportResponses[key]);
        });
    }
    std::vector<int> lids;
    std::vector<std::string> compLids;
    std::vector<std::vector<unsigned char>> requests;
    size_t reqOffset = 0;
    if (candidate->baseId == 93) {
        requests.push_back({0x10, 0xC0});
        reqOffset = 1;
    }
    for (size_t i = begin; i < end; ++i) {
        if(support[i-begin]!=DbSupport::Supported)continue;
        if (items[i].live) {
            int lid = items[i].live->lid;
            if (std::find(lids.begin(), lids.end(), lid) != lids.end()) continue;
            lids.push_back(lid);
            if (isUds) requests.push_back({0x22, static_cast<unsigned char>(lid >> 8), static_cast<unsigned char>(lid)});
            else requests.push_back({0x21, static_cast<unsigned char>(lid)});
        } else if (items[i].comp) {
            for (const auto& f : kDbComputedFields) {
                if (f.paramId != items[i].comp->id) continue;
                std::string lidStr = f.lid;
                if (std::find(compLids.begin(), compLids.end(), lidStr) != compLids.end()) continue;
                std::vector<unsigned char> lidBytes;
                if (!ParseSupportHex(lidStr, lidBytes) || lidBytes.empty()) continue;
                compLids.push_back(lidStr);
                if (lidBytes.size() == 1) {
                    requests.push_back({0x21, lidBytes[0]});
                } else if (lidBytes.size() == 2) {
                    requests.push_back({0x22, lidBytes[0], lidBytes[1]});
                } else if (lidBytes.size() == 5) {
                    std::vector<unsigned char> r = {0x23};
                    r.insert(r.end(), lidBytes.begin(), lidBytes.end());
                    requests.push_back(r);
                }
            }
        }
    }
    std::vector<std::vector<unsigned char>> responses;
    if (!requests.empty() && comm.isKline) g_diag.RunKLineSession(comm.klineTarget, requests, responses, 350);
    else if(!requests.empty()) SendBatch(g_diag, comm.canTx, comm.canRx, requests, responses, 350);

    std::map<std::string, std::vector<unsigned char>> compPayloads;
    size_t compBaseIdx = reqOffset + lids.size();
    for (size_t c = 0; c < compLids.size(); ++c) {
        size_t rIdx = compBaseIdx + c;
        if (rIdx < responses.size()) {
            const auto& response = responses[rIdx];
            const auto& request = requests[rIdx];
            if (!response.empty() && response[0] == static_cast<unsigned char>(request[0] + 0x40)) {
                size_t hdrLen = (request[0] == 0x22 && response.size() >= 3) ? 3 : ((request[0] == 0x21 && response.size() >= 2) ? 2 : 1);
                if (response.size() > hdrLen) {
                    compPayloads[compLids[c]] = std::vector<unsigned char>(response.begin() + hdrLen, response.end());
                }
            }
        }
    }

    std::ostringstream out;
    out << "{\"ok\":true,\"variantId\":" << candidate->variantId << ",\"total\":" << items.size()
        << ",\"page\":" << page << ",\"vehicleValidated\":" << (identity.exact() ? "true" : "false") << ",\"values\":[";
    for (size_t i = begin; i < end; ++i) {
        if (i > begin) out << ',';
        bool valid = false;
        double value = 0;
        if (items[i].live) {
            const auto& spec = *items[i].live;
            size_t index = reqOffset + (std::find(lids.begin(), lids.end(), spec.lid) - lids.begin());
            std::vector<unsigned char> payload;
            if (support[i-begin]==DbSupport::Supported && index < responses.size()) {
                const auto& response = responses[index];
                const auto& request = requests[index];
                if (response.size() >= request.size() && response[0] == request[0] + 0x40 && std::equal(request.begin() + 1, request.end(), response.begin() + 1)) {
                    payload.assign(response.begin() + request.size(), response.end());
                    valid = DecodeDbNumeric(spec, payload, value);
                }
            }
            out << "{\"id\":" << spec.id << ",\"labelKey\":" << JsonString(spec.label) << ",\"unit\":" << JsonString(spec.unit)
                << ",\"value\":" << (valid ? JsonNumber(value, 6) : "null")
                << ",\"dataType\":" << JsonString(spec.dataType ? spec.dataType : "double")
                << ",\"outputs\":" << (spec.outputs ? spec.outputs : "[]")
                << ",\"status\":" << JsonString(support[i-begin]==DbSupport::Unknown?"support_unknown":support[i-begin]==DbSupport::Unsupported?"not_supported":valid ? "decoded_unvalidated" : "unread_or_unsupported") << '}';
        } else if (items[i].comp) {
            const auto& cspec = *items[i].comp;
            if (support[i-begin] == DbSupport::Supported) {
                valid = DecodeDbComputed(cspec.id, compPayloads, value);
            }
            out << "{\"id\":" << cspec.id << ",\"labelKey\":" << JsonString(cspec.label) << ",\"unit\":" << JsonString(cspec.unit)
                << ",\"value\":" << (valid ? JsonNumber(value, 6) : "null")
                << ",\"dataType\":\"computed\",\"outputs\":[]"
                << ",\"status\":" << JsonString(support[i-begin]==DbSupport::Unknown?"support_unknown":support[i-begin]==DbSupport::Unsupported?"not_supported":valid ? "decoded_unvalidated" : "unread_or_unsupported") << '}';
        }
    }
    out << "]}";
    return out.str();
}
