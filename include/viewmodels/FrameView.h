#pragma once

#include <QImage>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// Lightweight publisher: AppController pushes decoded frames here and any
// number of FrameView items render them. Keeps QImage out of QML property
// bindings.
class FrameSource : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")

public:
    explicit FrameSource(QObject *parent = nullptr) : QObject(parent) {}
    void publish(const QImage &image) { emit frameReady(image); }

signals:
    void frameReady(const QImage &image);
};

// GPU video surface. Receives QImage frames from a FrameSource, uploads them
// as scene graph textures (QSGSimpleTextureNode) and letterboxes to fit.
class FrameView : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(FrameSource *source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)
    Q_PROPERTY(bool mirror READ mirror WRITE setMirror NOTIFY mirrorChanged)

public:
    explicit FrameView(QQuickItem *parent = nullptr);

    FrameSource *source() const { return m_source; }
    void setSource(FrameSource *source);

    bool hasFrame() const { return m_hasFrame; }

    bool mirror() const { return m_mirror; }
    void setMirror(bool mirror);

    Q_INVOKABLE void clear();

signals:
    void sourceChanged();
    void hasFrameChanged();
    void mirrorChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private slots:
    void onFrame(const QImage &image);

private:
    void setHasFrame(bool hasFrame);

    FrameSource *m_source = nullptr;
    QImage m_image;
    bool m_imageDirty = false;
    bool m_hasFrame = false;
    bool m_mirror = false;
};
