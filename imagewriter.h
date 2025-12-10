// imagewriter.h
#pragma once

#include <QThread>
#include <QCryptographicHash>
#include <QString>
#include <QVariant>

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
    };

    explicit ImageWriter(const Config& cfg, QObject* parent = nullptr);
    void cancel();

signals:
    void progress(int percent, const QString& status);
    void finished(bool success, const QString& message);

protected:
    void run() override;

private:
    Config m_cfg;
    volatile bool m_cancelled = false;
    static QByteArray computeHash(const QString& path, qint64 maxSize = -1);
    bool writeImage();
    bool verifyImage();
};
