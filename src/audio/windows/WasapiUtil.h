#pragma once
// Shared WASAPI helpers for the Windows audio backend (speaker loopback
// capture + VB-CABLE virtual-mic bridge). Header-only; included ONLY from
// the _WIN32 sections of the audio .cpp files.

#ifdef _WIN32

#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <QString>
#include <cstdint>
#include <cstring>

namespace vcwin {

using Microsoft::WRL::ComPtr;

// Parsed essentials of a device mix format.
struct MixFormat {
    int rate = 0;
    int channels = 0;
    int bitsPerSample = 0;
    bool isFloat = false;
    int frameBytes = 0;

    static MixFormat from(const WAVEFORMATEX *wf) {
        MixFormat f;
        f.rate = int(wf->nSamplesPerSec);
        f.channels = int(wf->nChannels);
        f.bitsPerSample = int(wf->wBitsPerSample);
        f.frameBytes = int(wf->nBlockAlign);
        if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            f.isFloat = true;
        } else if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            const auto *ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(wf);
            f.isFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
        }
        return f;
    }
};

// One interleaved sample from a device buffer as float in [-1, 1].
inline float readSample(const uint8_t *frame, const MixFormat &f, int channel) {
    if (f.isFloat) {
        float v;
        std::memcpy(&v, frame + channel * 4, 4);
        return v;
    }
    if (f.bitsPerSample == 16) {
        int16_t v;
        std::memcpy(&v, frame + channel * 2, 2);
        return float(v) / 32768.0f;
    }
    if (f.bitsPerSample == 32) { // s32 or s24-in-32
        int32_t v;
        std::memcpy(&v, frame + channel * 4, 4);
        return float(v) / 2147483648.0f;
    }
    return 0.0f;
}

// Write one float sample into a device buffer frame.
inline void writeSample(uint8_t *frame, const MixFormat &f, int channel, float v) {
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    if (f.isFloat) {
        std::memcpy(frame + channel * 4, &v, 4);
    } else if (f.bitsPerSample == 16) {
        const int16_t s = int16_t(v * 32767.0f);
        std::memcpy(frame + channel * 2, &s, 2);
    } else if (f.bitsPerSample == 32) {
        const int32_t s = int32_t(double(v) * 2147483647.0);
        std::memcpy(frame + channel * 4, &s, 4);
    }
}

// COM init that tolerates Qt's existing STA on the caller's thread.
struct ScopedCom {
    bool ok = false;
    ScopedCom() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
        m_uninit = SUCCEEDED(hr);
    }
    ~ScopedCom() {
        if (m_uninit) CoUninitialize();
    }
private:
    bool m_uninit = false;
};

inline ComPtr<IMMDeviceEnumerator> makeEnumerator() {
    ComPtr<IMMDeviceEnumerator> e;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                     __uuidof(IMMDeviceEnumerator), &e);
    return e;
}

inline QString deviceFriendlyName(IMMDevice *dev) {
    ComPtr<IPropertyStore> store;
    if (FAILED(dev->OpenPropertyStore(STGM_READ, &store))) return {};
    PROPVARIANT pv;
    PropVariantInit(&pv);
    QString name;
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &pv)) &&
        pv.vt == VT_LPWSTR) {
        name = QString::fromWCharArray(pv.pwszVal);
    }
    PropVariantClear(&pv);
    return name;
}

} // namespace vcwin

#endif // _WIN32
