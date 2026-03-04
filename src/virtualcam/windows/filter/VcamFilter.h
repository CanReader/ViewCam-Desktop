#pragma once

// ViewCam DirectShow Virtual Camera Source Filter
// Minimal COM implementation — no dependency on DirectShow base classes.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dshow.h>
#include <dvdmedia.h>

#include <atomic>

// ── CLSID ────────────────────────────────────────────────────────
// {8D0B7A5E-6C14-4F2D-9A7B-3E1D5C8F0A2B}
// Declared here, defined in VcamFilter.cpp to avoid INITGUID issues.
EXTERN_C const GUID CLSID_ViewCamSource;

// ── Forward Declarations ─────────────────────────────────────────
class VcamSource;
class VcamPin;

// ── Constants ────────────────────────────────────────────────────
static constexpr int    VCAM_WIDTH     = 1280;
static constexpr int    VCAM_HEIGHT    = 720;
static constexpr int    VCAM_FPS       = 30;
// 100-nanosecond units per frame (DirectShow REFERENCE_TIME)
static constexpr REFERENCE_TIME VCAM_AVG_TIME_PER_FRAME = 10000000LL / VCAM_FPS;
static constexpr int    VCAM_BPP       = 24;
static constexpr int    VCAM_STRIDE    = VCAM_WIDTH * 3;
static constexpr int    VCAM_IMAGE_SIZE = VCAM_STRIDE * VCAM_HEIGHT;

static const WCHAR FILTER_NAME[] = L"ViewCam Virtual Camera";
static const WCHAR PIN_NAME[]    = L"Video";

// ── Helper: AM_MEDIA_TYPE utilities ──────────────────────────────
AM_MEDIA_TYPE *AllocMediaType();
void           FreeMediaType(AM_MEDIA_TYPE *pmt);
void           CopyMediaType(AM_MEDIA_TYPE *dst, const AM_MEDIA_TYPE *src);
HRESULT        FillRGB24MediaType(AM_MEDIA_TYPE *pmt, int w, int h, int fps);

// ── VcamEnumPins ─────────────────────────────────────────────────
class VcamEnumPins : public IEnumPins {
public:
    VcamEnumPins(VcamSource *filter, int pos = 0);

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched) override;
    STDMETHODIMP Skip(ULONG cPins) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumPins **ppEnum) override;

private:
    std::atomic<ULONG> m_refCount{1};
    VcamSource *m_filter;
    int         m_pos;
};

// ── VcamEnumMediaTypes ───────────────────────────────────────────
class VcamEnumMediaTypes : public IEnumMediaTypes {
public:
    VcamEnumMediaTypes(int pos = 0);

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP Next(ULONG cTypes, AM_MEDIA_TYPE **ppTypes, ULONG *pcFetched) override;
    STDMETHODIMP Skip(ULONG cTypes) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Clone(IEnumMediaTypes **ppEnum) override;

private:
    std::atomic<ULONG> m_refCount{1};
    int m_pos;
};

// ── VcamPin (output pin) ─────────────────────────────────────────
class VcamPin : public IPin,
                public IQualityControl,
                public IAMStreamConfig,
                public IKsPropertySet {
public:
    explicit VcamPin(VcamSource *filter);
    ~VcamPin();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPin
    STDMETHODIMP Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt) override;
    STDMETHODIMP ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt) override;
    STDMETHODIMP Disconnect() override;
    STDMETHODIMP ConnectedTo(IPin **pPin) override;
    STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt) override;
    STDMETHODIMP QueryPinInfo(PIN_INFO *pInfo) override;
    STDMETHODIMP QueryDirection(PIN_DIRECTION *pPinDir) override;
    STDMETHODIMP QueryId(LPWSTR *Id) override;
    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt) override;
    STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum) override;
    STDMETHODIMP QueryInternalConnections(IPin **apPin, ULONG *nPin) override;
    STDMETHODIMP EndOfStream() override;
    STDMETHODIMP BeginFlush() override;
    STDMETHODIMP EndFlush() override;
    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate) override;

    // IQualityControl
    STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q) override;
    STDMETHODIMP SetSink(IQualityControl *piqc) override;

    // IAMStreamConfig
    STDMETHODIMP SetFormat(AM_MEDIA_TYPE *pmt) override;
    STDMETHODIMP GetFormat(AM_MEDIA_TYPE **ppmt) override;
    STDMETHODIMP GetNumberOfCapabilities(int *piCount, int *piSize) override;
    STDMETHODIMP GetStreamCaps(int iIndex, AM_MEDIA_TYPE **ppmt, BYTE *pSCC) override;

    // IKsPropertySet
    STDMETHODIMP Set(REFGUID guidPropSet, DWORD dwPropID,
                     LPVOID pInstanceData, DWORD cbInstanceData,
                     LPVOID pPropData, DWORD cbPropData) override;
    STDMETHODIMP Get(REFGUID guidPropSet, DWORD dwPropID,
                     LPVOID pInstanceData, DWORD cbInstanceData,
                     LPVOID pPropData, DWORD cbPropData, DWORD *pcbReturned) override;
    STDMETHODIMP QuerySupported(REFGUID guidPropSet, DWORD dwPropID,
                                DWORD *pTypeSupport) override;

    // Streaming control (called by VcamSource)
    HRESULT StartStreaming();
    HRESULT StopStreaming();

private:
    static DWORD WINAPI StreamThreadProc(LPVOID lpParam);
    void StreamLoop();
    void FillBlackFrame(BYTE *buf, int size);
    bool ReadSharedFrame(BYTE *buf, int bufSize);

    std::atomic<ULONG> m_refCount{1};
    VcamSource        *m_filter;
    IPin              *m_connectedPin  = nullptr;
    IMemInputPin      *m_inputPin      = nullptr;
    IMemAllocator     *m_allocator     = nullptr;
    AM_MEDIA_TYPE      m_mediaType{};
    bool               m_connected     = false;

    // Streaming thread
    HANDLE             m_threadHandle  = nullptr;
    HANDLE             m_stopEvent     = nullptr;
    std::atomic<bool>  m_streaming{false};

    // Shared memory handles (opened on demand)
    HANDLE             m_sharedMem     = nullptr;
    void              *m_mappedPtr     = nullptr;
    HANDLE             m_frameEvent    = nullptr;
    HANDLE             m_mutex         = nullptr;
    uint64_t           m_lastFrameNum  = 0;

    // Internal frame buffer (last received frame)
    BYTE              *m_frameBuf      = nullptr;
    bool               m_hasFrame      = false;

    // Adaptive polling state
    enum class StreamState { Dormant, Idle, Active };
    StreamState        m_streamState   = StreamState::Dormant;
    DWORD              m_lastNewFrameTick = 0;
};

// ── VcamSource (the filter) ──────────────────────────────────────
class VcamSource : public IBaseFilter,
                   public IAMFilterMiscFlags {
public:
    VcamSource();
    ~VcamSource();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IPersist
    STDMETHODIMP GetClassID(CLSID *pClassID) override;

    // IMediaFilter
    STDMETHODIMP Stop() override;
    STDMETHODIMP Pause() override;
    STDMETHODIMP Run(REFERENCE_TIME tStart) override;
    STDMETHODIMP GetState(DWORD dwMilliSecsTimeout, FILTER_STATE *State) override;
    STDMETHODIMP SetSyncSource(IReferenceClock *pClock) override;
    STDMETHODIMP GetSyncSource(IReferenceClock **pClock) override;

    // IBaseFilter
    STDMETHODIMP EnumPins(IEnumPins **ppEnum) override;
    STDMETHODIMP FindPin(LPCWSTR Id, IPin **ppPin) override;
    STDMETHODIMP QueryFilterInfo(FILTER_INFO *pInfo) override;
    STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName) override;
    STDMETHODIMP QueryVendorInfo(LPWSTR *pVendorInfo) override;

    // IAMFilterMiscFlags
    STDMETHODIMP_(ULONG) GetMiscFlags() override;

    VcamPin *GetPin() { return m_pin; }

private:
    std::atomic<ULONG>  m_refCount{1};
    VcamPin            *m_pin        = nullptr;
    IFilterGraph       *m_graph      = nullptr;
    IReferenceClock    *m_clock      = nullptr;
    FILTER_STATE        m_state      = State_Stopped;
    WCHAR               m_name[128]  = {};
};

// ── VcamClassFactory ─────────────────────────────────────────────
class VcamClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    STDMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    std::atomic<ULONG> m_refCount{1};
};

// ── DLL globals ──────────────────────────────────────────────────
extern HMODULE g_hModule;
extern std::atomic<LONG> g_dllRefCount;
