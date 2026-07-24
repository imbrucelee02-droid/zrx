#pragma once
#include <string>
#include <map>
#include <vector>
#include <future>
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

    struct WriteTaskData
    {
        int convertMode = 0;
        int styleType = 1;
        BBox2D bbox;
        nlohmann::json fieldsData;
        std::vector<std::string> eraseHandles;
        std::promise<bool>* pPromise = nullptr;
        std::string outError;
    };

    class CadTableWriter
    {
    public:
        // Safely write CAD Native ZcDbTable in Application Context (CAD Main Thread)
        static bool WriteNativeTable(int convertMode, int styleType, const BBox2D& bbox, const nlohmann::json& fieldsData, const std::vector<std::string>& eraseHandles, std::string& outError);

    private:
        static void WriteTaskFunc(void* pData);
        static bool WriteNativeTableDirect(int convertMode, int styleType, const BBox2D& bbox, const nlohmann::json& fieldsData, const std::vector<std::string>& eraseHandles, std::string& outError);
    };
}