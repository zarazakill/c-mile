// imagewriter.cpp
#include "imagewriter.h"
#include "devicemanager.h"
#include "utils.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QElapsedTimer>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <cstring>
#include <cerrno>

ImageWriter::ImageWriter(const Config& cfg, QObject* parent)
: QThread(parent), m_cfg(cfg) {}

void ImageWriter::cancel() {
    m_cancelled.store(true, std::memory_order_release);
}

void ImageWriter::run() {
    m_cancelled.store(false, std::memory_order_release);

    emit progress(0, "Проверка файла и устройства...", 0, "-");

    QFileInfo imgInfo(m_cfg.imagePath);
    if (!imgInfo.exists()) {
        emit finished(false, "Файл образа не найден: " + m_cfg.imagePath);
        return;
    }

    if (!QFile::exists(m_cfg.devicePath)) {
        emit finished(false, "Устройство не найдено: " + m_cfg.devicePath);
        return;
    }

    emit progress(5, QString("Размер образа: %1").arg(Utils::formatSize(imgInfo.size())), 0, "-");

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
        emit progress(7, "Проверка целостности архива...", 0, "-");
        if (!Utils::verifyArchiveIntegrity(m_cfg.imagePath)) {
            if (!m_cfg.force) {
                emit finished(false, "Образ поврежден. Используйте опцию 'Принудительная запись' для продолжения");
                return;
            }
            emit progress(8, "Предупреждение: возможны проблемы с архивом", 0, "-");
        }
    }

    // Периодически проверяем флаг отмены
    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
        return;
    }

    emit progress(10, "Размонтирование устройства...", 0, "-");

    auto [unmountSuccess, unmountMessage] = DeviceManager::unmountAll(m_cfg.devicePath);

    if (!unmountSuccess) {
        QString errorMsg = QString("Ошибка размонтирования:\n%1").arg(unmountMessage);

        if (!m_cfg.force) {
            emit finished(false, errorMsg);
            return;
        }

        // В режиме принудительной записи предупреждаем, но продолжаем
        emit progress(12, QString("Предупреждение: %1").arg(unmountMessage), 0, "-");
        logDeviceStatus("WARNING", "Принудительная запись с несмонтированными разделами");
    } else {
        emit progress(12, unmountMessage, 0, "-");
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
        emit progress(15, "Предупреждение: образ больше устройства", 0, "-");
    }

    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
        return;
    }

    emit progress(20, "Запись образа...", 0, "-");
    if (!writeImage()) {
        emit finished(false, "Ошибка записи");
        return;
    }

    if (m_cancelled.load(std::memory_order_acquire)) {
        emit finished(false, "Операция отменена");
        return;
    }

    if (m_cfg.verify) {
        emit progress(95, "Проверка целостности...", 0, "-");
        if (!verifyImage()) {
            emit finished(false, "Проверка не пройдена");
            return;
        }
    }

    emit finished(true, "Запись успешно завершена!");
}

QByteArray ImageWriter::computeHash(const QString& path, qint64 maxSize) {
    int fd = open(path.toLocal8Bit().constData(), O_RDONLY);
    if (fd < 0) {
        qWarning() << "Ошибка открытия файла для хэширования:" << path << ":" << strerror(errno);
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    const qint64 bufferSize = 64 * 1024; // 64KB

    std::unique_ptr<char[]> buffer(new char[bufferSize]);

    while (maxSize < 0 || total < maxSize) {
        // Проверка отмены при вычислении хэша
        if (m_cancelled.load(std::memory_order_acquire)) {
            close(fd);
            return QByteArray();
        }

        qint64 toRead = (maxSize < 0) ? bufferSize : qMin<qint64>(bufferSize, maxSize - total);
        ssize_t nRead = read(fd, buffer.get(), toRead);

        if (nRead <= 0) {
            break; // Конец файла или ошибка
        }

        QByteArray chunk(buffer.get(), nRead);
        hash.addData(chunk);
        total += nRead;
    }

    close(fd);
    return hash.result();
}

bool ImageWriter::writeImage() {
    // Используем прямой ввод/вывод (O_DIRECT) для максимальной скорости
    int inputFd = open(m_cfg.imagePath.toLocal8Bit().constData(), O_RDONLY);
    if (inputFd < 0) {
        emit progress(-1, QString("Ошибка открытия файла образа: %1").arg(strerror(errno)), 0, "-");
        return false;
    }

    // Для устройства используем прямой доступ и отключаем кеширование
    int outputFd = open(m_cfg.devicePath.toLocal8Bit().constData(), O_WRONLY | O_SYNC | O_DIRECT);
    if (outputFd < 0) {
        // Если O_DIRECT не поддерживается, пробуем без него
        outputFd = open(m_cfg.devicePath.toLocal8Bit().constData(), O_WRONLY | O_SYNC);
        if (outputFd < 0) {
            close(inputFd);
            emit progress(-1, QString("Ошибка открытия устройства: %1").arg(strerror(errno)), 0, "-");
            return false;
        }
        emit progress(22, "Используется буферизированная запись", 0, "-");
    } else {
        emit progress(22, "Используется прямой доступ к устройству", 0, "-");
    }

    struct stat st;
    if (fstat(inputFd, &st) != 0) {
        close(inputFd); close(outputFd);
        emit progress(-1, QString("Ошибка получения размера файла: %1").arg(strerror(errno)), 0, "-");
        return false;
    }
    qint64 totalSize = st.st_size;

    emit progress(25, QString("Используется размер буфера: %1").arg(Utils::formatSize(m_cfg.blockSize)), 0, "-");

    qint64 written = 0;

    // Используем размер буфера из настроек, выровненный по 512 байтам для O_DIRECT
    size_t bufferSize = static_cast<size_t>(m_cfg.blockSize);
    if (bufferSize > totalSize) {
        bufferSize = totalSize;
    }

    // Выравниваем буфер для O_DIRECT
    #ifdef _POSIX_C_SOURCE
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0) {
        bufferSize = ((bufferSize + pageSize - 1) / pageSize) * pageSize;
    }
    #endif

    // Выделяем выровненную память для O_DIRECT
    void* alignedBuffer = nullptr;
    if (posix_memalign(&alignedBuffer, 512, bufferSize) != 0) {
        // Если не удалось выделить выровненную память, используем обычную
        alignedBuffer = malloc(bufferSize);
        emit progress(26, "Используется обычная память (без выравнивания)", 0, "-");
    }

    QElapsedTimer timer;
    timer.start();
    int lastPercent = 25;

    qint64 lastWritten = 0;
    qint64 lastTime = 0;
    double avgSpeed = 0;

    while (written < totalSize) {
        // Проверка отмены - атомарное чтение
        if (m_cancelled.load(std::memory_order_acquire)) {
            free(alignedBuffer);
            close(inputFd); close(outputFd);
            return false;
        }

        ssize_t toRead = qMin<ssize_t>(bufferSize, totalSize - written);
        ssize_t nRead = read(inputFd, alignedBuffer, toRead);
        if (nRead <= 0) {
            emit progress(-1, QString("Ошибка чтения файла: %1").arg(strerror(errno)), 0, "-");
            break;
        }

        ssize_t nWritten = write(outputFd, alignedBuffer, nRead);
        if (nWritten != nRead) {
            emit progress(-1, QString("Ошибка записи на устройство: %1").arg(strerror(errno)), 0, "-");
            break;
        }

        written += nWritten;
        double progressRatio = static_cast<double>(written) / totalSize;
        int percent = 25 + static_cast<int>(progressRatio * 70);  // От 25% до 95%

        // Рассчитываем скорость и оставшееся время
        qint64 elapsed = timer.elapsed();

        // Рассчитываем мгновенную и среднюю скорость
        if (elapsed > 0) {
            avgSpeed = (written / 1024.0 / 1024.0) / (elapsed / 1000.0);
        }

        // Рассчитываем оставшееся время
        QString timeLeft = "-";
        if (avgSpeed > 0.1) {  // Если скорость более-менее определена
            qint64 remainingBytes = totalSize - written;
            int remainingSec = static_cast<int>(remainingBytes / (avgSpeed * 1024 * 1024));

            if (remainingSec < 60) {
                timeLeft = QString("%1 сек").arg(remainingSec);
            } else if (remainingSec < 3600) {
                timeLeft = QString("%1 мин %2 сек").arg(remainingSec / 60).arg(remainingSec % 60);
            } else {
                int hours = remainingSec / 3600;
                int minutes = (remainingSec % 3600) / 60;
                int seconds = remainingSec % 60;
                timeLeft = QString("%1 ч %2 мин %3 сек").arg(hours).arg(minutes).arg(seconds);
            }
        }

        // Отправляем обновление каждые 1% или каждые 500 мс
        if (percent > lastPercent || elapsed - lastTime > 500) {
            QString status;
            if (elapsed < 2000) {  // Первые 2 секунды
                status = QString("Начало записи... %1%").arg(percent);
            } else {
                status = QString("Прогресс: %1%").arg(percent);
            }

            emit progress(percent, status, avgSpeed, timeLeft);

            lastPercent = percent;
            lastWritten = written;
            lastTime = elapsed;
        }
    }

    // Синхронизируем данные с устройством
    fsync(outputFd);

    #ifdef __linux__
    // Очищаем буферы устройства
    if (ioctl(outputFd, BLKFLSBUF, NULL) != 0) {
        qWarning() << "Предупреждение: не удалось очистить буферы устройства:" << strerror(errno);
    }
    #endif

    free(alignedBuffer);
    close(inputFd);
    close(outputFd);

    // Даем время устройству завершить операции
    QThread::msleep(1000);

    bool success = (written == totalSize);
    if (!success) {
        emit progress(-1, QString("Запись прервана. Записано: %1 из %2")
        .arg(Utils::formatSize(written))
        .arg(Utils::formatSize(totalSize)), 0, "-");
    } else {
        emit progress(95, "Запись завершена, синхронизация...", avgSpeed, "0 сек");
    }
    return success;
}

bool ImageWriter::verifyImage() {
    QFileInfo imgInfo(m_cfg.imagePath);

    emit progress(96, "Подготовка к проверке...", 0, "-");

    // Проверка отмены перед началом верификации
    if (m_cancelled.load(std::memory_order_acquire)) {
        return false;
    }

    // Ждем немного, чтобы данные точно записались на флешку
    QThread::msleep(2000);

    emit progress(97, "Вычисление хэша образа...", 0, "-");
    QByteArray imgHash = computeHash(m_cfg.imagePath);

    if (imgHash.isEmpty()) {
        emit progress(-1, "Ошибка вычисления хэша файла образа", 0, "-");
        return false;
    }

    // Проверка отмены после вычисления хэша файла
    if (m_cancelled.load(std::memory_order_acquire)) {
        return false;
    }

    emit progress(98, "Вычисление хэша устройства...", 0, "-");

    // Открываем устройство для чтения
    int deviceFd = open(m_cfg.devicePath.toLocal8Bit().constData(), O_RDONLY);
    if (deviceFd < 0) {
        emit progress(-1, QString("Ошибка открытия устройства: %1").arg(strerror(errno)), 0, "-");
        return false;
    }

    // Вычисляем хэш устройства
    QCryptographicHash deviceHash(QCryptographicHash::Sha256);
    qint64 total = 0;
    const qint64 bufferSize = 64 * 1024; // 64KB буфер

    std::unique_ptr<char[]> buffer(new char[bufferSize]);

    QElapsedTimer verifyTimer;
    verifyTimer.start();

    while (total < imgInfo.size()) {
        // Проверка отмены
        if (m_cancelled.load(std::memory_order_acquire)) {
            close(deviceFd);
            return false;
        }

        qint64 toRead = qMin<qint64>(bufferSize, imgInfo.size() - total);
        ssize_t nRead = read(deviceFd, buffer.get(), toRead);

        if (nRead <= 0) {
            emit progress(-1, QString("Ошибка чтения устройства при проверке: %1").arg(strerror(errno)), 0, "-");
            close(deviceFd);
            return false;
        }

        QByteArray chunk(buffer.get(), nRead);
        deviceHash.addData(chunk);
        total += nRead;

        // Обновляем прогресс проверки
        int percent = 98 + static_cast<int>((static_cast<double>(total) / imgInfo.size()) * 2);
        emit progress(percent, QString("Проверка: %1 / %2")
        .arg(Utils::formatSize(total))
        .arg(Utils::formatSize(imgInfo.size())), 0, "-");
    }

    close(deviceFd);

    QByteArray devHash = deviceHash.result();

    bool match = (imgHash == devHash);

    if (match) {
        emit progress(100, "Проверка пройдена успешно!", 0, "0 сек");
        return true;
    } else {
        // Детализируем ошибку
        QString imgHashStr = QString(imgHash.toHex()).left(32);
        QString devHashStr = QString(devHash.toHex()).left(32);

        qCritical() << "Хэши не совпадают!";
        qCritical() << "Файл:" << imgHashStr;
        qCritical() << "Устройство:" << devHashStr;

        emit progress(-1, QString("Хэши не совпадают (первые 8 символов)\n"
        "Файл: %1\n"
        "Устройство: %2")
        .arg(imgHashStr.left(8))
        .arg(devHashStr.left(8)), 0, "-");
        return false;
    }
}

void ImageWriter::logDeviceStatus(const QString& level, const QString& message) {
    qInfo().noquote() << QString("[%1] %2: %3")
    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
    .arg(level)
    .arg(message);
}
