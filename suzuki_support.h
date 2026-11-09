#pragma once
#include <functional>

enum class DbSupport { Unknown, Unsupported, Supported };

bool ParseSupportHex(const std::string& text, std::vector<unsigned char>& bytes) {
    bytes.clear();
    if (text.empty() || text.size() % 2) return false;
    auto digit=[](char c)->int { if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1; };
    for(size_t i=0;i<text.size();i+=2){int a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0){bytes.clear();return false;}bytes.push_back(static_cast<unsigned char>(a*16+b));}
    return true;
}
DbSupport MatchSupportMask(const DbSupportRule& rule, const std::vector<unsigned char>& response) {
    std::vector<unsigned char> mask,pattern,request;
    if(!ParseSupportHex(rule.tx,request)||!ParseSupportHex(rule.mask,mask)||!ParseSupportHex(rule.pattern,pattern)||mask.size()!=pattern.size())return DbSupport::Unknown;
    if(response.empty()||response[0]==0x7F||response[0]!=static_cast<unsigned char>(request[0]+0x40)||response.size()<mask.size())return DbSupport::Unknown;
    for(size_t i=0;i<mask.size();++i)if((response[i]&mask[i])!=(pattern[i]&mask[i]))return DbSupport::Unsupported;
    return DbSupport::Supported;
}
DbSupport EvaluateSupportRules(const std::vector<const DbSupportRule*>& rules,
                              const std::function<DbSupport(const DbSupportRule&)>& evaluate) {
    bool hasAdd=false,addMatch=false,addUnknown=false,deleteUnknown=false;
    for(const auto* rule:rules){
        std::string action=rule->action;
        if(action!="ADD"&&action!="DELETE")return DbSupport::Unknown;
        DbSupport result=evaluate(*rule);
        if(action=="ADD"){hasAdd=true;addMatch|=result==DbSupport::Supported;addUnknown|=result==DbSupport::Unknown;}
        else {if(result==DbSupport::Supported)return DbSupport::Unsupported;deleteUnknown|=result==DbSupport::Unknown;}
    }
    if(hasAdd&&!addMatch){if(!addUnknown)return DbSupport::Unsupported;return DbSupport::Unknown;}
    return deleteUnknown?DbSupport::Unknown:DbSupport::Supported;
}
