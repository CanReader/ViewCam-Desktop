#pragma once

#include "gpu/GpuBackend.h"
#include "gpu/GpuFrameFilter.h"

// Universal fallback: no GPU, identity passthrough. Always available.
class CpuBackend : public GpuBackend {
public:
    const char *name() const override { return "CPU"; }
    std::string device() const override { return "CPU (software)"; }
    bool initialize() override { m_available = true; return true; }
    bool isAvailable() const override { return m_available; }
    bool runProofOfLife() override { return true; } // trivial: nothing to prove
    std::unique_ptr<GpuFrameFilter> createPassthroughFilter() override {
        return std::make_unique<PassthroughFilter>();
    }

private:
    bool m_available = false;
};
