#pragma once
#include "suzuki_dtc.generated.h"

inline bool LookupDbDtcText(int variantId, const std::string& code, std::string& trOut, std::string& enOut) {
    trOut.clear();
    enOut.clear();
    if (variantId > 0) {
        for (const auto& definition : kDbDtcDefinitions) {
            if (code != definition.code) continue;
            for (const auto& link : kDbDtcLinks) {
                if (link.variantId == variantId && link.dtcId == definition.id) {
                    trOut = definition.tr;
                    enOut = definition.en;
                    return true;
                }
            }
        }
    }
    for (const auto& definition : kDbDtcDefinitions) {
        if (code == definition.code && definition.tr[0] != '\0') {
            trOut = definition.tr;
            enOut = definition.en;
            return true;
        }
    }
    return false;
}

std::string DbDtcDescriptionJson(int variantId, const std::string& code) {
    std::ostringstream out;
    out << "{\"variantId\":" << variantId << ",\"code\":" << JsonString(code)
        << ",\"referenceOnly\":true,\"descriptions\":[";
    bool first=true;
    for(const auto& definition:kDbDtcDefinitions){
        if(code!=definition.code)continue;
        bool linked=(variantId==0);
        if(!linked){
            for(const auto& link:kDbDtcLinks)if(link.variantId==variantId&&link.dtcId==definition.id){linked=true;break;}
        }
        if(!linked)continue;
        if(!first)out<<',';first=false;
        out<<"{\"id\":"<<definition.id<<",\"tr\":"<<JsonString(definition.tr)<<",\"en\":"<<JsonString(definition.en)<<'}';
    }
    out<<"]}";
    return out.str();
}
