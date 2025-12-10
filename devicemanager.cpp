#include "devicemanager.h"
#include "utils.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <sys/mount.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

QList<DeviceInfo> DeviceManager::scanDevices() {
    QList<DeviceInfo> devices;

    QDir sysBlock("/sys/block");
    for (const QString& entry : sysBlock.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        // Пропускаем loop, ram, zram
        if (entry.startsWith("loop") || entry.startsWith("ram") || entry.startsWith("zram"))
            continue;
        if (!entry.startsWith("sd") && !entry.startsWith("mmcblk"))
            continue;

        QString devPath = "/dev/" + entry;
        if (!QFile::exists(devPath)) continue;

        bool removable = isRemovable(entry);
        quint64 sizeBytes = getDeviceSizeBytes(entry);
        QString sizeStr = Utils::formatSize(sizeBytes);
        QString model;
        QFile modelFile("/sys/block/" + entry + "/device/model");
        if (modelFile.open(QIODevice::ReadOnly))
            model = modelFile.readAll().trimmed();

        DeviceInfo dev;
        dev.path = devPath;
        dev.sizeBytes = sizeBytes;
        dev.sizeStr = sizeStr;
        dev.model = model;
        dev.removable = removable;
        dev.mountPoints = getMountPoints(devPath);

        devices.append(dev);
    }
    return devices;
}

bool DeviceManager::isRemovable(const QString& devName) {
    QFile file("/sys/block/" + devName + "/removable");
    if (!file.open(QIODevice::ReadOnly)) return false;
    return file.readAll().trimmed() == "1";
}

quint64 DeviceManager::getDeviceSizeBytes(const QString& devName) {
    QFile file("/sys/block/" + devName + "/size");
    if (!file.exists()) return 0;
    if (!file.open(QIODevice::ReadOnly)) return 0;
    bool ok;
    quint64 sectors = file.readAll().trimmed().toULongLong(&ok);
    return ok ? sectors * 512 : 0;
}

QList<QString> DeviceManager::getMountPoints(const QString& devicePath) {
    QList<QString> mounts;
    QFile mountsFile("/proc/mounts");
    if (!mountsFile.open(QIODevice::ReadOnly)) return mounts;

    QTextStream in(&mountsFile);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith(devicePath + " ")) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2) mounts << parts[1];
        }
    }
    return mounts;
}

bool DeviceManager::unmountAll(const QString& devicePath) {
    auto mounts = getMountPoints(devicePath);
    bool success = true;
    for (const QString& mp : mounts) {
        QByteArray mpBytes = mp.toLocal8Bit();
        if (umount2(mpBytes.constData(), MNT_DETACH) != 0) {
            success = false;
        }
    }
    return success;
}
