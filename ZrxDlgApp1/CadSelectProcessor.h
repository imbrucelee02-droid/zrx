#pragma once
#include <string>
#include <vector>
#include <future>
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
        // Safely execute CAD selection in Application Context (Main UI Thread) without deadlock
        static SelectResult ExecuteRealSelection(int convertMode);

    private:
        static void SelectionTaskFunc(void* pData);
    };

    struct SelectionTaskData
    {
        int convertMode = 1;
        std::promise<SelectResult>* pPromise = nullptr;
    };
}