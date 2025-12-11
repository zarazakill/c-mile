// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QTimer>
#include <QLabel>

#include "devicemanager.h"
#include "imagewriter.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI actions
    void refreshAll();
    void refreshDevices();
    void refreshDevicesSilent();
    void refreshImages();
    void browseImage();
    void browseImagesDir();
    void onStartWrite();
    void onCancelWrite();

    // UI events
    void onImageSelected(int index);
    void onDeviceSelected(int index);
    void onImageDoubleClicked(int row, int column);

    // Worker events
    void onWriteProgress(int percent, const QString& status);
    void onWriteFinished(bool success, const QString& message);

    // Logging
    void logMessage(const QString& level, const QString& msg);

    void onVerifyImage();
    void onCalculateHash();
    void onWipeDevice();
    void onTestSpeed();
    void onAdvancedSettings();

private:
    void setupUi();
    void setupWriteTab();
    void setupConnections();
    void loadSettings();
    void checkReadyState();

    void updateDevicesUi(const QList<DeviceInfo>& devices);
    void updateImageUi(const QList<ImageInfo>& images);

    // Новые функции (можно оставить как приватные, не слоты)
    bool validateWriteSettings();
    void verifyImageBeforeWrite();
    void calculateImageHash();
    void wipeDeviceSecurely();
    void testWriteSpeed();

    void handleWriteSuccess(const QString& message);
    void handleWriteError(const QString& message);
    void prepareForNewWrite();
    void clearSelection();
    QString formatMessageForDisplay(const QString& message);
    QString formatErrorMessage(const QString& error);
    void showErrorDetails(const QString& details);
    void saveErrorLog(const QString& error);

    // UI pointers
    QTabWidget* m_tabWidget = nullptr;

    QComboBox* m_deviceCombo = nullptr;
    QComboBox* m_imageCombo = nullptr;
    QComboBox* m_blockSizeCombo = nullptr;
    QCheckBox* m_verifyCheckbox = nullptr;
    QCheckBox* m_forceCheckbox = nullptr;

    QLineEdit* m_imagesDirEdit = nullptr;

    QLabel* m_deviceInfoLabel = nullptr;
    QLabel* m_imageInfoLabel = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QTextEdit* m_logView = nullptr;

    QPushButton* m_writeBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;

    // State
    QList<DeviceInfo> m_devices;
    QList<ImageInfo> m_images;

    DeviceInfo m_selectedDevice;
    ImageInfo m_selectedImage;

    ImageWriter* m_writer = nullptr;
    QTimer* m_refreshTimer = nullptr;
};
