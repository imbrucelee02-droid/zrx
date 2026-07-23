#pragma once
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include "CadTableWriter.h"

namespace NS_CadSelect
{
    struct SelectResult
    {
        bool success = false;
        std::string errorMsg;
        NS_CadTable::BBox2D bbox;
        std::vector<std::string> selectedHandles;
        nlohmann::json extractedFields;
    };

    class CadSelectProcessor
    {
    public:
        // Execute real CAD selection in UI context, get BBox, handles, and run real extraction
        static SelectResult ExecuteRealSelection(int convertMode);
    };
}
