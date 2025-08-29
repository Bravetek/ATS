#include "pch.h"          // ← 專案的 PCH
#include "ProcHelper.h"
#include <vector>

bool ProcHelper::Launch(const std::wstring& cmdline) {
    if (IsRunning()) return true;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    ZeroMemory(&m_pi, sizeof(m_pi));

    // CreateProcess 會就地修改命令列，所以要丟「可寫的」緩衝區
    std::vector<wchar_t> buf(cmdline.begin(), cmdline.end());
    buf.push_back(L'\0');

    BOOL ok = CreateProcessW(
        /*lpApplicationName*/ NULL,
        /*lpCommandLine   */  buf.data(),     // 可修改
        /*lpProcessAttributes*/ NULL,
        /*lpThreadAttributes */ NULL,
        /*bInheritHandles*/ FALSE,
        /*dwCreationFlags*/   CREATE_NO_WINDOW,
        /*lpEnvironment*/     NULL,
        /*lpCurrentDirectory*/NULL,
        &si, &m_pi
    );
    return ok == TRUE;
}

bool ProcHelper::IsRunning() const {
    if (!m_pi.hProcess) return false;
    DWORD code = 0;
    if (GetExitCodeProcess(m_pi.hProcess, &code)) {
        return code == STILL_ACTIVE;
    }
    return false;
}

void ProcHelper::Terminate() {
    if (m_pi.hProcess) {
        TerminateProcess(m_pi.hProcess, 0);
        CloseHandle(m_pi.hThread);
        CloseHandle(m_pi.hProcess);
        ZeroMemory(&m_pi, sizeof(m_pi));
    }
}