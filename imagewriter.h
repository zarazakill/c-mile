// imagewriter.h
#pragma once

#include <QThread>
#include <QCryptographicHash>
#include <QString>
#include <QVariant>
#include <atomic>

struct ImageInfo {
    QString path;
    qint64 size = 0;
    QString fileType;

    bool operator==(const ImageInfo& other) const {
        return path == other.path && size == other.size;
    }
};

Q_DECLARE_METATYPE(ImageInfo)

class ImageWriter : public QThread {
    Q_OBJECT

public:
    struct Config {
        QString imagePath;
        QString devicePath;
        bool verify = false;
        bool force = false;
        qint64 blockSize = 64 * 1024 * 1024;  // 64MB по умолчанию
        qint64 clusterSize = 32 * 1024;       // 32KB по умолчанию
    };

    explicit ImageWriter(const Config& cfg, QObject* parent = nullptr);
    void cancel();

signals:
    void progress(int percent, const QString& status, double speedMBps, const QString& timeLeft);
    void finished(bool success, const QString& message);

protected:
    void run() override;

private:
    Config m_cfg;
    std::atomic<bool> m_cancelled{false};

    QByteArray computeHash(const QString& path, qint64 maxSize = -1);
    bool writeImage();
    bool verifyImage();
    void logDeviceStatus(const QString& level, const QString& message);
};
