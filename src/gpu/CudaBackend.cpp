#include "gpu/CudaBackend.h"
#include "gpu/GpuFrameFilter.h"
#include "core/Logger.h"

#ifdef VIEWCAM_HAVE_CUDA
#include <cuda_runtime.h>

// Defined in CudaProof.cu (nvcc): trivial vector-add kernel round-trip.
bool vc_cuda_run_proof_of_life();
#endif

bool CudaBackend::nvidiaDevicePresent() {
#ifdef VIEWCAM_HAVE_CUDA
    int count = 0;
    return cudaGetDeviceCount(&count) == cudaSuccess && count > 0;
#else
    return false;
#endif
}

bool CudaBackend::initialize() {
#ifdef VIEWCAM_HAVE_CUDA
    int count = 0;
    cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess || count == 0) {
        VC_WARN("CUDA: no usable device ({})",
                err != cudaSuccess ? cudaGetErrorString(err) : "0 devices");
        return false;
    }
    // Highest compute capability wins.
    int best = 0, bestScore = -1;
    cudaDeviceProp bestProp{};
    for (int i = 0; i < count; ++i) {
        cudaDeviceProp p{};
        if (cudaGetDeviceProperties(&p, i) != cudaSuccess)
            continue;
        const int score = p.major * 10 + p.minor;
        if (score > bestScore) { bestScore = score; best = i; bestProp = p; }
    }
    if (cudaSetDevice(best) != cudaSuccess)
        return false;
    cudaFree(nullptr); // prime the primary context

    // bestProp.name already includes the "NVIDIA" vendor prefix.
    m_device = bestProp.name;
    m_available = true;
    const double gib =
        static_cast<double>(bestProp.totalGlobalMem) / (1024.0 * 1024.0 * 1024.0);
    VC_INFO("CUDA device: {}, CC {}.{}, {:.1f} GB", bestProp.name,
            bestProp.major, bestProp.minor, gib);
    return true;
#else
    return false;
#endif
}

bool CudaBackend::runProofOfLife() {
#ifdef VIEWCAM_HAVE_CUDA
    if (!m_available)
        return false;
    return vc_cuda_run_proof_of_life();
#else
    return false;
#endif
}

std::unique_ptr<GpuFrameFilter> CudaBackend::createPassthroughFilter() {
    // Scaffold: real CUDA filters plug in here later.
    return std::make_unique<PassthroughFilter>();
}

CudaBackend::~CudaBackend() {
#ifdef VIEWCAM_HAVE_CUDA
    if (m_available)
        cudaDeviceReset();
#endif
}
