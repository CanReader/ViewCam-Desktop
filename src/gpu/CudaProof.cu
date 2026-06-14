// CUDA proof-of-life: compiled by nvcc only when VIEWCAM_ENABLE_CUDA is ON.
// A trivial vector-add kernel that proves the toolchain compiles, links, and
// actually runs device code. No frame processing here yet.

#include <cuda_runtime.h>
#include <vector>

namespace {

__global__ void vecAdd(const float *a, const float *b, float *c, int n) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        c[i] = a[i] + b[i];
}

} // namespace

// Returns true if the kernel ran and produced correct results.
bool vc_cuda_run_proof_of_life() {
    constexpr int kN = 256;
    constexpr size_t kBytes = kN * sizeof(float);

    std::vector<float> ha(kN), hb(kN), hc(kN, 0.0f);
    for (int i = 0; i < kN; ++i) {
        ha[i] = static_cast<float>(i);
        hb[i] = static_cast<float>(2 * i);
    }

    float *da = nullptr, *db = nullptr, *dc = nullptr;
    bool ok = (cudaMalloc(&da, kBytes) == cudaSuccess) &&
              (cudaMalloc(&db, kBytes) == cudaSuccess) &&
              (cudaMalloc(&dc, kBytes) == cudaSuccess);

    if (ok) {
        cudaMemcpy(da, ha.data(), kBytes, cudaMemcpyHostToDevice);
        cudaMemcpy(db, hb.data(), kBytes, cudaMemcpyHostToDevice);

        const int threads = 256;
        const int blocks = (kN + threads - 1) / threads;
        vecAdd<<<blocks, threads>>>(da, db, dc, kN);

        ok = (cudaGetLastError() == cudaSuccess) &&
             (cudaDeviceSynchronize() == cudaSuccess);
        cudaMemcpy(hc.data(), dc, kBytes, cudaMemcpyDeviceToHost);
    }

    cudaFree(da);
    cudaFree(db);
    cudaFree(dc);

    if (ok) {
        for (int i = 0; i < kN; ++i) {
            if (hc[i] != ha[i] + hb[i]) {
                ok = false;
                break;
            }
        }
    }
    return ok;
}
