#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <unknwn.h>
#include <combaseapi.h>
#include <mfapi.h>

#include "virtualcam/MFVirtualCamDefs.h"
#include "MFVirtualCamManager.h"
#include "core/Logger.h"
#include <QCoreApplication>
#include <QDir>

// CLSID of ViewCamMFSource as a string — MFCreateVirtualCamera takes LPCWSTR, not GUID*
static const wchar_t CLSID_ViewCamMFSource_Str[] = L"{2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}";
// Binary form still needed for DllRegisterServer HKLM key lookup
static const GUID CLSID_ViewCamMFSource =
    {0x2c7b3a5f, 0x8d91, 0x4e2f, {0xb7, 0xc6, 0x1a, 0x9e, 0x3d, 0x0f, 0x4c, 0x8b}};

MFVirtualCamManager::MFVirtualCamManager() = default;

MFVirtualCamManager::~MFVirtualCamManager() {
    stop();
}

void MFVirtualCamManager::registerAndStart(const wchar_t *friendlyName) {
    if (m_vcam) return; // already running

    // MF runtime must be initialized before any MF API calls
    HRESULT hrMF = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    VC_INFO("MFStartup: 0x{:08X}", (unsigned)hrMF);
    if (FAILED(hrMF)) return;
    m_mfStarted = true;

    VC_INFO("Loading mfsensorgroup.dll…");
    // MFCreateVirtualCamera is in mfsensorgroup.dll (Windows 11 22H2+)
    m_hMF = LoadLibraryW(L"mfsensorgroup.dll");
    VC_INFO("mfsensorgroup.dll handle: {}", m_hMF ? "OK" : "NULL");
    if (!m_hMF) {
        VC_INFO("mfsensorgroup.dll not available — Windows Virtual Camera requires Win 11 22H2+");
        return;
    }
    auto pfnCreate = reinterpret_cast<PFN_MFCreateVirtualCamera>(
        GetProcAddress(static_cast<HMODULE>(m_hMF), "MFCreateVirtualCamera"));
    VC_INFO("MFCreateVirtualCamera ptr: {}", pfnCreate ? "found" : "NOT FOUND");
    if (!pfnCreate) {
        VC_INFO("MFCreateVirtualCamera not available — DirectShow only");
        return;
    }

    // --- Load and register ViewCamMFSource.dll in-process ---
    QString dllDir  = QCoreApplication::applicationDirPath();
    QString dllPath = QDir::toNativeSeparators(dllDir + "/ViewCamMFSource.dll");
    auto wPath      = reinterpret_cast<const wchar_t *>(dllPath.utf16());

    m_hSrcDll = LoadLibraryW(wPath);
    if (!m_hSrcDll) {
        VC_ERROR("MF virtual camera: failed to load {} (error {})",
                 dllPath.toStdString(), GetLastError());
        return;
    }

    // Register COM class in HKCU (no UAC)
    using RegFn = HRESULT(STDAPICALLTYPE *)();
    auto pfnReg = reinterpret_cast<RegFn>(
        GetProcAddress(static_cast<HMODULE>(m_hSrcDll), "DllRegisterServer"));
    if (pfnReg) {
        HRESULT hr = pfnReg();
        if (FAILED(hr))
            VC_WARN("MF source DllRegisterServer failed: 0x{:08X}", (unsigned)hr);
        else
            VC_INFO("ViewCamMFSource.dll registered (HKCU)");
    }

    // --- Create and start the Windows virtual camera device ---
    // Try AllUsers first (visible to all accounts); fall back to CurrentUser
    // if Start() returns E_ACCESSDENIED (no admin rights).
    struct { MFVirtualCameraAccess access; const char *label; } attempts[] = {
        { MFVirtualCameraAccess_AllUsers,   "AllUsers"   },
        { MFVirtualCameraAccess_CurrentUser, "CurrentUser" },
    };

    IMFVirtualCamera *vc = nullptr;
    for (auto &a : attempts) {
        IMFVirtualCamera *candidate = nullptr;
        HRESULT hr = pfnCreate(
            MFVirtualCameraType_SoftwareCameraSource,
            MFVirtualCameraLifetime_Session,
            a.access,
            friendlyName,
            CLSID_ViewCamMFSource_Str,
            nullptr, 0,
            &candidate);
        VC_INFO("MFCreateVirtualCamera({}): 0x{:08X}", a.label, (unsigned)hr);
        if (FAILED(hr)) continue;

        hr = candidate->Start(nullptr);
        VC_INFO("IMFVirtualCamera::Start({}): 0x{:08X}", a.label, (unsigned)hr);
        if (SUCCEEDED(hr)) {
            vc = candidate;
            break;
        }
        candidate->Remove();
        candidate->Release();
    }

    if (!vc) {
        VC_ERROR("IMFVirtualCamera: all access scopes failed — virtual camera unavailable");
        return;
    }

    m_vcam = vc;
    VC_INFO("Windows virtual camera '{}' started (visible to Windows Camera, Teams, Zoom…)",
            dllPath.toStdString());
}

void MFVirtualCamManager::stop() {
    if (m_vcam) {
        auto *vc = static_cast<IMFVirtualCamera *>(m_vcam);
        vc->Stop();
        vc->Remove();
        vc->Shutdown();
        vc->Release();
        m_vcam = nullptr;
        VC_INFO("Windows virtual camera stopped");
    }
    // MFShutdown drains all MF work queues — must happen BEFORE FreeLibrary
    // so that framework threads finish executing code in both DLLs.
    if (m_mfStarted) {
        MFShutdown();
        m_mfStarted = false;
    }
    if (m_hSrcDll) {
        FreeLibrary(static_cast<HMODULE>(m_hSrcDll));
        m_hSrcDll = nullptr;
    }
    if (m_hMF) {
        FreeLibrary(static_cast<HMODULE>(m_hMF));
        m_hMF = nullptr;
    }
}

#endif // _WIN32
