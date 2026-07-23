#pragma once
#include <string>
#include <map>
#include <vector>
#include "nlohmann/json.hpp"

namespace NS_CadTable
{
    struct BBox2D
    {
        double minX = 0.0;
        double minY = 0.0;
        double maxX = 0.0;
        double maxY = 0.0;
    };

    class CadTableWriter
    {
    public:
        // Write CAD Native ZcDbTable aligned with BBox bottom boundary
        static bool WriteNativeTable(int convertMode, int styleType, const BBox2D& bbox, const nlohmann::json& fieldsData);
    };
}