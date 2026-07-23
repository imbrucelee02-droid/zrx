#include "pch.h"
#include "CadWebViewDialog.h"
#include "ZrxHttpServer.h"
#include "Common.h"
#include <windows.h>
#include <shellapi.h>

namespace NS_CadWebView
{
    void CadWebViewDialog::ShowWebViewWindow(const std::wstring& url)
    {
        // Start HTTP Server
        NS_ZrxHttp::ZrxHttpServer::Instance().Start(18088);

        // Open Default Browser or WebView2 Host with Frontend Web App
        ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        acutPrintf(L"\n[AI_Convert] Local HTTP Server running on http://127.0.0.1:18088, Frontend UI opened: %s", url.c_str());
    }
}