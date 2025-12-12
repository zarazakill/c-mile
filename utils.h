// utils.h
#pragma once

#include <QString>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QStorageInfo>
#include <QThread>
#include <memory>
#include <QTemporaryFile>

#include <fcntl.h>     // для open, O_WRONLY, O_SYNC
#include <unistd.h>    // для close, write, fsync, fstat
#include <sys/stat.h>  // для struct stat
#include <cstring>

namespace Utils {

    /// Форматирование байтов в человекочитаемый вид: 1.23 GB
    inline QString formatSize(qint64 bytes) {
        if (bytes <= 0) return "0 B";
        const char* suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB"};
        int i = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && i < 5) {
            size /= 1024.0;
            ++i;
        }
        return QString::number(size, 'f', 2) + " " + suffixes[i];
    }

    /// Определение типа файла по сигнатуре (магическим числам)
    inline QString detectFileType(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return "Unknown";

        QByteArray header = file.read(16);
        file.close();

        // Сигнатуры файлов
        if (header.startsWith("\x1F\x8B")) return "GZIP Compressed";
        if (header.startsWith("PK\x03\x04")) return "ZIP Archive";
        if (header.startsWith("7z\xBC\xAF\x27\x1C")) return "7-Zip Archive";
        if (header.startsWith("BZh")) return "BZIP2 Compressed";
        if (header.startsWith("\xFD\x37\x7A\x58\x5A\x00")) return "XZ Compressed";
        if (header.startsWith("ISO")) return "ISO Image";
        if (header.startsWith("\x53\x70\x69\x66\x66")) return "Apple Disk Image (DMG)";
        if (header.startsWith("\x45\x52\x01\x00")) return "Raw Disk Image (ERD)";
        if (header.startsWith("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1")) return "Microsoft Disk Image";

        // По расширению (fallback)
        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();
        if (ext == "img" || ext == "raw" || ext == "dd" || ext == "bin") return "Disk Image";
        if (ext == "iso") return "ISO Image";
        if (ext == "gz") return "GZIP Compressed";
        if (ext == "xz") return "XZ Compressed";
        if (ext == "bz2") return "BZIP2 Compressed";
        if (ext == "zip") return "ZIP Archive";
        if (ext == "7z") return "7-Zip Archive";
        if (ext == "dmg") return "Apple Disk Image";

        // tar.* — проверяем расширение целиком
        if (fi.fileName().endsWith(".tar.gz", Qt::CaseInsensitive)) return "Tar GZIP Archive";
        if (fi.fileName().endsWith(".tar.xz", Qt::CaseInsensitive)) return "Tar XZ Archive";
        if (fi.fileName().endsWith(".tar.bz2", Qt::CaseInsensitive)) return "Tar BZIP2 Archive";

        return "Binary File";
    }

    /// Вычисление хэша файла (SHA256 по умолчанию)
    inline QByteArray calculateFileHash(const QString& filePath,
                                        QCryptographicHash::Algorithm algorithm = QCryptographicHash::Sha256,
                                        qint64 maxSize = -1) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray();

        QCryptographicHash hash(algorithm);
        qint64 total = 0;
        const qint64 bufferSize = 64 * 1024; // 64KB буфер

        while (!file.atEnd() && (maxSize < 0 || total < maxSize)) {
            qint64 toRead = (maxSize < 0) ? bufferSize : qMin<qint64>(bufferSize, maxSize - total);
            QByteArray chunk = file.read(toRead);
            if (chunk.isEmpty()) break;
            hash.addData(chunk);
            total += chunk.size();
        }

        file.close();
        return hash.result();
    }

    /// Форматирование хэша в строку HEX
    inline QString hashToHex(const QByteArray& hash) {
        return hash.toHex().toUpper();
    }

    /// Проверка целостности архива/образа (базовая проверка)
    inline bool verifyArchiveIntegrity(const QString& filePath) {
        QFileInfo fi(filePath);
        QString ext = fi.suffix().toLower();

        // Для сжатых файлов проверяем структуру
        if (ext == "gz") {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) return false;

            // Проверяем сигнатуру GZIP
            QByteArray header = file.read(10);
            file.close();

            return header.startsWith("\x1F\x8B\x08");
        }

        // Для ZIP файлов проверяем структуру
        if (ext == "zip") {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) return false;

            // Ищем сигнатуру в конце файла (EOCD)
            file.seek(file.size() - 22);
            QByteArray eocd = file.read(22);
            file.close();

            return eocd.startsWith("PK\x05\x06");
        }

        // Для других типов пока возвращаем true
        return true;
    }

    /// Проверка свободного места в директории
    inline qint64 getFreeSpace(const QString& path) {
        QStorageInfo storage(path);
        if (storage.isValid() && storage.isReady()) {
            return storage.bytesFree();
        }
        return -1;
    }

    /// Безопасное заполнение нулями (один проход)
    inline bool zeroFillDevice(const QString& devicePath, qint64 size = -1) {
        int fd = ::open(devicePath.toLocal8Bit().constData(), O_WRONLY | O_SYNC);
        if (fd < 0) return false;

        // Определяем размер устройства, если не указан
        if (size < 0) {
            struct stat st;
            if (::fstat(fd, &st) != 0) {
                ::close(fd);
                return false;
            }
            size = st.st_size;
        }

        const size_t bufferSize = 4 * 1024 * 1024; // 4MB
        std::unique_ptr<char[]> zeroBuffer(new char[bufferSize]()); // Инициализируем нулями

        qint64 written = 0;
        while (written < size) {
            size_t toWrite = qMin<size_t>(bufferSize, size - written);
            ssize_t result = ::write(fd, zeroBuffer.get(), toWrite);
            if (result < 0) {
                ::close(fd);
                return false;
            }
            written += result;
        }

        ::fsync(fd);
        ::close(fd);
        return written == size;
    }

    /// Проверка размера образа относительно устройства
    inline bool checkImageFitsDevice(const QString& imagePath, const QString& devicePath) {
        QFileInfo imageInfo(imagePath);
        if (!imageInfo.exists()) return false;

        // Получаем размер образа
        qint64 imageSize = imageInfo.size();

        // Получаем размер устройства из /sys/block
        QFileInfo devInfo(devicePath);
        QString devName = devInfo.fileName();
        QFile sizeFile("/sys/block/" + devName + "/size");

        if (sizeFile.open(QIODevice::ReadOnly)) {
            bool ok;
            quint64 sectors = sizeFile.readAll().trimmed().toULongLong(&ok);
            if (ok) {
                quint64 deviceSizeBytes = sectors * 512;
                return imageSize <= deviceSizeBytes;
            }
        }

        // Если не удалось получить размер из sysfs, используем fallback
        return true; // Временно разрешаем запись
    }

    /// Создание временного файла для тестирования записи
    inline QString createTestPatternFile(qint64 sizeMB) {
        QTemporaryFile tempFile;
        if (tempFile.open()) {
            QString path = tempFile.fileName();
            tempFile.close();

            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                // Создаем паттерн данных для тестирования
                const int patternSize = 1024 * 1024; // 1MB
                QByteArray pattern(patternSize, 0);

                // Заполняем паттерн данными
                for (int i = 0; i < patternSize; i++) {
                    pattern[i] = static_cast<char>(i % 256);
                }

                for (qint64 i = 0; i < sizeMB; i++) {
                    if (file.write(pattern) != patternSize) {
                        return QString();
                    }
                }

                file.close();
                return path;
            }
        }
        return QString();
    }

    /// Получение информации о файловой системе устройства
    inline QString getFilesystemType(const QString& devicePath) {
        QFile mountsFile("/proc/mounts");
        if (!mountsFile.open(QIODevice::ReadOnly)) return "unknown";

        QTextStream in(&mountsFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(devicePath + " ")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    return parts[2]; // Тип файловой системы
                }
            }
        }

        // Проверяем через чтение суперблока
        QFile devFile(devicePath);
        if (!devFile.open(QIODevice::ReadOnly)) {
            return "unknown";
        }

        // Читаем первые 2048 байт (где обычно находится суперблок)
        const int BUFFER_SIZE = 2048;
        char buffer[BUFFER_SIZE];
        qint64 bytesRead = devFile.read(buffer, BUFFER_SIZE);
        devFile.close();

        if (bytesRead < 512) {
            return "unknown";
        }

        // Вспомогательная функция для безопасной проверки памяти
        auto safeMemcmp = [&](int offset, const char* pattern, int length) -> bool {
            if (offset + length > bytesRead) return false;
            return memcmp(buffer + offset, pattern, length) == 0;
        };

        // Проверяем сигнатуры файловых систем
        // FAT32
        if (safeMemcmp(0x52, "FAT32", 5)) {
            return "FAT32";
        }
        // FAT16
        if (safeMemcmp(0x36, "FAT16", 5)) {
            return "FAT16";
        }
        // FAT12
        if (safeMemcmp(0x36, "FAT12", 5)) {
            return "FAT12";
        }
        // NTFS
        if (safeMemcmp(0x03, "NTFS", 4)) {
            return "NTFS";
        }
        // ext2/3/4 (сигнатура 0x53 0xEF)
        if (safeMemcmp(0x438, "\x53\xEF", 2)) {
            // Проверяем версию ext (только если есть достаточно данных)
            if (bytesRead >= 0x458 + 4) {
                quint32 sRevLevel = *reinterpret_cast<quint32*>(buffer + 0x458);
                if (sRevLevel == 0 || sRevLevel == 1) return "ext2/3";
                if (sRevLevel == 2) return "ext4";
            }
            return "ext";
        }
        // exFAT
        if (safeMemcmp(0x03, "EXFAT", 5)) {
            return "exFAT";
        }
        // HFS/HFS+
        if (safeMemcmp(0x400, "BD", 2)) {
            return "HFS/HFS+";
        }
        // ISO9660 (CD/DVD)
        if (safeMemcmp(0x8001, "CD001", 5)) {
            return "ISO9660";
        }
        // UDF
        if (safeMemcmp(0x8001, "BEA01", 5)) {
            return "UDF";
        }

        // Проверяем таблицу разделов
        // MBR signature
        if (safeMemcmp(0x1FE, "\x55\xAA", 2)) {
            // Проверяем тип раздела (только если есть достаточно данных)
            if (bytesRead >= 0x1C2 + 1) {
                unsigned char partitionType = buffer[0x1C2];
                switch (partitionType) {
                    case 0x01: return "FAT12";
                    case 0x04: case 0x06: case 0x0E: return "FAT16";
                    case 0x0B: case 0x0C: return "FAT32";
                    case 0x07: return "NTFS/exFAT/HPFS";
                    case 0x83: return "Linux";
                    case 0x82: return "Linux swap";
                    case 0x8E: return "Linux LVM";
                    case 0xAF: return "macOS HFS";
                    case 0xEE: return "GPT protective";
                    default: return "MBR partition";
                }
            }
            return "MBR";
        }

        // GPT signature
        if (safeMemcmp(0x200, "EFI PART", 8)) {
            return "GPT partition";
        }

        // Проверяем, пустое ли устройство (все нули)
        bool allZeros = true;
        qint64 checkSize = qMin<qint64>(512, bytesRead);
        for (int i = 0; i < checkSize; i++) {
            if (buffer[i] != 0) {
                allZeros = false;
                break;
            }
        }

        if (allZeros) {
            return "empty";
        }

        return "unknown";
    }

    /// Проверка, является ли файл сжатым архивом
    inline bool isCompressedArchive(const QString& filePath) {
        QString type = detectFileType(filePath);
        return type.contains("Compressed") || type.contains("Archive");
    }

} // namespace Utils