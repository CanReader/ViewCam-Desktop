#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Read-only product metadata, surfaced to QML as a singleton. Every field is
// backed by a macro from the CMake-generated ViewCamConfig.h — QML reads
// AppInfo.versionString / AppInfo.editionLabel etc. and never hardcodes them.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(QString edition READ edition CONSTANT)
    Q_PROPERTY(QString editionLabel READ editionLabel CONSTANT) // e.g. "Studio v1.0"
    Q_PROPERTY(QString organization READ organization CONSTANT)
    Q_PROPERTY(QString domain READ domain CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString homepage READ homepage CONSTANT)
    Q_PROPERTY(QString versionString READ versionString CONSTANT)
    Q_PROPERTY(QString shortVersion READ shortVersion CONSTANT)
    Q_PROPERTY(int versionMajor READ versionMajor CONSTANT)
    Q_PROPERTY(int versionMinor READ versionMinor CONSTANT)
    Q_PROPERTY(int versionPatch READ versionPatch CONSTANT)
    Q_PROPERTY(int buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(QString bundleId READ bundleId CONSTANT)
    Q_PROPERTY(QString copyright READ copyright CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString qtRuntimeVersion READ qtRuntimeVersion CONSTANT)
    Q_PROPERTY(QString buildType READ buildType CONSTANT)
    Q_PROPERTY(QString gitSha READ gitSha CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr) : QObject(parent) {}

    QString name() const;
    QString displayName() const;
    QString edition() const;
    QString editionLabel() const;
    QString organization() const;
    QString domain() const;
    QString description() const;
    QString homepage() const;
    QString versionString() const;
    QString shortVersion() const;
    int versionMajor() const;
    int versionMinor() const;
    int versionPatch() const;
    int buildNumber() const;
    QString bundleId() const;
    QString copyright() const;
    QString qtVersion() const;        // version Qt was built against
    QString qtRuntimeVersion() const; // version actually loaded at runtime
    QString buildType() const;
    QString gitSha() const;
};
