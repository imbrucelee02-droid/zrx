#pragma once
#include "acui.h"
#include "WebView2Host.h"
#include "IMessageHandler.h"
#include <memory>

// WebView2 Palette Panel -- a single tab inside a PaletteSet.
// Each panel hosts one WebView2Host navigated to a Vue route.
class CWebView2PalettePanel : public CZdUiPalette
{
    DECLARE_DYNCREATE(CWebView2PalettePanel)

public:
    CWebView2PalettePanel();
    CWebView2PalettePanel(const wchar_t* routePath);
    virtual ~CWebView2PalettePanel();

    // Send JSON data to the Vue frontend
    void SendJsonToUI(const std::wstring& json);

    // Set a custom message handler (takes ownership)
    void SetMessageHandler(std::unique_ptr<IMessageHandler> handler);

    WebView2Host* GetHost() const { return m_host.get(); }

protected:
    virtual BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()

private:
    std::unique_ptr<WebView2Host> m_host;
    std::unique_ptr<IMessageHandler> m_ownedHandler;
    std::wstring m_routePath;
    BOOL m_bInitialized;
};
