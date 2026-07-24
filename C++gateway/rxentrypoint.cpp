#include "pch.h"
#include "stdafx.h"
#include "adsmigr.h"
#include "adsdef.h"
#include "adscodes.h"
#include "acestext.h"
#include "acedads.h"

#include "Common.h"
#include "HTTP_Server.h"
#include "HTTP_Function_List.h"
#include "AgentTableSum.h"

void initapp()
{
	CZcModuleResourceOverride resOverride;

	// Register HTTP Tool command (transparent CAD command triggered via sendStringToExecute)
	acedRegCmds->addCommand(cmd_group_name, _T("HTTP_TOOL"), _T("HTTP_TOOL"), ACRX_CMD_TRANSPARENT, HttpToolCmd);

	// AI table recognition & BOM commands
	zcedRegCmds->addCommand(cmd_group_name, _T("PRASE_PATH_TABLE_JSON"), _T("PRASE_PATH_TABLE_JSON"), ZCRX_CMD_MODAL, PraseTables2);
	zcedRegCmds->addCommand(cmd_group_name, _T("AI_TABLE_RECOGNIZE"), _T("AI_TABLE_RECOGNIZE"), ZCRX_CMD_MODAL, AiTableRecognizeCmd);
	zcedRegCmds->addCommand(cmd_group_name, _T("AI_BOM_Convert"), _T("AI_BOM_Convert"), ZCRX_CMD_MODAL, AiBomConvertCmd);
	zcedRegCmds->addCommand(cmd_group_name, _T("AI_Convert"), _T("AI_Convert"), ZCRX_CMD_MODAL, AiConvertCmd);
}

void unloadapp()
{
	Zcad::ErrorStatus ret = zcedRegCmds->removeGroup(cmd_group_name);
}

extern "C" ZcRx::AppRetCode zcrxEntryPoint(ZcRx::AppMsgCode msg, void* appId)
{
	switch (msg)
	{
		case ZcRx::kInitAppMsg:
		{
			InitHttpFunc();

			HTTP_Server::CreateInstance();
			if (HTTP_Server::GetInstance())
			{
				HTTP_Server::GetInstance()->cmdStartHttpServer();
			}

			initapp();

			zcrxDynamicLinker->unlockApplication(appId);
			zcrxDynamicLinker->registerAppMDIAware(appId);
		}
		break;
		case ZcRx::kUnloadAppMsg:
		{
			if (HTTP_Server::GetInstance())
			{
				HTTP_Server::GetInstance()->cmdStopHttpServer();
			}
			HTTP_Server::DeleteInstance();

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
#else
#pragma comment(linker, "/export:_zcrxEntryPoint,PRIVATE")
#pragma comment(linker, "/export:_zcrxGetApiVersion,PRIVATE")
#endif
