#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class State { Checking, Downloading, Applying, UpToDate, Error };
    Q_ENUM(State)

    Q_PROPERTY(State   state       READ state       NOTIFY stateChanged)
    Q_PROPERTY(int     progress    READ progress    NOTIFY progressChanged)
    Q_PROPERTY(QString statusText  READ statusText  NOTIFY statusTextChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)

    static UpdateChecker *create(QQmlEngine *, QJSEngine *);
    static UpdateChecker *instance();

    State   state()         const { return m_state; }
    int     progress()      const { return m_progress; }
    QString statusText()    const { return m_statusText; }
    QString latestVersion() const { return m_latestVersion; }

    void start();

signals:
    void stateChanged();
    void progressChanged();
    void statusTextChanged();
    void latestVersionChanged();

private:
    explicit UpdateChecker(QObject *parent = nullptr);

    void setState(State s);
    void setStatusText(const QString &t);
    void setProgress(int p);

    void onManifestReply(QNetworkReply *reply);
    void startDownload(const QString &url);
    void onDownloadFinished(QNetworkReply *reply);
    void applyUpdate();
    void finish(const QString &logMsg = {}, const QString &userMsg = {});

    State   m_state    = State::UpToDate;
    int     m_progress = 0;
    QString m_statusText;
    QString m_latestVersion;
    QByteArray m_downloadedBytes;

    QNetworkAccessManager *m_nam = nullptr;
    static UpdateChecker  *s_instance;
};
