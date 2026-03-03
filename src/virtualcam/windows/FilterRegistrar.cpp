#include "virtualcam/FilterRegistrar.h"

#ifdef _WIN32

#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <QCoreApplication>
#include <QDir>

// Must match the CLSID in VcamFilter.h / VcamFilter.cpp
static const wchar_t CLSID_STR[] = L"{8D0B7A5E-6C14-4F2D-9A7B-3E1D5C8F0A2B}";
static const wchar_t REG_KEY[]   = L"CLSID\\{8D0B7A5E-6C14-4F2D-9A7B-3E1D5C8F0A2B}\\InprocServer32";

static QString readRegistryDllPath() {
    HKEY hKey = nullptr;
    LONG rc = RegOpenKeyExW(HKEY_CLASSES_ROOT, REG_KEY, 0, KEY_READ, &hKey);
    if (rc != ERROR_SUCCESS)
        return {};

    wchar_t buf[MAX_PATH]{};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    rc = RegQueryValueExW(hKey, nullptr, nullptr, &type, reinterpret_cast<BYTE *>(buf), &size);
    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS || type != REG_SZ)
        return {};

    return QString::fromWCharArray(buf);
}

static QString filterDllPath() {
    // The DLL is expected next to the main executable
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir::toNativeSeparators(appDir + "/ViewCamFilter.dll");
}

FilterRegistrar::Status FilterRegistrar::checkStatus() {
    QString regPath = readRegistryDllPath();
    if (regPath.isEmpty()) {
        return Status::NotRegistered;
    }

    QString expected = filterDllPath();
    if (regPath.compare(expected, Qt::CaseInsensitive) == 0) {
        return Status::Registered;
    }

    return Status::RegisteredStale;
}

QString FilterRegistrar::registeredDllPath() {
    return readRegistryDllPath();
}

QString FilterRegistrar::expectedDllPath() {
    return filterDllPath();
}

static bool runRegsvr32(const QString &dllPath, bool unregister) {
    QString args = unregister
        ? QString("/s /u \"%1\"").arg(dllPath)
        : QString("/s \"%1\"").arg(dllPath);

    SHELLEXECUTEINFOW sei{};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";         // Triggers UAC elevation
    sei.lpFile       = L"regsvr32.exe";
    sei.lpParameters = reinterpret_cast<const wchar_t *>(args.utf16());
    sei.nShow        = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            VC_WARN("User cancelled UAC prompt for filter {}registration",
                    unregister ? "un" : "");
        } else {
            VC_ERROR("ShellExecuteEx failed for regsvr32: error {}", err);
        }
        return false;
    }

    // Wait for regsvr32 to finish (should be fast)
    WaitForSingleObject(sei.hProcess, 30000);

    DWORD exitCode = 1;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);

    if (exitCode != 0) {
        VC_ERROR("regsvr32 exited with code {}", exitCode);
        return false;
    }

    VC_INFO("Filter {}registered successfully", unregister ? "un" : "");
    return true;
}

bool FilterRegistrar::registerFilter() {
    QString dll = filterDllPath();
    VC_INFO("Registering filter: {}", dll.toStdString());
    return runRegsvr32(dll, false);
}

bool FilterRegistrar::unregisterFilter() {
    QString dll = filterDllPath();
    VC_INFO("Unregistering filter: {}", dll.toStdString());
    return runRegsvr32(dll, true);
}

QString FilterRegistrar::statusText(Status s) {
    switch (s) {
    case Status::Registered:      return QObject::tr("Installed");
    case Status::RegisteredStale: return QObject::tr("Path mismatch");
    case Status::NotRegistered:   return QObject::tr("Not installed");
    }
    return QObject::tr("Unknown");
}

#endif // _WIN32
