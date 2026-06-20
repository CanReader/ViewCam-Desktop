// VcamMFSource.cpp -- ViewCam Media Foundation Virtual Camera Source
//
// Compiled into ViewCamMFSource.dll.  Windows loads this DLL whenever any app
// (Windows Camera, Teams, Zoom, Discord...) opens the ViewCam virtual camera.
// Reads bottom-up BGR24 frames from the shared memory written by the main app
// and converts them to top-down NV12 (the format all modern camera consumers want).
//
// CLSID: {2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}

#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfobjects.h>
#include <propidl.h>
#include <new>

#include "virtualcam/SharedFrameProtocol.h"

// {2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}
DEFINE_GUID(CLSID_ViewCamMFSource,
    0x2c7b3a5f, 0x8d91, 0x4e2f, 0xb7, 0xc6, 0x1a, 0x9e, 0x3d, 0x0f, 0x4c, 0x8b);

static HMODULE       g_hModule  = nullptr;
static volatile LONG g_refCount = 0;

static constexpr UINT32 W        = ViewCam::DEFAULT_WIDTH;   // 1280
static constexpr UINT32 H        = ViewCam::DEFAULT_HEIGHT;  // 720
static constexpr UINT32 FPS      = ViewCam::DEFAULT_FPS;     // 30
static constexpr DWORD  NV12_SZ  = W * H * 3 / 2;

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while (false)

// MinGW defines neither GUID_NULL nor IID_NULL; supply our own all-zeros GUID.
static const GUID kNullGuid = {0, 0, 0, {0,0,0,0,0,0,0,0}};
#define GUID_NULL kNullGuid

// ---- RGB24 (BGR, bottom-up) to NV12 (top-down) conversion -------------------
// Input : width*height*3 bytes, rows stored bottom-to-top, each pixel B G R
// Output: NV12 top-down: Y plane (W*H) then interleaved UV half-res (W*H/2)
static void Bgr24BottomUpToNV12(const BYTE *bgr, BYTE *nv12) {
    BYTE *yp  = nv12;
    BYTE *uvp = nv12 + W * H;

    for (UINT32 row = 0; row < H; ++row) {
        const BYTE *src  = bgr + (H - 1 - row) * W * 3;  // flip row order
        BYTE       *dstY = yp + row * W;

        for (UINT32 col = 0; col < W; ++col) {
            int b = src[col * 3 + 0];
            int g = src[col * 3 + 1];
            int r = src[col * 3 + 2];

            // BT.601 limited range
            int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            dstY[col] = (BYTE)(y < 16 ? 16 : y > 235 ? 235 : y);

            // Chroma subsampled 2x2 -- use top-left sample of each block
            if ((row & 1) == 0 && (col & 1) == 0) {
                int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                UINT32 base     = (row / 2) * W + col;
                uvp[base]     = (BYTE)(u < 16 ? 16 : u > 240 ? 240 : u);
                uvp[base + 1] = (BYTE)(v < 16 ? 16 : v > 240 ? 240 : v);
            }
        }
    }
}

// ---- Forward declaration -------------------------------------------------------
class VcamMFSource;

// ---- VcamMFStream -------------------------------------------------------------
class VcamMFStream final : public IMFMediaStream {
public:
    VcamMFStream(VcamMFSource *src, IMFStreamDescriptor *sd);
    ~VcamMFStream();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_ref);
    }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (!r) delete this;
        return r;
    }

    // IMFMediaEventGenerator -- delegate to the event queue
    STDMETHODIMP GetEvent(DWORD f, IMFMediaEvent **e) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->GetEvent(f, e);
    }
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback *c, IUnknown *s) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->BeginGetEvent(c, s);
    }
    STDMETHODIMP EndGetEvent(IMFAsyncResult *r, IMFMediaEvent **e) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->EndGetEvent(r, e);
    }
    STDMETHODIMP QueueEvent(MediaEventType t, REFGUID g, HRESULT s, const PROPVARIANT *v) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->QueueEventParamVar(t, g, s, v);
    }

    // IMFMediaStream
    STDMETHODIMP GetMediaSource(IMFMediaSource **src) override;
    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor **sd) override;
    STDMETHODIMP RequestSample(IUnknown *token) override;

    void OnStart(LONGLONG pos);
    void OnStop();
    void OnShutdown();

private:
    void    EnsureHandles();
    HRESULT BuildAndQueueSample(IUnknown *token);

    volatile LONG        m_ref      = 1;
    VcamMFSource        *m_src;
    IMFStreamDescriptor *m_sd      = nullptr;
    IMFMediaEventQueue  *m_q       = nullptr;
    bool                 m_active   = false;
    bool                 m_shutdown = false;

    HANDLE  m_hMem    = nullptr;
    HANDLE  m_hMutex  = nullptr;
    HANDLE  m_hEvent  = nullptr;
    void   *m_mapped  = nullptr;
    UINT64  m_lastFrm = 0;
    UINT64  m_tick    = 0;
};

// ---- VcamMFSource ------------------------------------------------------------
class VcamMFSource final : public IMFMediaSource {
public:
    VcamMFSource();
    ~VcamMFSource();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_ref);
    }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = InterlockedDecrement(&m_ref);
        if (!r) delete this;
        return r;
    }

    // IMFMediaEventGenerator
    STDMETHODIMP GetEvent(DWORD f, IMFMediaEvent **e) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->GetEvent(f, e);
    }
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback *c, IUnknown *s) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->BeginGetEvent(c, s);
    }
    STDMETHODIMP EndGetEvent(IMFAsyncResult *r, IMFMediaEvent **e) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->EndGetEvent(r, e);
    }
    STDMETHODIMP QueueEvent(MediaEventType t, REFGUID g, HRESULT s, const PROPVARIANT *v) override {
        return m_shutdown ? MF_E_SHUTDOWN : m_q->QueueEventParamVar(t, g, s, v);
    }

    // IMFMediaSource
    STDMETHODIMP GetCharacteristics(DWORD *ch) override;
    STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor **pd) override;
    STDMETHODIMP Start(IMFPresentationDescriptor *pd, const GUID *tf, const PROPVARIANT *pos) override;
    STDMETHODIMP Stop() override;
    STDMETHODIMP Pause() override { return MF_E_INVALID_STATE_TRANSITION; }
    STDMETHODIMP Shutdown() override;

    VcamMFStream *GetStream() const { return m_stream; }

private:
    volatile LONG               m_ref      = 1;
    IMFMediaEventQueue         *m_q        = nullptr;
    IMFPresentationDescriptor  *m_pd       = nullptr;
    VcamMFStream               *m_stream   = nullptr;
    bool                        m_shutdown = false;
};

// ---- Class factory -----------------------------------------------------------
class VcamMFClassFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID iid, void **ppv) override {
        if (iid == IID_IUnknown || iid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef()  override { return 2; }  // static singleton
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP CreateInstance(IUnknown *outer, REFIID iid, void **ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto *s = new (std::nothrow) VcamMFSource();
        if (!s) return E_OUTOFMEMORY;
        HRESULT hr = s->QueryInterface(iid, ppv);
        s->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_refCount);
        else      InterlockedDecrement(&g_refCount);
        return S_OK;
    }
};
static VcamMFClassFactory g_classFactory;

// =============================================================================
// DLL entry points
// =============================================================================

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        // MFStartup is not called here — it must be called by the host process
        // before using any MF APIs. Calling it in DllMain risks loader-lock issues.
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **ppv) {
    if (clsid != CLSID_ViewCamMFSource)
        return CLASS_E_CLASSNOTAVAILABLE;
    return g_classFactory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() {
    return (g_refCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH]{};
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    static const wchar_t kRoot[]   =
        L"Software\\Classes\\CLSID\\{2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}";
    static const wchar_t kInproc[] =
        L"Software\\Classes\\CLSID\\{2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}\\InprocServer32";

    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRoot, 0, nullptr, 0, KEY_WRITE,
                        nullptr, &hk, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    const wchar_t *nm = L"ViewCam MF Source";
    RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE *)nm,
                   (DWORD)(wcslen(nm) + 1) * sizeof(wchar_t));
    RegCloseKey(hk);

    if (RegCreateKeyExW(HKEY_CURRENT_USER, kInproc, 0, nullptr, 0, KEY_WRITE,
                        nullptr, &hk, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE *)dllPath,
                   (DWORD)(wcslen(dllPath) + 1) * sizeof(wchar_t));
    const wchar_t *tm = L"Both";
    RegSetValueExW(hk, L"ThreadingModel", 0, REG_SZ, (const BYTE *)tm,
                   (DWORD)(wcslen(tm) + 1) * sizeof(wchar_t));
    RegCloseKey(hk);
    return S_OK;
}

STDAPI DllUnregisterServer() {
    RegDeleteTreeW(HKEY_CURRENT_USER,
        L"Software\\Classes\\CLSID\\{2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}");
    return S_OK;
}

// =============================================================================
// VcamMFStream
// =============================================================================

VcamMFStream::VcamMFStream(VcamMFSource *src, IMFStreamDescriptor *sd)
    : m_src(src), m_sd(sd)
{
    if (m_sd) m_sd->AddRef();
    MFCreateEventQueue(&m_q);
    InterlockedIncrement(&g_refCount);
}

VcamMFStream::~VcamMFStream() {
    if (m_mapped)  UnmapViewOfFile(m_mapped);
    if (m_hMem)    CloseHandle(m_hMem);
    if (m_hMutex)  CloseHandle(m_hMutex);
    if (m_hEvent)  CloseHandle(m_hEvent);
    SAFE_RELEASE(m_sd);
    SAFE_RELEASE(m_q);
    InterlockedDecrement(&g_refCount);
}

STDMETHODIMP VcamMFStream::QueryInterface(REFIID iid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (iid == IID_IUnknown || iid == IID_IMFMediaEventGenerator ||
        iid == IID_IMFMediaStream) {
        *ppv = static_cast<IMFMediaStream *>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP VcamMFStream::GetMediaSource(IMFMediaSource **src) {
    if (!src) return E_POINTER;
    if (m_shutdown) return MF_E_SHUTDOWN;
    *src = static_cast<IMFMediaSource *>(m_src);
    m_src->AddRef();
    return S_OK;
}

STDMETHODIMP VcamMFStream::GetStreamDescriptor(IMFStreamDescriptor **sd) {
    if (!sd) return E_POINTER;
    if (m_shutdown) return MF_E_SHUTDOWN;
    *sd = m_sd;
    m_sd->AddRef();
    return S_OK;
}

void VcamMFStream::EnsureHandles() {
    if (!m_hMem) {
        m_hMem = OpenFileMappingW(FILE_MAP_READ, FALSE, ViewCam::SHMEM_NAME_W);
        if (m_hMem)
            m_mapped = MapViewOfFile(m_hMem, FILE_MAP_READ, 0, 0,
                                     ViewCam::SHMEM_TOTAL_SIZE);
    }
    if (!m_hMutex)
        m_hMutex = OpenMutexW(SYNCHRONIZE, FALSE, ViewCam::MUTEX_NAME_W);
    if (!m_hEvent)
        m_hEvent = OpenEventW(SYNCHRONIZE, FALSE, ViewCam::EVENT_NAME_W);
}

STDMETHODIMP VcamMFStream::RequestSample(IUnknown *token) {
    if (m_shutdown) return MF_E_SHUTDOWN;
    if (!m_active)  return MF_E_INVALIDREQUEST;
    return BuildAndQueueSample(token);
}

HRESULT VcamMFStream::BuildAndQueueSample(IUnknown *token) {
    EnsureHandles();

    // Wait up to one frame period for a new frame signal
    if (m_hEvent)
        WaitForSingleObject(m_hEvent, 1000 / FPS + 5);

    IMFMediaBuffer *pBuf = nullptr;
    HRESULT hr = MFCreateMemoryBuffer(NV12_SZ, &pBuf);
    if (FAILED(hr)) return hr;

    BYTE *pData = nullptr;
    pBuf->Lock(&pData, nullptr, nullptr);

    bool got = false;
    if (m_mapped && m_hMutex &&
        WaitForSingleObject(m_hMutex, 16) == WAIT_OBJECT_0)
    {
        const auto *hdr = static_cast<const ViewCam::SharedFrameHeader *>(m_mapped);
        if (hdr->magic == ViewCam::FRAME_MAGIC &&
            hdr->frame_number != m_lastFrm &&
            hdr->width == W && hdr->height == H)
        {
            const BYTE *raw = static_cast<const BYTE *>(m_mapped)
                              + ViewCam::SHMEM_HEADER_SIZE;
            Bgr24BottomUpToNV12(raw, pData);
            m_lastFrm = hdr->frame_number;
            got = true;
        }
        ReleaseMutex(m_hMutex);
    }

    if (!got) {
        // No source running -- neutral grey (Y=128, UV=128)
        memset(pData,            128, W * H);
        memset(pData + W * H,    128, W * H / 2);
    }

    pBuf->Unlock();
    pBuf->SetCurrentLength(NV12_SZ);

    IMFSample *pSample = nullptr;
    hr = MFCreateSample(&pSample);
    if (SUCCEEDED(hr)) {
        pSample->AddBuffer(pBuf);
        LONGLONG dur = 10000000LL / FPS;
        pSample->SetSampleTime(static_cast<LONGLONG>(m_tick) * dur);
        pSample->SetSampleDuration(dur);
        ++m_tick;
        if (token) pSample->SetUnknown(MFSampleExtension_Token, token);

        PROPVARIANT pv;
        PropVariantInit(&pv);
        pv.vt      = VT_UNKNOWN;
        pv.punkVal = pSample;
        pSample->AddRef();
        m_q->QueueEventParamVar(MEMediaSample, GUID_NULL, S_OK, &pv);
        PropVariantClear(&pv);
        pSample->Release();
    }
    pBuf->Release();
    return hr;
}

void VcamMFStream::OnStart(LONGLONG pos) {
    m_active  = true;
    m_tick    = 0;
    m_lastFrm = 0;
    EnsureHandles();

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt            = VT_I8;
    pv.hVal.QuadPart = pos;
    m_q->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, &pv);
    PropVariantClear(&pv);
}

void VcamMFStream::OnStop() {
    m_active = false;
    m_q->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr);
}

void VcamMFStream::OnShutdown() {
    m_shutdown = true;
    m_active   = false;
    if (m_q) m_q->Shutdown();
}

// =============================================================================
// VcamMFSource
// =============================================================================

VcamMFSource::VcamMFSource() {
    InterlockedIncrement(&g_refCount);
    MFCreateEventQueue(&m_q);

    // Build stream descriptor: NV12 1280x720 @ 30 fps
    IMFMediaType *pType = nullptr;
    if (FAILED(MFCreateMediaType(&pType))) return;

    pType->SetGUID(MF_MT_MAJOR_TYPE,              MFMediaType_Video);
    pType->SetGUID(MF_MT_SUBTYPE,                 MFVideoFormat_NV12);
    MFSetAttributeSize (pType, MF_MT_FRAME_SIZE,          W, H);
    MFSetAttributeRatio(pType, MF_MT_FRAME_RATE,          FPS, 1);
    MFSetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO,  1, 1);
    pType->SetUINT32(MF_MT_INTERLACE_MODE,        MFVideoInterlace_Progressive);
    pType->SetUINT32(MF_MT_DEFAULT_STRIDE,        (INT32)W);
    pType->SetUINT32(MF_MT_SAMPLE_SIZE,           NV12_SZ);
    pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

    IMFStreamDescriptor *pSD = nullptr;
    IMFMediaType *arr[1]     = { pType };
    MFCreateStreamDescriptor(0, 1, arr, &pSD);
    pType->Release();

    if (pSD) {
        IMFMediaTypeHandler *h = nullptr;
        if (SUCCEEDED(pSD->GetMediaTypeHandler(&h))) {
            // re-get the type from the descriptor so we set the right pointer
            IMFMediaType *cur = nullptr;
            if (SUCCEEDED(h->GetMediaTypeByIndex(0, &cur))) {
                h->SetCurrentMediaType(cur);
                cur->Release();
            }
            h->Release();
        }

        m_stream = new (std::nothrow) VcamMFStream(this, pSD);

        IMFStreamDescriptor *sdArr[1] = { pSD };
        if (SUCCEEDED(MFCreatePresentationDescriptor(1, sdArr, &m_pd)))
            m_pd->SelectStream(0);

        pSD->Release();
    }
}

VcamMFSource::~VcamMFSource() {
    SAFE_RELEASE(m_q);
    SAFE_RELEASE(m_pd);
    SAFE_RELEASE(m_stream);
    InterlockedDecrement(&g_refCount);
}

STDMETHODIMP VcamMFSource::QueryInterface(REFIID iid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (iid == IID_IUnknown || iid == IID_IMFMediaEventGenerator ||
        iid == IID_IMFMediaSource) {
        *ppv = static_cast<IMFMediaSource *>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP VcamMFSource::GetCharacteristics(DWORD *ch) {
    if (!ch) return E_POINTER;
    if (m_shutdown) return MF_E_SHUTDOWN;
    *ch = MFMEDIASOURCE_IS_LIVE;
    return S_OK;
}

STDMETHODIMP VcamMFSource::CreatePresentationDescriptor(IMFPresentationDescriptor **pd) {
    if (!pd) return E_POINTER;
    if (m_shutdown) return MF_E_SHUTDOWN;
    if (!m_pd) return MF_E_NOT_INITIALIZED;
    return m_pd->Clone(pd);
}

STDMETHODIMP VcamMFSource::Start(IMFPresentationDescriptor *,
                                  const GUID *, const PROPVARIANT *pos) {
    if (m_shutdown) return MF_E_SHUTDOWN;
    if (!m_stream)  return MF_E_NOT_INITIALIZED;

    LONGLONG startPos = 0;
    if (pos && pos->vt == VT_I8) startPos = pos->hVal.QuadPart;

    m_stream->OnStart(startPos);

    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt            = VT_I8;
    pv.hVal.QuadPart = startPos;
    m_q->QueueEventParamVar(MESourceStarted, GUID_NULL, S_OK, &pv);
    PropVariantClear(&pv);

    // Announce the stream to source readers / media sessions
    PROPVARIANT sv;
    PropVariantInit(&sv);
    sv.vt      = VT_UNKNOWN;
    sv.punkVal = m_stream;
    m_stream->AddRef();
    m_q->QueueEventParamVar(MENewStream, GUID_NULL, S_OK, &sv);
    PropVariantClear(&sv);

    return S_OK;
}

STDMETHODIMP VcamMFSource::Stop() {
    if (m_shutdown) return MF_E_SHUTDOWN;
    if (m_stream)   m_stream->OnStop();
    m_q->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, nullptr);
    return S_OK;
}

STDMETHODIMP VcamMFSource::Shutdown() {
    if (m_shutdown) return MF_E_SHUTDOWN;
    m_shutdown = true;
    if (m_stream) m_stream->OnShutdown();
    if (m_q)      m_q->Shutdown();
    return S_OK;
}
