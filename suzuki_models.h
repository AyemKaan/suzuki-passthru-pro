#pragma once
#include "suzuki_models.generated.h"

std::string MatchDbModelsJson(int baseId, const std::string& cid, const std::string& partNumber) {
    std::vector<const DbModelRecord*> matches;
    int best = -1;
    if (!cid.empty() || !partNumber.empty()) {
        for (const auto& model : kDbModels) {
            if (model.baseId != baseId) continue;
            std::string keyCid = model.cid, keyPart = model.partNumber;
            if ((!keyCid.empty() && keyCid != cid) || (!keyPart.empty() && keyPart != partNumber)) continue;
            int rank = (keyCid.empty() ? 0 : 2) + (keyPart.empty() ? 0 : 1);
            if (rank < best) continue;
            if (rank > best) { matches.clear(); best = rank; }
            matches.push_back(&model);
        }
    }
    std::ostringstream out;
    out << "{\"vehicleVerified\":false,\"genericFallback\":" << (best <= 0 ? "true" : "false") << ",\"models\":[";
    bool first = true;
    for (const auto* model : matches) {
        if (!first) out << ',';
        first = false;
        out << model->detail;
    }
    out << "]}";
    return out.str();
}
