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
        // 1. Ensure HTTP Gateway is running on port 18088
        NS_ZrxHttp::ZrxHttpServer::Instance().Start(18088);

        // 2. Locate Microsoft Edge full executable path safely
        std::wstring edgePath = L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
        DWORD dwAttrib = GetFileAttributesW(edgePath.c_str());
        if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
        {
            edgePath = L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe";
        }

        std::wstring appCmd = L"--app=" + url;

        // 3. Safely launch standalone chromeless app window
        HINSTANCE hInst = ShellExecuteW(NULL, L"open", edgePath.c_str(), appCmd.c_str(), NULL, SW_SHOWNORMAL);
        if ((INT_PTR)hInst <= 32)
        {
            // Fallback to default browser launch if msedge.exe is not found
            ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        acutPrintf(L"\n[AI_Convert] Local HTTP Gateway active on http://127.0.0.1:18088, Frontend UI launched: %s", url.c_str());
    }
}