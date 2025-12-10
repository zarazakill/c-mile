// imagewriter.cpp
#include "imagewriter.h"
#include "devicemanager.h"
#include "utils.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstring>
#include <cerrno>

ImageWriter::ImageWriter(const Config& cfg, QObject* parent)
: QThread(parent), m_cfg(cfg) {}

void ImageWriter::cancel() {
    m_cancelled.store(true, std::memory_order_release);
}

void ImageWriter::run() {
    m_cancelled.store(false, std::memory_order_release);

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

    // --- ПРОВЕРКИ ДО НАЧАЛА ЗАПИСИ ---
    // Проверка свободного места для временных файлов
    qint64 freeSpace = Utils::getFreeSpace("/tmp");
    qint64 imageSize = imgInfo.size();
    if (freeSpace > 0 && freeSpace < imageSize * 2) {
        emit finished(false, "Недостаточно свободного места в /tmp");
        return;
    }

    // Проверка целостности архива перед записью
    if (Utils::isCompressedArchive(m_cfg.imagePath)) {
        emit progress(2, "Проверка целостности архива...");
        if (!Utils::verifyArchiveIntegrity(m_cfg.imagePath)) {
            if (!m_cfg.force) {
                emit finished(false, "Образ поврежден. Используйте опцию 'Принудительная запись' для продолжения");
                return;
            }
            emit progress(3, "Предупреждение: возможны проблемы с архивом");
        }
    }

    // Периодически проверяем флаг отмены
    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
        return;
    }

    emit progress(5, "Размонтирование устройства...");

    auto [unmountSuccess, unmountMessage] = DeviceManager::unmountAll(m_cfg.devicePath);

    if (!unmountSuccess) {
        QString errorMsg = QString("Ошибка размонтирования:\n%1").arg(unmountMessage);

        if (!m_cfg.force) {
            emit finished(false, errorMsg);
            return;
        }

        // В режиме принудительной записи предупреждаем, но продолжаем
        emit progress(6, QString("Предупреждение: %1").arg(unmountMessage));
        logDeviceStatus("WARNING", "Принудительная запись с несмонтированными разделами");
    } else {
        emit progress(6, unmountMessage);
    }

    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
        return;
    }

    // Проверка размера образа
    if (!Utils::checkImageFitsDevice(m_cfg.imagePath, m_cfg.devicePath)) {
        if (!m_cfg.force) {
            emit finished(false, "Размер образа превышает размер устройства!");
            return;
        }
        emit progress(7, "Предупреждение: образ больше устройства");
    }

    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
        return;
    }

    emit progress(10, "Запись образа...");
    if (!writeImage()) {
        emit finished(false, "Ошибка записи");
        return;
    }

    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
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
}

// УБРАТЬ static из этой функции, так как она использует m_cancelled
QByteArray ImageWriter::computeHash(const QString& path, qint64 maxSize) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;

    // Буфер для чтения
    const qint64 bufferSize = 64 * 1024; // 64KB

    while (!file.atEnd() && (maxSize < 0 || total < maxSize)) {
        // Проверка отмены при вычислении хэша
        if (m_cancelled.load(std::memory_order_acquire)) {
            return {};
        }

        qint64 toRead = (maxSize < 0) ? bufferSize : qMin<qint64>(bufferSize, maxSize - total);
        QByteArray chunk = file.read(toRead);
        if (chunk.isEmpty()) break;
        hash.addData(chunk);
        total += chunk.size();
    }
    return hash.result();
}

bool ImageWriter::writeImage() {
    int inputFd = open(m_cfg.imagePath.toLocal8Bit().constData(), O_RDONLY);
    if (inputFd < 0) {
        emit progress(-1, QString("Ошибка открытия файла образа: %1").arg(strerror(errno)));
        return false;
    }

    int outputFd = open(m_cfg.devicePath.toLocal8Bit().constData(), O_WRONLY | O_SYNC);
    if (outputFd < 0) {
        close(inputFd);
        emit progress(-1, QString("Ошибка открытия устройства: %1").arg(strerror(errno)));
        return false;
    }

    struct stat st;
    if (fstat(inputFd, &st) != 0) {
        close(inputFd); close(outputFd);
        emit progress(-1, QString("Ошибка получения размера файла: %1").arg(strerror(errno)));
        return false;
    }
    qint64 totalSize = st.st_size;
    qint64 written = 0;

    // Оптимальный размер буфера (адаптивный)
    size_t bufferSize = 4 * 1024 * 1024; // 4MB по умолчанию

    // Попытка определить оптимальный размер блока для файловой системы
    struct statvfs vfs;
    if (statvfs(m_cfg.devicePath.toLocal8Bit().constData(), &vfs) == 0) {
        // Используем оптимальный размер блока для файловой системы
        bufferSize = vfs.f_bsize;
        // Ограничиваем разумными пределами
        if (bufferSize < 512) bufferSize = 512;
        if (bufferSize > 16 * 1024 * 1024) bufferSize = 16 * 1024 * 1024; // Макс 16MB
        emit progress(10, QString("Используется размер блока: %1").arg(Utils::formatSize(bufferSize)));
    } else {
        // Если не удалось получить информацию о FS, используем размер по умолчанию
        emit progress(10, QString("Используется размер блока по умолчанию: %1").arg(Utils::formatSize(bufferSize)));
    }

    std::unique_ptr<char[]> buffer(new char[bufferSize]);

    while (written < totalSize) {
        // Проверка отмены - атомарное чтение
        if (m_cancelled.load(std::memory_order_acquire)) {
            close(inputFd); close(outputFd);
            return false;
        }

        ssize_t toRead = qMin<ssize_t>(bufferSize, totalSize - written);
        ssize_t nRead = read(inputFd, buffer.get(), toRead);
        if (nRead <= 0) {
            emit progress(-1, QString("Ошибка чтения файла: %1").arg(strerror(errno)));
            break;
        }

        ssize_t nWritten = write(outputFd, buffer.get(), nRead);
        if (nWritten != nRead) {
            emit progress(-1, QString("Ошибка записи на устройство: %1").arg(strerror(errno)));
            break;
        }

        written += nWritten;
        int percent = 10 + static_cast<int>((written * 80) / totalSize);
        emit progress(percent, QString("Записано: %1 / %2")
        .arg(Utils::formatSize(written))
        .arg(Utils::formatSize(totalSize)));
    }

    fsync(outputFd);
    close(inputFd);
    close(outputFd);

    bool success = (written == totalSize);
    if (!success) {
        emit progress(-1, QString("Запись прервана. Записано: %1 из %2")
        .arg(Utils::formatSize(written))
        .arg(Utils::formatSize(totalSize)));
    }
    return success;
}

bool ImageWriter::verifyImage() {
    QFileInfo imgInfo(m_cfg.imagePath);

    // Проверка отмены перед началом верификации
    if (m_cancelled.load(std::memory_order_acquire)) {
        return false;
    }

    QByteArray imgHash = computeHash(m_cfg.imagePath);

    // Проверка отмены после вычисления хэша файла
    if (m_cancelled.load(std::memory_order_acquire)) {
        return false;
    }

    QByteArray devHash = computeHash(m_cfg.devicePath, imgInfo.size());

    if (imgHash.isEmpty() || devHash.isEmpty()) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            return false;
        }
        emit progress(-1, "Ошибка вычисления хэша");
        return false;
    }

    bool match = (imgHash == devHash);
    if (!match) {
        emit progress(-1, QString("Хэши не совпадают:\nФайл: %1\nУстройство: %2")
        .arg(QString(imgHash.toHex()).left(32))
        .arg(QString(devHash.toHex()).left(32)));
    }
    return match;
}

void ImageWriter::logDeviceStatus(const QString& level, const QString& message) {
    qInfo().noquote() << QString("[%1] %2: %3")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
    .arg(level)
    .arg(message);
}
