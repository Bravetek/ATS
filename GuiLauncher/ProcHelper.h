
#pragma once
#include <windows.h>
#include <string>

class ProcHelper {
public:
    ProcHelper() { ZeroMemory(&m_pi, sizeof(m_pi)); }
    ~ProcHelper() { /*���۰� Terminate�A��ѩI�s�ݨM�w*/ }

    // �H���R�O�C�ҰʡA�Ҧp�GL"\"C:\\Python\\python.exe\" -u \"...\\main.py\" --plan ... --out ..."
    bool Launch(const std::wstring& cmdline);

    bool IsRunning() const;
    void Terminate();
    HANDLE Handle() const { return m_pi.hProcess; }

private:
    PROCESS_INFORMATION m_pi{};
};
