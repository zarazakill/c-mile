// formatmanager.h
#pragma once

#include <QString>
#include <QList>
#include <QMap>

// Структура для информации о файловой системе
struct FilesystemInfo {
    QString name;           // Название (FAT32, NTFS, etc.)
    QString description;    // Описание
    quint64 minSize;        // Минимальный размер (байты)
    quint64 maxSize;        // Максимальный размер (байты)
    int defaultClusterSize; // Размер кластера по умолчанию (байты)
    QList<int> clusterSizes; // Поддерживаемые размеры кластеров
    bool supportsJournaling; // Поддержка журналирования
    bool caseSensitive;     // Чувствительность к регистру
    bool unicodeSupport;    // Поддержка Unicode
    QString osCompatibility; // Совместимые ОС
};

// Структура для рекомендаций по форматированию
struct FormatRecommendation {
    QString filesystem;     // Рекомендуемая ФС
    int clusterSize;        // Рекомендуемый размер кластера
    QString explanation;    // Объяснение выбора
    bool needsPartitioning; // Нужно ли создавать разделы
};

class FormatManager {
public:
    static FormatManager& instance();

    // Получить информацию о всех поддерживаемых файловых системах
    QList<FilesystemInfo> getAllFilesystems() const;

    // Получить информацию о конкретной файловой системе
    FilesystemInfo getFilesystemInfo(const QString& name) const;

    // Получить рекомендации по форматированию для устройства
    FormatRecommendation getRecommendation(const QString& devicePath,
                                           qint64 sizeBytes,
                                           const QString& intendedUse = "") const;

                                           // Проверить, поддерживается ли файловая система для размера
                                           bool isFilesystemSupported(const QString& fsName, qint64 sizeBytes) const;

                                           // Получить оптимальный размер кластера
                                           int getOptimalClusterSize(const QString& fsName, qint64 sizeBytes) const;

                                           // Форматировать устройство
                                           bool formatDevice(const QString& devicePath,
                                                             const QString& filesystem,
                                                             int clusterSize = 0,
                                                             const QString& label = "",
                                                             bool quickFormat = true) const;

                                                             // Создать разделы на устройстве
                                                             bool createPartition(const QString& devicePath,
                                                                                  qint64 sizeBytes,
                                                                                  const QString& filesystem,
                                                                                  const QString& label = "") const;

private:
    FormatManager();
    ~FormatManager() = default;

    QMap<QString, FilesystemInfo> m_filesystems;

    void initializeFilesystems();
};
