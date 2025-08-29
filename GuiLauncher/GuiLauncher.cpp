
#include "pch.h"
#include "MainDlg.h"

class CApp : public CWinApp {
public:
    BOOL InitInstance() override {
        CWinApp::InitInstance();
        CMainDlg dlg; m_pMainWnd=&dlg; dlg.DoModal(); return FALSE;
    }
} theApp;
