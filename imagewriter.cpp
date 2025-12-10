#include "imagewriter.h"
#include "devicemanager.h"
#include "utils.h"
#include <QFile>
#include <QFileInfo>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>

ImageWriter::ImageWriter(const Config& cfg, QObject* parent)
: QThread(parent), m_cfg(cfg) {}

void ImageWriter::cancel() {
    m_cancelled = true;
}

void ImageWriter::run() {
    emit progress(0, "Проверка файла и устройства...");

    QFileInfo imgInfo(m_cfg.imagePath);
    if (!imgInfo.exists()) {
        emit finished(false, "Файл образа не найден: " + m_cfg.imagePath);
        return;
    }

    if (!QFile::exists(m_cfg.devicePath)) {
        emit finished(false, "Устройство не найдено: " + m_cfg.devicePath);
        return;
    }

    emit progress(5, "Размонтирование устройства...");
    if (!DeviceManager::unmountAll(m_cfg.devicePath)) {
        if (!m_cfg.force) {
            emit finished(false, "Не удалось размонтировать устройство");
            return;
        }
    }

    emit progress(10, "Запись образа...");
    if (!writeImage()) {
        emit finished(false, "Ошибка записи");
        return;
    }

    if (m_cfg.verify) {
        emit progress(90, "Проверка целостности...");
        if (!verifyImage()) {
            emit finished(false, "Проверка не пройдена");
            return;
        }
    }

    emit finished(true, "Запись успешно завершена!");

    // Проверка свободного места для временных файлов
    qint64 freeSpace = Utils::getFreeSpace("/tmp");
    if (freeSpace > 0 && freeSpace < m_cfg.imagePath.size() * 2) {
        emit finished(false, "Недостаточно свободного места в /tmp");
        return;
    }

    // Проверка целостности архива перед записью
    if (Utils::isCompressedArchive(m_cfg.imagePath)) {
        emit progress(2, "Проверка целостности архива...");
        if (!Utils::verifyArchiveIntegrity(m_cfg.imagePath)) {
            if (!m_cfg.force) {
                emit finished(false, "Образ поврежден. Используйте --force для принудительной записи");
                return;
            }
            emit progress(3, "Предупреждение: возможны проблемы с архивом");
        }
    }
}

QByteArray ImageWriter::computeHash(const QString& path, qint64 maxSize) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!file.atEnd() && (maxSize < 0 || total < maxSize)) {
        qint64 toRead = (maxSize < 0) ? 65536 : qMin<qint64>(65536, maxSize - total);
        QByteArray chunk = file.read(toRead);
        if (chunk.isEmpty()) break;
        hash.addData(chunk);
        total += chunk.size();
    }
    return hash.result();
}

bool ImageWriter::writeImage() {
    int inputFd = open(m_cfg.imagePath.toLocal8Bit().constData(), O_RDONLY);
    if (inputFd < 0) return false;

    int outputFd = open(m_cfg.devicePath.toLocal8Bit().constData(), O_WRONLY | O_SYNC);
    if (outputFd < 0) {
        close(inputFd);
        return false;
    }

    struct stat st;
    if (fstat(inputFd, &st) != 0) {
        close(inputFd); close(outputFd);
        return false;
    }
    qint64 totalSize = st.st_size;
    qint64 written = 0;
    const size_t bufferSize = 4 * 1024 * 1024; // 4MB
    std::unique_ptr<char[]> buffer(new char[bufferSize]);

    while (written < totalSize) {
        if (m_cancelled) {
            close(inputFd); close(outputFd);
            return false;
        }

        ssize_t toRead = qMin<ssize_t>(bufferSize, totalSize - written);
        ssize_t nRead = read(inputFd, buffer.get(), toRead);
        if (nRead <= 0) break;

        ssize_t nWritten = write(outputFd, buffer.get(), nRead);
        if (nWritten != nRead) break;

        written += nWritten;
        int percent = 10 + static_cast<int>((written * 80) / totalSize);
        emit progress(percent, QString("Записано: %1 / %2")
        .arg(Utils::formatSize(written))
        .arg(Utils::formatSize(totalSize)));
    }

    fsync(outputFd);
    close(inputFd);
    close(outputFd);
    return (written == totalSize);
}

bool ImageWriter::verifyImage() {
    QFileInfo imgInfo(m_cfg.imagePath);
    QByteArray imgHash = computeHash(m_cfg.imagePath);
    QByteArray devHash = computeHash(m_cfg.devicePath, imgInfo.size());
    return imgHash == devHash;
}
