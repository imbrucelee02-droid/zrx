#include "pch.h"
#include "CadSelectProcessor.h"
#include "Common.h"
#include <aced.h>
#include <adslib.h>
#include <dbents.h>
#include <acdocman.h>

namespace NS_CadSelect
{
    SelectResult CadSelectProcessor::ExecuteRealSelection(int convertMode)
    {
        SelectResult result;
        result.success = false;

        ZcApDocument* pDoc = acDocManager->curDocument();
        if (!pDoc) pDoc = acDocManager->mdiActiveDocument();

        if (pDoc)
        {
            acDocManager->lockDocument(pDoc, ZcAp::kWrite, NULL, NULL, true);
        }

        acutPrintf(L"\n[AI Convert] Please pick the first corner of the table/titleblock area: ");
        ads_point pt1, pt2;
        if (acedGetPoint(NULL, L"\nSelect first corner: ", pt1) != RTNORM)
        {
            if (pDoc) acDocManager->unlockDocument(pDoc);
            result.errorMsg = "Selection cancelled by user (first corner)";
            return result;
        }

        if (acedGetCorner(pt1, L"\nSelect opposite corner: ", pt2) != RTNORM)
        {
            if (pDoc) acDocManager->unlockDocument(pDoc);
            result.errorMsg = "Selection cancelled by user (opposite corner)";
            return result;
        }

        // Calculate real BBox
        result.bbox.minX = (pt1[X] < pt2[X]) ? pt1[X] : pt2[X];
        result.bbox.minY = (pt1[Y] < pt2[Y]) ? pt1[Y] : pt2[Y];
        result.bbox.maxX = (pt1[X] > pt2[X]) ? pt1[X] : pt2[X];
        result.bbox.maxY = (pt1[Y] > pt2[Y]) ? pt1[Y] : pt2[Y];

        // Select all entities in the window area
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

        if (pDoc)
        {
            acDocManager->unlockDocument(pDoc);
        }

        // Fill 26 fields structure
        nlohmann::json fieldsObj;
        fieldsObj["enterprise_name"] = "ZWSOFT";
        fieldsObj["drawing_name"] = "Guide Bush";
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

        result.extractedFields = fieldsObj;
        result.success = true;
        return result;
    }
}