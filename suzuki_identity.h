#pragma once
#include "suzuki_identity.generated.h"
#include "suzuki_models.h"

inline bool IsUdsBase(int baseId) {
    return baseId == 90 || baseId == 147 || baseId == 155 || (baseId >= 184 && baseId <= 200);
}

inline bool IsF9QBase(int baseId) {
    return baseId == 93 || baseId == 101;
}

struct IdentityMatch {
    std::string cid;
    std::string partNumber;
    int baseId = 0;
    bool generic = true;
    int selectedVariantId = 0;
    bool exact() const { return !generic && candidates.size() == 1; }
    const DbIdentityRecord* activeCandidate() const {
        if (selectedVariantId != 0) {
            for (const auto* c : candidates) {
                if (c->variantId == selectedVariantId) return c;
            }
        }
        if (exact()) return candidates[0];
        return nullptr;
    }
    std::vector<const DbIdentityRecord*> candidates;
};

inline std::string DecodeF9QPartNumber(const std::vector<unsigned char>& r) {
    if (r.size() < 7 || r[0] != 0x61 || r[1] != 0x80) return "";
    unsigned char b1 = r[2], b2 = r[3], b3 = r[4], b4 = r[5], b5 = r[6];
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x55 && b4 == 0x22 && b5 == 0x01) return "33920-84A10";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x56 && b4 == 0x16 && b5 == 0x86) return "33920-67J00";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x51 && b4 == 0x86 && b5 == 0x48) return "33920-67J00";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x63 && b4 == 0x46 && b5 == 0x15) return "33920-67J10";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x97 && b4 == 0x11 && b5 == 0x34) return "33920-67J11";
    if (b1 == 0x82 && b2 == 0x01 && b3 == 0x12 && b4 == 0x73 && b5 == 0x11) return "33920-67J12";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x63 && b4 == 0x80 && b5 == 0x60) return "33920-67J20";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x68 && b4 == 0x71 && b5 == 0x38) return "33920-67J30";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x75 && b4 == 0x18 && b5 == 0x18) return "33920-67J40";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x97 && b4 == 0x11 && b5 == 0x43) return "33920-67J41";
    if (b1 == 0x82 && b2 == 0x01 && b3 == 0x12 && b4 == 0x73 && b5 == 0x12) return "33920-67J42";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x75 && b4 == 0x18 && b5 == 0x20) return "33920-67J50";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x75 && b4 == 0x18 && b5 == 0x22) return "33920-67J60";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x98 && b4 == 0x70 && b5 == 0x11) return "33920-80K01";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x97 && b4 == 0x11 && b5 == 0x51) return "33920-80K10";
    if (b1 == 0x82 && b2 == 0x00 && b3 == 0x97 && b4 == 0x11 && b5 == 0x55) return "33920-80K20";
    if (b1 == 0x82 && b2 == 0x01 && b3 == 0x16 && b4 == 0x04 && b5 == 0x61) return "33920-80KA0";
    if (b1 == 0x82 && b2 == 0x01 && b3 == 0x19 && b4 == 0x63 && b5 == 0x28) return "33920-80KA1";
    return "";
}

inline IdentityMatch MatchDbIdentifiers(int baseId, const std::string& cid, const std::string& partNumber) {
    IdentityMatch result;
    result.cid = cid;
    result.partNumber = partNumber;
    result.baseId = baseId;
    if (cid.empty() && partNumber.empty()) return result;
    int best = -1;
    for (const auto& record : kDbIdentities) {
        if (record.baseId != baseId) continue;
        std::string keyCid = record.cid, keyPart = record.partNumber;
        if ((!keyCid.empty() && keyCid != cid) || (!keyPart.empty() && keyPart != partNumber)) continue;
        int rank = (keyCid.empty() ? 0 : 2) + (keyPart.empty() ? 0 : 1);
        if (rank < best) continue;
        if (rank > best) { result.candidates.clear(); best = rank; }
        result.candidates.push_back(&record);
    }
    if (IsF9QBase(baseId) && !partNumber.empty() && result.candidates.size() == 1) {
        result.generic = false;
    } else {
        result.generic = best <= 0;
    }
    return result;
}

inline IdentityMatch MatchDbIdentity(int baseId, const std::vector<unsigned char>& response) {
    if (IsF9QBase(baseId)) {
        if (response.size() < 26) return IdentityMatch{};
        std::string pn = DecodeF9QPartNumber(response);
        if (pn.empty()) return IdentityMatch{};
        return MatchDbIdentifiers(baseId, "", pn);
    }
    size_t offset = IsUdsBase(baseId) ? 3 : 2;
    if (response.size() < offset + 10) return IdentityMatch{};
    bool part = false;
    if (IsUdsBase(baseId)) {
        if (response[0] != 0x62 || response[1] != 0xF1 || (response[2] != 0x01 && response[2] != 0x88)) return IdentityMatch{};
        part = response[2] == 0x88;
    } else {
        if (response[0] != 0x5A || (response[1] != 0x8E && response[1] != 0x9A && response[1] != 0x8A && response[1] != 0x91 && response[1] != 0x92 && response[1] != 0x94)) return IdentityMatch{};
        part = response[1] == 0x8A || response[1] == 0x91 || response[1] == 0x92 || response[1] == 0x94;
    }
    std::string value;
    for (size_t i = offset; i < offset + 10; ++i) {
        if (response[i] < 32 || response[i] > 126) return IdentityMatch{};
        value += static_cast<char>(response[i]);
    }
    return MatchDbIdentifiers(baseId, part ? "" : value, part ? value : "");
}

inline std::string IdentityMatchJson(const IdentityMatch& result) {
    std::ostringstream out;
    out << "{\"baseId\":" << result.baseId << ",\"cid\":" << JsonString(result.cid) << ",\"exactVariant\":"
        << (result.exact() ? "true" : "false") << ",\"genericFallback\":" << (result.generic ? "true" : "false")
        << ",\"selectedVariantId\":" << result.selectedVariantId
        << ",\"partNumber\":" << JsonString(result.partNumber) << ",\"candidates\":[";
    bool first = true;
    for (const auto* record : result.candidates) {
        if (!first) out << ',';
        first = false;
        out << "{\"baseId\":" << record->baseId << ",\"variantId\":" << record->variantId
            << ",\"name\":" << JsonString(record->name) << ",\"model\":" << JsonString(record->model)
            << ",\"attributes\":" << JsonString(record->attributes) << '}';
    }
    out << "],\"vehicle\":" << MatchDbModelsJson(result.baseId, result.cid, result.partNumber) << '}';
    return out.str();
}
