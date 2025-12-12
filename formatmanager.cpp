// formatmanager.cpp
#include "formatmanager.h"
#include "utils.h"
#include "devicemanager.h"
#include <QProcess>
#include <QDebug>
#include <QThread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <cstring>
#include <cerrno>

FormatManager& FormatManager::instance() {
    static FormatManager instance;
    return instance;
}

FormatManager::FormatManager() {
    initializeFilesystems();
}

void FormatManager::initializeFilesystems() {
    // FAT32
    FilesystemInfo fat32;
    fat32.name = "FAT32";
    fat32.description = "Совместимая файловая система для всех ОС и устройств";
    fat32.minSize = 33 * 1024 * 1024; // 33 MB
    fat32.maxSize = 2 * 1024 * 1024 * 1024LL; // 2 TB (теоретически 16TB, но ограничение Windows)
    fat32.defaultClusterSize = 4096; // 4KB
    fat32.clusterSizes = {512, 1024, 2048, 4096, 8192, 16384, 32768};
    fat32.supportsJournaling = false;
    fat32.caseSensitive = false;
    fat32.unicodeSupport = true;
    fat32.osCompatibility = "Windows, macOS, Linux, Android, игровые приставки";
    m_filesystems["FAT32"] = fat32;

    // exFAT
    FilesystemInfo exfat;
    exfat.name = "exFAT";
    exfat.description = "Современная файловая система для флеш-накопителей";
    exfat.minSize = 512 * 1024; // 512KB
    exfat.maxSize = 128 * 1024 * 1024 * 1024LL; // 128 PB
    exfat.defaultClusterSize = 131072; // 128KB
    exfat.clusterSizes = {512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576};
    exfat.supportsJournaling = false;
    exfat.caseSensitive = false;
    exfat.unicodeSupport = true;
    exfat.osCompatibility = "Windows (Vista+), macOS (10.6.5+), Linux (с драйвером)";
    m_filesystems["exFAT"] = exfat;

    // NTFS
    FilesystemInfo ntfs;
    ntfs.name = "NTFS";
    ntfs.description = "Файловая система Windows с поддержкой больших файлов и прав доступа";
    ntfs.minSize = 1 * 1024 * 1024; // 1MB
    ntfs.maxSize = 256 * 1024 * 1024 * 1024LL; // 256 TB
    ntfs.defaultClusterSize = 4096; // 4KB
    ntfs.clusterSizes = {512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072};
    ntfs.supportsJournaling = true;
    ntfs.caseSensitive = false;
    ntfs.unicodeSupport = true;
    ntfs.osCompatibility = "Windows, macOS (только чтение), Linux (полная поддержка)";
    m_filesystems["NTFS"] = ntfs;

    // ext4
    FilesystemInfo ext4;
    ext4.name = "ext4";
    ext4.description = "Современная файловая система Linux с журналированием";
    ext4.minSize = 16 * 1024 * 1024; // 16MB
    ext4.maxSize = 1 * 1024 * 1024 * 1024 * 1024LL; // 1 EB
    ext4.defaultClusterSize = 4096; // 4KB
    ext4.clusterSizes = {1024, 2048, 4096, 8192, 16384, 32768, 65536};
    ext4.supportsJournaling = true;
    ext4.caseSensitive = true;
    ext4.unicodeSupport = true;
    ext4.osCompatibility = "Linux, Windows (с драйвером), macOS (с драйвером)";
    m_filesystems["ext4"] = ext4;

    // ext3
    FilesystemInfo ext3;
    ext3.name = "ext3";
    ext3.description = "Журналируемая файловая система Linux";
    ext3.minSize = 16 * 1024 * 1024; // 16MB
    ext3.maxSize = 32 * 1024 * 1024 * 1024LL; // 32 TB
    ext3.defaultClusterSize = 4096; // 4KB
    ext3.clusterSizes = {1024, 2048, 4096, 8192, 16384, 32768};
    ext3.supportsJournaling = true;
    ext3.caseSensitive = true;
    ext3.unicodeSupport = true;
    ext3.osCompatibility = "Linux";
    m_filesystems["ext3"] = ext3;

    // ext2
    FilesystemInfo ext2;
    ext2.name = "ext2";
    ext2.description = "Базовая файловая система Linux";
    ext2.minSize = 16 * 1024 * 1024; // 16MB
    ext2.maxSize = 32 * 1024 * 1024 * 1024LL; // 32 TB
    ext2.defaultClusterSize = 4096; // 4KB
    ext2.clusterSizes = {1024, 2048, 4096, 8192, 16384, 32768};
    ext2.supportsJournaling = false;
    ext2.caseSensitive = true;
    ext2.unicodeSupport = true;
    ext2.osCompatibility = "Linux";
    m_filesystems["ext2"] = ext2;
}

QList<FilesystemInfo> FormatManager::getAllFilesystems() const {
    return m_filesystems.values();
}

FilesystemInfo FormatManager::getFilesystemInfo(const QString& name) const {
    return m_filesystems.value(name, FilesystemInfo());
}

FormatRecommendation FormatManager::getRecommendation(const QString& devicePath,
                                                      qint64 sizeBytes,
                                                      const QString& intendedUse) const {
                                                          FormatRecommendation recommendation;

                                                          // Определяем тип устройства
                                                          bool isRemovable = devicePath.contains("sd") || devicePath.contains("mmc");
                                                          bool isLargeDevice = sizeBytes > 32 * 1024 * 1024 * 1024LL; // > 32GB

                                                          // Анализируем предполагаемое использование
                                                          QString use = intendedUse.toLower();

                                                          if (use.contains("windows") || use.contains("win")) {
                                                              recommendation.filesystem = isLargeDevice ? "NTFS" : "FAT32";
                                                              recommendation.explanation = "Для Windows рекомендуется " + recommendation.filesystem;
                                                          }
                                                          else if (use.contains("linux")) {
                                                              recommendation.filesystem = "ext4";
                                                              recommendation.explanation = "Для Linux рекомендуется ext4";
                                                          }
                                                          else if (use.contains("mac") || use.contains("osx")) {
                                                              recommendation.filesystem = "exFAT";
                                                              recommendation.explanation = "Для macOS рекомендуется exFAT для лучшей совместимости";
                                                          }
                                                          else if (use.contains("android")) {
                                                              recommendation.filesystem = "FAT32";
                                                              recommendation.explanation = "Для Android рекомендуется FAT32";
                                                          }
                                                          else if (use.contains("мульти") || use.contains("multi") || use.contains("все")) {
                                                              recommendation.filesystem = "exFAT";
                                                              recommendation.explanation = "Для максимальной совместимости между разными ОС";
                                                          }
                                                          else if (use.contains("игры") || use.contains("game") || use.contains("приставка")) {
                                                              recommendation.filesystem = "exFAT";
                                                              recommendation.explanation = "Для игровых приставок рекомендуется exFAT";
                                                          }
                                                          else if (use.contains("резерв") || use.contains("backup") || use.contains("хранилище")) {
                                                              recommendation.filesystem = isLargeDevice ? "NTFS" : "FAT32";
                                                              recommendation.explanation = "Для хранения данных рекомендуется " + recommendation.filesystem;
                                                          }
                                                          else {
                                                              // Умный выбор по умолчанию
                                                              if (isRemovable) {
                                                                  if (sizeBytes <= 32 * 1024 * 1024 * 1024LL) { // ≤ 32GB
                                                                      recommendation.filesystem = "FAT32";
                                                                      recommendation.explanation = "USB-накопитель до 32GB - рекомендуется FAT32 для максимальной совместимости";
                                                                  } else {
                                                                      recommendation.filesystem = "exFAT";
                                                                      recommendation.explanation = "USB-накопитель более 32GB - рекомендуется exFAT для поддержки больших файлов";
                                                                  }
                                                              } else {
                                                                  // Внутренний диск
                                                                  recommendation.filesystem = "ext4";
                                                                  recommendation.explanation = "Внутренний диск - рекомендуется ext4 для Linux систем";
                                                              }
                                                          }

                                                          // Определяем оптимальный размер кластера
                                                          recommendation.clusterSize = getOptimalClusterSize(recommendation.filesystem, sizeBytes);

                                                          // Для больших устройств рекомендуем разделы
                                                          recommendation.needsPartitioning = (sizeBytes > 500 * 1024 * 1024 * 1024LL); // > 500GB

                                                          return recommendation;
                                                      }

                                                      bool FormatManager::isFilesystemSupported(const QString& fsName, qint64 sizeBytes) const {
                                                          if (!m_filesystems.contains(fsName)) return false;

                                                          const FilesystemInfo& fs = m_filesystems[fsName];
                                                          return sizeBytes >= fs.minSize && sizeBytes <= fs.maxSize;
                                                      }

                                                      int FormatManager::getOptimalClusterSize(const QString& fsName, qint64 sizeBytes) const {
                                                          if (!m_filesystems.contains(fsName)) return 4096; // По умолчанию 4KB

                                                          const FilesystemInfo& fs = m_filesystems[fsName];

                                                          // Логика выбора оптимального размера кластера
                                                          if (fsName == "FAT32") {
                                                              if (sizeBytes <= 2 * 1024 * 1024 * 1024LL) { // ≤ 2GB
                                                                  return 4096; // 4KB
                                                              } else {
                                                                  return 32768; // 32KB
                                                              }
                                                          }
                                                          else if (fsName == "exFAT") {
                                                              if (sizeBytes <= 8 * 1024 * 1024 * 1024LL) { // ≤ 8GB
                                                                  return 32768; // 32KB
                                                              } else if (sizeBytes <= 32 * 1024 * 1024 * 1024LL) { // ≤ 32GB
                                                                  return 131072; // 128KB
                                                              } else {
                                                                  return 262144; // 256KB
                                                              }
                                                          }
                                                          else if (fsName == "NTFS") {
                                                              if (sizeBytes <= 16 * 1024 * 1024 * 1024LL) { // ≤ 16GB
                                                                  return 4096; // 4KB
                                                              } else {
                                                                  return 8192; // 8KB
                                                              }
                                                          }
                                                          else if (fsName == "ext4" || fsName == "ext3" || fsName == "ext2") {
                                                              // Для ext файловых систем
                                                              if (sizeBytes <= 1 * 1024 * 1024 * 1024LL) { // ≤ 1GB
                                                                  return 1024; // 1KB
                                                              } else if (sizeBytes <= 16 * 1024 * 1024 * 1024LL) { // ≤ 16GB
                                                                  return 4096; // 4KB
                                                              } else {
                                                                  return 8192; // 8KB
                                                              }
                                                          }

                                                          return fs.defaultClusterSize;
                                                      }

                                                      bool FormatManager::formatDevice(const QString& devicePath,
                                                                                       const QString& filesystem,
                                                                                       int clusterSize,
                                                                                       const QString& label,
                                                                                       bool quickFormat) const {
                                                                                           // Проверяем, что устройство не смонтировано
                                                                                           QStringList mountPoints = DeviceManager::getMountPoints(devicePath);
                                                                                           if (!mountPoints.isEmpty()) {
                                                                                               qWarning() << "Устройство смонтировано:" << mountPoints;
                                                                                               return false;
                                                                                           }

                                                                                           // Определяем команды форматирования в зависимости от файловой системы
                                                                                           QString program;
                                                                                           QStringList arguments;

                                                                                           if (filesystem == "FAT32") {
                                                                                               program = "mkfs.fat";
                                                                                               arguments << "-F" << "32";
                                                                                               if (clusterSize > 0) {
                                                                                                   arguments << "-S" << QString::number(clusterSize);
                                                                                               }
                                                                                               if (!label.isEmpty()) {
                                                                                                   arguments << "-n" << label.left(11); // FAT32 ограничение 11 символов
                                                                                               }
                                                                                               arguments << devicePath;
                                                                                           }
                                                                                           else if (filesystem == "exFAT") {
                                                                                               program = "mkfs.exfat";
                                                                                               if (!label.isEmpty()) {
                                                                                                   arguments << "-n" << label.left(15); // exFAT ограничение 15 символов
                                                                                               }
                                                                                               arguments << devicePath;
                                                                                           }
                                                                                           else if (filesystem == "NTFS") {
                                                                                               program = "mkfs.ntfs";
                                                                                               arguments << "-f"; // Быстрое форматирование
                                                                                               if (clusterSize > 0) {
                                                                                                   arguments << "-c" << QString::number(clusterSize);
                                                                                               }
                                                                                               if (!label.isEmpty()) {
                                                                                                   arguments << "-L" << label.left(32); // NTFS ограничение 32 символа
                                                                                               }
                                                                                               if (quickFormat) {
                                                                                                   arguments << "-Q";
                                                                                               }
                                                                                               arguments << devicePath;
                                                                                           }
                                                                                           else if (filesystem.startsWith("ext")) {
                                                                                               program = QString("mkfs.%1").arg(filesystem);
                                                                                               if (clusterSize > 0) {
                                                                                                   arguments << "-b" << QString::number(clusterSize);
                                                                                               }
                                                                                               if (!label.isEmpty()) {
                                                                                                   arguments << "-L" << label.left(16); // ext ограничение 16 символов
                                                                                               }
                                                                                               if (quickFormat) {
                                                                                                   arguments << "-E" << "lazy_itable_init=1,lazy_journal_init=1";
                                                                                               }
                                                                                               arguments << devicePath;
                                                                                           }
                                                                                           else {
                                                                                               qWarning() << "Неподдерживаемая файловая система:" << filesystem;
                                                                                               return false;
                                                                                           }

                                                                                           // Выполняем форматирование
                                                                                           QProcess process;
                                                                                           process.setProcessChannelMode(QProcess::MergedChannels);

                                                                                           qDebug() << "Форматирование:" << program << arguments;

                                                                                           process.start(program, arguments);

                                                                                           if (!process.waitForStarted()) {
                                                                                               qWarning() << "Не удалось запустить процесс форматирования:" << program;
                                                                                               return false;
                                                                                           }

                                                                                           // Ждем завершения с таймаутом
                                                                                           if (!process.waitForFinished(30000)) { // 30 секунд
                                                                                               qWarning() << "Таймаут форматирования";
                                                                                               process.kill();
                                                                                               return false;
                                                                                           }

                                                                                           if (process.exitCode() != 0) {
                                                                                               qWarning() << "Ошибка форматирования:" << process.readAllStandardError();
                                                                                               return false;
                                                                                           }

                                                                                           qDebug() << "Форматирование успешно завершено";
                                                                                           return true;
                                                                                       }

                                                                                       bool FormatManager::createPartition(const QString& devicePath,
                                                                                                                           qint64 sizeBytes,
                                                                                                                           const QString& filesystem,
                                                                                                                           const QString& label) const {
                                                                                                                               // Используем parted для создания разделов
                                                                                                                               QProcess process;

                                                                                                                               // Создаем таблицу разделов GPT для больших дисков, MBR для маленьких
                                                                                                                               QString partitionTable = (sizeBytes > 2 * 1024 * 1024 * 1024LL) ? "gpt" : "msdos";

                                                                                                                               // Создаем таблицу разделов
                                                                                                                               process.start("parted", QStringList() << devicePath << "mklabel" << partitionTable);
                                                                                                                               process.waitForFinished();

                                                                                                                               if (process.exitCode() != 0) {
                                                                                                                                   qWarning() << "Ошибка создания таблицы разделов:" << process.readAllStandardError();
                                                                                                                                   return false;
                                                                                                                               }

                                                                                                                               // Создаем раздел на всем устройстве
                                                                                                                               process.start("parted", QStringList() << devicePath << "mkpart" << "primary"
                                                                                                                               << filesystem << "0%" << "100%");
                                                                                                                               process.waitForFinished();

                                                                                                                               if (process.exitCode() != 0) {
                                                                                                                                   qWarning() << "Ошибка создания раздела:" << process.readAllStandardError();
                                                                                                                                   return false;
                                                                                                                               }

                                                                                                                               // Форматируем раздел
                                                                                                                               QString partitionPath = devicePath + "1"; // Первый раздел
                                                                                                                               return formatDevice(partitionPath, filesystem, 0, label, true);
                                                                                                                           }
