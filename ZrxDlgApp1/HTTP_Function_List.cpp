#include "pch.h"
#include <locale>
#include <codecvt>
#include <fstream>
#include <sstream>
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
#include "CadTableWriter.h"

extern void AiBomConvertCmd();
extern void AiTableRecognizeCmd();

static commandstatus parseCommandState(const nlohmann::json& jsonResult)
{
    if (jsonResult.contains("state") && jsonResult["state"].is_string())
    {
        std::string state = jsonResult["state"].get<std::string>();
        if (state == "failed") {
            return CmdStatus_Fail;
        }
        else if (state == "succeed") {
            return CmdStatus_Success;
        }
    }
    return CmdStatus_Success;
}

// 1. Tool action for BOM conversion
static nlohmann::json toolSelectBom(const nlohmann::json& args)
{
    nlohmann::json jsonOutput;
    acutPrintf(_T("\n[HTTP Gateway] Executing BOM conversion on CAD main thread..."));
    AiBomConvertCmd();

    ZcDbDatabase* pDb = zcdbHostApplicationServices()->workingDatabase();
    ZcString dwgPath;
    if (pDb)
    {
        const ZTCHAR* pDwgName = nullptr;
        pDb->getFilename(pDwgName);
        dwgPath = pDwgName ? pDwgName : _T("");
    }
    ZcString shortFileName = GetShortFileName(dwgPath);
    if (shortFileName.isEmpty()) shortFileName = _T("AiBomConvert");

    std::wstring resultJsonPath = L"C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\dify_results\\" + std::wstring(shortFileName.kwszPtr()) + L".json";
    std::ifstream ifs(resultJsonPath, std::ios::binary);
    std::string resultJsonStr = "";
    if (ifs.is_open())
    {
        std::stringstream ssBuf;
        ssBuf << ifs.rdbuf();
        resultJsonStr = ssBuf.str();
        ifs.close();
    }

    if (!resultJsonStr.empty())
    {
        try {
            jsonOutput["state"] = "succeed";
            jsonOutput["output"] = nlohmann::json::parse(resultJsonStr);
        } catch(...) {
            jsonOutput["state"] = "succeed";
            jsonOutput["output"] = resultJsonStr;
        }
    }
    else
    {
        jsonOutput["state"] = "failed";
        jsonOutput["output"] = "BOM result file not found";
    }

    return jsonOutput;
}

// 2. Tool action for TitleBlock recognition
static nlohmann::json toolSelectTitleblock(const nlohmann::json& args)
{
    nlohmann::json jsonOutput;
    acutPrintf(_T("\n[HTTP Gateway] Executing TitleBlock recognition on CAD main thread..."));
    AiTableRecognizeCmd();

    ZcDbDatabase* pDb = zcdbHostApplicationServices()->workingDatabase();
    ZcString dwgPath;
    if (pDb)
    {
        const ZTCHAR* pDwgName = nullptr;
        pDb->getFilename(pDwgName);
        dwgPath = pDwgName ? pDwgName : _T("");
    }
    ZcString shortFileName = GetShortFileName(dwgPath);
    if (shortFileName.isEmpty()) shortFileName = _T("AiTableRecognize");

    std::wstring resultJsonPath = L"C:\\Users\\zwsoft\\Desktop\\transform\\testdata\\dify_results\\" + std::wstring(shortFileName.kwszPtr()) + L".json";
    std::ifstream ifs(resultJsonPath, std::ios::binary);
    std::string resultJsonStr = "";
    if (ifs.is_open())
    {
        std::stringstream ssBuf;
        ssBuf << ifs.rdbuf();
        resultJsonStr = ssBuf.str();
        ifs.close();
    }

    if (!resultJsonStr.empty())
    {
        try {
            jsonOutput["state"] = "succeed";
            jsonOutput["output"] = nlohmann::json::parse(resultJsonStr);
        } catch(...) {
            jsonOutput["state"] = "succeed";
            jsonOutput["output"] = resultJsonStr;
        }
    }
    else
    {
        jsonOutput["state"] = "failed";
        jsonOutput["output"] = "TitleBlock result file not found";
    }

    return jsonOutput;
}

// 3. Tool action for Table Writeback
static nlohmann::json toolWritebackTable(const nlohmann::json& args)
{
    nlohmann::json jsonOutput;
    try
    {
        int convertMode = args.value("convert_mode", 0);
        int styleType = args.value("style_type", 1);

        NS_CadTable::BBox2D bbox;
        if (args.contains("bbox")) {
            bbox.minX = args["bbox"].value("min_x", 0.0);
            bbox.minY = args["bbox"].value("min_y", 0.0);
            bbox.maxX = args["bbox"].value("max_x", 0.0);
            bbox.maxY = args["bbox"].value("max_y", 0.0);
        }

        nlohmann::json fieldsData;
        if (args.contains("fields_data")) fieldsData = args["fields_data"];

        std::vector<std::string> eraseHandles;
        if (args.contains("selected_handles") && args["selected_handles"].is_array()) {
            for (const auto& item : args["selected_handles"]) {
                if (item.is_string()) eraseHandles.push_back(item.get<std::string>());
            }
        }

        std::string outError = "";
        bool ok = NS_CadTable::CadTableWriter::WriteNativeTable(convertMode, styleType, bbox, fieldsData, eraseHandles, outError);
        if (ok)
        {
            jsonOutput["state"] = "succeed";
            jsonOutput["output"] = "Native ZcDbTable written successfully";
        }
        else
        {
            jsonOutput["state"] = "failed";
            jsonOutput["output"] = "Writeback error: " + outError;
        }
    }
    catch (std::exception& e)
    {
        jsonOutput["state"] = "failed";
        jsonOutput["output"] = std::string("Writeback exception: ") + e.what();
    }
    return jsonOutput;
}

void InitHttpFunc()
{
    GlobalFuncTable2::getInstance().registerFunc("select_bom", toolSelectBom);
    GlobalFuncTable2::getInstance().registerFunc("select_titleblock", toolSelectTitleblock);
    GlobalFuncTable2::getInstance().registerFunc("writeback_table", toolWritebackTable);
}

void HttpToolCmd()
{
    HTTP_IO_Data::Instance().setState(CmdStatus_OnCommand);
    nlohmann::json jsonInput = HTTP_IO_Data::Instance().getInput();
    if (!jsonInput.contains("action") || !jsonInput.contains("arguments"))
    {
        nlohmann::json errOut = { {"state", "failed"}, {"error", "Invalid HTTP_TOOL input payload"} };
        HTTP_IO_Data::Instance().saveResult(errOut);
        HTTP_IO_Data::Instance().setState(CmdStatus_Fail);
        return;
    }

    std::string tool_name = jsonInput["action"];
    nlohmann::json arguments = jsonInput["arguments"];

    acutPrintf(_T("\n[HTTP Gateway] Executing tool_name: %s"), AcString(tool_name.c_str()).kwszPtr());

    if (!GlobalFuncTable2::getInstance().hasFunc(tool_name))
    {
        nlohmann::json jsonOutput = { {"state", "failed"}, {"output", {{"type", "text"}, {"text", "Tool not registered: " + tool_name}}} };
        HTTP_IO_Data::Instance().saveResult(jsonOutput);
        HTTP_IO_Data::Instance().setState(CmdStatus_Fail);
        return;
    }

    nlohmann::json jsonOutput = CALL_HTTP_TOOL_FUNC(tool_name, arguments);

    HTTP_IO_Data::Instance().saveResult(jsonOutput);
    commandstatus es = parseCommandState(jsonOutput);
    HTTP_IO_Data::Instance().setState(es);
}
