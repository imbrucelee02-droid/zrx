#pragma once
#include <string>

namespace NS_CadWebView
{
    class CadWebViewDialog
    {
    public:
        static void ShowWebViewWindow(const std::wstring& url = L"http://127.0.0.1:8501");
    };
}