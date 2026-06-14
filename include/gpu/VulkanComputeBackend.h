#pragma once

#include "gpu/GpuBackend.h"
#include <memory>

// Portable GPU compute backend (NVIDIA / AMD / Intel) via a minimal standalone
// Vulkan compute pipeline running a proof-of-life compute shader. Used as the
// non-NVIDIA path (and force-selectable via VIEWCAM_GPU_BACKEND=vulkan). Real
// impl compiled only when VIEWCAM_ENABLE_VULKAN_COMPUTE (VIEWCAM_HAVE_VULKAN).
//
// Device policy: the Vulkan path is the "not-NVIDIA" route, so it prefers a
// non-NVIDIA physical device when one exists (so it lands on Intel/AMD here),
// falling back to any compute-capable device otherwise.
//
// Vulkan handles live in a PIMPL so this header stays Vulkan-free and includable
// from ordinary host code regardless of build config.
class VulkanComputeBackend : public GpuBackend {
public:
    VulkanComputeBackend();
    ~VulkanComputeBackend() override;

    const char *name() const override { return "Vulkan compute"; }
    std::string device() const override { return m_device; }
    bool initialize() override;
    bool isAvailable() const override { return m_available; }
    bool runProofOfLife() override;
    std::unique_ptr<GpuFrameFilter> createPassthroughFilter() override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    bool m_available = false;
    std::string m_device;
};
