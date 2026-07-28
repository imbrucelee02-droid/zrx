#pragma once
#include "StdAfx.h"
#include "resource.h"
#include "WebView2Host.h"
#include "IMessageHandler.h"
#include <memory>

// Modal WebView2 Dialog.
// Displays a WebView2 page and blocks until the user closes it.
// Vue can send { type: "close", data: { result: "ok", ... } } to close
// the dialog programmatically.
class CWebView2ModalDialog : public CZcUiDialog
{
    DECLARE_DYNAMIC(CWebView2ModalDialog)

public:
    // |routePath| -- Vue hash route (e.g. "/settings")
    CWebView2ModalDialog(const wchar_t* routePath = L"/",
                         CWnd* pParent = nullptr);
    virtual ~CWebView2ModalDialog();

    enum { IDD = IDD_WEBVIEW2_DIALOG };

    // Set a custom message handler (takes ownership)
    void SetMessageHandler(std::unique_ptr<IMessageHandler> handler);

    // Result JSON returned by the Vue frontend (via "close" message)
    const std::wstring& GetResultJson() const { return m_resultJson; }

    virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    DECLARE_MESSAGE_MAP()

private:
    std::unique_ptr<WebView2Host> m_host;
    std::unique_ptr<IMessageHandler> m_ownedHandler;
    std::wstring m_routePath;
    std::wstring m_resultJson;

    // Internal handler wrapper that intercepts "close" messages
    class ModalMessageProxy;
    std::unique_ptr<ModalMessageProxy> m_proxy;
};
