
#pragma once
#include "resource.h"
#include "ProcHelper.h"
#include <map>
#include <vector>

struct SeriesKey { double i1; double i23; bool operator<(const SeriesKey& o) const { return i1==o.i1 ? i23<o.i23 : i1<o.i1; } };
struct PtF { double x, y; };  // ← 浮點座標
//struct Serial_value { double vin; double i1; double i23; double value; };

class CMainDlg : public CDialogEx {
public:
    CMainDlg(): CDialogEx(IDD_MAINDLG) {}
protected:
    virtual BOOL OnInitDialog();
    afx_msg void OnBnBrowsePy(); afx_msg void OnBnBrowseMain(); afx_msg void OnBnBrowsePlan();
    afx_msg void OnBnStart();
    afx_msg void OnTimer(UINT_PTR id);
    DECLARE_MESSAGE_MAP()
private:
    CString GetText(int id); void SetText(int id, const CString& s); void Log(const CString& s);
    void DrawChart();

    void DrawChartI23();

    void ClearChart();
    void LoadProgress();
    void TailEvents();
    CString m_runDir; ProcHelper m_proc; int m_progress=0;
    // chart data
    //std::map<SeriesKey, std::vector<CPoint>> m_series;
    std::map<SeriesKey, std::vector<PtF>> m_series;   // ← 用 PtF 取代 CPoint
    std::map<SeriesKey, COLORREF> m_colors;
    long long m_eventsRead=0; // bytes read
};
