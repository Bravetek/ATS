
#pragma once
#include <windows.h>
#include <string>

class ProcHelper {
public:
    ProcHelper() { ZeroMemory(&m_pi, sizeof(m_pi)); }
    ~ProcHelper() { /*不自動 Terminate，交由呼叫端決定*/ }

    // 以整串命令列啟動，例如：L"\"C:\\Python\\python.exe\" -u \"...\\main.py\" --plan ... --out ..."
    bool Launch(const std::wstring& cmdline);

    bool IsRunning() const;
    void Terminate();
    HANDLE Handle() const { return m_pi.hProcess; }

private:
    PROCESS_INFORMATION m_pi{};
};
