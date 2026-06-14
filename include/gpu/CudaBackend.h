#pragma once

#include "gpu/GpuBackend.h"

// NVIDIA-only backend. Real implementation is compiled only when the build is
// configured with VIEWCAM_ENABLE_CUDA (VIEWCAM_HAVE_CUDA defined); otherwise
// every method reports "unavailable" so the selector falls through to Vulkan.
class CudaBackend : public GpuBackend {
public:
    const char *name() const override { return "CUDA"; }
    std::string device() const override { return m_device; }
    bool initialize() override;
    bool isAvailable() const override { return m_available; }
    bool runProofOfLife() override;
    std::unique_ptr<GpuFrameFilter> createPassthroughFilter() override;
    ~CudaBackend() override;

    // Cheap policy probe for the selector: is there >=1 usable NVIDIA device?
    // Always false when built without CUDA.
    static bool nvidiaDevicePresent();

private:
    bool m_available = false;
    std::string m_device;
};
