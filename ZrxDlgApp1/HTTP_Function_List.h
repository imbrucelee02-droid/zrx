#pragma once
#include <unordered_map>
#include <functional>
#include <string>
#include "nlohmann/json.hpp"

typedef std::function<nlohmann::json (const nlohmann::json&)> commandFunc2;

class GlobalFuncTable2 
{
public:
    static GlobalFuncTable2& getInstance() {
        static GlobalFuncTable2 instance;
        return instance;
    }

    void registerFunc(const std::string& name, commandFunc2 func) 
    {
        table_[name] = std::move(func);
    }

    nlohmann::json callFunc(const std::string& name, const nlohmann::json& args)
    {
        auto it = table_.find(name);
        if (it != table_.end()) 
        {
            return it->second(args);
        }
        nlohmann::json jsonOutput;
        jsonOutput = { {"state", "failed"}, {"output", {{"type", "text"}, {"text", "Failed to find the tool."}}} };
        return jsonOutput;
    }

    bool hasFunc(const std::string& name)
    {
        return table_.find(name) != table_.end();
    }

    GlobalFuncTable2(const GlobalFuncTable2&) = delete;
    GlobalFuncTable2& operator=(const GlobalFuncTable2&) = delete;

private:
    GlobalFuncTable2() = default;
    std::unordered_map<std::string, commandFunc2> table_;
};

#define CALL_HTTP_TOOL_FUNC(name, args) GlobalFuncTable2::getInstance().callFunc(name, args)

void HttpToolCmd();
void InitHttpFunc();
