// ViewCam DirectShow Virtual Camera Source Filter
// Complete COM implementation — zero external dependencies beyond Windows SDK.

#include "VcamFilter.h"
#include "virtualcam/SharedFrameProtocol.h"

#include <cstring>
#include <algorithm>

// Explicitly define our CLSID (avoids INITGUID collisions with strmiids.lib)
const GUID CLSID_ViewCamSource =
    {0x8d0b7a5e, 0x6c14, 0x4f2d, {0x9a, 0x7b, 0x3e, 0x1d, 0x5c, 0x8f, 0x0a, 0x2b}};

// ── DLL Globals ──────────────────────────────────────────────────
HMODULE              g_hModule       = nullptr;
std::atomic<LONG>    g_dllRefCount{0};

// ── AM_MEDIA_TYPE utilities ──────────────────────────────────────

AM_MEDIA_TYPE *AllocMediaType() {
    auto *pmt = static_cast<AM_MEDIA_TYPE *>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    if (pmt) ZeroMemory(pmt, sizeof(AM_MEDIA_TYPE));
    return pmt;
}

void FreeMediaType(AM_MEDIA_TYPE *pmt) {
    if (!pmt) return;
    if (pmt->cbFormat) CoTaskMemFree(pmt->pbFormat);
    if (pmt->pUnk) pmt->pUnk->Release();
    CoTaskMemFree(pmt);
}

void CopyMediaType(AM_MEDIA_TYPE *dst, const AM_MEDIA_TYPE *src) {
    *dst = *src;
    if (src->cbFormat && src->pbFormat) {
        dst->pbFormat = static_cast<BYTE *>(CoTaskMemAlloc(src->cbFormat));
        if (dst->pbFormat)
            memcpy(dst->pbFormat, src->pbFormat, src->cbFormat);
    }
    if (dst->pUnk) dst->pUnk->AddRef();
}

HRESULT FillRGB24MediaType(AM_MEDIA_TYPE *pmt, int w, int h, int fps) {
    ZeroMemory(pmt, sizeof(AM_MEDIA_TYPE));
    pmt->majortype            = MEDIATYPE_Video;
    pmt->subtype              = MEDIASUBTYPE_RGB24;
    pmt->bFixedSizeSamples    = TRUE;
    pmt->bTemporalCompression = FALSE;
    pmt->lSampleSize          = w * h * 3;
    pmt->formattype           = FORMAT_VideoInfo;
    pmt->cbFormat             = sizeof(VIDEOINFOHEADER);
    pmt->pbFormat             = static_cast<BYTE *>(CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
    if (!pmt->pbFormat) return E_OUTOFMEMORY;

    auto *vih = reinterpret_cast<VIDEOINFOHEADER *>(pmt->pbFormat);
    ZeroMemory(vih, sizeof(VIDEOINFOHEADER));
    vih->rcSource        = {0, 0, w, h};
    vih->rcTarget        = {0, 0, w, h};
    vih->AvgTimePerFrame = 10000000LL / fps;

    vih->bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    vih->bmiHeader.biWidth         = w;
    vih->bmiHeader.biHeight        = h; // Positive = bottom-up (standard DIB)
    vih->bmiHeader.biPlanes        = 1;
    vih->bmiHeader.biBitCount      = 24;
    vih->bmiHeader.biCompression   = BI_RGB;
    vih->bmiHeader.biSizeImage     = w * h * 3;

    return S_OK;
}

// ═══════════════════════════════════════════════════════════════════
//  VcamEnumPins
// ═══════════════════════════════════════════════════════════════════

VcamEnumPins::VcamEnumPins(VcamSource *filter, int pos)
    : m_filter(filter), m_pos(pos)
{
    m_filter->AddRef();
    g_dllRefCount++;
}

STDMETHODIMP VcamEnumPins::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IEnumPins) {
        *ppv = static_cast<IEnumPins *>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) VcamEnumPins::AddRef()  { return ++m_refCount; }
STDMETHODIMP_(ULONG) VcamEnumPins::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) { m_filter->Release(); g_dllRefCount--; delete this; }
    return ref;
}

STDMETHODIMP VcamEnumPins::Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched) {
    if (!ppPins) return E_POINTER;
    ULONG fetched = 0;
    while (fetched < cPins && m_pos < 1) {
        ppPins[fetched] = m_filter->GetPin();
        ppPins[fetched]->AddRef();
        m_pos++;
        fetched++;
    }
    if (pcFetched) *pcFetched = fetched;
    return (fetched == cPins) ? S_OK : S_FALSE;
}

STDMETHODIMP VcamEnumPins::Skip(ULONG cPins)  { m_pos += (int)cPins; return (m_pos <= 1) ? S_OK : S_FALSE; }
STDMETHODIMP VcamEnumPins::Reset()            { m_pos = 0; return S_OK; }
STDMETHODIMP VcamEnumPins::Clone(IEnumPins **ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new VcamEnumPins(m_filter, m_pos);
    return S_OK;
}

// ═══════════════════════════════════════════════════════════════════
//  VcamEnumMediaTypes
// ═══════════════════════════════════════════════════════════════════

VcamEnumMediaTypes::VcamEnumMediaTypes(int pos) : m_pos(pos) {
    g_dllRefCount++;
}

STDMETHODIMP VcamEnumMediaTypes::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IEnumMediaTypes) {
        *ppv = static_cast<IEnumMediaTypes *>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) VcamEnumMediaTypes::AddRef()  { return ++m_refCount; }
STDMETHODIMP_(ULONG) VcamEnumMediaTypes::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) { g_dllRefCount--; delete this; }
    return ref;
}

STDMETHODIMP VcamEnumMediaTypes::Next(ULONG cTypes, AM_MEDIA_TYPE **ppTypes, ULONG *pcFetched) {
    if (!ppTypes) return E_POINTER;
    ULONG fetched = 0;
    // We offer exactly one media type: RGB24 1280x720 @ 30fps
    while (fetched < cTypes && m_pos < 1) {
        ppTypes[fetched] = AllocMediaType();
        if (!ppTypes[fetched]) return E_OUTOFMEMORY;
        FillRGB24MediaType(ppTypes[fetched], VCAM_WIDTH, VCAM_HEIGHT, VCAM_FPS);
        m_pos++;
        fetched++;
    }
    if (pcFetched) *pcFetched = fetched;
    return (fetched == cTypes) ? S_OK : S_FALSE;
}

STDMETHODIMP VcamEnumMediaTypes::Skip(ULONG cTypes)  { m_pos += (int)cTypes; return (m_pos <= 1) ? S_OK : S_FALSE; }
STDMETHODIMP VcamEnumMediaTypes::Reset()             { m_pos = 0; return S_OK; }
STDMETHODIMP VcamEnumMediaTypes::Clone(IEnumMediaTypes **ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new VcamEnumMediaTypes(m_pos);
    return S_OK;
}

// ═══════════════════════════════════════════════════════════════════
//  VcamPin — Output Pin
// ═══════════════════════════════════════════════════════════════════

VcamPin::VcamPin(VcamSource *filter) : m_filter(filter) {
    m_frameBuf = new BYTE[VCAM_IMAGE_SIZE];
    FillBlackFrame(m_frameBuf, VCAM_IMAGE_SIZE);
    ZeroMemory(&m_mediaType, sizeof(m_mediaType));
    g_dllRefCount++;
}

VcamPin::~VcamPin() {
    StopStreaming();
    delete[] m_frameBuf;
    if (m_mediaType.pbFormat) CoTaskMemFree(m_mediaType.pbFormat);
    if (m_allocator)    m_allocator->Release();
    if (m_inputPin)     m_inputPin->Release();
    if (m_connectedPin) m_connectedPin->Release();
    // Close shared memory handles
    if (m_mappedPtr)   UnmapViewOfFile(m_mappedPtr);
    if (m_sharedMem)   CloseHandle(m_sharedMem);
    if (m_frameEvent)  CloseHandle(m_frameEvent);
    if (m_mutex)       CloseHandle(m_mutex);
    g_dllRefCount--;
}

// ── IUnknown ─────────────────────────────────────────────────────

STDMETHODIMP VcamPin::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if      (riid == IID_IUnknown)         *ppv = static_cast<IPin *>(this);
    else if (riid == IID_IPin)             *ppv = static_cast<IPin *>(this);
    else if (riid == IID_IQualityControl)  *ppv = static_cast<IQualityControl *>(this);
    else if (riid == IID_IAMStreamConfig)  *ppv = static_cast<IAMStreamConfig *>(this);
    else if (riid == IID_IKsPropertySet)   *ppv = static_cast<IKsPropertySet *>(this);
    else { *ppv = nullptr; return E_NOINTERFACE; }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) VcamPin::AddRef()  { return ++m_refCount; }
STDMETHODIMP_(ULONG) VcamPin::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

// ── IPin ─────────────────────────────────────────────────────────

STDMETHODIMP VcamPin::Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt) {
    if (!pReceivePin) return E_POINTER;
    if (m_connected) return VFW_E_ALREADY_CONNECTED;

    // Determine media type to propose
    AM_MEDIA_TYPE proposedMt{};
    if (pmt && pmt->majortype != GUID_NULL) {
        CopyMediaType(&proposedMt, pmt);
    } else {
        FillRGB24MediaType(&proposedMt, VCAM_WIDTH, VCAM_HEIGHT, VCAM_FPS);
    }

    // Propose to the downstream input pin
    HRESULT hr = pReceivePin->ReceiveConnection(static_cast<IPin *>(this), &proposedMt);
    if (FAILED(hr)) {
        if (proposedMt.pbFormat) CoTaskMemFree(proposedMt.pbFormat);
        // Try our default type if the proposed one failed
        if (pmt && pmt->majortype != GUID_NULL) {
            FillRGB24MediaType(&proposedMt, VCAM_WIDTH, VCAM_HEIGHT, VCAM_FPS);
            hr = pReceivePin->ReceiveConnection(static_cast<IPin *>(this), &proposedMt);
            if (FAILED(hr)) {
                if (proposedMt.pbFormat) CoTaskMemFree(proposedMt.pbFormat);
                return hr;
            }
        } else {
            return hr;
        }
    }

    // Save the connected pin and media type
    m_connectedPin = pReceivePin;
    m_connectedPin->AddRef();
    if (m_mediaType.pbFormat) CoTaskMemFree(m_mediaType.pbFormat);
    CopyMediaType(&m_mediaType, &proposedMt);
    if (proposedMt.pbFormat) CoTaskMemFree(proposedMt.pbFormat);

    // Get IMemInputPin from downstream
    hr = m_connectedPin->QueryInterface(IID_IMemInputPin, reinterpret_cast<void **>(&m_inputPin));
    if (FAILED(hr)) {
        Disconnect();
        return hr;
    }

    // Negotiate allocator
    hr = m_inputPin->GetAllocator(&m_allocator);
    if (FAILED(hr)) {
        // Create our own allocator
        hr = CoCreateInstance(CLSID_MemoryAllocator, nullptr, CLSCTX_INPROC_SERVER,
                              IID_IMemAllocator, reinterpret_cast<void **>(&m_allocator));
        if (FAILED(hr)) { Disconnect(); return hr; }
    }

    ALLOCATOR_PROPERTIES props = {1, VCAM_IMAGE_SIZE, 1, 0};
    ALLOCATOR_PROPERTIES actual{};
    hr = m_allocator->SetProperties(&props, &actual);
    if (FAILED(hr)) { Disconnect(); return hr; }

    hr = m_inputPin->NotifyAllocator(m_allocator, FALSE);
    if (FAILED(hr)) { Disconnect(); return hr; }

    m_connected = true;
    return S_OK;
}

STDMETHODIMP VcamPin::ReceiveConnection(IPin *, const AM_MEDIA_TYPE *) {
    return E_UNEXPECTED; // Output pins don't receive connections
}

STDMETHODIMP VcamPin::Disconnect() {
    StopStreaming();
    if (m_allocator)    { m_allocator->Release();    m_allocator = nullptr; }
    if (m_inputPin)     { m_inputPin->Release();     m_inputPin = nullptr; }
    if (m_connectedPin) { m_connectedPin->Release(); m_connectedPin = nullptr; }
    if (m_mediaType.pbFormat) { CoTaskMemFree(m_mediaType.pbFormat); m_mediaType.pbFormat = nullptr; }
    ZeroMemory(&m_mediaType, sizeof(m_mediaType));
    m_connected = false;
    return S_OK;
}

STDMETHODIMP VcamPin::ConnectedTo(IPin **pPin) {
    if (!pPin) return E_POINTER;
    if (!m_connected) { *pPin = nullptr; return VFW_E_NOT_CONNECTED; }
    *pPin = m_connectedPin;
    (*pPin)->AddRef();
    return S_OK;
}

STDMETHODIMP VcamPin::ConnectionMediaType(AM_MEDIA_TYPE *pmt) {
    if (!pmt) return E_POINTER;
    if (!m_connected) { ZeroMemory(pmt, sizeof(*pmt)); return VFW_E_NOT_CONNECTED; }
    CopyMediaType(pmt, &m_mediaType);
    return S_OK;
}

STDMETHODIMP VcamPin::QueryPinInfo(PIN_INFO *pInfo) {
    if (!pInfo) return E_POINTER;
    pInfo->pFilter = static_cast<IBaseFilter *>(m_filter);
    pInfo->pFilter->AddRef();
    pInfo->dir = PINDIR_OUTPUT;
    wcscpy_s(pInfo->achName, PIN_NAME);
    return S_OK;
}

STDMETHODIMP VcamPin::QueryDirection(PIN_DIRECTION *pPinDir) {
    if (!pPinDir) return E_POINTER;
    *pPinDir = PINDIR_OUTPUT;
    return S_OK;
}

STDMETHODIMP VcamPin::QueryId(LPWSTR *Id) {
    if (!Id) return E_POINTER;
    *Id = static_cast<LPWSTR>(CoTaskMemAlloc(sizeof(PIN_NAME)));
    if (!*Id) return E_OUTOFMEMORY;
    wcscpy_s(*Id, sizeof(PIN_NAME) / sizeof(WCHAR), PIN_NAME);
    return S_OK;
}

STDMETHODIMP VcamPin::QueryAccept(const AM_MEDIA_TYPE *pmt) {
    if (!pmt) return E_POINTER;
    if (pmt->majortype != MEDIATYPE_Video) return S_FALSE;
    if (pmt->subtype != MEDIASUBTYPE_RGB24) return S_FALSE;
    if (pmt->formattype != FORMAT_VideoInfo) return S_FALSE;
    return S_OK;
}

STDMETHODIMP VcamPin::EnumMediaTypes(IEnumMediaTypes **ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new VcamEnumMediaTypes();
    return S_OK;
}

STDMETHODIMP VcamPin::QueryInternalConnections(IPin **, ULONG *nPin) {
    if (nPin) *nPin = 0;
    return E_NOTIMPL;
}

STDMETHODIMP VcamPin::EndOfStream()  { return S_OK; }
STDMETHODIMP VcamPin::BeginFlush()   { return S_OK; }
STDMETHODIMP VcamPin::EndFlush()     { return S_OK; }
STDMETHODIMP VcamPin::NewSegment(REFERENCE_TIME, REFERENCE_TIME, double) { return S_OK; }

// ── IQualityControl ──────────────────────────────────────────────

STDMETHODIMP VcamPin::Notify(IBaseFilter *, Quality)     { return S_OK; }
STDMETHODIMP VcamPin::SetSink(IQualityControl *)         { return S_OK; }

// ── IAMStreamConfig ──────────────────────────────────────────────

STDMETHODIMP VcamPin::SetFormat(AM_MEDIA_TYPE *pmt) {
    if (!pmt) return E_POINTER;
    // Accept only our supported format
    if (pmt->majortype != MEDIATYPE_Video)      return VFW_E_INVALIDMEDIATYPE;
    if (pmt->subtype != MEDIASUBTYPE_RGB24)     return VFW_E_INVALIDMEDIATYPE;
    if (pmt->formattype != FORMAT_VideoInfo)    return VFW_E_INVALIDMEDIATYPE;
    return S_OK;
}

STDMETHODIMP VcamPin::GetFormat(AM_MEDIA_TYPE **ppmt) {
    if (!ppmt) return E_POINTER;
    *ppmt = AllocMediaType();
    if (!*ppmt) return E_OUTOFMEMORY;
    return FillRGB24MediaType(*ppmt, VCAM_WIDTH, VCAM_HEIGHT, VCAM_FPS);
}

STDMETHODIMP VcamPin::GetNumberOfCapabilities(int *piCount, int *piSize) {
    if (!piCount || !piSize) return E_POINTER;
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

STDMETHODIMP VcamPin::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **ppmt, BYTE *pSCC) {
    if (!ppmt || !pSCC) return E_POINTER;
    if (iIndex != 0) return S_FALSE;

    *ppmt = AllocMediaType();
    if (!*ppmt) return E_OUTOFMEMORY;
    HRESULT hr = FillRGB24MediaType(*ppmt, VCAM_WIDTH, VCAM_HEIGHT, VCAM_FPS);
    if (FAILED(hr)) { FreeMediaType(*ppmt); *ppmt = nullptr; return hr; }

    auto *caps = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS *>(pSCC);
    ZeroMemory(caps, sizeof(VIDEO_STREAM_CONFIG_CAPS));
    caps->guid               = FORMAT_VideoInfo;
    caps->VideoStandard      = 0;
    caps->InputSize          = {VCAM_WIDTH, VCAM_HEIGHT};
    caps->MinCroppingSize    = {VCAM_WIDTH, VCAM_HEIGHT};
    caps->MaxCroppingSize    = {VCAM_WIDTH, VCAM_HEIGHT};
    caps->CropGranularityX   = 1;
    caps->CropGranularityY   = 1;
    caps->CropAlignX         = 1;
    caps->CropAlignY         = 1;
    caps->MinOutputSize      = {VCAM_WIDTH, VCAM_HEIGHT};
    caps->MaxOutputSize      = {VCAM_WIDTH, VCAM_HEIGHT};
    caps->OutputGranularityX = 1;
    caps->OutputGranularityY = 1;
    caps->MinFrameInterval   = VCAM_AVG_TIME_PER_FRAME;
    caps->MaxFrameInterval   = VCAM_AVG_TIME_PER_FRAME;
    caps->MinBitsPerSecond   = static_cast<LONG>(VCAM_IMAGE_SIZE) * 8 * VCAM_FPS;
    caps->MaxBitsPerSecond   = static_cast<LONG>(VCAM_IMAGE_SIZE) * 8 * VCAM_FPS;

    return S_OK;
}

// ── IKsPropertySet ───────────────────────────────────────────────
// Required so apps recognize this as a capture device.

STDMETHODIMP VcamPin::Set(REFGUID, DWORD, LPVOID, DWORD, LPVOID, DWORD) {
    return E_NOTIMPL;
}

STDMETHODIMP VcamPin::Get(REFGUID guidPropSet, DWORD dwPropID,
                           LPVOID, DWORD, LPVOID pPropData, DWORD cbPropData,
                           DWORD *pcbReturned) {
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    if (cbPropData < sizeof(GUID)) return E_UNEXPECTED;

    // Report as a capture pin
    *reinterpret_cast<GUID *>(pPropData) = PIN_CATEGORY_CAPTURE;
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    return S_OK;
}

STDMETHODIMP VcamPin::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport) {
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    return S_OK;
}

// ── Streaming ────────────────────────────────────────────────────

HRESULT VcamPin::StartStreaming() {
    if (m_streaming) return S_OK;
    if (!m_allocator) return E_UNEXPECTED;

    m_allocator->Commit();
    m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_streaming = true;
    m_threadHandle = CreateThread(nullptr, 0, StreamThreadProc, this, 0, nullptr);
    return S_OK;
}

HRESULT VcamPin::StopStreaming() {
    if (!m_streaming) return S_OK;
    m_streaming = false;
    if (m_stopEvent) SetEvent(m_stopEvent);
    if (m_threadHandle) {
        WaitForSingleObject(m_threadHandle, 2000);
        CloseHandle(m_threadHandle);
        m_threadHandle = nullptr;
    }
    if (m_stopEvent) { CloseHandle(m_stopEvent); m_stopEvent = nullptr; }
    if (m_allocator) m_allocator->Decommit();

    // Close shared memory
    if (m_mappedPtr)  { UnmapViewOfFile(m_mappedPtr); m_mappedPtr = nullptr; }
    if (m_sharedMem)  { CloseHandle(m_sharedMem);     m_sharedMem = nullptr; }
    if (m_frameEvent) { CloseHandle(m_frameEvent);     m_frameEvent = nullptr; }
    if (m_mutex)      { CloseHandle(m_mutex);          m_mutex = nullptr; }
    m_hasFrame = false;
    m_lastFrameNum = 0;

    return S_OK;
}

DWORD WINAPI VcamPin::StreamThreadProc(LPVOID lpParam) {
    auto *pin = static_cast<VcamPin *>(lpParam);
    pin->StreamLoop();
    return 0;
}

void VcamPin::StreamLoop() {
    LONGLONG frameNum = 0;
    m_streamState = StreamState::Dormant;
    m_lastNewFrameTick = 0;

    // Adaptive polling intervals (milliseconds)
    static constexpr DWORD INTERVAL_ACTIVE  = 1000 / VCAM_FPS; // ~33ms (30fps)
    static constexpr DWORD INTERVAL_IDLE    = 100;              // 10fps
    static constexpr DWORD INTERVAL_DORMANT = 500;              // 2fps
    static constexpr DWORD IDLE_THRESHOLD   = 500;              // ms without new frame -> idle

    while (m_streaming) {
        DWORD startTick = GetTickCount();

        // ── Try to open shared memory if not yet open ──
        if (!m_sharedMem) {
            m_sharedMem = OpenFileMappingW(FILE_MAP_READ, FALSE, ViewCam::SHMEM_NAME_W);
            if (m_sharedMem) {
                m_mappedPtr = MapViewOfFile(m_sharedMem, FILE_MAP_READ, 0, 0, 0);
                m_frameEvent = OpenEventW(SYNCHRONIZE, FALSE, ViewCam::EVENT_NAME_W);
                m_mutex = OpenMutexW(SYNCHRONIZE, FALSE, ViewCam::MUTEX_NAME_W);
            }
        }

        // ── Detect if app has closed (shared memory disappeared or magic gone) ──
        if (m_mappedPtr) {
            auto *hdr = static_cast<const ViewCam::SharedFrameHeader *>(m_mappedPtr);
            if (hdr->magic != ViewCam::FRAME_MAGIC) {
                // App closed — release handles so we can reopen later
                if (m_mappedPtr)  { UnmapViewOfFile(m_mappedPtr); m_mappedPtr = nullptr; }
                if (m_sharedMem)  { CloseHandle(m_sharedMem);     m_sharedMem = nullptr; }
                if (m_frameEvent) { CloseHandle(m_frameEvent);    m_frameEvent = nullptr; }
                if (m_mutex)      { CloseHandle(m_mutex);         m_mutex = nullptr; }
                m_hasFrame = false;
                m_lastFrameNum = 0;
                m_streamState = StreamState::Dormant;
            }
        }

        // ── Read frame from shared memory ──
        bool gotNewFrame = false;
        if (m_mappedPtr) {
            gotNewFrame = ReadSharedFrame(m_frameBuf, VCAM_IMAGE_SIZE);
        }

        // ── Update adaptive state ──
        if (gotNewFrame) {
            m_lastNewFrameTick = startTick;
            m_streamState = StreamState::Active;
        } else if (m_streamState == StreamState::Active) {
            DWORD sinceLastFrame = startTick - m_lastNewFrameTick;
            if (sinceLastFrame > IDLE_THRESHOLD) {
                m_streamState = m_mappedPtr ? StreamState::Idle : StreamState::Dormant;
            }
        } else if (!m_mappedPtr) {
            m_streamState = StreamState::Dormant;
        }

        // ── Deliver frame ──
        if (m_inputPin && m_allocator) {
            IMediaSample *pSample = nullptr;
            HRESULT hr = m_allocator->GetBuffer(&pSample, nullptr, nullptr, 0);
            if (SUCCEEDED(hr) && pSample) {
                BYTE *pBuf = nullptr;
                pSample->GetPointer(&pBuf);
                long bufLen = pSample->GetSize();
                if (pBuf && bufLen >= VCAM_IMAGE_SIZE) {
                    memcpy(pBuf, m_frameBuf, VCAM_IMAGE_SIZE);
                } else if (pBuf) {
                    FillBlackFrame(pBuf, bufLen);
                }
                pSample->SetActualDataLength(VCAM_IMAGE_SIZE);

                REFERENCE_TIME rtStart = frameNum * VCAM_AVG_TIME_PER_FRAME;
                REFERENCE_TIME rtEnd   = rtStart + VCAM_AVG_TIME_PER_FRAME;
                pSample->SetTime(&rtStart, &rtEnd);
                pSample->SetSyncPoint(TRUE);

                hr = m_inputPin->Receive(pSample);
                pSample->Release();

                if (FAILED(hr)) break;
                frameNum++;
            }
        }

        // ── Adaptive sleep ──
        if (!m_streaming) break;

        DWORD interval;
        switch (m_streamState) {
        case StreamState::Active:  interval = INTERVAL_ACTIVE;  break;
        case StreamState::Idle:    interval = INTERVAL_IDLE;    break;
        case StreamState::Dormant: interval = INTERVAL_DORMANT; break;
        }

        DWORD elapsed = GetTickCount() - startTick;
        DWORD sleepTime = (elapsed < interval) ? (interval - elapsed) : 0;

        if (sleepTime > 0) {
            // Use event-driven wakeup: stop event + frame event for instant
            // transition from idle/dormant -> active when app starts streaming
            HANDLE handles[2] = { m_stopEvent, nullptr };
            DWORD handleCount = 1;
            if (m_frameEvent) {
                handles[handleCount++] = m_frameEvent;
            }
            WaitForMultipleObjects(handleCount, handles, FALSE, sleepTime);
        }
    }
}

bool VcamPin::ReadSharedFrame(BYTE *buf, int bufSize) {
    if (!m_mappedPtr || !m_mutex) return false;

    DWORD wait = WaitForSingleObject(m_mutex, 5);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) return false;

    auto *hdr = static_cast<const ViewCam::SharedFrameHeader *>(m_mappedPtr);
    bool gotNew = false;

    if (hdr->magic == ViewCam::FRAME_MAGIC && hdr->frame_number > m_lastFrameNum) {
        const BYTE *src = static_cast<const BYTE *>(m_mappedPtr) + ViewCam::SHMEM_HEADER_SIZE;
        int srcW   = static_cast<int>(hdr->width);
        int srcH   = static_cast<int>(hdr->height);
        int srcStride = srcW * 3;

        if (srcW == VCAM_WIDTH && srcH == VCAM_HEIGHT && hdr->frame_size <= (uint32_t)bufSize) {
            // Direct copy — resolution matches
            memcpy(buf, src, hdr->frame_size);
            gotNew = true;
        } else if (srcW > 0 && srcH > 0) {
            // Resolution mismatch: simple nearest-neighbor scale
            for (int y = 0; y < VCAM_HEIGHT; ++y) {
                int srcY = y * srcH / VCAM_HEIGHT;
                const BYTE *srcRow = src + srcY * srcStride;
                BYTE *dstRow = buf + y * VCAM_STRIDE;
                for (int x = 0; x < VCAM_WIDTH; ++x) {
                    int srcX = x * srcW / VCAM_WIDTH;
                    dstRow[x * 3 + 0] = srcRow[srcX * 3 + 0];
                    dstRow[x * 3 + 1] = srcRow[srcX * 3 + 1];
                    dstRow[x * 3 + 2] = srcRow[srcX * 3 + 2];
                }
            }
            gotNew = true;
        }
        m_lastFrameNum = hdr->frame_number;
        m_hasFrame = true;
    }

    ReleaseMutex(m_mutex);
    return gotNew;
}

void VcamPin::FillBlackFrame(BYTE *buf, int size) {
    memset(buf, 0, size);
}

// ═══════════════════════════════════════════════════════════════════
//  VcamSource — The Filter
// ═══════════════════════════════════════════════════════════════════

VcamSource::VcamSource() {
    m_pin = new VcamPin(this);
    g_dllRefCount++;
}

VcamSource::~VcamSource() {
    delete m_pin;
    if (m_clock) m_clock->Release();
    g_dllRefCount--;
}

STDMETHODIMP VcamSource::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if      (riid == IID_IUnknown)           *ppv = static_cast<IBaseFilter *>(this);
    else if (riid == IID_IPersist)           *ppv = static_cast<IPersist *>(static_cast<IMediaFilter *>(static_cast<IBaseFilter *>(this)));
    else if (riid == IID_IMediaFilter)       *ppv = static_cast<IMediaFilter *>(static_cast<IBaseFilter *>(this));
    else if (riid == IID_IBaseFilter)        *ppv = static_cast<IBaseFilter *>(this);
    else if (riid == IID_IAMFilterMiscFlags) *ppv = static_cast<IAMFilterMiscFlags *>(this);
    else { *ppv = nullptr; return E_NOINTERFACE; }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) VcamSource::AddRef()  { return ++m_refCount; }
STDMETHODIMP_(ULONG) VcamSource::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

// IPersist
STDMETHODIMP VcamSource::GetClassID(CLSID *pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_ViewCamSource;
    return S_OK;
}

// IMediaFilter
STDMETHODIMP VcamSource::Stop() {
    m_pin->StopStreaming();
    m_state = State_Stopped;
    return S_OK;
}

STDMETHODIMP VcamSource::Pause() {
    if (m_state == State_Stopped)
        m_pin->StartStreaming();
    m_state = State_Paused;
    return S_OK;
}

STDMETHODIMP VcamSource::Run(REFERENCE_TIME) {
    if (m_state == State_Stopped)
        m_pin->StartStreaming();
    m_state = State_Running;
    return S_OK;
}

STDMETHODIMP VcamSource::GetState(DWORD, FILTER_STATE *State) {
    if (!State) return E_POINTER;
    *State = m_state;
    return S_OK;
}

STDMETHODIMP VcamSource::SetSyncSource(IReferenceClock *pClock) {
    if (m_clock) m_clock->Release();
    m_clock = pClock;
    if (m_clock) m_clock->AddRef();
    return S_OK;
}

STDMETHODIMP VcamSource::GetSyncSource(IReferenceClock **pClock) {
    if (!pClock) return E_POINTER;
    *pClock = m_clock;
    if (m_clock) m_clock->AddRef();
    return S_OK;
}

// IBaseFilter
STDMETHODIMP VcamSource::EnumPins(IEnumPins **ppEnum) {
    if (!ppEnum) return E_POINTER;
    *ppEnum = new VcamEnumPins(this);
    return S_OK;
}

STDMETHODIMP VcamSource::FindPin(LPCWSTR Id, IPin **ppPin) {
    if (!ppPin) return E_POINTER;
    if (wcscmp(Id, PIN_NAME) == 0) {
        *ppPin = m_pin;
        (*ppPin)->AddRef();
        return S_OK;
    }
    *ppPin = nullptr;
    return VFW_E_NOT_FOUND;
}

STDMETHODIMP VcamSource::QueryFilterInfo(FILTER_INFO *pInfo) {
    if (!pInfo) return E_POINTER;
    wcscpy_s(pInfo->achName, m_name[0] ? m_name : FILTER_NAME);
    pInfo->pGraph = m_graph;
    if (m_graph) m_graph->AddRef();
    return S_OK;
}

STDMETHODIMP VcamSource::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName) {
    m_graph = pGraph; // No AddRef — weak reference (prevents circular ref)
    if (pName) wcscpy_s(m_name, pName);
    else       m_name[0] = L'\0';
    return S_OK;
}

STDMETHODIMP VcamSource::QueryVendorInfo(LPWSTR *pVendorInfo) {
    if (!pVendorInfo) return E_POINTER;
    const WCHAR vendor[] = L"ViewCam";
    *pVendorInfo = static_cast<LPWSTR>(CoTaskMemAlloc(sizeof(vendor)));
    if (!*pVendorInfo) return E_OUTOFMEMORY;
    wcscpy_s(*pVendorInfo, sizeof(vendor) / sizeof(WCHAR), vendor);
    return S_OK;
}

// IAMFilterMiscFlags
STDMETHODIMP_(ULONG) VcamSource::GetMiscFlags() {
    return AM_FILTER_MISC_FLAGS_IS_SOURCE;
}

// ═══════════════════════════════════════════════════════════════════
//  VcamClassFactory
// ═══════════════════════════════════════════════════════════════════

STDMETHODIMP VcamClassFactory::QueryInterface(REFIID riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        *ppv = static_cast<IClassFactory *>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) VcamClassFactory::AddRef()  { return ++m_refCount; }
STDMETHODIMP_(ULONG) VcamClassFactory::Release() {
    ULONG ref = --m_refCount;
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP VcamClassFactory::CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) {
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;
    auto *filter = new VcamSource();
    HRESULT hr = filter->QueryInterface(riid, ppv);
    filter->Release(); // QI already AddRef'd
    return hr;
}

STDMETHODIMP VcamClassFactory::LockServer(BOOL fLock) {
    if (fLock) g_dllRefCount++;
    else       g_dllRefCount--;
    return S_OK;
}

// ═══════════════════════════════════════════════════════════════════
//  DLL Entry Points
// ═══════════════════════════════════════════════════════════════════

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    if (rclsid != CLSID_ViewCamSource) return CLASS_E_CLASSNOTAVAILABLE;
    auto *factory = new VcamClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (g_dllRefCount == 0) ? S_OK : S_FALSE;
}

// ── Registry helpers ─────────────────────────────────────────────

static HRESULT RegisterCLSID() {
    WCHAR dllPath[MAX_PATH];
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

    // Convert CLSID to string
    WCHAR clsidStr[64];
    StringFromGUID2(CLSID_ViewCamSource, clsidStr, 64);

    // HKCR\CLSID\{...}
    WCHAR keyPath[256];
    swprintf_s(keyPath, L"CLSID\\%s", clsidStr);
    HKEY hKey;
    RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE *>(FILTER_NAME),
                   (DWORD)(wcslen(FILTER_NAME) + 1) * sizeof(WCHAR));
    RegCloseKey(hKey);

    // HKCR\CLSID\{...}\InprocServer32
    swprintf_s(keyPath, L"CLSID\\%s\\InprocServer32", clsidStr);
    RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE *>(dllPath),
                   (DWORD)(wcslen(dllPath) + 1) * sizeof(WCHAR));
    const WCHAR threading[] = L"Both";
    RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE *>(threading),
                   sizeof(threading));
    RegCloseKey(hKey);

    return S_OK;
}

static HRESULT RegisterInCategory() {
    IFilterMapper2 *pFM2 = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FilterMapper2, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IFilterMapper2, reinterpret_cast<void **>(&pFM2));
    if (FAILED(hr)) return hr;

    // Define the output pin's media types
    REGPINTYPES pinTypes = {&MEDIATYPE_Video, &MEDIASUBTYPE_RGB24};
    REGFILTERPINS2 pin = {};
    pin.dwFlags        = REG_PINFLAG_B_OUTPUT;
    pin.nMediaTypes    = 1;
    pin.lpMediaType    = &pinTypes;

    REGFILTER2 rf2 = {};
    rf2.dwVersion  = 2;
    rf2.dwMerit    = MERIT_DO_NOT_USE;
    rf2.cPins2     = 1;
    rf2.rgPins2    = &pin;

    // Register under "Video Capture Sources" category
    hr = pFM2->RegisterFilter(
        CLSID_ViewCamSource,
        FILTER_NAME,
        nullptr,
        &CLSID_VideoInputDeviceCategory,
        FILTER_NAME,
        &rf2);

    pFM2->Release();
    return hr;
}

STDAPI DllRegisterServer() {
    HRESULT hr = RegisterCLSID();
    if (FAILED(hr)) return hr;
    CoInitialize(nullptr);
    hr = RegisterInCategory();
    CoUninitialize();
    return hr;
}

STDAPI DllUnregisterServer() {
    // Unregister from DirectShow category
    CoInitialize(nullptr);
    IFilterMapper2 *pFM2 = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FilterMapper2, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IFilterMapper2, reinterpret_cast<void **>(&pFM2));
    if (SUCCEEDED(hr)) {
        pFM2->UnregisterFilter(&CLSID_VideoInputDeviceCategory, FILTER_NAME, CLSID_ViewCamSource);
        pFM2->Release();
    }
    CoUninitialize();

    // Remove CLSID keys
    WCHAR clsidStr[64];
    StringFromGUID2(CLSID_ViewCamSource, clsidStr, 64);

    WCHAR keyPath[256];
    swprintf_s(keyPath, L"CLSID\\%s\\InprocServer32", clsidStr);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath);
    swprintf_s(keyPath, L"CLSID\\%s", clsidStr);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath);

    return S_OK;
}
