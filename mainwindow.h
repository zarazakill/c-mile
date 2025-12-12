// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QProgressDialog>

#include "devicemanager.h"
#include "imagewriter.h"
#include "formatmanager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void refreshDevices();
    void browseImage();
    void onStartWrite();
    void onCancelWrite();
    void onImageSelected(int index);
    void onDeviceSelected(int index);
    void onWriteProgress(int percent, const QString& status, double speedMBps, const QString& timeLeft);
    void onWriteFinished(bool success, const QString& message);
    void logMessage(const QString& level, const QString& msg);

    void onFormatDevice();
    void onShowFormatDialog();
    void onFormatProgress(const QString& message, int percent);
    void onFormatFinished(bool success, const QString& message);

private:
    void setupUi();
    void setupConnections();
    void checkReadyState();
    bool validateWriteSettings();
    qint64 parseBlockSize(const QString& sizeStr);
    void updateSpeedInfo(double speedMBps, const QString& timeLeft);

    void showFormatDialog();
    void formatDeviceIntelligently(const QString& devicePath, qint64 sizeBytes,
                                   const QString& filesystem = "", int clusterSize = 0,
                                   const QString& label = "", bool quickFormat = true);
    void updateFormatProgress(const QString& message, int percent);

    // UI pointers
    QComboBox* m_deviceCombo = nullptr;
    QComboBox* m_imageCombo = nullptr;
    QComboBox* m_blockSizeCombo = nullptr;
    QComboBox* m_clusterSizeCombo = nullptr;  // Добавили размер кластера
    QCheckBox* m_verifyCheckbox = nullptr;
    QCheckBox* m_forceCheckbox = nullptr;

    QLabel* m_deviceInfoLabel = nullptr;
    QLabel* m_imageInfoLabel = nullptr;
    QLabel* m_speedLabel = nullptr;      // Для отображения скорости
    QLabel* m_timeLeftLabel = nullptr;   // Для отображения оставшегося времени

    QProgressBar* m_progressBar = nullptr;
    QTextEdit* m_logView = nullptr;

    QPushButton* m_writeBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_browseBtn = nullptr;

    // State
    QList<DeviceInfo> m_devices;

    DeviceInfo m_selectedDevice;
    ImageInfo m_selectedImage;

    ImageWriter* m_writer = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QElapsedTimer* m_writeTimer = nullptr;

    bool m_cancelled = false;
    qint64 m_totalImageSize = 0;
    QString m_lastProgressMessage;

    QPushButton* m_formatBtn = nullptr;
    FormatManager* m_formatManager = nullptr;
    QProgressDialog* m_formatProgressDialog = nullptr;
    QString m_currentFormatDevice;

};
