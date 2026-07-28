#pragma once
#include "StdAfx.h"
#include "resource.h"
#include "WebView2Host.h"
#include "IMessageHandler.h"
#include <memory>

// Modeless WebView2 Dialog with ZWCAD focus management.
// Handles WM_ACAD_KEEPFOCUS to prevent ZWCAD from stealing focus.
// ESC hides the dialog (ShowWindow(SW_HIDE)) instead of destroying it.
class CWebView2ModelessDialog : public CZcUiDialog
{
    DECLARE_DYNAMIC(CWebView2ModelessDialog)

public:
    // |routePath| -- Vue hash route (e.g. "/tools")
    CWebView2ModelessDialog(const wchar_t* routePath = L"/",
                            CWnd* pParent = nullptr);
    virtual ~CWebView2ModelessDialog();

    enum { IDD = IDD_WEBVIEW2_DIALOG };

    // Create and show the modeless dialog
    void ShowModeless();

    // Send JSON to Vue frontend
    void SendJsonToUI(const std::wstring& jsonData);

    // Set a custom message handler (takes ownership)
    void SetMessageHandler(std::unique_ptr<IMessageHandler> handler);

    virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnCancel();
    virtual void OnOK();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnClose();
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg LRESULT OnAcadKeepFocus(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    std::unique_ptr<WebView2Host> m_host;
    std::unique_ptr<IMessageHandler> m_ownedHandler;
    std::wstring m_routePath;
};

// Global modeless dialog instance pointer
extern CWebView2ModelessDialog* g_pModelessDialog;
