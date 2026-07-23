#include "pch.h"
#include "CadWebViewDialog.h"
#include "ZrxHttpServer.h"
#include "stdafx.h"
#include "adsmigr.h"
#include "adsdef.h"
#include "adscodes.h"
#include "acestext.h"
#include "acedads.h"
#include "helloworld.h"

#include "PraseDwgTables2.h"

#include "Common.h"
#include "AgentTableSum.h"
#include <fstream>
#include "nlohmann/json.hpp"
using json = nlohmann::json;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#ifndef MFC_DLL_LOAD


//Test1?????????????
void Test1()
{
	zcutPrintf(_T("\nHello ZWCAD ZRX !"));
}

//task3-1 subfuction ?????????
int CreatLine()
{
	//?????????
	ZcGePoint3d startPt(4.0, 2.0, 0.0);
	ZcGePoint3d endPt(10.0, 7.0, 0.0);
	ZcDbLine *pLine = new ZcDbLine(startPt, endPt);
	if (pLine == NULL)
	{
		zcutPrintf(_T("\n new zcDbLine failed!"));
		return -1;
	}

	//???ú??
	pLine->setColorIndex(1);

	//??????????
	ZcDbBlockTable *pBlockTable = NULL;
	zcdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, ZcDb::kForRead);
	if (pBlockTable == NULL)
	{
		zcutPrintf(_T("\n getSymbolTable failed!"));
		return -1;
	}

	ZcDbBlockTableRecord *pBlockTableRecord = NULL;
	pBlockTable->getAt(ZCDB_MODEL_SPACE, pBlockTableRecord, ZcDb::kForWrite);
	if (pBlockTableRecord == NULL)
	{
		zcutPrintf(_T("\n pBlockTableRecord init failed!"));
		return -1;
	}

	pBlockTable->close();
	//??????????????????
	ZcDbObjectId lineId;
	pBlockTableRecord->appendZcDbEntity(lineId, pLine);
	//????????????壺
	pBlockTableRecord->close();
	pLine->close();
	return 0;
}

// task3-1.????????????壬???????????????????????.
void CreateRedLineCmd()
{
	//TODO...
	zcutPrintf(_T("\n CreatLine start:"));
	int nReturn = CreatLine();
	if (nReturn == 0)
	{
		zcutPrintf(_T("\n CreatLine success!"));
	}
	else
	{
		zcutPrintf(_T("\n CreatLine failed!"));
	}
}


// AI table recognition command
void AiTableRecognizeCmd()
{
	AcGePoint2d minPt, maxPt;
	if (!NS_TableSum::PickWindow(minPt, maxPt))
	{
		acutPrintf(_T("\nPick cancelled!"));
		return;
	}

	ads_name ss;
	int nRet = acedSSGet(_T("C"), &minPt, &maxPt, NULL, ss);
	if (nRet != RTNORM)
	{
		acutPrintf(_T("\nNo entities selected!"));
		return;
	}

	AcDbObjectIdArray idArr;
	int nCount = 0;
	acedSSLength(ss, &nCount);
	acutPrintf(_T("\nSelected %d entities"), nCount);

	ads_name entName;
	AcDbObjectId objId;
	for (int i = 0; i < nCount; i++)
	{
		acedSSName(ss, i, entName);
		if (acdbGetObjectId(objId, entName) == Acad::eOk)
		{
			idArr.append(objId);
		}
	}
	acedSSFree(ss);

	NS_TableSum::RecognizeParam param;
	std::vector<ZcGePoint2d> points = Get4PointsFromMinMaxPt(minPt, maxPt);
	if (points.size() != 4)
	{
		acutPrintf(_T("\nCalculate region error!"));
		return;
	}
	param.m_firstPt = ZcGePoint3d(points[0].x, points[0].y, 0.0);
	param.m_secondPt = ZcGePoint3d(points[1].x, points[1].y, 0.0);
	param.m_thirdPt = ZcGePoint3d(points[2].x, points[2].y, 0.0);
	param.m_fourthPt = ZcGePoint3d(points[3].x, points[3].y, 0.0);

	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
	ZcString dwgPath;
	if (pDb)
	{
		const ZTCHAR* pDwgName = nullptr;
		pDb->getFilename(pDwgName);
		dwgPath = pDwgName ? pDwgName : _T("");
	}
	ZcString shortFileName = GetShortFileName(dwgPath);
	if (shortFileName.isEmpty())
	{
		shortFileName = _T("AiTableRecognize");
	}

	std::unordered_map<int, std::vector<NS_TableSum::AIConvertTableInfo>> outTableInfoVecMap;
	std::unordered_map<int, std::vector<NS_TableSum::AIConvertTextInfo>> outTextInfoVecMap;
	NS_TableSum::AIConvertType convertType = NS_TableSum::AIConvertType::tableRecOne;

	acutPrintf(_T("\nCalling AI recognition service, please wait..."));
	bool bRet = NS_TableSum::GetEntitysTableResult(idArr, convertType, param,
		outTableInfoVecMap, outTextInfoVecMap, "", AcStringToUtf8(shortFileName));

	if (!bRet)
	{
		acutPrintf(_T("\nTable recognition failed!"));
		return;
	}

	acutPrintf(_T("\n========== AI Recognition Result =========="));
	for (const auto& pair : outTableInfoVecMap)
	{
		int pageIdx = pair.first;
		const auto& tables = pair.second;
		acutPrintf(_T("\nPage %d: %d table(s) found"), pageIdx, (int)tables.size());
		for (size_t t = 0; t < tables.size(); t++)
		{
			const auto& tbl = tables[t];
			acutPrintf(_T("\n  Table[%d]: %d rows x %d cols, %d cells"),
				(int)t, tbl.m_rows, tbl.m_cols, (int)tbl.m_cells.size());
			for (const auto& cell : tbl.m_cells)
			{
				if (!cell.m_mergeText.empty())
				{
					acutPrintf(_T("\n    [r%dc%d] %s"),
						cell.m_rowStartIdx, cell.m_colStartIdx,
						cell.m_mergeText.c_str());
				}
			}
		}
	}
	for (const auto& pair : outTextInfoVecMap)
	{
		int pageIdx = pair.first;
		const auto& texts = pair.second;
		acutPrintf(_T("\nPage %d: %d text(s) found"), pageIdx, (int)texts.size());
		for (size_t i = 0; i < texts.size(); i++)
		{
			acutPrintf(_T("\n  Text[%d]: %s"), (int)i, texts[i].m_textStr.c_str());
		}
	}
	acutPrintf(_T("\n================================\n"));

	// Local BOM Table Format detection (Type 2 if 'DaiHao' keyword exists in OCR result, otherwise Type 1 default)
	bool bHasDaiHao = false;
	const wchar_t wszDaiHao[] = { 0x4EE3, 0x53F7, 0 }; // L"DaiHao"

	for (const auto& pair : outTableInfoVecMap)
	{
		for (const auto& tbl : pair.second)
		{
			for (const auto& cell : tbl.m_cells)
			{
				ZcString cellText(cell.m_mergeText.c_str());
				if (cellText.find(wszDaiHao) != -1)
				{
					bHasDaiHao = true;
					break;
				}
			}
			if (bHasDaiHao) break;
		}
		if (bHasDaiHao) break;
	}

	if (!bHasDaiHao)
	{
		for (const auto& pair : outTextInfoVecMap)
		{
			for (const auto& txt : pair.second)
			{
				ZcString textStr(txt.m_textStr.c_str());
				if (textStr.find(wszDaiHao) != -1)
				{
					bHasDaiHao = true;
					break;
				}
			}
			if (bHasDaiHao) break;
		}
	}

	// Local variable to store BOM format (1 = Drawing No / TuHao, 2 = Code No / DaiHao)
	int nBomFormatType = bHasDaiHao ? 2 : 1;
	if (nBomFormatType == 2)
	{
		acutPrintf(_T("\n[BOM Convert] Detected Table Format: Type 2 (Header Column 2: Code No / DaiHao)"));
	}
	else
	{
		acutPrintf(_T("\n[BOM Convert] Detected Table Format: Type 1 (Header Column 2: Drawing No / TuHao - Default)"));
	}

	// Export AI Table Recognition result to JSON

	json aiJson;
	aiJson["file"] = AcStringToUtf8(shortFileName) + ".dwg";
	
	json tablesArray = json::array();
	int tableId = 0;
	for (const auto& pair : outTableInfoVecMap)
	{
		for (const auto& tbl : pair.second)
		{
			json tableObj;
			tableObj["handle_hex"] = "AI_TABLE_" + std::to_string(tableId++);
			
			json cellsArray = json::array();
			int cellId = 0;
			for (const auto& cell : tbl.m_cells)
			{
				json cellObj;
				cellObj["cell_id"] = cellId++;
				cellObj["text"] = AcStringToUtf8(AcString(cell.m_mergeText.c_str()));
				
				json rangeObj;
				rangeObj["start_row"] = cell.m_rowStartIdx;
				rangeObj["end_row"] = cell.m_rowEndIdx;
				rangeObj["start_col"] = cell.m_colStartIdx;
				rangeObj["end_col"] = cell.m_colEndIdx;
				cellObj["cell_range"] = rangeObj;
				
				cellsArray.push_back(cellObj);
			}
			tableObj["cells"] = cellsArray;
			tablesArray.push_back(tableObj);
		}
	}
	aiJson["tables"] = tablesArray;
	
	ZcString exportDir = _T("C:\\Users\\zwsoft\\Desktop\\transform\\testdata\\表格识别结果0629_cell_json\\");
	CreateSingleDirectory(exportDir);

	std::wstring outFile = exportDir.kTCharPtr();
	outFile += shortFileName.kTCharPtr();
	outFile += L".json";

	//For Local Test Only
	std::ofstream outFs(outFile);
	if (outFs.is_open())
	{
		outFs << aiJson.dump(4);
		outFs.close();
		acutPrintf(_T("\nAI table JSON exported successfully to: %s"), outFile.c_str());
	}
	else
	{
		acutPrintf(_T("\nFailed to open AI export path: %s"), outFile.c_str());
	}

	//For Local Test Only
	acutPrintf(_T("\nCalling Dify workflow, please wait..."));
	std::string difyError;
	std::wstring difyFileName = shortFileName.kTCharPtr();
	difyFileName += L".json";
	if (NS_TableSum::RunDifyWorkflowForFile(outFile, difyFileName, difyError))
	{
		acutPrintf(_T("\nDify workflow completed successfully! Results saved to C:\\Users\\zwsoft\\Desktop\\transform\\testdata\\dify_results\\"));
	}
	else
	{
		acutPrintf(_T("\nDify workflow failed: %s"), string2wstring(difyError).c_str());
	}
}


// AI BOM standard conversion command
void AiBomConvertCmd()
{
	AcGePoint2d minPt, maxPt;
	if (!NS_TableSum::PickWindow(minPt, maxPt))
	{
		acutPrintf(_T("\nPick cancelled!"));
		return;
	}

	ads_name ss;
	int nRet = acedSSGet(_T("C"), &minPt, &maxPt, NULL, ss);
	if (nRet != RTNORM)
	{
		acutPrintf(_T("\nNo entities selected!"));
		return;
	}

	AcDbObjectIdArray idArr;
	int nCount = 0;
	acedSSLength(ss, &nCount);
	acutPrintf(_T("\nSelected %d entities"), nCount);

	ads_name entName;
	AcDbObjectId objId;
	for (int i = 0; i < nCount; i++)
	{
		acedSSName(ss, i, entName);
		if (acdbGetObjectId(objId, entName) == Acad::eOk)
		{
			idArr.append(objId);
		}
	}
	acedSSFree(ss);

	NS_TableSum::RecognizeParam param;
	std::vector<ZcGePoint2d> points = Get4PointsFromMinMaxPt(minPt, maxPt);
	if (points.size() != 4)
	{
		acutPrintf(_T("\nCalculate region error!"));
		return;
	}
	param.m_firstPt = ZcGePoint3d(points[0].x, points[0].y, 0.0);
	param.m_secondPt = ZcGePoint3d(points[1].x, points[1].y, 0.0);
	param.m_thirdPt = ZcGePoint3d(points[2].x, points[2].y, 0.0);
	param.m_fourthPt = ZcGePoint3d(points[3].x, points[3].y, 0.0);

	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
	ZcString dwgPath;
	if (pDb)
	{
		const ZTCHAR* pDwgName = nullptr;
		pDb->getFilename(pDwgName);
		dwgPath = pDwgName ? pDwgName : _T("");
	}
	ZcString shortFileName = GetShortFileName(dwgPath);
	if (shortFileName.isEmpty())
	{
		shortFileName = _T("AiBomConvert");
	}

	std::unordered_map<int, std::vector<NS_TableSum::AIConvertTableInfo>> outTableInfoVecMap;
	std::unordered_map<int, std::vector<NS_TableSum::AIConvertTextInfo>> outTextInfoVecMap;
	NS_TableSum::AIConvertType convertType = NS_TableSum::AIConvertType::tableRecOne;

	acutPrintf(_T("\nCalling AI recognition service, please wait..."));
	bool bRet = NS_TableSum::GetEntitysTableResult(idArr, convertType, param,
		outTableInfoVecMap, outTextInfoVecMap, "C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\ocr_result_json\\", AcStringToUtf8(shortFileName));

	if (!bRet)
	{
		acutPrintf(_T("\nTable recognition failed!"));
		return;
	}

	acutPrintf(_T("\n========== AI Recognition Result =========="));
	for (const auto& pair : outTableInfoVecMap)
	{
		int pageIdx = pair.first;
		const auto& tables = pair.second;
		acutPrintf(_T("\nPage %d: %d table(s) found"), pageIdx, (int)tables.size());
		for (size_t t = 0; t < tables.size(); t++)
		{
			const auto& tbl = tables[t];
			acutPrintf(_T("\n  Table[%d]: %d rows x %d cols, %d cells"),
				(int)t, tbl.m_rows, tbl.m_cols, (int)tbl.m_cells.size());
			for (const auto& cell : tbl.m_cells)
			{
				if (!cell.m_mergeText.empty())
				{
					acutPrintf(_T("\n    [r%dc%d] %s"),
						cell.m_rowStartIdx, cell.m_colStartIdx,
						cell.m_mergeText.c_str());
				}
			}
		}
	}
	for (const auto& pair : outTextInfoVecMap)
	{
		int pageIdx = pair.first;
		const auto& texts = pair.second;
		acutPrintf(_T("\nPage %d: %d text(s) found"), pageIdx, (int)texts.size());
		for (size_t i = 0; i < texts.size(); i++)
		{
			acutPrintf(_T("\n  Text[%d]: %s"), (int)i, texts[i].m_textStr.c_str());
		}
	}
	acutPrintf(_T("\n================================\n"));

	// Local BOM Table Format detection (Type 2 if 'DaiHao' keyword exists in OCR result, otherwise Type 1 default)
	bool bHasDaiHao = false;
	const wchar_t wszDaiHao[] = { 0x4EE3, 0x53F7, 0 }; // L"DaiHao"

	for (const auto& pair : outTableInfoVecMap)
	{
		for (const auto& tbl : pair.second)
		{
			for (const auto& cell : tbl.m_cells)
			{
				ZcString cellText(cell.m_mergeText.c_str());
				if (cellText.find(wszDaiHao) != -1)
				{
					bHasDaiHao = true;
					break;
				}
			}
			if (bHasDaiHao) break;
		}
		if (bHasDaiHao) break;
	}

	if (!bHasDaiHao)
	{
		for (const auto& pair : outTextInfoVecMap)
		{
			for (const auto& txt : pair.second)
			{
				ZcString textStr(txt.m_textStr.c_str());
				if (textStr.find(wszDaiHao) != -1)
				{
					bHasDaiHao = true;
					break;
				}
			}
			if (bHasDaiHao) break;
		}
	}

	// Local variable to store BOM format (1 = Drawing No / TuHao, 2 = Code No / DaiHao)
	int nBomFormatType = bHasDaiHao ? 2 : 1;
	if (nBomFormatType == 2)
	{
		acutPrintf(_T("\n[BOM Convert] Detected Table Format: Type 2 (Header Column 2: Code No / DaiHao)"));
	}
	else
	{
		acutPrintf(_T("\n[BOM Convert] Detected Table Format: Type 1 (Header Column 2: Drawing No / TuHao - Default)"));
	}

	// Export AI Table Recognition result to JSON

	json aiJson;
	aiJson["file"] = AcStringToUtf8(shortFileName) + ".dwg";
	
	json tablesArray = json::array();
	int tableId = 0;
	for (const auto& pair : outTableInfoVecMap)
	{
		for (const auto& tbl : pair.second)
		{
			json tableObj;
			tableObj["handle_hex"] = "AI_TABLE_" + std::to_string(tableId++);
			
			json cellsArray = json::array();
			int cellId = 0;
			for (const auto& cell : tbl.m_cells)
			{
				json cellObj;
				cellObj["cell_id"] = cellId++;
				cellObj["text"] = AcStringToUtf8(AcString(cell.m_mergeText.c_str()));
				
				json rangeObj;
				rangeObj["start_row"] = cell.m_rowStartIdx;
				rangeObj["end_row"] = cell.m_rowEndIdx;
				rangeObj["start_col"] = cell.m_colStartIdx;
				rangeObj["end_col"] = cell.m_colEndIdx;
				cellObj["cell_range"] = rangeObj;
				
				cellsArray.push_back(cellObj);
			}
			tableObj["cells"] = cellsArray;
			tablesArray.push_back(tableObj);
		}
	}
	aiJson["tables"] = tablesArray;
	
	ZcString exportDir = _T("C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\cell_json\\");
	CreateSingleDirectory(exportDir);

	std::wstring outFile = exportDir.kTCharPtr();
	outFile += shortFileName.kTCharPtr();
	outFile += L".json";

	//For Local Test Only
	std::ofstream outFs(outFile);
	if (outFs.is_open())
	{
		outFs << aiJson.dump(4);
		outFs.close();
		acutPrintf(_T("\nAI BOM JSON exported successfully to: %s"), outFile.c_str());
	}
	else
	{
		acutPrintf(_T("\nFailed to open AI export path: %s"), outFile.c_str());
	}

	//For Local Test Only
	acutPrintf(_T("\nCalling Dify BOM workflow, please wait..."));
	std::string difyError;
	std::wstring difyFileName = shortFileName.kTCharPtr();
	difyFileName += L".json";
	if (NS_TableSum::RunDifyWorkflowForString(outFile, difyFileName, difyError, "app-DnkpWQxiXmg2lZt2mQ8rnI5u", L"C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\dify_results\\"))
	{
		acutPrintf(_T("\nDify workflow completed successfully! Results saved to C:\\Users\\zwsoft\\Desktop\\transform\\BOM_testdata\\dify_results\\"));
	}
	else
	{
		acutPrintf(_T("\nDify workflow failed: %s"), string2wstring(difyError).c_str());
	}
}


void AiConvertCmd()
{
    NS_CadWebView::CadWebViewDialog::ShowWebViewWindow(L"http://127.0.0.1:8501");
}

void initapp()
{
	CZcModuleResourceOverride resOverride;

	////for test
	zcedRegCmds->addCommand(cmd_group_name, _T("helloworld"), _T("helloworld"), ZCRX_CMD_MODAL, helloworld);

	zcedRegCmds->addCommand(cmd_group_name, _T("create_red_line"), _T("create_red_line"), ZCRX_CMD_MODAL, CreateRedLineCmd);

	//????·???б???????json
	zcedRegCmds->addCommand(cmd_group_name, _T("PRASE_PATH_TABLE_JSON"), _T("PRASE_PATH_TABLE_JSON"), ZCRX_CMD_MODAL, PraseTables2);

	//AI table recognition: window-select entities, send to AI service, get table/text results
	zcedRegCmds->addCommand(cmd_group_name, _T("AI_TABLE_RECOGNIZE"), _T("AI_TABLE_RECOGNIZE"), ZCRX_CMD_MODAL, AiTableRecognizeCmd);

	//AI BOM standard conversion command
		zcedRegCmds->addCommand(cmd_group_name, _T("AI_BOM_Convert"), _T("AI_BOM_Convert"), ZCRX_CMD_MODAL, AiBomConvertCmd);
	zcedRegCmds->addCommand(cmd_group_name, _T("AI_Convert"), _T("AI_Convert"), ZCRX_CMD_MODAL, AiConvertCmd);
}


void unloadapp()
{
	Zcad::ErrorStatus ret = zcedRegCmds->removeGroup(cmd_group_name);

	int x = 1;
}


extern "C" ZcRx::AppRetCode zcrxEntryPoint(ZcRx::AppMsgCode msg, void* appId)
{
	switch (msg)
	{
		case ZcRx::kInitAppMsg:
		{

			initapp();

			zcrxDynamicLinker->unlockApplication(appId); //????ж??
			zcrxDynamicLinker->registerAppMDIAware(appId); //

		}
		break;
		case ZcRx::kUnloadAppMsg:
		{
			unloadapp();
		}
		break;

		default:
			break;
	}
	return ZcRx::kRetOK;
}


#ifdef _WIN64
#pragma comment(linker, "/export:zcrxEntryPoint,PRIVATE")
#pragma comment(linker, "/export:zcrxGetApiVersion,PRIVATE")
#else // WIN32
#pragma comment(linker, "/export:_zcrxEntryPoint,PRIVATE")
#pragma comment(linker, "/export:_zcrxGetApiVersion,PRIVATE")
#endif


#endif
