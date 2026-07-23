#include "pch.h"
#include "CadTableWriter.h"
#include "Common.h"
#include <dbtable.h>
#include <dbapserv.h>
#include <dbents.h>

namespace NS_CadTable
{
    bool CadTableWriter::WriteNativeTable(int convertMode, int styleType, const BBox2D& bbox, const nlohmann::json& fieldsData)
    {
        ZcDbDatabase* pDb = zcdbHostApplicationServices()->workingDatabase();
        if (!pDb) return false;

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
        pTable->setDatabaseDefaults();
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
        // Top Y = bbox.minY + totalHeight
        ZcGePoint3d insertionPt(bbox.minX, bbox.minY + totalHeight, 0.0);
        pTable->setPosition(insertionPt);

        // Add to Block Table Record
        ZcDbBlockTable* pBlockTable = nullptr;
        if (pDb->getBlockTable(pBlockTable, ZcDb::kForRead) != Zcad::eOk)
        {
            delete pTable;
            return false;
        }

        ZcDbBlockTableRecord* pModelSpace = nullptr;
        if (pBlockTable->getAt(ZCDB_MODEL_SPACE, pModelSpace, ZcDb::kForWrite) != Zcad::eOk)
        {
            pBlockTable->close();
            delete pTable;
            return false;
        }
        pBlockTable->close();

        ZcDbObjectId tableId;
        if (pModelSpace->appendZcDbEntity(tableId, pTable) != Zcad::eOk)
        {
            pModelSpace->close();
            delete pTable;
            return false;
        }

        pTable->close();
        pModelSpace->close();
        return true;
    }
}