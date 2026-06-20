#include "virtualcam/FilterRegistrar.h"

#ifdef _WIN32

#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

// Call DllRegisterServer / DllUnregisterServer directly in this process.
// This avoids regsvr32.exe, which runs from System32 and cannot find the GCC
// runtime DLLs next to our exe. Running in-process, our application directory
// (bin/) is on the DLL search path so all dependencies resolve. Non-elevated
// writes to HKEY_CLASSES_ROOT are redirected by Windows to
// HKCU\Software\Classes — no UAC required — and DirectShow reads that through
// the HKCR merge, so OBS / Zoom / Meet all find the filter automatically.
static bool runInProcess(const QString &dllPath, bool unregister) {
    auto wPath = reinterpret_cast<const wchar_t *>(dllPath.utf16());

    HMODULE hDll = LoadLibraryW(wPath);
    if (!hDll) {
        VC_ERROR("LoadLibrary failed for {}: error {}", dllPath.toStdString(), GetLastError());
        return false;
    }

    const char *fnName = unregister ? "DllUnregisterServer" : "DllRegisterServer";
    using RegFn = HRESULT(STDAPICALLTYPE *)();
    auto fn = reinterpret_cast<RegFn>(GetProcAddress(hDll, fnName));
    if (!fn) {
        VC_ERROR("GetProcAddress({}) failed", fnName);
        FreeLibrary(hDll);
        return false;
    }

    HRESULT hr = fn();
    FreeLibrary(hDll);

    if (FAILED(hr)) {
        VC_ERROR("{}() returned HRESULT 0x{:08X}", fnName, static_cast<unsigned>(hr));
        return false;
    }
    VC_INFO("Filter {}registered successfully", unregister ? "un" : "");
    return true;
}

bool FilterRegistrar::registerFilter() {
    QString dll = filterDllPath();
    VC_INFO("Registering filter: {}", dll.toStdString());
    return runInProcess(dll, false);
}

bool FilterRegistrar::unregisterFilter() {
    QString dll = filterDllPath();
    VC_INFO("Unregistering filter: {}", dll.toStdString());
    return runInProcess(dll, true);
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
