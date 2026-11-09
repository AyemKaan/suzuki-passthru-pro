#pragma once
#include "suzuki_computed.generated.h"

bool EvaluateDbExpression(const std::string& expression, const std::map<std::string,double>& variables, double& result) {
    std::istringstream input(expression);
    std::vector<double> stack;
    std::string token;
    size_t tokens=0;
    while(input>>token){
        if(++tokens>256)return false;
        if(token=="+"||token=="-"||token=="*"||token=="/"){
            if(stack.size()<2)return false;
            double right=stack.back();stack.pop_back();double left=stack.back();stack.pop_back();
            if(token=="/"&&right==0)return false;
            double value=token=="+"?left+right:token=="-"?left-right:token=="*"?left*right:left/right;
            if(!std::isfinite(value))return false;
            stack.push_back(value);
        }else if(!token.empty()&&token[0]=='$'){
            auto found=variables.find(token.substr(1));
            if(found==variables.end()||!std::isfinite(found->second))return false;
            stack.push_back(found->second);
        }else{
            try{size_t consumed=0;double value=std::stod(token,&consumed);if(consumed!=token.size()||!std::isfinite(value))return false;stack.push_back(value);}catch(...){return false;}
        }
    }
    if(stack.size()!=1)return false;
    result=stack.back();return true;
}
bool DecodeDbComputed(int paramId, const std::map<std::string,std::vector<unsigned char>>& payloads, double& result) {
    const DbComputedSpec* spec=nullptr;
    for(const auto& item:kDbComputedSpecs)if(item.id==paramId)spec=&item;
    if(!spec)return false;
    std::map<std::string,double> variables;
    for(const auto& field:kDbComputedFields){
        if(field.paramId!=paramId)continue;
        auto payload=payloads.find(field.lid);
        if(payload==payloads.end()||field.offset<0||field.width<1||field.width>4||static_cast<size_t>(field.offset+field.width)>payload->second.size())return false;
        unsigned long value=0;
        for(int i=0;i<field.width;++i)value=(value<<8)|payload->second[field.offset+i];
        double decoded=value&field.mask;
        auto existing=variables.find(field.variable);
        if(existing!=variables.end()&&existing->second!=decoded)return false;
        variables[field.variable]=decoded;
    }
    return EvaluateDbExpression(spec->expression,variables,result);
}
