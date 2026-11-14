#pragma once

bool ParseCapturedPayloads(const std::string& text,std::map<std::string,std::vector<unsigned char>>& payloads){
    payloads.clear();
    std::istringstream lines(text);std::string line;size_t count=0;
    while(std::getline(lines,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty())continue;
        if(++count>32)return false;
        size_t split=line.find('=');if(split==std::string::npos||split==0||split>32)return false;
        std::string lid=line.substr(0,split),hex=line.substr(split+1);
        std::vector<unsigned char> data;
        if(payloads.count(lid)||!ParseSupportHex(hex,data)||data.size()>4096)return false;
        payloads.emplace(lid,std::move(data));
    }
    return !payloads.empty();
}

std::string DecodeCapturedUdsDtc(const std::vector<unsigned char>& response){
    if(response.size()<3||response[0]!=0x59||response[1]!=0x02||(response.size()-3)%4)return "{\"ok\":false,\"error\":\"Expected complete 59 02 DTC/status records\"}";
    std::ostringstream out;out<<"{\"ok\":true,\"offlineOnly\":true,\"vehicleVerified\":false,\"statusAvailabilityMask\":"<<static_cast<int>(response[2])<<",\"records\":[";
    for(size_t i=3;i<response.size();i+=4){
        if(i>3)out<<',';
        std::vector<unsigned char> dtc(response.begin()+i,response.begin()+i+3);
        out<<"{\"dtcHex\":"<<JsonString(HexBytes(dtc))<<",\"code\":"<<JsonString(FormatDTCCode(response[i],response[i+1]))<<",\"status\":"<<static_cast<int>(response[i+3])<<'}';
    }
    out<<"]}";return out.str();
}

std::string DecodeCapturedFreezeFrame(const std::vector<unsigned char>& response) {
    if (response.size() >= 6 && response[0] == 0x59 && response[1] == 0x04) {
        std::string code = FormatDTCCode(response[2], response[3]);
        unsigned char dtcSub = response[4];
        unsigned char dtcStatus = response[5];
        std::ostringstream out;
        out << "{\"ok\":true,\"protocol\":\"UDS_19_04\",\"code\":" << JsonString(code)
            << ",\"dtcLowByte\":" << static_cast<int>(dtcSub)
            << ",\"status\":" << static_cast<int>(dtcStatus)
            << ",\"rawData\":" << JsonString(HexBytes(response, 6))
            << ",\"records\":[";
        size_t pos = 6;
        bool firstRec = true;
        if (pos + 1 < response.size()) {
            unsigned char recNum = response[pos];
            unsigned char numDids = response[pos + 1];
            out << "{\"recordNumber\":" << static_cast<int>(recNum)
                << ",\"declaredDidCount\":" << static_cast<int>(numDids)
                << ",\"payloadHex\":" << JsonString(HexBytes(response, pos + 2)) << "}";
            firstRec = false;
        }
        out << "]}";
        return out.str();
    }
    if (response.size() >= 3 && response[0] == 0x52) {
        unsigned char frameNo = response[1];
        unsigned char recId = response[2];
        std::ostringstream out;
        out << "{\"ok\":true,\"protocol\":\"KWP_12_52\",\"frameNumber\":" << static_cast<int>(frameNo)
            << ",\"recordId\":" << static_cast<int>(recId)
            << ",\"payloadHex\":" << JsonString(HexBytes(response, 3)) << "}";
        return out.str();
    }
    if (response.size() >= 2 && response[0] == 0x61 &&
        (response[1] == 0x87 || response[1] == 0x82 || response[1] == 0xC1)) {
        std::ostringstream out;
        out << "{\"ok\":true,\"protocol\":\"KWP_21_" << HexAddr(response[1], 2).substr(2)
            << "\",\"recordId\":" << static_cast<int>(response[1])
            << ",\"payloadHex\":" << JsonString(HexBytes(response, 2)) << "}";
        return out.str();
    }
    return "{\"ok\":false,\"error\":\"Expected 59 04 (UDS FreezeFrame), 52 (KWP ReadFreezeFrameData), or 61 87/82/C1 (KWP Snapshot)\"}";
}
