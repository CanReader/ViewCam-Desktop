#include "viewmodels/FrameView.h"
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

FrameView::FrameView(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

void FrameView::setSource(FrameSource *source) {
  if (m_source == source)
    return;
  if (m_source)
    disconnect(m_source, nullptr, this, nullptr);
  m_source = source;
  if (m_source)
    connect(m_source, &FrameSource::frameReady, this, &FrameView::onFrame);
  emit sourceChanged();
}

void FrameView::setMirror(bool mirror) {
  if (m_mirror == mirror)
    return;
  m_mirror = mirror;
  emit mirrorChanged();
  update();
}

void FrameView::clear() {
  m_image = QImage();
  m_imageDirty = true;
  setHasFrame(false);
  update();
}

void FrameView::onFrame(const QImage &image) {
  m_image = image;
  m_imageDirty = true;
  setHasFrame(!image.isNull());
  update();
}

void FrameView::setHasFrame(bool hasFrame) {
  if (m_hasFrame == hasFrame)
    return;
  m_hasFrame = hasFrame;
  emit hasFrameChanged();
}

QSGNode *FrameView::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
  auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);

  if (m_image.isNull()) {
    delete node;
    return nullptr;
  }

  if (!node) {
    node = new QSGSimpleTextureNode();
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);
    m_imageDirty = true;
  }

  if (m_imageDirty) {
    // createTextureFromImage needs the item to be in a window with a live scene
    // graph and a ready GPU context. If that isn't true yet (or the upload
    // fails), DON'T install a null texture — that paints nothing and looks like
    // "connected but blank". Leave the frame dirty so the next delivered frame
    // retries (frames arrive continuously while streaming).
    auto *tex = window() ? window()->createTextureFromImage(
                               m_image, QQuickWindow::TextureIsOpaque)
                         : nullptr;
    if (tex) {
      node->setTexture(tex);
      m_imageDirty = false;
    } else {
      qWarning("FrameView: texture upload unavailable (scene graph not ready); "
               "retrying on next frame");
    }
  }

  // First frame can arrive before the scene graph is ready: rather than emit a
  // node with no texture (blank preview / possible assert), drop this pass and
  // let the next frame's update retry the upload.
  if (!node->texture()) {
    delete node;
    return nullptr;
  }

  // Letterbox: preserve aspect ratio, centered
  const QSizeF item(width(), height());
  const QSizeF img(m_image.size());
  QSizeF scaled = img.scaled(item, Qt::KeepAspectRatio);
  QRectF target(QPointF((item.width() - scaled.width()) / 2,
                        (item.height() - scaled.height()) / 2),
                scaled);
  node->setRect(target);

  QRectF src(QPointF(0, 0), QSizeF(m_image.size()));
  if (m_mirror)
    src = QRectF(src.right(), src.top(), -src.width(), src.height());
  node->setSourceRect(src);

  return node;
}
