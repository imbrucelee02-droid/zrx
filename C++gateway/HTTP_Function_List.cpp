#include "pch.h"
#include <locale>
#include <codecvt>
#include "nlohmann/json.hpp"
#include "dbents.h"
#include "zaced.h"
#include "dbsymtb.h"
#include "zdbapserv.h"
#include "zactrans.h"
#include "dbmtext.h"
#include "dbtable.h"
#include "acutmem.h"

#include "HTTP_Function_List.h"
#include "HTTP_Data.h"
#include "AgentTableSum.h"
#include "Common.h"

class ZcTransaction;

static std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_str(utf8_len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], utf8_len, nullptr, nullptr);
    utf8_str.pop_back();
    return utf8_str;
}

static AcString Utf8ToAcString(const std::string& utf8Str) {
    if (utf8Str.empty()) return AcString();

    int wstrLen = MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.length()), nullptr, 0
    );

    std::wstring wstr(wstrLen, 0);
    MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.length()), &wstr[0], wstrLen
    );

    return AcString(wstr.c_str());
}

static commandstatus parseCommandState(const nlohmann::json& jsonResult)
{
    if (jsonResult.contains("state") && jsonResult["state"].is_string())
    {
        std::string state = jsonResult["state"].get<std::string>();
        if (state == "failed") {
            return commandstatus::Fail;
        }
        else if (state == "succeed") {
            return commandstatus::Success;
        }
    }

    return commandstatus::Fail;
}

//图纸解析与表格识别
nlohmann::json tableSumTableReconize(const nlohmann::json& jsonInput)
{
    return NS_TableSum::TableReconize(jsonInput);
}

//交互识别查漏补缺
nlohmann::json tableSumInteract(const nlohmann::json& jsonInput)
{
    return NS_TableSum::Interact(jsonInput);
}

//表格汇总
nlohmann::json tableSumAiTableSum(const nlohmann::json& jsonInput)
{
    return NS_TableSum::AiTableSum(jsonInput);
}

void InitHttpFunc()
{
    GlobalFuncTable2::getInstance().registerFunc("table_sum_reconize", tableSumTableReconize);
    GlobalFuncTable2::getInstance().registerFunc("table_sum_interact", tableSumInteract);
    GlobalFuncTable2::getInstance().registerFunc("table_sum_aitablesum", tableSumAiTableSum);
}

void HttpToolCmd()
{
    HTTP_IO_Data::Instance().setState(commandstatus::OnCommand);
    nlohmann::json jsonInput = HTTP_IO_Data::Instance().getInput();
    if (!jsonInput.contains("action") || !jsonInput.contains("arguments"))
    {
        HTTP_IO_Data::Instance().setState(commandstatus::Fail);
        return;
    }

    std::string tool_name = jsonInput["action"];
    nlohmann::json arguments = jsonInput["arguments"];

    acutPrintf(_T("\ncall tool_name:%s"), Utf8ToAcString(tool_name).kTCharPtr());

    if (!GlobalFuncTable2::getInstance().hasFunc(tool_name))
    {
        nlohmann::json jsonOutput;
        jsonOutput = { {"state", "failed"}, {"output", {{"type", "text"}, {"text", "Failed to find the tool."}}} };
        HTTP_IO_Data::Instance().saveResult(jsonOutput);
        HTTP_IO_Data::Instance().setState(commandstatus::Fail);
        return;
    }

    nlohmann::json jsonOutput = CALL_HTTP_TOOL_FUNC(tool_name, arguments);

    HTTP_IO_Data::Instance().saveResult(jsonOutput);
    commandstatus es = parseCommandState(jsonOutput);
    HTTP_IO_Data::Instance().setState(es);
}
