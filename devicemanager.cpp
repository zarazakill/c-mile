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

std::pair<bool, QString> DeviceManager::unmountPoint(const QString& mountPoint) {
    QByteArray mpBytes = mountPoint.toLocal8Bit();

    // Сначала пробуем lazy unmount
    if (umount2(mpBytes.constData(), MNT_DETACH) == 0) {
        return {true, ""};
    }

    int err = errno;
    QString errorMsg = QString("Не удалось размонтировать %1: %2")
    .arg(mountPoint)
    .arg(strerror(err));

    return {false, errorMsg};
}

QString DeviceManager::getMountInfo(const QString& devicePath) {
    auto mounts = getMountPoints(devicePath);

    if (mounts.isEmpty()) {
        return "Устройство не смонтировано";
    }

    QStringList info;
    info << QString("Устройство %1 смонтировано в %2 точках:").arg(devicePath).arg(mounts.size());

    QFile mountsFile("/proc/mounts");
    if (mountsFile.open(QIODevice::ReadOnly)) {
        QTextStream in(&mountsFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(devicePath + " ")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 4) {
                    QString mountInfo = QString("  • %1 (тип: %2, флаги: %3)")
                    .arg(parts[1])  // точка монтирования
                    .arg(parts[2])  // тип файловой системы
                    .arg(parts[3]); // флаги монтирования
                    info << mountInfo;
                }
            }
        }
        mountsFile.close();
    }

    return info.join("\n");
}

std::pair<bool, QString> DeviceManager::unmountAll(const QString& devicePath) {
    auto mounts = getMountPoints(devicePath);

    if (mounts.isEmpty()) {
        return {true, "Устройство не было смонтировано"};
    }

    QString mountInfo = getMountInfo(devicePath);
    qDebug() << "Попытка размонтирования:\n" << mountInfo;

    QStringList failedMounts;
    QStringList errorMessages;
    QStringList successMounts;

    // Сначала пытаемся размонтировать все точки
    for (const QString& mp : mounts) {
        auto [success, error] = unmountPoint(mp);
        if (success) {
            successMounts << mp;
        } else {
            failedMounts << mp;
            errorMessages << error;
        }
    }

    // Если все точки успешно размонтированы
    if (failedMounts.isEmpty()) {
        QString successMsg = QString("Успешно размонтировано %1 точек:").arg(successMounts.size());
        for (const QString& mp : successMounts) {
            successMsg += "\n  • " + mp;
        }
        return {true, successMsg};
    }

    // Формируем детальный отчет об ошибках
    QString errorReport = QString("Проблемы с размонтированием %1:\n").arg(devicePath);

    if (!successMounts.isEmpty()) {
        errorReport += QString("\nУспешно размонтировано (%1):").arg(successMounts.size());
        for (const QString& mp : successMounts) {
            errorReport += "\n  ✓ " + mp;
        }
    }

    errorReport += QString("\n\nНе удалось размонтировать (%1):").arg(failedMounts.size());
    for (int i = 0; i < failedMounts.size(); ++i) {
        errorReport += QString("\n  ✗ %1: %2").arg(failedMounts[i]).arg(errorMessages[i]);
    }

    // Дополнительная диагностика
    errorReport += QString("\n\nДиагностика:\n%1").arg(getMountInfo(devicePath));

    return {false, errorReport};
}
