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
        acutPrintf(L"\n[AI Convert] Single box selection completed. Executing OCR & Dify...\n");
        acedRedraw(NULL, 0);

        AcGePoint2d minPt( (pt1[X] < pt2[X]) ? pt1[X] : pt2[X], (pt1[Y] < pt2[Y]) ? pt1[Y] : pt2[Y] );
        AcGePoint2d maxPt( (pt1[X] > pt2[X]) ? pt1[X] : pt2[X], (pt1[Y] > pt2[Y]) ? pt1[Y] : pt2[Y] );

        // Calculate real BBox
        result.bbox.minX = minPt.x;
        result.bbox.minY = minPt.y;
        result.bbox.maxX = maxPt.x;
        result.bbox.maxY = maxPt.y;

        // Collect entities & handles from single box selection (NO DOUBLE PROMPT)
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

        // Prepare parameters for 100% native AiBomConvertCmd / AiTableRecognizeCmd execution
        NS_TableSum::RecognizeParam param;
        std::vector<ZcGePoint2d> points = Get4PointsFromMinMaxPt(minPt, maxPt);
        if (points.size() == 4)
        {
            param.m_firstPt = ZcGePoint3d(points[0].x, points[0].y, 0.0);
            param.m_secondPt = ZcGePoint3d(points[1].x, points[1].y, 0.0);
            param.m_thirdPt = ZcGePoint3d(points[2].x, points[2].y, 0.0);
            param.m_fourthPt = ZcGePoint3d(points[3].x, points[3].y, 0.0);
        }

        ZcDbDatabase* pDb = zcdbHostApplicationServices()->workingDatabase();
        ZcString dwgPath;
        if (pDb)
        {
            const ZTCHAR* pDwgName = nullptr;
            pDb->getFilename(pDwgName);
            dwgPath = pDwgName ? pDwgName : L"";
        }
        ZcString shortFileName = GetShortFileName(dwgPath);
        if (shortFileName.isEmpty())
        {
            shortFileName = (pTask->convertMode == 1) ? L"AiBomConvert" : L"AiTableRecognize";
        }

        std::unordered_map<int, std::vector<NS_TableSum::AIConvertTableInfo>> outTableInfoVecMap;
        std::unordered_map<int, std::vector<NS_TableSum::AIConvertTextInfo>> outTextInfoVecMap;
        NS_TableSum::AIConvertType convertType = NS_TableSum::AIConvertType::tableRecOne;

        std::string ocrOutDir = (pTask->convertMode == 1) ? 
            "C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\ocr_result_json\\" : "";

        // 1. Run 100% Native OCR Recognition (GetEntitysTableResult)
        acutPrintf(L"\n[AI Convert] Running OCR entity & table recognition...");
        bool bOcrRet = NS_TableSum::GetEntitysTableResult(idArray, convertType, param,
            outTableInfoVecMap, outTextInfoVecMap, ocrOutDir, AcStringToUtf8(shortFileName));

        if (!bOcrRet)
        {
            acutPrintf(L"\n[AI Convert] OCR Table recognition failed!");
        }

        // 2. Prepare temp path & filenames for Dify Workflow
        ZcString zcTmpPath = zcdbHostApplicationServices()->getTempPath() + L"aiconvert\\";
        CreateSingleDirectory(zcTmpPath);
        std::wstring timestampStr = string2wstring(NS_TableSum::GenerateTimestampFilename());
        std::wstring jsonFileName = shortFileName.kwszPtr();
        jsonFileName += L".json";
        std::wstring outFile = zcTmpPath.kwszPtr() + jsonFileName;

        std::string difyError;
        std::string difyApiKey = (pTask->convertMode == 1) ? "app-DnkpWQxiXmg2lZt2mQ8rnI5u" : "app-0B7nJIc5Jd1lblBjfADmRvkM";
        std::wstring difyOutDir = (pTask->convertMode == 1) ? L"C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\dify_results\\" : L"C:\\Users\\zwsoft\\Desktop\\transform\\testdata\\dify_results\\";
        CreateSingleDirectory(difyOutDir.c_str());

        // 3. Run 100% Native Dify Workflow (RunDifyWorkflowForString / RunDifyWorkflowForFile)
        bool difyOk = false;
        for (int retry = 1; retry <= 3; ++retry)
        {
            acutPrintf(L"\n[AI Convert] Calling Dify workflow (attempt %d/3)...", retry);
            if (pTask->convertMode == 1)
            {
                difyOk = NS_TableSum::RunDifyWorkflowForString(outFile, jsonFileName, difyError, difyApiKey, difyOutDir);
            }
            else
            {
                difyOk = NS_TableSum::RunDifyWorkflowForFile(outFile, jsonFileName, difyError, difyApiKey, difyOutDir);
            }

            if (difyOk) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        if (!difyOk)
        {
            result.success = false;
            result.errorMsg = "Dify workflow failed after 3 retries: " + difyError;
            pTask->pPromise->set_value(result);
            return;
        }

        // 4. Read the Dify Workflow output file
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
                nlohmann::json respJson = nlohmann::json::parse(resultJsonStr);
                if (respJson.contains("data") && respJson["data"].contains("outputs"))
                {
                    result.extractedFields = respJson["data"]["outputs"];
                }
                else
                {
                    result.extractedFields = respJson;
                }
                result.success = true;
            } catch (std::exception& e) {
                result.success = false;
                result.errorMsg = std::string("Failed to parse Dify JSON response: ") + e.what();
            }
        }
        else
        {
            result.success = false;
            result.errorMsg = "Dify output result file is empty: " + wstring2string(resultJsonPath);
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