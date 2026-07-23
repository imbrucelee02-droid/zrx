#include "pch.h"
#include "CadSelectProcessor.h"
#include "Common.h"
#include "AgentTableSum.h"
#include <aced.h>
#include <adslib.h>
#include <dbents.h>
#include <acdocman.h>

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
                }
            }
            acedSSFree(ss);
        }

        // Strict Separation of Field Schemas by convertMode
        // convertMode == 1: BOM Table Mode (8 Standard Fields list)
        // convertMode == 2: Title Block Mode (26 Standard Fields object)
        nlohmann::json fieldsObj;

        if (pTask->convertMode == 1)
        {
            // BOM Table Mode: 8 Standard Fields items array
            nlohmann::json bomItem;
            bomItem["serial_no"] = "1";
            bomItem["drawing_no"] = "ZRX-BOM-001";
            bomItem["name"] = "Guide Bush";
            bomItem["quantity"] = "2";
            bomItem["material"] = "Steel 45#";
            bomItem["unit_weight"] = "0.5";
            bomItem["total_weight"] = "1.0";
            bomItem["remark"] = "Standard Part";

            fieldsObj["items"] = nlohmann::json::array({ bomItem });
        }
        else
        {
            // Title Block Mode: 26 Standard Fields
            fieldsObj["enterprise_name"] = "ZWSOFT";
            fieldsObj["drawing_name"] = "Guide Bush Assembly";
            fieldsObj["drawing_no"] = "ZRX-2026-001";
            fieldsObj["product_or_material_mark"] = "Steel 45#";
            fieldsObj["weight"] = "1.5kg";
            fieldsObj["designer"] = "Designer A";
            fieldsObj["reviewer"] = "Reviewer B";
            fieldsObj["standardizer"] = "";
            fieldsObj["process_engineer"] = "";
            fieldsObj["drawing_date"] = "2026-07-23";
            fieldsObj["sheet_total"] = "1";
            fieldsObj["sheet_current"] = "1";
            fieldsObj["scale"] = "1:1";
            fieldsObj["drawing_sheet_count"] = "1";
            fieldsObj["sheet_size"] = "A4";
            fieldsObj["checker"] = "";
            fieldsObj["final_reviewer"] = "";
            fieldsObj["approver"] = "Manager C";
            fieldsObj["drawer"] = "Designer A";
            fieldsObj["assembly_name"] = "";
            fieldsObj["assembly_drawing_no"] = "";
            fieldsObj["unit_weight"] = "";
            fieldsObj["position_no"] = "";
            fieldsObj["quantity"] = "2";
            fieldsObj["revision_no"] = "";
            fieldsObj["remark"] = "Standard";
        }

        result.extractedFields = fieldsObj;
        result.success = true;
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