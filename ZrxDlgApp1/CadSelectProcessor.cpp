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
#include <curl/curl.h>

namespace NS_CadSelect
{
    static size_t LocalCurlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* s)
    {
        size_t newLength = size * nmemb;
        if (!s) return 0;
        try
        {
            s->append((char*)contents, newLength);
            return newLength;
        }
        catch (...)
        {
            return 0;
        }
    }

    // Helper function for direct cURL POST to Dify Workflow (http://192.168.57.56/v1/workflows/run)
    static bool QuickDifyCall(const std::string& jsonContent, const std::string& apiKey, std::string& outResponseStr, std::string& outErr)
    {
        CURL* curl = curl_easy_init();
        if (!curl) { outErr = "curl_easy_init failed"; return false; }

        curl_easy_setopt(curl, CURLOPT_PROXY, "");

        nlohmann::json payload;
        payload["inputs"]["json_string"] = jsonContent;
        payload["response_mode"] = "blocking";
        payload["user"] = "zwsoft-desktop";

        std::string reqBody = payload.dump();
        struct curl_slist* headers = NULL;
        std::string authHeader = "Authorization: Bearer " + apiKey;
        headers = curl_slist_append(headers, authHeader.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");

        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.57.56/v1/workflows/run");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reqBody.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)reqBody.size());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, LocalCurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponseStr);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); // 5s connect timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);        // 15s total timeout

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            outErr = "cURL error: " + std::string(curl_easy_strerror(res));
            return false;
        }

        if (httpCode != 200)
        {
            outErr = "Dify HTTP status " + std::to_string(httpCode) + ", Body: " + outResponseStr;
            return false;
        }

        return true;
    }

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
        acutPrintf(L"\n[AI Convert] Single box selection completed. Processing Dify Workflow...\n");
        acedRedraw(NULL, 0);

        AcGePoint2d minPt( (pt1[X] < pt2[X]) ? pt1[X] : pt2[X], (pt1[Y] < pt2[Y]) ? pt1[Y] : pt2[Y] );
        AcGePoint2d maxPt( (pt1[X] > pt2[X]) ? pt1[X] : pt2[X], (pt1[Y] > pt2[Y]) ? pt1[Y] : pt2[Y] );

        // Calculate real BBox
        result.bbox.minX = minPt.x;
        result.bbox.minY = minPt.y;
        result.bbox.maxX = maxPt.x;
        result.bbox.maxY = maxPt.y;

        // Collect entities, handles & DWG text content directly from single box selection
        ads_name ss;
        std::vector<NS_TableSum::TextBox> textBoxVec;
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

                    // Open entity to extract DWG text directly
                    ZcDbEntity* pEnt = nullptr;
                    if (zcdbOpenObject(pEnt, objId, ZcDb::kForRead) == Zcad::eOk && pEnt)
                    {
                        if (pEnt->isKindOf(ZcDbText::desc()))
                        {
                            ZcDbText* pText = ZcDbText::cast(pEnt);
                            if (pText && pText->textString())
                            {
                                NS_TableSum::TextBox tb;
                                tb.content = pText->textString();
                                tb.bBoxValid = true;
                                pText->getGeomExtents(tb.textBox);
                                textBoxVec.push_back(tb);
                            }
                        }
                        else if (pEnt->isKindOf(ZcDbMText::desc()))
                        {
                            ZcDbMText* pMText = ZcDbMText::cast(pEnt);
                            if (pMText && pMText->contents())
                            {
                                NS_TableSum::TextBox tb;
                                tb.content = pMText->contents();
                                tb.bBoxValid = true;
                                pMText->getGeomExtents(tb.textBox);
                                textBoxVec.push_back(tb);
                            }
                        }
                        pEnt->close();
                    }
                }
            }
            acedSSFree(ss);
        }

        // Serialize DWG text to JSON payload
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
            textsArr.push_back(tbJson);
        }
        jsonVal["texts"] = textsArr;
        std::string dwgJsonStr = jsonVal.dump(4);

        // Mode Check: convertMode == 1 is BOM Mode; convertMode == 0 or 2 is TitleBlock Mode
        bool isBomMode = (pTask->convertMode == 1);
        std::string difyApiKey = isBomMode ? "app-DnkpWQxiXmg2lZt2mQ8rnI5u" : "app-0B7nJIc5Jd1lblBjfADmRvkM";

        std::string difyResponseStr = "";
        std::string difyErr = "";
        bool difyOk = false;

        // Run Dify Workflow (http://192.168.57.56/v1/workflows/run) with 3 retries
        for (int retry = 1; retry <= 3; ++retry)
        {
            acutPrintf(L"\n[AI Convert] Calling Dify workflow (mode=%d, attempt %d/3)...", pTask->convertMode, retry);
            difyOk = QuickDifyCall(dwgJsonStr, difyApiKey, difyResponseStr, difyErr);
            if (difyOk) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        if (!difyOk)
        {
            result.success = false;
            result.errorMsg = "Dify server request failed after 3 retries: " + difyErr;
            pTask->pPromise->set_value(result);
            return;
        }

        try {
            nlohmann::json respJson = nlohmann::json::parse(difyResponseStr);
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
            result.errorMsg = std::string("Failed to parse Dify JSON response: ") + e.what() + ", Response: " + difyResponseStr;
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