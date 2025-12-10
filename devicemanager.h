// devicemanager.h
#pragma once

#include <QString>
#include <QList>
#include <QVariant>

struct DeviceInfo {
    QString path;
    QString sizeStr;      // "14.9G"
    quint64 sizeBytes = 0;
    QString model;
    bool removable = false;
    QList<QString> mountPoints;

    // Для QVariant
    bool operator==(const DeviceInfo& other) const {
        return path == other.path && sizeBytes == other.sizeBytes;
    }
};

Q_DECLARE_METATYPE(DeviceInfo)

class DeviceManager {
public:
    static QList<DeviceInfo> scanDevices();
    static bool unmountAll(const QString& devicePath);
    static QList<QString> getMountPoints(const QString& devicePath);
    static bool isRemovable(const QString& devName);
    static quint64 getDeviceSizeBytes(const QString& devName);
};
