#pragma once

#include <memory>
#include <string>

class GpuFrameFilter;

// Abstract GPU compute backend. The rest of the app talks ONLY to this — never
// to CUDA or Vulkan directly. Concrete backends: CudaBackend (NVIDIA),
// VulkanComputeBackend (AMD/Intel/portable), CpuBackend (universal fallback).
// Scaffold only: runProofOfLife() proves the toolchain; createPassthroughFilter
// is the seam real GPU filters will plug into later.
class GpuBackend {
public:
    virtual ~GpuBackend() = default;

    // Short identifier, e.g. "CUDA", "Vulkan compute", "CPU".
    virtual const char *name() const = 0;
    // Human-readable selected device, e.g. "NVIDIA RTX 4050" / "Intel Iris Xe".
    virtual std::string device() const = 0;

    // One-time setup. Non-fatal: returns false if the backend can't run here.
    virtual bool initialize() = 0;
    virtual bool isAvailable() const = 0;

    // Trivial GPU op proving the toolchain compiles, links and executes.
    virtual bool runProofOfLife() = 0;

    // Filter factory seam (scaffold returns the no-op Passthrough for now).
    virtual std::unique_ptr<GpuFrameFilter> createPassthroughFilter() = 0;
};

// Which backend to build. Auto applies the hardware policy (+ env override);
// the explicit kinds are used by the env override / the GPU-processing toggle.
enum class GpuBackendKind { Auto, Cuda, Vulkan, Cpu };

// Runtime selector. Policy (Auto): NVIDIA present -> CUDA; else Vulkan compute;
// else CPU. Honors the VIEWCAM_GPU_BACKEND=cuda|vulkan|cpu override only in
// Auto mode. Cpu is forced when requested (GPU-processing toggle OFF). Always
// returns a usable, initialized backend (CpuBackend in the worst case) and logs
// the choice + reason.
namespace GpuBackendFactory {
std::unique_ptr<GpuBackend> select(GpuBackendKind requested);
}
