
#include "pch.h"
#include "MainDlg.h"
#include <shlwapi.h>
#include <shellapi.h>
#include <algorithm>  // std::sort
#pragma comment(lib, "Shlwapi.lib")
#include "resource.h"

#define debug_mode

#ifdef debug_mode
    CString debug_path = L"runs\\Test";
#endif

BEGIN_MESSAGE_MAP(CMainDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_BROWSE_PY, &CMainDlg::OnBnBrowsePy)
    ON_BN_CLICKED(IDC_BTN_BROWSE_MAIN, &CMainDlg::OnBnBrowseMain)
    ON_BN_CLICKED(IDC_BTN_BROWSE_PLAN, &CMainDlg::OnBnBrowsePlan)
    ON_BN_CLICKED(IDC_BTN_START, &CMainDlg::OnBnStart)
    ON_WM_TIMER()
END_MESSAGE_MAP()

//static CString ReadAllText(const CString& path) {
//    CStdioFile f; CString s, line;
//    if (!f.Open(path, CFile::modeRead | CFile::typeText)) return L"";
//    while (f.ReadString(line)) { s += line; s += L"\n"; }
//    return s;
//}

static CString ReadAllText(const CString& path) {   //for "[Errno 13] Permission denied: 'runs\\\\Test\\\\progress.json'"
    HANDLE h = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return L"";
    std::string buf; buf.reserve(4096);
    char tmp[4096]; DWORD got = 0;
    while (ReadFile(h, tmp, sizeof(tmp), &got, NULL) && got > 0) { buf.append(tmp, tmp + got); }
    CloseHandle(h);
    if (buf.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)buf.size(), NULL, 0);
    CString out; wchar_t* pw = out.GetBuffer(wlen);
    MultiByteToWideChar(CP_UTF8, 0, buf.data(), (int)buf.size(), pw, wlen);
    out.ReleaseBuffer(wlen);
    return out;
}

static bool FileExists(const CString& path) { return PathFileExistsW(path); }

BOOL CMainDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    
    //if (auto p = GetDlgItem(IDC_STATIC_GRAPH)) {
    //    p->ShowWindow(SW_SHOW);
    //    p->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // 浮到最上層
    //    CRect r; p->GetWindowRect(&r); ScreenToClient(&r);
    //    TRACE(L"GRAPH rc=(%d,%d,%d,%d)\n", r.left, r.top, r.right, r.bottom);
    //}
    //else {
    //    AfxMessageBox(L"找不到 IDC_STATIC_GRAPH（可能是 RC ID 寫成 IDC_STATIC=-1）");
    //}
    
    SetText(IDC_EDIT_PYTHON, L"python.exe");
    SetText(IDC_EDIT_MAINPY, L"..\\backend\\main.py");
    SetText(IDC_EDIT_PLAN,   L"..\\backend\\plan.yaml");
    SetTimer(1, 500, NULL);



    return TRUE;
}

CString CMainDlg::GetText(int id) { CString s; GetDlgItemText(id, s); return s; }
void CMainDlg::SetText(int id, const CString& s) { SetDlgItemText(id, s); }
void CMainDlg::Log(const CString& s) {
    auto lb = (CListBox*)GetDlgItem(IDC_LIST_LOG);
    lb->AddString(s); lb->SetCurSel(lb->GetCount()-1);
}

void CMainDlg::OnBnBrowsePy() { CFileDialog d(TRUE, L"exe", NULL, OFN_FILEMUSTEXIST, L"Executable (*.exe)|*.exe||"); if (d.DoModal()==IDOK) SetText(IDC_EDIT_PYTHON, d.GetPathName()); }
void CMainDlg::OnBnBrowseMain() { CFileDialog d(TRUE, L"py", NULL, OFN_FILEMUSTEXIST, L"Python (*.py)|*.py||"); if (d.DoModal()==IDOK) SetText(IDC_EDIT_MAINPY, d.GetPathName()); }
void CMainDlg::OnBnBrowsePlan() { CFileDialog d(TRUE, L"yaml", NULL, OFN_FILEMUSTEXIST, L"YAML/JSON (*.yaml;*.yml;*.json)|*.yaml;*.yml;*.json||"); if (d.DoModal()==IDOK) SetText(IDC_EDIT_PLAN, d.GetPathName()); }

void CMainDlg::OnBnStart() {
    // create run folder
    SYSTEMTIME st; GetLocalTime(&st);
    CString run; run.Format(L"runs\\%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    //CreateDirectoryW(L"runs", NULL); CreateDirectoryW(run, NULL);

#ifdef debug_mode
    run = debug_path;
#else
    CreateDirectoryW(L"runs", NULL); CreateDirectoryW(run, NULL);
#endif
    m_runDir = run;
    // copy plan
    CString plan = GetText(IDC_EDIT_PLAN);
    CString dst = run + L"\\plan.yaml"; 
#ifndef debug_mode
    CopyFileW(plan, dst, FALSE);
#endif

    // launch python
    CString py = GetText(IDC_EDIT_PYTHON), mainpy = GetText(IDC_EDIT_MAINPY);
    CString cmd; cmd.Format(L"\"%s\" -u \"%s\" --plan \"%s\" --out \"%s\"", py.GetString(), mainpy.GetString(), dst.GetString(), run.GetString());
    
    std::wstring wcmd = cmd.GetString();
    std::vector<wchar_t> buf(wcmd.begin(), wcmd.end());
    buf.push_back(L'\0');

    //clear drawing
    ClearChart();


    //if (m_proc.Launch(cmd)) 
    if (m_proc.Launch(std::wstring(buf.data()))) 
    { Log(L"Backend started"); 
    } else { Log(L"Launch failed"); }
    
    
    m_progress = 0;
    m_series.clear(); m_colors.clear(); m_eventsRead = 0;
}

void CMainDlg::OnTimer(UINT_PTR) {
    LoadProgress();
    TailEvents();
    DrawChart();
    DrawChartI23();
}

void CMainDlg::LoadProgress() {
    CString p = m_runDir + L"\\progress.json";
    if (!FileExists(p)) return;
    CString s = ReadAllText(p);
    // naive parse percent
    int pos = s.Find(L"\"percent\""); if (pos >= 0) {
        pos = s.Find(L":", pos); if (pos>0) {
            int start = pos + 1;

            //int end = s.FindOneOf(L",}\r\n", pos+1);
            // 跳過空白
            while (start < s.GetLength() && wcschr(L" \t\r\n", s[start]) != nullptr) ++start;
            // 對「從 start 開始的子字串」做 FindOneOf，然後把相對位移加回去
            int rel = s.Mid(start).FindOneOf(L",}] \r\n");
            int end = (rel < 0) ? s.GetLength() : start + rel;

            //CString num = s.Mid(pos+1, end-pos-1); num.Trim();
            CString num = s.Mid(start, end - start);
            num.Trim();
            
            int val = _ttoi(num);
            if (val != m_progress) {
                m_progress = val;
                CProgressCtrl* pr = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS);
                pr->SetRange(0,100); pr->SetPos(m_progress);
                CString msg; msg.Format(L"Progress: %d%%", val); Log(msg);
            }
        }
    }
}

void CMainDlg::TailEvents() {
    CString path = m_runDir + L"\\events.ndjson";
    if (!FileExists(path)) return;

    //HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER size; GetFileSizeEx(h, &size);
    if (m_eventsRead > size.QuadPart) m_eventsRead = 0; // 檔案被覆寫，從頭來

    LARGE_INTEGER mv; mv.QuadPart = m_eventsRead;   
    SetFilePointerEx(h, mv, NULL, FILE_BEGIN);  //移動檔案指標

    std::string buffer;
    char buf[4096]; DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got > 0) {
        buffer.append(buf, buf + got);
    }
    CloseHandle(h);

#ifdef debug_mode
    struct Serial_value { double vin; double i1; double i23; double value; };
    Serial_value _sv {};
#endif
   
    size_t start = 0;
    size_t consumed = 0; // ★ 真正消耗的位元組數（只算到最後一個換行）
    while (true) {
        size_t nl = buffer.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = buffer.substr(start, nl - start);
        consumed = nl + 1;           // 只推進到最後完整一行的下一個位元組
        start = nl + 1;

//#ifdef debug_mode
//        double value

        // --- parse line (同你現有的) ---
        // old version
        //if (line.find("\"type\":\"kpi\"") != std::string::npos) {
        //    double vin = 0, i1 = 0, i23 = 0, val = 0;
        //    auto get = [&](const char* k)->double {
        //        size_t p = line.find(k); if (p == std::string::npos) return 0;
        //        p = line.find(':', p); if (p == std::string::npos) return 0;
        //        size_t e = line.find_first_of(",}\r", p + 1); return atof(line.substr(p + 1, e - p - 1).c_str());
        //        };
        //    vin = get("\"vin\""); i1 = get("\"i_ch1\""); i23 = get("\"i23\""); val = get("\"value\"");
        //    SeriesKey key{ i1, i23 };
        //    if (m_colors.find(key) == m_colors.end()) {
        //        static COLORREF table[] = { RGB(200,0,0), RGB(0,120,0), RGB(0,0,200), RGB(200,120,0), RGB(120,0,200) };
        //        m_colors[key] = table[m_colors.size() % _countof(table)];
        //    }
        //    m_series[key].push_back(CPoint((int)(vin * 100), (int)(val * 100)));
        //    CString msg; msg.Format(L"KPI eff vin=%.2f i1=%.3f i23=%.3f val=%.3f", vin, i1, i23, val);
        //    Log(msg);
        //}
        auto get_num = [&](std::initializer_list<const char*> keys)->double {
            for (auto k : keys) {
                size_t p = line.find(k);
                if (p != std::string::npos) {
                    p = line.find(':', p);
                    if (p != std::string::npos) {
                        size_t e = line.find_first_of(",}\r\n", p + 1);
                        return atof(line.substr(p + 1, e - (p + 1)).c_str());
                    }
                }
            }
            return 0.0;
            };
        if (line.find("\"type\": \"kpi\"") != std::string::npos) { //important space after type :" "//
            double vin = get_num({ "\"vin\"", "\"Vin\"" });
            double i1 = get_num({ "\"i_ch1\"", "\"i1\"", "\"I1\"" });
            double i23 = get_num({ "\"i23\"", "\"I23\"" });
            double val = get_num({ "\"value\"", "\"eff\"", "\"efficiency\"" });

            // ★ 正規化效率：若像 85% 請轉成 0.85；若 >150% 或 <0 視為無效
            double eta = val;
            if (eta > 5.0 && eta <= 150.0) eta = eta / 100.0; // 以百分比傳來
            if (eta < 0.0 || eta > 1.5) {
                CString warn; warn.Format(L"[skip] weird efficiency: %.6f @ Vin=%.3f I1=%.3f I23=%.3f", val, vin, i1, i23);
                Log(warn);
                return; // 丟掉異常點，避免把圖尺度撐爆
            }

            SeriesKey key{ i1, i23 };
#ifdef debug_mode
            Serial_value sv1 { vin,i1, i23, val };
            _sv = sv1;
#endif
            if (m_colors.find(key) == m_colors.end()) {
                static COLORREF table[] = { RGB(200,0,0), RGB(0,120,0), RGB(0,0,200), RGB(200,120,0), RGB(120,0,200) };
                m_colors[key] = table[m_colors.size() % _countof(table)];
            }
            m_series[key].push_back(PtF{ vin, eta }); // ★ 存浮點，不再乘以 100

            CString msg; msg.Format(L"KPI eff vin=%.2f i1=%.3f i23=%.3f eff=%.4f", vin, i1, i23, eta);
            Log(msg);
        }
        else if (line.find("\"type\":\"log\"") != std::string::npos) {
            size_t p = line.find("\"msg\""); if (p != std::string::npos) {
                p = line.find(':', p); size_t q = line.find('"', p + 2); size_t r = line.find('"', q + 1);
                CString m = CString(line.substr(q + 1, r - q - 1).c_str()); Log(m);
            }
        }
        else if (line.find("\"type\":\"done\"") != std::string::npos) {
            Log(L"Job done");
        }

        //debug message -------------------------------------------------------
        SYSTEMTIME st;
        GetLocalTime(&st);
        CString s;  s.Format(L"%02d:%02d.%03d :", st.wMinute, st.wSecond, st.wMilliseconds);
        CString k;
        k.Format(L"[debug] series (vin=%.3f, i1=%.3f,i23=%.3f) value=%.3f",
            _sv.vin, _sv.i1, _sv.i23, _sv.value);

        if (line.find("\"type\": \"kpi\"") != std::string::npos)  //important space after type :" "//
            OutputDebugString(s + k + L"\r\n");

            //for (auto& kv : m_series) {
            //    //CString k; k.Format(L"[debug] series (vin=%.3f, i1=%.3f,i23=%.3f) size=%d, ",
            //    //    kv.first.i1, kv.first.i23, (int)kv.second.size() );
            //    //OutputDebugString(k + L"\r\n");

            //    k.Format(L"[debug] series (vin=%.3f, i1=%.3f,i23=%.3f) value=%.3f, ",
            //        _sv.vin, _sv.i1, _sv.i23, _sv.value);
            //    OutputDebugString(s + k + L"\r\n");
            //    //TRACE(s + k + L"\r\n");
            //}
    }

    // ★ 只把 m_eventsRead 前進到「處理完的最後一個換行」
    m_eventsRead += consumed;


    //oldversion ------------------------------------------
    //CString path = m_runDir + L"\\events.ndjson";
    //if (!FileExists(path)) return;
    //HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    //if (h==INVALID_HANDLE_VALUE) return;
    //LARGE_INTEGER size; GetFileSizeEx(h, &size);
    //LARGE_INTEGER move; move.QuadPart = m_eventsRead;
    //SetFilePointerEx(h, move, NULL, FILE_BEGIN);
    //const DWORD BUFSZ=4096; char buf[BUFSZ+1]; DWORD got=0;
    //std::string chunk;
    //while (ReadFile(h, buf, BUFSZ, &got, NULL) && got>0) {
    //    chunk.append(buf, buf+got);
    //    size_t pos=0;
    //    while (true) {
    //        size_t nl = chunk.find('\n', pos);
    //        if (nl==std::string::npos) break;
    //        std::string line = chunk.substr(pos, nl-pos);
    //        // very naive parse of KPI line
    //        if (line.find("\"type\":\"kpi\"")!=std::string::npos) {
    //            double vin=0,i1=0,i23=0,val=0;
    //            auto get = [&](const char* k)->double{
    //                size_t p=line.find(k); if(p==std::string::npos) return 0;
    //                p=line.find(':',p); if(p==std::string::npos) return 0;
    //                size_t e=line.find_first_of(",}\r",p+1); return atof(line.substr(p+1,e-p-1).c_str());
    //            };
    //            vin=get("\"vin\""); i1=get("\"i_ch1\""); i23=get("\"i23\""); val=get("\"value\"");
    //            // map to series
    //            SeriesKey key{ i1, i23 };
    //            COLORREF color;
    //            if (m_colors.find(key)==m_colors.end()) {
    //                static COLORREF table[] = { RGB(200,0,0), RGB(0,120,0), RGB(0,0,200), RGB(200,120,0), RGB(120,0,200) };
    //                color = table[m_colors.size()%5];
    //                m_colors[key]=color;
    //            } else color = m_colors[key];
    //            // push point (vin, val)
    //            m_series[key].push_back( CPoint((int)(vin*100), (int)(val*100)) ); // scale
    //            CString msg; msg.Format(L"KPI eff vin=%.2f i1=%.3f i23=%.3f val=%.3f", vin,i1,i23,val);
    //            Log(msg);
    //        } else if (line.find("\"type\":\"log\"")!=std::string::npos) {
    //            // extract msg
    //            size_t p=line.find("\"msg\""); if(p!=std::string::npos){ p=line.find(':',p); size_t q=line.find('"',p+2); size_t r=line.find('"',q+1);
    //                CString m = CString(line.substr(q+1,r-q-1).c_str()); Log(m);
    //            }
    //        } else if (line.find("\"type\":\"done\"")!=std::string::npos) {
    //            Log(L"Job done");
    //        }
    //        pos = nl+1;
    //    }
    //    m_eventsRead += got;
    //    chunk.erase(0, pos);
    //}
    //CloseHandle(h);
}

void CMainDlg::DrawChart() {

    ////new flow=========================================
    //CWnd* pView = GetDlgItem(IDC_STATIC_GRAPH);
    //if (!pView || !::IsWindow(pView->GetSafeHwnd())) return;

    //CRect rc; pView->GetClientRect(&rc);
    //CClientDC dc(pView);
    //CDC mem; mem.CreateCompatibleDC(&dc);
    //CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    //HGDIOBJ oldBmp = mem.SelectObject(&bmp);

    //mem.FillSolidRect(&rc, RGB(255, 255, 255));
    //mem.Rectangle(&rc);

    //if (m_series.empty()) { dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY); mem.SelectObject(oldBmp); return; }

    //// 1) bounds（x=Vin、y=eta，eta 預期 0..1）
    //double xmin = 1e9, xmax = -1e9, ymin = 1e9, ymax = -1e9;
    //for (auto& kv : m_series) for (auto& pt : kv.second) {
    //    xmin = min(xmin, pt.x); xmax = max(xmax, pt.x);
    //    ymin = min(ymin, pt.y); ymax = max(ymax, pt.y);
    //}
    //if (xmax <= xmin) { xmin -= 0.1; xmax += 0.1; }
    //if (ymax <= ymin) { ymin = 0.0; ymax = 1.0; }      // 預設效率範圍
    //// 小 padding
    //double xpad = (xmax - xmin) * 0.05; if (xpad <= 0) xpad = 0.1;
    //double ypad = (ymax - ymin) * 0.05; if (ypad <= 0) ypad = 0.05;
    //xmin -= xpad; xmax += xpad; ymin = max(0.0, ymin - ypad); ymax = min(1.5, ymax + ypad);

    //const int margin = 12;
    //const int w = max(1, rc.Width() - 2 * margin);
    //const int h = max(1, rc.Height() - 2 * margin);
    //auto X = [&](double x) { return rc.left + margin + int((x - xmin) / (xmax - xmin + 1e-12) * w); };
    //auto Y = [&](double y) { return rc.bottom - margin - int((y - ymin) / (ymax - ymin + 1e-12) * h); };

    //// 2) 畫系列 + 點
    //for (auto& kv : m_series) {
    //    COLORREF clr = m_colors[kv.first];
    //    CPen pen; pen.CreatePen(PS_SOLID, 2, clr);
    //    CPen* oldPen = mem.SelectObject(&pen);

    //    bool first = true;
    //    for (auto& p : kv.second) {
    //        int x = X(p.x), y = Y(p.y);
    //        if (first) { mem.MoveTo(x, y); first = false; }
    //        else { mem.LineTo(x, y); }
    //        // 點標記（讓單點也看得見）
    //        CBrush b(clr); CBrush* ob = mem.SelectObject(&b);
    //        mem.Ellipse(x - 3, y - 3, x + 3, y + 3);
    //        mem.SelectObject(ob);
    //    }
    //    mem.SelectObject(oldPen);
    //}

    //// 3) 顯示
    //dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    //mem.SelectObject(oldBmp);

    //oldversion************************
    // 1) 取得子控制項視窗與客戶區
    CWnd* pView = GetDlgItem(IDC_STATIC_GRAPH);
    if (!pView || !::IsWindow(pView->GetSafeHwnd())) return;

    CRect rc;
    pView->GetClientRect(&rc);        // ← 用子視窗的 client 區

    // 2) 用子視窗 DC + 雙緩衝，避免被覆蓋與閃爍
    CClientDC dc(pView);              // ← 對「子視窗」作畫（不是 this）
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    HGDIOBJ oldBmp = mem.SelectObject(&bmp);

    // 3) 背景與邊框
    mem.FillSolidRect(&rc, RGB(255, 255, 255));
    mem.Rectangle(&rc);

    if (m_series.empty()) {
        // 沒資料直接拷貝出去
        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
        mem.SelectObject(oldBmp);
        return;
    }

    // 4) 計算範圍
    double xmin = 1e9, xmax = -1e9, ymin = 1e9, ymax = -1e9;
    for (auto& kv : m_series) {
        for (auto& pt : kv.second) {
            xmin = min(xmin, pt.x / 100.0); xmax = max(xmax, pt.x / 100.0);
            ymin = min(ymin, pt.y / 100.0); ymax = max(ymax, pt.y / 100.0);
        }
    }
    if (xmax <= xmin) { xmin -= 1; xmax += 1; }
    if (ymax <= ymin) { ymin -= 1; ymax += 1; }

    // 邊界與對應函式
    const int margin = 12;
    const int w = max(1, rc.Width() - 2 * margin);
    const int h = max(1, rc.Height() - 2 * margin);
    auto X = [&](double x) { return rc.left + margin + int((x - xmin) / (xmax - xmin + 1e-12) * w); };
    auto Y = [&](double y) { return rc.bottom - margin - int((y - ymin) / (ymax - ymin + 1e-12) * h); };

    // 5) 畫每條序列（不同顏色）
    for (auto& kv : m_series) {
        COLORREF clr = RGB(0, 0, 0);
        auto it = m_colors.find(kv.first);
        if (it != m_colors.end()) clr = it->second;

        CPen pen; pen.CreatePen(PS_SOLID, 2, clr);
        CPen* pOldPen = mem.SelectObject(&pen);

        bool first = true; int px = 0, py = 0;
        for (auto& pt : kv.second) {
            int x = X(pt.x / 100.0);
            int y = Y(pt.y / 100.0);
            if (first) { mem.MoveTo(x, y); first = false; }
            else { mem.LineTo(x, y); }
            px = x; py = y;

            // ★ 畫點（3x3 小圓）
            CBrush b(clr); CBrush* oldB = mem.SelectObject(&b);
            mem.Ellipse(x - 2, y - 2, x + 2, y + 2);
            mem.SelectObject(oldB);
        }
        mem.SelectObject(pOldPen);
    }

    // 6) 顯示到畫面
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(oldBmp);

    //old code ---------------------------------------------
    //CRect rc; GetDlgItem(IDC_STATIC_GRAPH)->GetWindowRect(&rc); ScreenToClient(&rc);
    //CClientDC dc(this);

    //CBrush bg(RGB(255,255,255)); dc.FillRect(&rc, &bg); dc.Rectangle(&rc);
    //if (m_series.empty()) return;
    //// determine bounds
    //double xmin=1e9,xmax=-1e9,ymin=1e9,ymax=-1e9;
    //for (auto& kv: m_series) for (auto& pt: kv.second) {
    //    xmin=min(xmin, pt.x/100.0); xmax=max(xmax, pt.x/100.0);
    //    ymin=min(ymin, pt.y/100.0); ymax=max(ymax, pt.y/100.0);
    //}
    //if (ymax<=ymin) { ymin=0; ymax=1; }
    //int margin=10;
    //auto X=[&](double x){ return rc.left+margin + int((x-xmin)/(xmax-xmin+1e-9)*(rc.Width()-2*margin)); };
    //auto Y=[&](double y){ return rc.bottom-margin - int((y-ymin)/(ymax-ymin+1e-9)*(rc.Height()-2*margin)); };
    //for (auto& kv: m_series) {
    //    CPen pen(PS_SOLID, 2, m_colors[kv.first]); CPen* old=dc.SelectObject(&pen);
    //    bool first=true; int px=0, py=0;
    //    for (auto& pt: kv.second) {
    //        int x = X(pt.x/100.0), y = Y(pt.y/100.0);
    //        if (!first) dc.MoveTo(px,py), dc.LineTo(x,y); first=false; px=x; py=y;
    //    }
    //    dc.SelectObject(old);
    //}

    //test code-----------------------------------------
    //CWnd* w = GetDlgItem(IDC_STATIC_GRAPH);
    //if (!w) return;
    //CRect rc; w->GetClientRect(&rc);
    //CClientDC dc(w);                 // ✅ 對「子視窗」畫
    //CDC mem; mem.CreateCompatibleDC(&dc);
    //CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    //HGDIOBJ old = mem.SelectObject(&bmp);

    //mem.FillSolidRect(&rc, RGB(255, 255, 255));
    //mem.Rectangle(&rc);
    //// demo：畫一條對角線
    //mem.MoveTo(4, 4); mem.LineTo(rc.right - 4, rc.bottom - 4);

    //dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    //mem.SelectObject(old);
}

void CMainDlg::ClearChart()
{
    CWnd* p = GetDlgItem(IDC_STATIC_GRAPH);
    if (!p || !::IsWindow(p->GetSafeHwnd())) return;

    CRect rc;
    p->GetClientRect(&rc);

    // 在子視窗上畫（不是 this）
    CClientDC dc(p);

    // 填白底
    dc.FillSolidRect(&rc, RGB(255, 255, 255));

    // 可選：畫一圈 1px 黑色邊框
    CBrush frame; frame.CreateSolidBrush(RGB(0, 0, 0));
    dc.FrameRect(&rc, &frame);
}

void CMainDlg::DrawChartI23()
{
    // Right-side plot: X = i23, Y = efficiency (eta), series = VIN
    CWnd* pView = GetDlgItem(IDC_STATIC_GRAPH2);
    if (!pView || !::IsWindow(pView->GetSafeHwnd())) return;

    CRect rc; pView->GetClientRect(&rc);
    CClientDC dc(pView);
    CDC mem; mem.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    HGDIOBJ oldBmp = mem.SelectObject(&bmp);

    // background & frame
    mem.FillSolidRect(&rc, RGB(255, 255, 255));
    mem.Rectangle(&rc);

    if (m_series.empty()) {
        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
        mem.SelectObject(oldBmp);
        return;
    }

    struct XY { double x; double y; };
    std::map<double, std::vector<XY>> byVin;   // vin -> [(i23, eta)]
    std::map<double, COLORREF> colorVin;       // vin -> color

    double xmin = 1e9, xmax = -1e9, ymin = 1e9, ymax = -1e9;

    // m_series: key=(i1,i23), value=vector<PtF{ vin, eta }>
    for (auto& kv : m_series) {
        const double i23 = kv.first.i23;
        for (auto& pt : kv.second) {
            double vin = pt.x;
            double eta = pt.y;
            if (eta > 5.0 && eta <= 150.0) eta /= 100.0;  // accept percent form just in case
            if (eta < 0.0 || eta > 1.5) continue;

            byVin[vin].push_back({ i23, eta });

            if (i23 < xmin) xmin = i23;
            if (i23 > xmax) xmax = i23;
            if (eta < ymin) ymin = eta;
            if (eta > ymax) ymax = eta;

            if (colorVin.find(vin) == colorVin.end()) {
                static COLORREF table[] = { RGB(200,0,0), RGB(0,120,0), RGB(0,0,200), RGB(200,120,0), RGB(120,0,200) };
                int tlen = (int)(sizeof(table) / sizeof(table[0]));
                colorVin[vin] = table[colorVin.size() % tlen];
            }
        }
    }

    if (byVin.empty()) {
        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
        mem.SelectObject(oldBmp);
        return;
    }

    if (xmax <= xmin) { xmin -= 0.1; xmax += 0.1; }
    if (ymax <= ymin) { ymin = 0.0; ymax = 1.0; }
    double xpad = (xmax - xmin) * 0.05; if (xpad <= 0) xpad = 0.1;
    double ypad = (ymax - ymin) * 0.05; if (ypad <= 0) ypad = 0.05;
    xmin -= xpad; xmax += xpad;
    ymin = max(0.0, ymin - ypad); ymax = min(1.5, ymax + ypad);

    const int margin = 12;
    const int w = max(1, rc.Width() - 2 * margin);
    const int h = max(1, rc.Height() - 2 * margin);
    auto X = [&](double x) { return rc.left + margin + int((x - xmin) / (xmax - xmin + 1e-12) * w); };
    auto Y = [&](double y) { return rc.bottom - margin - int((y - ymin) / (ymax - ymin + 1e-12) * h); };

    // axes
    CPen axis; axis.CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    CPen* oAxis = mem.SelectObject(&axis);
    mem.MoveTo(rc.left + margin, rc.bottom - margin);  mem.LineTo(rc.right - margin, rc.bottom - margin); // X
    mem.MoveTo(rc.left + margin, rc.bottom - margin);  mem.LineTo(rc.left + margin, rc.top + margin);     // Y
    mem.SelectObject(oAxis);

    // Draw each VIN-series
    for (auto& it : byVin) {
        auto& pts = it.second;
        if (pts.empty()) continue;
        std::sort(pts.begin(), pts.end(), [](const XY& a, const XY& b) { return a.x < b.x; });

        COLORREF clr = colorVin[it.first];
        CPen pen; pen.CreatePen(PS_SOLID, 2, clr);
        CPen* oldPen = mem.SelectObject(&pen);

        bool first = true;
        for (auto& p : pts) {
            int x = X(p.x);
            int y = Y(p.y);
            if (first) { mem.MoveTo(x, y); first = false; }
            else { mem.LineTo(x, y); }
            // point marker
            CBrush b(clr); CBrush* ob = mem.SelectObject(&b);
            mem.Ellipse(x - 2, y - 2, x + 2, y + 2);
            mem.SelectObject(ob);
        }
        mem.SelectObject(oldPen);
    }

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(oldBmp);
}


// ==== Axis tick helpers ====
static double NiceNum(double range, bool roundToNearest)
{
    // From Graphics Gems: choose 1,2,5 * 10^k
    double expv = floor(log10(range > 1e-300 ? range : 1.0));
    double f = range / pow(10.0, expv);
    double nf;
    if (roundToNearest) {
        if (f < 1.5) nf = 1.0;
        else if (f < 3.0) nf = 2.0;
        else if (f < 7.0) nf = 5.0;
        else              nf = 10.0;
    }
    else {
        if (f <= 1.0) nf = 1.0;
        else if (f <= 2.0) nf = 2.0;
        else if (f <= 5.0) nf = 5.0;
        else               nf = 10.0;
    }
    return nf * pow(10.0, expv);
}

static void CalcTicks(double minv, double maxv, int maxTicks, double& tickStart, double& tickStep, int& nTicks)
{
    if (maxv < minv) { std::swap(maxv, minv); }
    double range = max(1e-12, maxv - minv);
    double step = NiceNum(range / max(1, maxTicks - 1), true);
    tickStart = floor(minv / step) * step;
    double tickEnd = ceil(maxv / step) * step;
    nTicks = int((tickEnd - tickStart) / step + 0.5) + 1;
    tickStep = step;
    if (nTicks <= 0) { nTicks = 1; }
}

static CString FormatTick(double v)
{
    CString s;
    double av = fabs(v);
    if (av >= 100.0)      s.Format(L"%.0f", v);
    else if (av >= 10.0)  s.Format(L"%.1f", v);
    else if (av >= 1.0)   s.Format(L"%.2f", v);
    else if (av >= 0.1)   s.Format(L"%.3f", v);
    else                  s.Format(L"%.4f", v);
    return s;
}
