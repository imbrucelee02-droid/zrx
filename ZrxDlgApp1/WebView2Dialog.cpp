#include "StdAfx.h"
#include "WebView2Dialog.h"
#include "SampleMessageHandler.h"
#include "CADLogger.h"

// Proxy that intercepts "close" messages and delegates the rest
class CWebView2ModalDialog::ModalMessageProxy : public IMessageHandler
{
public:
    ModalMessageProxy(CWebView2ModalDialog* dlg, IMessageHandler* inner)
        : m_dlg(dlg), m_inner(inner) {}

    void OnMessage(const std::wstring& type, const nlohmann::json& payload,
                   WebView2Host* host) override
    {
        if (type == L"close") {
            // Store result data and close the modal dialog
            if (payload.contains("data")) {
                m_dlg->m_resultJson = ToWString(payload["data"].dump());
            }
            m_dlg->EndDialog(IDOK);
            return;
        }
        if (m_inner) {
            m_inner->OnMessage(type, payload, host);
        }
    }

private:
    CWebView2ModalDialog* m_dlg;
    IMessageHandler* m_inner;
};

IMPLEMENT_DYNAMIC(CWebView2ModalDialog, CZcUiDialog)

CWebView2ModalDialog::CWebView2ModalDialog(const wchar_t* routePath, CWnd* pParent)
    : CZcUiDialog(IDD_WEBVIEW2_DIALOG, pParent)
    , m_routePath(routePath ? routePath : L"/")
{
}

CWebView2ModalDialog::~CWebView2ModalDialog()
{
    if (m_host) {
        m_host->Close();
    }
}

void CWebView2ModalDialog::DoDataExchange(CDataExchange* pDX)
{
    CZcUiDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CWebView2ModalDialog, CZcUiDialog)
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_WM_ACTIVATE()
    ON_WM_SETFOCUS()
END_MESSAGE_MAP()

BOOL CWebView2ModalDialog::OnInitDialog()
{
    CZcUiDialog::OnInitDialog();

    SetWindowText(_T("WebView2 Dialog"));

    // Create message handler chain
    if (!m_ownedHandler) {
        m_ownedHandler = std::make_unique<SampleMessageHandler>();
    }
    m_proxy = std::make_unique<ModalMessageProxy>(this, m_ownedHandler.get());

    m_host = std::make_unique<WebView2Host>();
    m_host->SetMessageHandler(m_proxy.get());
    m_host->Initialize(m_hWnd, m_routePath);

    return TRUE;
}

void CWebView2ModalDialog::OnSize(UINT nType, int cx, int cy)
{
    CZcUiDialog::OnSize(nType, cx, cy);
    if (m_host) {
        RECT bounds;
        GetClientRect(&bounds);
        m_host->SetBounds(bounds);
    }
}

void CWebView2ModalDialog::OnDestroy()
{
    if (m_host) {
        m_host->Close();
        m_host.reset();
    }
    m_proxy.reset();
    m_ownedHandler.reset();
    CZcUiDialog::OnDestroy();
}

void CWebView2ModalDialog::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
    CZcUiDialog::OnActivate(nState, pWndOther, bMinimized);
    if (nState != WA_INACTIVE && m_host && m_host->GetController()) {
        m_host->GetController()->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void CWebView2ModalDialog::OnSetFocus(CWnd* pOldWnd)
{
    CZcUiDialog::OnSetFocus(pOldWnd);
    if (m_host && m_host->GetController()) {
        m_host->GetController()->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

BOOL CWebView2ModalDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) {
        EndDialog(IDCANCEL);
        return TRUE;
    }

    // Let WebView handle keyboard and mouse
    if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
        return FALSE;
    if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
        return FALSE;

    return CZcUiDialog::PreTranslateMessage(pMsg);
}

void CWebView2ModalDialog::SetMessageHandler(std::unique_ptr<IMessageHandler> handler)
{
    m_ownedHandler = std::move(handler);
}
