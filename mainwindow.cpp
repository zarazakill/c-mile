// mainwindow.cpp
#include "mainwindow.h"
#include "devicemanager.h"
#include "imagewriter.h"
#include "utils.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>  // Изменено здесь
#include <QElapsedTimer>
#include <QFuture>  // для QFuture
#include <fcntl.h>         // для open, O_WRONLY, O_SYNC
#include <unistd.h>        // для close, write, fsync

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent),
m_deviceCombo(new QComboBox),
m_imageCombo(new QComboBox),
m_blockSizeCombo(new QComboBox),
m_verifyCheckbox(new QCheckBox("Проверить запись")),
m_forceCheckbox(new QCheckBox("Принудительная запись")),
m_devicesTable(new QTableWidget),
m_imagesTable(new QTableWidget),
m_imagesDirEdit(new QLineEdit),
m_progressBar(new QProgressBar),
m_logView(new QTextEdit),
m_writeBtn(new QPushButton("Записать образ")),
m_cancelBtn(new QPushButton("Отмена")),
m_refreshBtn(new QPushButton("Обновить"))
{
    setWindowTitle("Advanced Image Writer v3.0.0");
    resize(1000, 700);

    setupUi();
    setupConnections();
    loadSettings();

    refreshDevices();
    refreshImages();

    // Автообновление устройств каждые 5 сек
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshDevicesSilent);
    m_refreshTimer->start(5000);
}

void MainWindow::setupUi() {
    auto central = new QWidget(this);
    setCentralWidget(central);
    auto mainLayout = new QVBoxLayout(central);

    // Заголовок
    auto title = new QLabel("Advanced Image Writer v3.0.0");
    title->setAlignment(Qt::AlignCenter);
    QFont f;
    f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    mainLayout->addWidget(title);

    // Вкладки
    m_tabWidget = new QTabWidget;
    mainLayout->addWidget(m_tabWidget);

    setupWriteTab();
    setupDevicesTab();
    setupImagesTab();

    // Прогресс и лог
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(120);

    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_logView);

    // Кнопки
    auto btnLayout = new QHBoxLayout;
    btnLayout->addWidget(m_writeBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(btnLayout);
}

void MainWindow::setupWriteTab() {
    auto tab = new QWidget;
    auto layout = new QVBoxLayout(tab);

    // Образ
    auto imgGroup = new QGroupBox("Выбор образа");
    auto imgLay = new QVBoxLayout;
    auto imgTop = new QHBoxLayout;
    auto browseImgBtn = new QPushButton("Обзор...");
    connect(browseImgBtn, &QPushButton::clicked, this, &MainWindow::browseImage);
    imgTop->addWidget(new QLabel("Образ:"));
    imgTop->addWidget(m_imageCombo, 1);
    imgTop->addWidget(browseImgBtn);
    m_imageInfoLabel = new QLabel("Выберите образ");
    m_imageInfoLabel->setWordWrap(true);
    imgLay->addLayout(imgTop);
    imgLay->addWidget(m_imageInfoLabel);
    imgGroup->setLayout(imgLay);

    // Устройство
    auto devGroup = new QGroupBox("Выбор устройства");
    auto devLay = new QVBoxLayout;
    auto devTop = new QHBoxLayout;
    auto refreshDevBtn = new QPushButton("Обновить список");
    connect(refreshDevBtn, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    devTop->addWidget(new QLabel("Устройство:"));
    devTop->addWidget(m_deviceCombo, 1);
    devTop->addWidget(refreshDevBtn);
    m_deviceInfoLabel = new QLabel("Выберите устройство");
    m_deviceInfoLabel->setWordWrap(true);
    devLay->addLayout(devTop);
    devLay->addWidget(m_deviceInfoLabel);
    devGroup->setLayout(devLay);

    // Настройки
    auto settingsGroup = new QGroupBox("Настройки записи");
    auto settingsLay = new QVBoxLayout;
    auto blockLay = new QHBoxLayout;
    blockLay->addWidget(new QLabel("Размер блока:"));
    m_blockSizeCombo->addItems({"512", "1K", "2K", "4K", "8K", "16K", "32K", "64K", "128K", "256K", "512K", "1M", "2M", "4M", "8M", "16M"});
    m_blockSizeCombo->setCurrentText("4M");
    blockLay->addWidget(m_blockSizeCombo);
    blockLay->addStretch();
    m_verifyCheckbox->setChecked(true);
    settingsLay->addLayout(blockLay);
    settingsLay->addWidget(m_verifyCheckbox);
    settingsLay->addWidget(m_forceCheckbox);
    settingsGroup->setLayout(settingsLay);

    layout->addWidget(imgGroup);
    layout->addWidget(devGroup);
    layout->addWidget(settingsGroup);
    layout->addStretch();

    m_tabWidget->addTab(tab, "Запись");
}

void MainWindow::setupDevicesTab() {
    auto tab = new QWidget;
    auto layout = new QVBoxLayout(tab);
    m_devicesTable->setColumnCount(5);
    m_devicesTable->setHorizontalHeaderLabels({"Устройство", "Размер", "Модель", "Тип", "Монтирование"});
    m_devicesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_devicesTable);
    m_tabWidget->addTab(tab, "Устройства");
}

void MainWindow::setupImagesTab() {
    auto tab = new QWidget;
    auto layout = new QVBoxLayout(tab);

    auto ctrl = new QHBoxLayout;
    auto browseDirBtn = new QPushButton("Обзор...");
    auto scanBtn = new QPushButton("Сканировать");
    connect(browseDirBtn, &QPushButton::clicked, this, &MainWindow::browseImagesDir);
    connect(scanBtn, &QPushButton::clicked, this, &MainWindow::refreshImages);
    m_imagesDirEdit->setText(QDir::homePath() + "/Загрузки");
    ctrl->addWidget(new QLabel("Директория:"));
    ctrl->addWidget(m_imagesDirEdit, 1);
    ctrl->addWidget(browseDirBtn);
    ctrl->addWidget(scanBtn);
    layout->addLayout(ctrl);

    m_imagesTable->setColumnCount(4);
    m_imagesTable->setHorizontalHeaderLabels({"Имя файла", "Размер", "Тип", "Путь"});
    m_imagesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    connect(m_imagesTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onImageDoubleClicked);
    layout->addWidget(m_imagesTable);

    m_tabWidget->addTab(tab, "Образы");
}

void MainWindow::setupConnections() {
    connect(m_imageCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onImageSelected);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onDeviceSelected);
    connect(m_writeBtn, &QPushButton::clicked, this, &MainWindow::onStartWrite);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelWrite);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshAll);
}

void MainWindow::loadSettings() {
    m_cancelBtn->setEnabled(false);
}

// === Слоты ===

void MainWindow::refreshAll() {
    refreshDevices();
    refreshImages();
}

void MainWindow::refreshDevices() {
    logMessage("INFO", "Сканирование устройств...");
    m_devices = DeviceManager::scanDevices();
    updateDevicesUi(m_devices);
    logMessage("SUCCESS", QString("Найдено устройств: %1").arg(m_devices.size()));
}

void MainWindow::refreshDevicesSilent() {
    auto devs = DeviceManager::scanDevices();
    updateDevicesTable(devs);
}

void MainWindow::refreshImages() {
    QString dir = m_imagesDirEdit->text();
    logMessage("INFO", "Сканирование образов в " + dir);
    m_images.clear();

    QDir d(dir);
    if (!d.exists()) {
        logMessage("ERROR", "Директория не найдена: " + dir);
        return;
    }

    QStringList filters;
    filters << "*.img" << "*.iso" << "*.gz" << "*.xz" << "*.bz2"
    << "*.zip" << "*.raw" << "*.dd" << "*.bin" << "*.7z"
    << "*.tar.gz" << "*.tar.xz" << "*.tar.bz2";

    for (const QString& filter : filters) {
        QFileInfoList files = d.entryInfoList({filter}, QDir::Files | QDir::Readable);
        for (const QFileInfo& fi : files) {
            if (fi.isFile()) {
                ImageInfo img;
                img.path = fi.absoluteFilePath();
                img.size = fi.size();
                img.fileType = Utils::detectFileType(img.path);
                m_images.append(img);
            }
        }
    }

    updateImageUi(m_images);
    logMessage("SUCCESS", QString("Найдено образов: %1").arg(m_images.size()));
}

void MainWindow::updateDevicesUi(const QList<DeviceInfo>& devices) {
    m_deviceCombo->clear();
    for (const auto& dev : devices) {
        m_deviceCombo->addItem(dev.path + " (" + dev.sizeStr + ")", QVariant::fromValue(dev));
    }
    updateDevicesTable(devices);
}

void MainWindow::updateDevicesTable(const QList<DeviceInfo>& devices) {
    m_devicesTable->setRowCount(devices.size());
    for (int i = 0; i < devices.size(); ++i) {
        const auto& dev = devices[i];
        m_devicesTable->setItem(i, 0, new QTableWidgetItem(dev.path));
        m_devicesTable->setItem(i, 1, new QTableWidgetItem(dev.sizeStr));
        m_devicesTable->setItem(i, 2, new QTableWidgetItem(dev.model));
        m_devicesTable->setItem(i, 3, new QTableWidgetItem(dev.removable ? "USB/SD" : "Внутренний"));
        m_devicesTable->setItem(i, 4, new QTableWidgetItem(dev.mountPoints.join(", ")));
    }
}

void MainWindow::updateImageUi(const QList<ImageInfo>& images) {
    m_imageCombo->clear();
    m_imagesTable->setRowCount(images.size());
    for (int i = 0; i < images.size(); ++i) {
        const auto& img = images[i];
        QString name = QFileInfo(img.path).fileName();
        QString sizeStr = Utils::formatSize(img.size);

        m_imageCombo->addItem(name + " (" + sizeStr + ")", QVariant::fromValue(img));

        m_imagesTable->setItem(i, 0, new QTableWidgetItem(name));
        m_imagesTable->setItem(i, 1, new QTableWidgetItem(sizeStr));
        m_imagesTable->setItem(i, 2, new QTableWidgetItem(img.fileType));
        m_imagesTable->setItem(i, 3, new QTableWidgetItem(img.path));
    }
}

void MainWindow::onImageSelected(int index) {
    if (index < 0) return;
    QVariant var = m_imageCombo->itemData(index);
    if (!var.isValid()) return;
    m_selectedImage = var.value<ImageInfo>();

    QString name = QFileInfo(m_selectedImage.path).fileName();
    m_imageInfoLabel->setText(
        QString("<b>%1</b><br>Размер: %2<br>Тип: %3<br>Путь: %4")
        .arg(name)
        .arg(Utils::formatSize(m_selectedImage.size))
        .arg(m_selectedImage.fileType)
        .arg(m_selectedImage.path)
    );
    checkReadyState();
}

void MainWindow::onDeviceSelected(int index) {
    if (index < 0) return;
    QVariant var = m_deviceCombo->itemData(index);
    if (!var.isValid()) return;
    m_selectedDevice = var.value<DeviceInfo>();

    QString mounts = m_selectedDevice.mountPoints.isEmpty() ? "Нет" : m_selectedDevice.mountPoints.join(", ");
    m_deviceInfoLabel->setText(
        QString("<b>%1</b><br>Размер: %2<br>Модель: %3<br>Точки монтирования: %4<br>Съёмное: %5")
        .arg(m_selectedDevice.path)
        .arg(m_selectedDevice.sizeStr)
        .arg(m_selectedDevice.model)
        .arg(mounts)
        .arg(m_selectedDevice.removable ? "Да" : "Нет")
    );
    checkReadyState();
}

void MainWindow::onImageDoubleClicked(int row, int) {
    if (row >= 0 && row < m_images.size()) {
        QString path = m_images[row].path;
        m_tabWidget->setCurrentIndex(0);
        for (int i = 0; i < m_imageCombo->count(); ++i) {
            if (m_imageCombo->itemData(i).value<ImageInfo>().path == path) {
                m_imageCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void MainWindow::checkReadyState() {
    bool ready = !m_selectedImage.path.isEmpty() && !m_selectedDevice.path.isEmpty();
    m_writeBtn->setEnabled(ready);
}

void MainWindow::browseImage() {
    QString path = QFileDialog::getOpenFileName(this, "Выберите образ", m_imagesDirEdit->text(),
                                                "Образы дисков (*.img *.iso *.gz *.xz *.bz2 *.zip *.raw *.dd *.bin *.7z *.tar.gz *.tar.xz *.tar.bz2);;Все файлы (*)");
    if (!path.isEmpty()) {
        ImageInfo img;
        img.path = path;
        img.size = QFileInfo(path).size();
        img.fileType = Utils::detectFileType(path);

        m_imageCombo->insertItem(0, QFileInfo(path).fileName() + " (" + Utils::formatSize(img.size) + ")", QVariant::fromValue(img));
        m_imageCombo->setCurrentIndex(0);
        refreshImages();
    }
}

void MainWindow::browseImagesDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите директорию", m_imagesDirEdit->text());
    if (!dir.isEmpty()) {
        m_imagesDirEdit->setText(dir);
        refreshImages();
    }
}

void MainWindow::onStartWrite() {
    if (m_selectedImage.path.isEmpty() || m_selectedDevice.path.isEmpty()) {
        logMessage("ERROR", "Не выбран образ или устройство!");
        return;
    }

    QString msg = QString(
        "<b>ВНИМАНИЕ! Все данные на %1 будут уничтожены!</b><br><br>"
        "Образ: <b>%2</b><br>"
        "Устройство: <b>%1</b><br><br>"
        "Продолжить?")
    .arg(m_selectedDevice.path)
    .arg(QFileInfo(m_selectedImage.path).fileName());

    if (QMessageBox::question(this, "Подтверждение", msg, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        logMessage("INFO", "Операция отменена");
        return;
    }

    m_writeBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    ImageWriter::Config cfg;
    cfg.imagePath = m_selectedImage.path;
    cfg.devicePath = m_selectedDevice.path;
    cfg.verify = m_verifyCheckbox->isChecked();
    cfg.force = m_forceCheckbox->isChecked();

    m_writer = new ImageWriter(cfg, this);
    connect(m_writer, &ImageWriter::progress, this, &MainWindow::onWriteProgress);
    connect(m_writer, &ImageWriter::finished, this, &MainWindow::onWriteFinished);
    m_writer->start();

    logMessage("INFO", "Начало записи образа...");
}

void MainWindow::onCancelWrite() {
    if (m_writer) {
        m_writer->cancel();
        logMessage("WARNING", "Отмена операции...");
        m_cancelBtn->setEnabled(false);
    }
}

void MainWindow::onWriteProgress(int percent, const QString& status) {
    m_progressBar->setValue(percent);
    logMessage("PROGRESS", status);
}

void MainWindow::onWriteFinished(bool success, const QString& message) {
    m_progressBar->setValue(100);
    m_writeBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_progressBar->setVisible(false);

    if (success) {
        logMessage("SUCCESS", message);
        QMessageBox::information(this, "Успех", message);
    } else {
        logMessage("ERROR", message);
        QMessageBox::critical(this, "Ошибка", message);
    }

    refreshDevices();
    if (m_writer) {
        m_writer->deleteLater();
        m_writer = nullptr;
    }
}

void MainWindow::logMessage(const QString&, const QString& msg) {
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString color;
    if (msg.contains("ERROR", Qt::CaseSensitive)) color = "#ff4444";
    else if (msg.contains("SUCCESS", Qt::CaseSensitive)) color = "#44ff44";
    else if (msg.contains("WARNING", Qt::CaseSensitive)) color = "#ffaa44";
    else if (msg.contains("PROGRESS", Qt::CaseSensitive)) color = "#ff88ff";
    else color = "#4488ff";

    m_logView->append(QString("<font color=\"%1\">[%2] %3</font>").arg(color).arg(time).arg(msg));
    m_logView->moveCursor(QTextCursor::End);
    qInfo().noquote() << QString("[%1] %2").arg(time).arg(msg);
}

bool MainWindow::validateWriteSettings() {
    // Проверка размера образа относительно устройства
    if (!Utils::checkImageFitsDevice(m_selectedImage.path, m_selectedDevice.path)) {
        logMessage("ERROR", "Размер образа превышает размер устройства!");
        return false;
    }

    // Проверка свободного места для временных файлов
    qint64 freeSpace = Utils::getFreeSpace("/tmp");
    if (freeSpace > 0 && freeSpace < 100 * 1024 * 1024) { // Меньше 100MB
        logMessage("WARNING", "Мало свободного места в /tmp: " + Utils::formatSize(freeSpace));
    }

    // Проверка целостности архива
    if (Utils::isCompressedArchive(m_selectedImage.path)) {
        if (!Utils::verifyArchiveIntegrity(m_selectedImage.path)) {
            logMessage("WARNING", "Возможные проблемы с целостностью архива!");
        }
    }

    return true;
}

void MainWindow::verifyImageBeforeWrite() {
    if (m_selectedImage.path.isEmpty()) return;

    logMessage("INFO", "Проверка целостности образа...");

    // Проверка доступности файла
    QFileInfo fi(m_selectedImage.path);
    if (!fi.exists() || !fi.isReadable()) {
        logMessage("ERROR", "Файл образа недоступен для чтения!");
        return;
    }

    // Проверка размера
    if (fi.size() == 0) {
        logMessage("ERROR", "Файл образа пустой!");
        return;
    }

    // Для сжатых архивов проверяем структуру
    if (Utils::isCompressedArchive(m_selectedImage.path)) {
        if (!Utils::verifyArchiveIntegrity(m_selectedImage.path)) {
            logMessage("WARNING", "Образ может быть поврежден!");
        } else {
            logMessage("SUCCESS", "Структура архива в порядке");
        }
    }

    logMessage("SUCCESS", "Базовая проверка образа завершена");
}

void MainWindow::calculateImageHash() {
    if (m_selectedImage.path.isEmpty()) return;

    QProgressDialog progress("Вычисление хэша образа...", "Отмена", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    // Запускаем в отдельном потоке для вычисления хэша
    QFuture<void> future = QtConcurrent::run([this]() {
        QByteArray hash = Utils::calculateFileHash(m_selectedImage.path,
                                                   QCryptographicHash::Sha256);

        if (!hash.isEmpty()) {
            QString hashStr = Utils::hashToHex(hash);
            QMetaObject::invokeMethod(this, [this, hashStr]() {
                logMessage("INFO", "SHA256 хэш образа: " + hashStr);
                QMessageBox::information(this, "Хэш образа",
                                         "SHA256 хэш:\n" + hashStr);
            }, Qt::QueuedConnection);
        }
    });

    // Создаем таймер для обновления прогресса
    QTimer updateTimer;
    updateTimer.setInterval(100);
    int progressValue = 0;

    connect(&updateTimer, &QTimer::timeout, [&]() {
        if (future.isFinished()) {
            progress.setValue(100);
            updateTimer.stop();
        } else {
            progressValue = (progressValue + 5) % 100;
            progress.setValue(progressValue);
        }
    });

    updateTimer.start();

    // Запускаем событийный цикл
    while (!future.isFinished()) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            future.cancel();
            break;
        }
    }
}

void MainWindow::wipeDeviceSecurely() {
    if (m_selectedDevice.path.isEmpty()) return;

    QString msg = QString(
        "<b>ВНИМАНИЕ! Все данные на %1 будут безвозвратно уничтожены!</b><br><br>"
        "Устройство будет заполнено нулями.<br><br>"
        "Продолжить?")
    .arg(m_selectedDevice.path);

    if (QMessageBox::critical(this, "Подтверждение", msg,
        QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
        }

        logMessage("INFO", "Начало безопасного удаления данных...");

    // Отключаем автообновление
    m_refreshTimer->stop();

    QProgressDialog progress("Безопасное удаление данных...", "Отмена", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    QFuture<void> future = QtConcurrent::run([this]() {
        bool success = Utils::zeroFillDevice(m_selectedDevice.path);

        QMetaObject::invokeMethod(this, [this, success]() {
            if (success) {
                logMessage("SUCCESS", "Данные успешно удалены с устройства");
            } else {
                logMessage("ERROR", "Ошибка при удалении данных");
            }
            // Включаем автообновление обратно
            m_refreshTimer->start(5000);
            refreshDevices();
        }, Qt::QueuedConnection);
    });

    // Создаем таймер для обновления прогресса
    QTimer updateTimer;
    updateTimer.setInterval(100);
    int progressValue = 0;

    connect(&updateTimer, &QTimer::timeout, [&]() {
        if (future.isFinished()) {
            progress.setValue(100);
            updateTimer.stop();
        } else {
            progressValue = (progressValue + 5) % 100;
            progress.setValue(progressValue);
        }
    });

    updateTimer.start();

    // Запускаем событийный цикл
    while (!future.isFinished()) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            future.cancel();
            break;
        }
    }
}

void MainWindow::testWriteSpeed() {
    if (m_selectedDevice.path.isEmpty()) return;

    logMessage("INFO", "Тестирование скорости записи...");

    // Создаем временный файл для тестирования (10MB)
    QString testFile = Utils::createTestPatternFile(10);
    if (testFile.isEmpty()) {
        logMessage("ERROR", "Не удалось создать тестовый файл");
        return;
    }

    QProgressDialog progress("Тестирование скорости записи...", "Отмена", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    QFuture<void> future = QtConcurrent::run([this, testFile]() {
        QElapsedTimer timer;
        timer.start();

        int fd = ::open(m_selectedDevice.path.toLocal8Bit().constData(), O_WRONLY | O_SYNC);
        if (fd < 0) {
            QFile::remove(testFile);
            return;
        }

        QFile file(testFile);
        if (!file.open(QIODevice::ReadOnly)) {
            ::close(fd);
            QFile::remove(testFile);
            return;
        }

        qint64 totalSize = file.size();
        qint64 written = 0;
        const qint64 bufferSize = 4 * 1024 * 1024;

        while (!file.atEnd()) {
            QByteArray chunk = file.read(bufferSize);
            ssize_t result = ::write(fd, chunk.constData(), chunk.size());
            if (result != static_cast<ssize_t>(chunk.size())) break;
            written += result;
        }

        ::fsync(fd);
        ::close(fd);
        file.close();
        QFile::remove(testFile);

        qint64 elapsed = timer.elapsed();
        double speed = (elapsed > 0) ? (totalSize / 1024.0 / 1024.0) / (elapsed / 1000.0) : 0;

        QMetaObject::invokeMethod(this, [this, speed, elapsed]() {
            logMessage("SUCCESS",
                       QString("Тест скорости завершен: %1 МБ/с, время: %2 мс")
                       .arg(speed, 0, 'f', 2)
                       .arg(elapsed));
        }, Qt::QueuedConnection);
    });

    // Создаем таймер для обновления прогресса
    QTimer updateTimer;
    updateTimer.setInterval(100);
    int progressValue = 0;

    connect(&updateTimer, &QTimer::timeout, [&]() {
        if (future.isFinished()) {
            progress.setValue(100);
            updateTimer.stop();
        } else {
            progressValue = (progressValue + 5) % 100;
            progress.setValue(progressValue);
        }
    });

    updateTimer.start();

    // Запускаем событийный цикл
    while (!future.isFinished()) {
        QApplication::processEvents();
        if (progress.wasCanceled()) {
            future.cancel();
            break;
        }
    }
}

void MainWindow::onVerifyImage() {
    // Реализация проверки образа
    if (m_selectedImage.path.isEmpty()) {
        logMessage("ERROR", "Не выбран образ для проверки!");
        return;
    }
    verifyImageBeforeWrite();
}

void MainWindow::onCalculateHash() {
    // Реализация вычисления хэша
    if (m_selectedImage.path.isEmpty()) {
        logMessage("ERROR", "Не выбран образ для вычисления хэша!");
        return;
    }
    calculateImageHash();
}

void MainWindow::onWipeDevice() {
    // Реализация очистки устройства
    if (m_selectedDevice.path.isEmpty()) {
        logMessage("ERROR", "Не выбрано устройство для очистки!");
        return;
    }
    wipeDeviceSecurely();
}

void MainWindow::onTestSpeed() {
    // Реализация теста скорости
    if (m_selectedDevice.path.isEmpty()) {
        logMessage("ERROR", "Не выбрано устройство для теста скорости!");
        return;
    }
    testWriteSpeed();
}

void MainWindow::onAdvancedSettings() {
    // Реализация расширенных настроек
    QMessageBox::information(this, "Расширенные настройки",
                             "Расширенные настройки будут реализованы в будущих версиях.");
}
