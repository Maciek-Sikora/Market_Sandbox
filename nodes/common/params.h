#pragma once
#include <string>
#include <unordered_map>

using ParamMap = std::unordered_map<std::string, std::string>;

inline std::string getParam(const ParamMap& params, const std::string& key, const std::string& def) {
    auto it = params.find(key);
    return it != params.end() ? it->second : def;
}

inline double getParamD(const ParamMap& params, const std::string& key, double def) {
    auto it = params.find(key);
    return it != params.end() ? std::stod(it->second) : def;
}

inline int64_t getParamI(const ParamMap& params, const std::string& key, int64_t def) {
    auto it = params.find(key);
    return it != params.end() ? std::stoll(it->second) : def;
}

inline bool getParamB(const ParamMap& params, const std::string& key, bool def) {
    auto it = params.find(key);
    if (it == params.end()) return def;
    return it->second == "true" || it->second == "1";
}
