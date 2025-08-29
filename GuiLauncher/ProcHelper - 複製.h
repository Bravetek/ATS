
#pragma once
#include <windows.h>
#include <string>

class ProcHelper {
public:
    ProcHelper(): m_pi{} {}
    bool Launch(const std::wstring& cmdline) {
        if (IsRunning()) return true;
        STARTUPINFOW si{}; si.cb = sizeof(si);
        ZeroMemory(&m_pi, sizeof(m_pi));
        std::wstring cmd = cmdline;
        BOOL ok = CreateProcessW(NULL, cmd.data(), NULL, NULL, FALSE,
            CREATE_NO_WINDOW, NULL, NULL, &si, &m_pi);
        return ok == TRUE;
    }
    bool IsRunning() const {
        if (!m_pi.hProcess) return false;
        DWORD code=0; if (GetExitCodeProcess(m_pi.hProcess, &code)) return code==STILL_ACTIVE;
        return false;
    }
    void Terminate() {
        if (m_pi.hProcess) { TerminateProcess(m_pi.hProcess, 0); CloseHandle(m_pi.hThread); CloseHandle(m_pi.hProcess); ZeroMemory(&m_pi, sizeof(m_pi)); }
    }
private:
    PROCESS_INFORMATION m_pi;
};
