#include "network/FirewallGuard.h"

#ifdef _WIN32

#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <netfw.h>
#include <objbase.h>
#include <shellapi.h>

#include <QCoreApplication>
#include <QDir>

namespace {

// Must match installer/viewcam_setup.iss (FirewallRuleName) and the beacon
// port (VIEWCAM_BEACON_PORT / vc::kBeaconPort).
const wchar_t kRuleName[] = L"ViewCam Studio (Discovery)";
constexpr int kDiscoveryPort = 8081;

QString exePath() {
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

// Tolerates COM already being initialized on this thread (Qt's Windows QPA
// plugin initializes OLE for drag-and-drop) — only tears down what we
// ourselves initialized.
struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope() {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
    bool ok() const { return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE; }
};

} // namespace

FirewallGuard::Status FirewallGuard::checkDiscoveryStatus() {
    ComScope com;
    if (!com.ok()) {
        VC_WARN("Firewall check: COM init failed (0x{:08X})", static_cast<unsigned>(com.hr));
        return Status::Unknown;
    }

    INetFwPolicy2 *policy = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_INetFwPolicy2, reinterpret_cast<void **>(&policy));
    if (FAILED(hr) || !policy) {
        VC_WARN("Firewall check: CoCreateInstance(NetFwPolicy2) failed (0x{:08X})",
                 static_cast<unsigned>(hr));
        return Status::Unknown;
    }

    long currentProfiles = 0;
    policy->get_CurrentProfileTypes(&currentProfiles);

    INetFwRules *rules = nullptr;
    hr = policy->get_Rules(&rules);
    if (FAILED(hr) || !rules) {
        policy->Release();
        return Status::Unknown;
    }

    BSTR name = SysAllocString(kRuleName);
    INetFwRule *rule = nullptr;
    hr = rules->Item(name, &rule);
    SysFreeString(name);

    // Item() failing (E_ELEMENT_NOT_FOUND) means no such rule — that's the
    // common "never approved" case, not an error worth surfacing as Unknown.
    Status result = Status::Blocked;
    if (SUCCEEDED(hr) && rule) {
        VARIANT_BOOL enabled = VARIANT_FALSE;
        NET_FW_RULE_DIRECTION dir = NET_FW_RULE_DIR_MAX;
        long profileMask = 0;
        BSTR appName = nullptr;

        rule->get_Enabled(&enabled);
        rule->get_Direction(&dir);
        rule->get_Profiles(&profileMask);
        rule->get_ApplicationName(&appName);

        const QString ruleApp = appName ? QString::fromWCharArray(appName) : QString();
        if (appName)
            SysFreeString(appName);

        const bool profileMatches = (profileMask & currentProfiles) != 0;
        const bool appMatches = ruleApp.compare(exePath(), Qt::CaseInsensitive) == 0;

        if (enabled == VARIANT_TRUE && dir == NET_FW_RULE_DIR_IN && profileMatches && appMatches)
            result = Status::Allowed;

        rule->Release();
    }

    rules->Release();
    policy->Release();
    return result;
}

bool FirewallGuard::requestApproval() {
    const QString exe = exePath();

    // Delete-then-add mirrors the installer's rule and makes retries
    // idempotent instead of piling up duplicate rules of the same name.
    const QString cmd =
        QStringLiteral("/c netsh advfirewall firewall delete rule name=\"ViewCam Studio (Discovery)\" "
                        ">nul 2>&1 & netsh advfirewall firewall add rule "
                        "name=\"ViewCam Studio (Discovery)\" dir=in action=allow protocol=UDP "
                        "localport=%1 program=\"%2\" profile=any")
            .arg(kDiscoveryPort)
            .arg(exe);
    const std::wstring params = cmd.toStdWString();

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        VC_INFO("Firewall fix: elevation declined or launch failed (error {})", GetLastError());
        return false;
    }

    if (!sei.hProcess)
        return false;

    WaitForSingleObject(sei.hProcess, 15000);
    DWORD exitCode = 1;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);

    VC_INFO("Firewall fix: netsh exited with code {}", exitCode);
    return exitCode == 0;
}

QString FirewallGuard::statusText(Status s) {
    switch (s) {
    case Status::Allowed: return QObject::tr("Allowed");
    case Status::Blocked: return QObject::tr("Blocked");
    case Status::Unknown: return QObject::tr("Unknown");
    }
    return QObject::tr("Unknown");
}

#endif // _WIN32
