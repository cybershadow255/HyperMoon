// ================================================================
//  HyperMon - Setup Launcher & HVCI Checker
//  Compile with: cl setup.cpp /Fe:setup.exe /W4 /D_CRT_SECURE_NO_WARNINGS
// ================================================================

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <string>
#include <winternl.h>

#include "../driver/shared.h"
#include "mapper.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool IsHvciEnabled() {
    DWORD data = 0;
    DWORD size = sizeof(data);
    // Registry key for Memory Integrity (HVCI)
    LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
        L"Enabled", RRF_RT_REG_DWORD, NULL, &data, &size);

    if (status != ERROR_SUCCESS) return false;
    return data == 1;
}

void OpenHvciSettings() {
    std::wcout << L"[*] Oeffne Kernisolierungseinstellungen... Bitte 'Speicherintegritaet' ausschalten." << std::endl;
    ShellExecuteW(NULL, L"open", L"ms-settings:windowsdefender-coreisolation", NULL, NULL, SW_SHOWNORMAL);
}

bool SetRunOnce() {
    HKEY hKey;
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);

    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce", 0, KEY_WRITE, &hKey);
    if (status != ERROR_SUCCESS) return false;

    status = RegSetValueExW(hKey, L"HyperMonSetup", 0, REG_SZ, (BYTE*)szPath, (wcslen(szPath) + 1) * sizeof(WCHAR));
    RegCloseKey(hKey);
    return status == ERROR_SUCCESS;
}

// ----------------------------------------------------------------
//  Driver Loading (Simple Service for now, Manual Mapping later)
// ----------------------------------------------------------------

bool UnloadDriver() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return false;

    SC_HANDLE hService = OpenServiceW(hSCM, L"HyperMon", SERVICE_ALL_ACCESS);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }

    SERVICE_STATUS status;
    BOOL ok = ControlService(hService, SERVICE_CONTROL_STOP, &status);
    ok &= DeleteService(hService);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return ok == TRUE;
}

bool LoadDriver(const std::wstring& driverPath) {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return false;

    SC_HANDLE hService = CreateServiceW(hSCM, L"HyperMon", L"HyperMon Kernel Monitor",
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        driverPath.c_str(), NULL, NULL, NULL, NULL, NULL);

    if (!hService) {
        if (GetLastError() == ERROR_SERVICE_EXISTS) {
            hService = OpenServiceW(hSCM, L"HyperMon", SERVICE_ALL_ACCESS);
        }
    }

    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }

    BOOL ok = StartService(hService, 0, NULL);
    if (!ok && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) ok = TRUE;

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    return ok == TRUE;
}

// ----------------------------------------------------------------
//  Main logic
// ----------------------------------------------------------------

int main() {
    if (!IsAdmin()) {
        std::cerr << "[-] Bitte als Administrator ausfuehren!" << std::endl;
        return 1;
    }

    std::cout << "--- HyperMon Setup & Schutz ---" << std::endl;

    if (IsHvciEnabled()) {
        std::cout << "[!] Kernisolierung (HVCI) ist aktiv." << std::endl;
        std::cout << "[*] Um den Treiber ohne Neustart zu laden, muss HVCI einmalig deaktiviert werden." << std::endl;

        if (SetRunOnce()) {
            std::cout << "[+] Persistenz eingerichtet. Programm startet nach Neustart automatisch." << std::endl;
        }

        OpenHvciSettings();

        std::cout << "\nBitte schalte 'Speicherintegritaet' aus und starte den PC neu." << std::endl;
        std::cout << "Druecke Enter zum Beenden..." << std::endl;
        std::cin.get();
        return 0;
    }

    std::cout << "[+] HVCI ist deaktiviert. Lade Treiber..." << std::endl;

    WCHAR szPath[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, szPath);
    std::wstring driverPath = std::wstring(szPath) + L"\\HyperMon.sys";

    if (!LoadDriver(driverPath)) {
        std::cout << "[*] Standard-Laden fehlgeschlagen. Versuche Manual Mapping..." << std::endl;
        if (!ManualMapper::MapDriver(driverPath)) {
             std::cerr << "[-] Treiber konnte nicht geladen werden (Error: " << GetLastError() << ")." << std::endl;
             std::cerr << "    Ist Secure Boot deaktiviert?" << std::endl;
             return 1;
        }
    }

    std::cout << "[+] Treiber geladen. Starte Schutz fuer 10 Sekunden..." << std::endl;
    std::cout << "[!] INFO: Der Treiber deinstalliert sich danach automatisch." << std::endl;

    HANDLE hDev = CreateFileW(HYPERMON_USER_PATH, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDev != INVALID_HANDLE_VALUE) {
        DWORD pid = GetCurrentProcessId();
        DWORD bytesRet = 0;
        if (DeviceIoControl(hDev, IOCTL_HM_PROTECT_PID, &pid, sizeof(pid), NULL, 0, &bytesRet, NULL)) {
            std::cout << "[+] Prozess-Schutz aktiv (PID: " << pid << ")." << std::endl;
        } else {
            std::cerr << "[-] IOCTL fehlgeschlagen." << std::endl;
        }

        std::cout << "[*] Starte Main-Programm (Simulation)..." << std::endl;
        // Hier würde man normalerweise das Ring-3 Programm starten

        for (int i = 10; i > 0; --i) {
            std::cout << "\rSchutz aktiv: " << i << "s verbleibend... " << std::flush;
            Sleep(1000);
        }
        std::cout << "\n[+] 10 Sekunden abgelaufen." << std::endl;

        CloseHandle(hDev);
    }

    std::cout << "[*] Deinstalliere Treiber..." << std::endl;
    if (UnloadDriver()) {
        std::cout << "[+] Treiber erfolgreich deinstalliert." << std::endl;
    } else {
        std::cout << "[!] Hinweis: Treiber konnte nicht via SCM deinstalliert werden (vllt. via Manual Mapping geladen)." << std::endl;
    }

    return 0;
}
