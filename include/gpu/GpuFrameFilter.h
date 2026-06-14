#pragma once

#include <QImage>

// Seam for future GPU frame filters (blur, virtual background, HDR tone-map,
// denoise, …). The decode → filter → preview/virtualcam pipeline will run
// frames through an ordered list of these. SCAFFOLD ONLY: the sole concrete
// implementation today is PassthroughFilter, which returns the frame unchanged.
// Real filters (CUDA-backed) plug in here later.
class GpuFrameFilter {
public:
    virtual ~GpuFrameFilter() = default;

    // Transform an input frame into an output frame. Implementations may return
    // the input unchanged (passthrough) or a processed copy.
    virtual QImage apply(const QImage &in) = 0;

    // Human-readable filter name (logging / debug UI).
    virtual const char *name() const = 0;
};

// No-op placeholder: the identity filter. Proves the interface/wiring without
// doing any GPU work yet.
class PassthroughFilter : public GpuFrameFilter {
public:
    QImage apply(const QImage &in) override { return in; }
    const char *name() const override { return "Passthrough"; }
};
