#pragma once
// Manual declarations for IMFVirtualCamera and MFCreateVirtualCamera.
// Matches Windows 11 SDK 10.0.26100.0 um/mfvirtualcamera.h exactly.
// Not yet shipped in MinGW headers — available in mfsensorgroup.dll (Win 11 22H2+).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfidl.h>

enum MFVirtualCameraType     { MFVirtualCameraType_SoftwareCameraSource = 0 };
enum MFVirtualCameraLifetime { MFVirtualCameraLifetime_Session = 0, MFVirtualCameraLifetime_System = 1 };
enum MFVirtualCameraAccess   { MFVirtualCameraAccess_CurrentUser = 0, MFVirtualCameraAccess_AllUsers = 1 };

// IMFVirtualCamera vtable layout (SDK 10.0.26100.0):
//   IUnknown       : slots 0-2   (QueryInterface, AddRef, Release)
//   IMFAttributes  : slots 3-32  (30 methods — stubs below)
//   own methods    : slots 33-43
//
// Correct IID: {1C08A864-EF6C-4C75-AF59-5F2D68DA9563}
MIDL_INTERFACE("1C08A864-EF6C-4C75-AF59-5F2D68DA9563")
IMFVirtualCamera : public IUnknown {
    // --- IMFAttributes (30 stubs: vtable slots 3-32) ---
    virtual HRESULT STDMETHODCALLTYPE GetItem(REFGUID, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetItemType(REFGUID, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE CompareItem(REFGUID, void *, BOOL *) = 0;
    virtual HRESULT STDMETHODCALLTYPE Compare(void *, UINT32, BOOL *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetUINT32(REFGUID, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetUINT64(REFGUID, UINT64 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDouble(REFGUID, double *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGUID(REFGUID, GUID *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStringLength(REFGUID, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetString(REFGUID, LPWSTR, UINT32, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAllocatedString(REFGUID, LPWSTR *, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBlobSize(REFGUID, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBlob(REFGUID, BYTE *, UINT32, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAllocatedBlob(REFGUID, BYTE **, UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetUnknown(REFGUID, REFIID, void **) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetItem(REFGUID, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeleteItem(REFGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeleteAllItems() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetUINT32(REFGUID, UINT32) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetUINT64(REFGUID, UINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDouble(REFGUID, double) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetGUID(REFGUID, REFGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetString(REFGUID, LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBlob(REFGUID, const BYTE *, UINT32) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetUnknown(REFGUID, IUnknown *) = 0;
    virtual HRESULT STDMETHODCALLTYPE LockStore() = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockStore() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCount(UINT32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetItemByIndex(UINT32, GUID *, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE CopyAllItems(void *) = 0;
    // --- IMFVirtualCamera own methods (vtable slots 33-43) ---
    virtual HRESULT STDMETHODCALLTYPE AddDeviceSourceInfo(LPCWSTR) = 0;                         // 33
    virtual HRESULT STDMETHODCALLTYPE AddProperty(const void *, ULONG, const BYTE *, ULONG) = 0;// 34
    virtual HRESULT STDMETHODCALLTYPE AddRegistryEntry(LPCWSTR, LPCWSTR, DWORD,
                                                        const BYTE *, ULONG) = 0;               // 35
    virtual HRESULT STDMETHODCALLTYPE Start(void *pCallback) = 0;                               // 36 (IMFAsyncCallback* — pass nullptr for sync)
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;                                               // 37
    virtual HRESULT STDMETHODCALLTYPE Remove() = 0;                                             // 38
    virtual HRESULT STDMETHODCALLTYPE GetMediaSource(void **) = 0;                              // 39
    virtual HRESULT STDMETHODCALLTYPE SendCameraProperty(REFGUID, ULONG, ULONG,
                                                          void *, ULONG, void *,
                                                          ULONG, ULONG *) = 0;                  // 40
    virtual HRESULT STDMETHODCALLTYPE CreateSyncEvent(REFGUID, ULONG, ULONG,
                                                       HANDLE, void **) = 0;                    // 41
    virtual HRESULT STDMETHODCALLTYPE CreateSyncSemaphore(REFGUID, ULONG, ULONG,
                                                           HANDLE, LONG, void **) = 0;          // 42
    virtual HRESULT STDMETHODCALLTYPE Shutdown() = 0;                                           // 43
};

// sourceId is LPCWSTR — a null-terminated CLSID string, e.g. L"{2C7B3A5F-8D91-4E2F-B7C6-1A9E3D0F4C8B}"
// NOT a GUID pointer. Passing a GUID* causes the DLL to interpret binary bytes as a string,
// feed garbage to CLSIDFromString, and return CO_E_CLASSSTRING (0x800401F3).
using PFN_MFCreateVirtualCamera = HRESULT(STDAPICALLTYPE *)(
    MFVirtualCameraType,
    MFVirtualCameraLifetime,
    MFVirtualCameraAccess,
    LPCWSTR /*friendlyName*/,
    LPCWSTR /*sourceId — CLSID string*/,
    const GUID * /*categories*/,
    ULONG /*categoryCount*/,
    IMFVirtualCamera **);
