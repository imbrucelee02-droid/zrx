#include "pch.h"
#include "CadTableWriter.h"
#include "Common.h"
#include <dbtable.h>
#include <dbapserv.h>
#include <dbents.h>
#include <aced.h>
#include <acdocman.h>

namespace NS_CadTable
{
    bool CadTableWriter::WriteNativeTable(int convertMode, int styleType, const BBox2D& bbox, const nlohmann::json& fieldsData, const std::vector<std::string>& eraseHandles, std::string& outError)
    {
        outError.clear();

        ZcApDocument* pDoc = acDocManager->curDocument();
        if (!pDoc)
        {
            pDoc = acDocManager->mdiActiveDocument();
        }

        ZcDbDatabase* pDb = pDoc ? pDoc->database() : zcdbHostApplicationServices()->workingDatabase();
        if (!pDb)
        {
            outError = "No working CAD database available";
            return false;
        }

        // Lock Document for multi-threading safety
        ZcApDocument* pLockDoc = pDoc;
        if (pLockDoc)
        {
            acDocManager->lockDocument(pLockDoc, ZcAp::kWrite, NULL, NULL, true);
        }

        // Scheme A: Real Erase old selected entities if handles provided
        int erasedCount = 0;
        for (const auto& hStr : eraseHandles)
        {
            if (hStr.empty()) continue;
            try {
                ZcDbHandle h(string2wstring(hStr).c_str());
                ZcDbObjectId objId;
                if (pDb->getAcDbObjectId(objId, false, h) == Zcad::eOk && !objId.isNull())
                {
                    ZcDbEntity* pOldEnt = nullptr;
                    if (zcdbOpenObject(pOldEnt, objId, ZcDb::kForWrite) == Zcad::eOk && pOldEnt)
                    {
                        pOldEnt->erase(true);
                        pOldEnt->close();
                        erasedCount++;
                    }
                }
            } catch (...) {}
        }

        std::vector<std::pair<std::string, std::string>> kvPairs;
        if (fieldsData.is_object())
        {
            for (auto it = fieldsData.begin(); it != fieldsData.end(); ++it)
            {
                std::string valStr = "";
                if (it.value().is_string()) valStr = it.value().get<std::string>();
                else if (!it.value().is_null()) valStr = it.value().dump();
                kvPairs.push_back({ it.key(), valStr });
            }
        }

        if (kvPairs.empty())
        {
            kvPairs.push_back({ "Status", "No fields provided" });
        }

        int numRows = (int)kvPairs.size() + 1; // +1 header row
        int numCols = 2; // Key, Value

        ZcDbTable* pTable = new ZcDbTable();
        pTable->setDatabaseDefaults(pDb);
        pTable->setSize(numRows, numCols);

        double rowHeight = 8.0;
        double colWidth0 = 40.0;
        double colWidth1 = 80.0;

        pTable->setRowHeight(rowHeight);
        pTable->setColumnWidth(0, colWidth0);
        pTable->setColumnWidth(1, colWidth1);

        // Header
        std::wstring headerTitle = (convertMode == 1) ? L"BOM Table (Style " : L"Title Block (Style ";
        headerTitle += std::to_wstring(styleType) + L")";
        pTable->setTextString(0, 0, headerTitle.c_str());

        // Fill Pairs
        for (size_t i = 0; i < kvPairs.size(); ++i)
        {
            int r = (int)i + 1;
            std::wstring wKey = string2wstring(kvPairs[i].first);
            std::wstring wVal = string2wstring(kvPairs[i].second);
            pTable->setTextString(r, 0, wKey.c_str());
            pTable->setTextString(r, 1, wVal.c_str());
        }

        // Calculate total table height
        double totalHeight = numRows * rowHeight;

        // Alignment: Table Bottom Boundary = bbox.minY
        ZcGePoint3d insertionPt(bbox.minX, bbox.minY + totalHeight, 0.0);
        pTable->setPosition(insertionPt);

        // Add to Block Table Record
        ZcDbBlockTable* pBlockTable = nullptr;
        Zcad::ErrorStatus es = pDb->getBlockTable(pBlockTable, ZcDb::kForRead);
        if (es != Zcad::eOk)
        {
            if (pLockDoc) acDocManager->unlockDocument(pLockDoc);
            delete pTable;
            outError = "getBlockTable failed with status code: " + std::to_string((int)es);
            return false;
        }

        ZcDbBlockTableRecord* pModelSpace = nullptr;
        es = pBlockTable->getAt(ZCDB_MODEL_SPACE, pModelSpace, ZcDb::kForWrite);
        if (es != Zcad::eOk)
        {
            pBlockTable->close();
            if (pLockDoc) acDocManager->unlockDocument(pLockDoc);
            delete pTable;
            outError = "getAt(ZCDB_MODEL_SPACE) failed with status code: " + std::to_string((int)es);
            return false;
        }
        pBlockTable->close();

        ZcDbObjectId tableId;
        es = pModelSpace->appendZcDbEntity(tableId, pTable);
        if (es != Zcad::eOk)
        {
            pModelSpace->close();
            if (pLockDoc) acDocManager->unlockDocument(pLockDoc);
            delete pTable;
            outError = "appendZcDbEntity failed with status code: " + std::to_string((int)es);
            return false;
        }

        pTable->close();
        pModelSpace->close();

        if (pLockDoc)
        {
            acDocManager->unlockDocument(pLockDoc);
        }

        // Force viewport update
        acedUpdateDisplay();
        return true;
    }
}