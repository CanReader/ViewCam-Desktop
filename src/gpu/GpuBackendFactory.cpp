#include "gpu/GpuBackend.h"
#include "gpu/CpuBackend.h"
#include "gpu/CudaBackend.h"
#include "gpu/VulkanComputeBackend.h"
#include "core/Logger.h"

#include <cstdlib>
#include <memory>
#include <string>

namespace {

std::unique_ptr<GpuBackend> tryInit(std::unique_ptr<GpuBackend> b) {
    if (b && b->initialize() && b->isAvailable())
        return b;
    return nullptr;
}

std::unique_ptr<GpuBackend> makeCuda() { return tryInit(std::make_unique<CudaBackend>()); }
std::unique_ptr<GpuBackend> makeVulkan() { return tryInit(std::make_unique<VulkanComputeBackend>()); }

std::unique_ptr<GpuBackend> makeCpu() {
    auto b = std::make_unique<CpuBackend>();
    b->initialize();
    return b;
}

} // namespace

namespace GpuBackendFactory {

std::unique_ptr<GpuBackend> select(GpuBackendKind requested) {
    // GPU-processing toggle OFF -> CPU, unconditionally (ignores env override).
    if (requested == GpuBackendKind::Cpu) {
        VC_INFO("GPU backend: CPU (GPU processing disabled)");
        return makeCpu();
    }

    // Env override (Auto mode only). Lets us force the Vulkan path on this
    // hybrid box where CUDA would otherwise always win.
    const char *envc = std::getenv("VIEWCAM_GPU_BACKEND");
    const std::string env = envc ? envc : "";
    if (env == "cpu") {
        VC_INFO("GPU backend: CPU (forced via VIEWCAM_GPU_BACKEND=cpu)");
        return makeCpu();
    }
    if (env == "cuda") {
        if (auto b = makeCuda()) {
            VC_INFO("GPU backend: CUDA ({}) [forced via VIEWCAM_GPU_BACKEND]",
                    b->device());
            return b;
        }
        VC_WARN("VIEWCAM_GPU_BACKEND=cuda requested but CUDA unavailable; "
                "falling back");
    }
    if (env == "vulkan") {
        if (auto b = makeVulkan()) {
            VC_INFO("GPU backend: Vulkan compute ({}) [forced via "
                    "VIEWCAM_GPU_BACKEND]", b->device());
            return b;
        }
        VC_WARN("VIEWCAM_GPU_BACKEND=vulkan requested but Vulkan unavailable; "
                "falling back");
    }
    if (!env.empty() && env != "cuda" && env != "vulkan")
        VC_WARN("Ignoring unknown VIEWCAM_GPU_BACKEND='{}'", env);

    // Auto policy: NVIDIA present -> CUDA; else Vulkan compute; else CPU.
    if (CudaBackend::nvidiaDevicePresent()) {
        if (auto b = makeCuda()) {
            VC_INFO("GPU backend: CUDA ({}) — NVIDIA device present", b->device());
            return b;
        }
        VC_WARN("NVIDIA present but CUDA init failed; trying Vulkan");
    }
    if (auto b = makeVulkan()) {
        VC_INFO("GPU backend: Vulkan compute ({}) — no NVIDIA/CUDA path",
                b->device());
        return b;
    }
    VC_INFO("GPU backend: CPU — no usable GPU compute backend");
    return makeCpu();
}

} // namespace GpuBackendFactory
