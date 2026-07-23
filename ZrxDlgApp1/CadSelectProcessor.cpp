#include "pch.h"
#include "CadSelectProcessor.h"
#include "Common.h"
#include "AgentTableSum.h"
#include <aced.h>
#include <adslib.h>
#include <dbents.h>
#include <zdbapserv.h>
#include <acdocman.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

namespace NS_CadSelect
{
    void CadSelectProcessor::SelectionTaskFunc(void* pData)
    {
        SelectionTaskData* pTask = static_cast<SelectionTaskData*>(pData);
        if (!pTask || !pTask->pPromise) return;

        SelectResult result;
        result.success = false;

        acutPrintf(L"\n[AI Convert] Please pick the first corner of the table/titleblock area: ");
        ads_point pt1, pt2;
        if (acedGetPoint(NULL, L"\nSelect first corner: ", pt1) != RTNORM)
        {
            result.errorMsg = "Selection cancelled by user (first corner)";
            pTask->pPromise->set_value(result);
            return;
        }

        if (acedGetCorner(pt1, L"\nSelect opposite corner: ", pt2) != RTNORM)
        {
            result.errorMsg = "Selection cancelled by user (opposite corner)";
            pTask->pPromise->set_value(result);
            return;
        }

        // Clear CLI prompt leftover and redraw viewport
        acutPrintf(L"\n[AI Convert] Selection completed successfully.\n");
        acedRedraw(NULL, 0);

        // Calculate real BBox
        result.bbox.minX = (pt1[X] < pt2[X]) ? pt1[X] : pt2[X];
        result.bbox.minY = (pt1[Y] < pt2[Y]) ? pt1[Y] : pt2[Y];
        result.bbox.maxX = (pt1[X] > pt2[X]) ? pt1[X] : pt2[X];
        result.bbox.maxY = (pt1[Y] > pt2[Y]) ? pt1[Y] : pt2[Y];

        // Select all entities in the window area & collect real handles
        ads_name ss;
        ZcDbObjectIdArray idArray;
        int res = acedSSGet(L"C", pt1, pt2, NULL, ss);
        if (res == RTNORM)
        {
            ZSoft::Int32 len = 0;
            acedSSLength(ss, &len);
            for (long i = 0; i < len; i++)
            {
                ads_name ent;
                ZcDbObjectId objId;
                acedSSName(ss, i, ent);
                if (acdbGetObjectId(objId, ent) == Zcad::eOk && !objId.isNull())
                {
                    idArray.append(objId);
                    ZcDbHandle h = objId.handle();
                    if (!h.isNull())
                    {
                        wchar_t buf[64] = { 0 };
                        h.getIntoAsciiBuffer(buf);
                        std::string hStr = wstring2string(std::wstring(buf));
                        if (!hStr.empty())
                        {
                            result.selectedHandles.push_back(hStr);
                        }
                    }
                }
            }
            acedSSFree(ss);
        }

        // Real Entity Extraction
        AcString allContent;
        std::vector<NS_TableSum::TextBox> textBoxVec;
        std::vector<NS_TableSum::TableData> tableBoxVec;
        ZcDbObjectIdArray textIdArray;
        NS_TableSum::GetTextEntitiesByUser(allContent, textBoxVec, tableBoxVec, textIdArray);

        ZcString zcTmpPath = zcdbHostApplicationServices()->getTempPath() + L"aiconvert\\";
        CreateSingleDirectory(zcTmpPath);
        std::wstring timestampStr = string2wstring(NS_TableSum::GenerateTimestampFilename());
        std::wstring jsonFileName = L"select_extract_" + timestampStr + L".json";
        std::wstring outFile = zcTmpPath.kwszPtr() + jsonFileName;

        ZcDbDatabase* pCurDb = zcdbHostApplicationServices()->workingDatabase();
        const ACHAR* pDwgPathName = nullptr;
        std::string dwgNameStr = "";
        if (pCurDb && pCurDb->getFilename(pDwgPathName) == Zcad::eOk && pDwgPathName)
        {
            dwgNameStr = wstring2string(pDwgPathName);
        }

        nlohmann::json jsonVal;
        jsonVal["dwgname"] = dwgNameStr;
        nlohmann::json textsArr = nlohmann::json::array();
        for (const auto& tb : textBoxVec)
        {
            nlohmann::json tbJson;
            tbJson["content"] = wstring2string(tb.content.c_str());
            if (tb.bBoxValid)
            {
                tbJson["minPt"] = { tb.textBox.minPoint().x, tb.textBox.minPoint().y };
                tbJson["maxPt"] = { tb.textBox.maxPoint().x, tb.textBox.maxPoint().y };
            }
            else
            {
                tbJson["minPt"] = { 0.0, 0.0 };
                tbJson["maxPt"] = { 0.0, 0.0 };
            }
            textsArr.push_back(tbJson);
        }
        jsonVal["texts"] = textsArr;

        std::string jsonStr = jsonVal.dump(4);
        std::ofstream ofs(outFile, std::ios::binary);
        if (ofs.is_open())
        {
            ofs.write(jsonStr.c_str(), jsonStr.length());
            ofs.close();
        }

        // Run Real Dify Workflow with 3-times retry mechanism (No Local Fallback)
        std::string difyError;
        std::string difyApiKey = (pTask->convertMode == 1) ? "app-DnkpWQxiXmg2lZt2mQ8rnI5u" : "app-0B7nJIc5Jd1lblBjfADmRvkM";
        std::wstring difyOutDir = (pTask->convertMode == 1) ? L"C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\dify_results\\" : L"C:\\Users\\zwsoft\\Desktop\\transform\\testdata\\dify_results\\";

        bool difyOk = false;
        for (int retry = 1; retry <= 3; ++retry)
        {
            acutPrintf(L"\n[AI Convert] Calling Dify server (attempt %d/3)...", retry);
            difyOk = NS_TableSum::RunDifyWorkflowForString(outFile, jsonFileName, difyError, difyApiKey, difyOutDir);
            if (difyOk) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (!difyOk)
        {
            result.success = false;
            result.errorMsg = "Dify network request failed after 3 retries: " + difyError;
            pTask->pPromise->set_value(result);
            return;
        }

        std::wstring resultJsonPath = difyOutDir + jsonFileName;
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
                result.extractedFields = nlohmann::json::parse(resultJsonStr);
                result.success = true;
            } catch (std::exception& e) {
                result.success = false;
                result.errorMsg = std::string("Failed to parse Dify JSON response: ") + e.what();
            }
        }
        else
        {
            result.success = false;
            result.errorMsg = "Dify returned empty response file";
        }

        pTask->pPromise->set_value(result);
    }

    SelectResult CadSelectProcessor::ExecuteRealSelection(int convertMode)
    {
        std::promise<SelectResult> selPromise;
        std::future<SelectResult> selFuture = selPromise.get_future();

        SelectionTaskData taskData;
        taskData.convertMode = convertMode;
        taskData.pPromise = &selPromise;

        // Execute in Application Context (CAD Main Thread) to avoid UI deadlock
        acDocManager->executeInApplicationContext(SelectionTaskFunc, &taskData);

        // Wait for main thread UI interaction to complete (timeout: 120s)
        std::future_status status = selFuture.wait_for(std::chrono::seconds(120));
        if (status == std::future_status::ready)
        {
            return selFuture.get();
        }
        else
        {
            SelectResult errRes;
            errRes.success = false;
            errRes.errorMsg = "Selection timed out waiting for CAD UI user input";
            return errRes;
        }
    }
}