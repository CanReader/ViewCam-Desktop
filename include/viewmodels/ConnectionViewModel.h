#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include "core/FrameData.h"

// Connection state + live stream statistics (FPS, bitrate, frame pacing),
// computed from real frame traffic. UI binds here, never to the socket.
class ConnectionViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY stateChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceOs READ deviceOs NOTIFY deviceChanged)
    // Active-camera descriptor from HELLO/STATUS (e.g. "Back · ƒ1.8"); empty = unknown.
    Q_PROPERTY(QString lens READ lens NOTIFY deviceChanged)
    Q_PROPERTY(QString host READ host NOTIFY deviceChanged)
    Q_PROPERTY(int port READ port NOTIFY deviceChanged)
    // Stable per-install id of the connected phone (empty for manual connects).
    // The sidebar de-dups by this, not by host — a Wi-Fi roam changes the IP.
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceChanged)
    Q_PROPERTY(int fps READ fps NOTIFY statsChanged)
    Q_PROPERTY(QString streamCodec READ streamCodec NOTIFY statsChanged)
    // Encoder codecs the connected phone advertises (HELLO). Always contains
    // "mjpeg"; drives the Settings protocol-picker gray-out.
    Q_PROPERTY(QStringList phoneCodecs READ phoneCodecs NOTIFY deviceChanged)
    Q_PROPERTY(double bitrateMbps READ bitrateMbps NOTIFY statsChanged)
    Q_PROPERTY(double frameIntervalMs READ frameIntervalMs NOTIFY statsChanged)
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY statsChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY statsChanged)
    // Phone battery (0-100) from HELLO/STATUS; -1 when unknown (UI shows "—").
    Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY batteryChanged)
    // Link quality derived from live frame pacing — 0=Terrible..3=Strong.
    Q_PROPERTY(int linkQuality READ linkQuality NOTIFY statsChanged)
    Q_PROPERTY(QString linkQualityLabel READ linkQualityLabel NOTIFY statsChanged)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY uptimeChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(bool sessionLimited READ sessionLimited NOTIFY sessionLimitedChanged)
    // The connected phone's Pro entitlement (from HELLO/STATUS). QML gates 4K on
    // it; false when disconnected or on a free phone.
    Q_PROPERTY(bool pro READ pro NOTIFY proChanged)

public:
    enum State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    explicit ConnectionViewModel(QObject *parent = nullptr);

    State state() const { return m_state; }
    bool isConnected() const { return m_state == Connected; }
    QString deviceName() const { return m_deviceName; }
    QString deviceOs() const { return m_deviceOs; }
    QString lens() const { return m_lens; }
    QString host() const { return m_host; }
    int port() const { return m_port; }
    QString deviceId() const { return m_deviceId; }
    int fps() const { return m_fps; }
    double bitrateMbps() const { return m_bitrateMbps; }
    double frameIntervalMs() const { return m_frameIntervalMs; }
    int frameWidth() const { return m_frameWidth; }
    int frameHeight() const { return m_frameHeight; }
    int battery() const { return m_battery; }
    bool charging() const { return m_charging; }
    int linkQuality() const;
    QString linkQualityLabel() const;
    QString uptimeText() const;
    QString errorText() const { return m_errorText; }
    bool sessionLimited() const { return m_sessionLimited; }
    bool pro() const { return m_pro; }

    // Driven by AppController
    void beginConnecting(const QString &name, const QString &host, int port,
                         const QString &deviceId = QString());
    // HELLO arrived: authoritative device metadata + transition to connected.
    void setHelloInfo(const QString &name, const QString &os);
    // Battery % (-1 = unknown) and charging flag, from HELLO or a STATUS frame.
    void setPowerStatus(int batteryPercent, bool charging);
    // Active-camera descriptor from HELLO or a STATUS lens-flip (empty = unknown).
    void setLens(const QString &lens);
    void markConnected();
    void markDisconnected();
    void markError(const QString &error);
    void setSessionLimited(bool limited);
    void setPro(bool pro);
    void onFrame(const FrameData &frame);

signals:
    void stateChanged();
    void deviceChanged();
    void batteryChanged();
    void statsChanged();
    void uptimeChanged();
    void errorChanged();
    void sessionLimitedChanged();
    void proChanged();

private:
    void setState(State s);
    void resetStats();

    State m_state = Disconnected;
    // One stream_connected per session, fired on the first decoded frame.
    bool m_reportedStreaming = false;
    QString m_deviceName;
    QString m_deviceOs;
    QString m_lens;
    QString m_host;
    int m_port = 8080;
    QString m_deviceId;
    QString m_errorText;
    bool m_sessionLimited = false;
    bool m_pro = false;
    int m_battery = -1;
    bool m_charging = false;

    // stats
    QTimer m_statsTimer;
    QTimer m_uptimeTimer;
    QElapsedTimer m_uptime;
    QElapsedTimer m_lastFrameTime;
    int m_framesThisSecond = 0;
    qint64 m_bytesThisSecond = 0;
    double m_intervalAccum = 0;
    int m_intervalSamples = 0;
    int m_fps = 0;
    double m_bitrateMbps = 0;
    double m_frameIntervalMs = 0;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
    // Codec of the most recent VIDEO frame (header format byte) — what the
    // phone is ACTUALLY sending, not a setting. "MJPEG" until told otherwise.
    QString m_streamCodec = QStringLiteral("MJPEG");
    QStringList m_phoneCodecs{QStringLiteral("mjpeg")};

public:
    QString streamCodec() const { return m_streamCodec; }
    QStringList phoneCodecs() const { return m_phoneCodecs; }
    void setPhoneCodecs(const QStringList &codecs) {
        if (codecs == m_phoneCodecs) return;
        m_phoneCodecs = codecs;
        emit deviceChanged();
    }
};
