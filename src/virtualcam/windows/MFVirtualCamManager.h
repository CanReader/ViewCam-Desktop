#pragma once
#ifdef _WIN32

// Manages the Windows 11 Virtual Camera device (IMFVirtualCamera).
// Call registerAndStart() once — it registers ViewCamMFSource.dll and
// creates the virtual camera device visible to all apps (Windows Camera,
// Teams, Zoom, Discord, OBS, etc.).  Call stop() on shutdown.
class MFVirtualCamManager {
public:
    MFVirtualCamManager();
    ~MFVirtualCamManager();

    // Register the MF source DLL and start the virtual camera device.
    // No-ops gracefully on Windows 10 (MFCreateVirtualCamera not available).
    void registerAndStart(const wchar_t *friendlyName = L"ViewCam Studio");

    void stop();
    bool isActive() const { return m_vcam != nullptr; }

private:
    void *m_vcam      = nullptr;   // IMFVirtualCamera* (opaque to avoid header dep)
    void *m_hMF       = nullptr;   // HMODULE mfsensorgroup.dll
    void *m_hSrcDll   = nullptr;   // HMODULE ViewCamMFSource.dll
    bool  m_mfStarted = false;     // true only when MFStartup succeeded
};

#endif // _WIN32
