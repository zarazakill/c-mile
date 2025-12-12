// mainwindow.cpp
#include "mainwindow.h"
#include "devicemanager.h"
#include "imagewriter.h"
#include "formatmanager.h"
#include "utils.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>
#include <QClipboard>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_deviceCombo(new QComboBox),
      m_imageCombo(new QComboBox),
      m_blockSizeCombo(new QComboBox),
      m_clusterSizeCombo(new QComboBox),
      m_verifyCheckbox(new QCheckBox("Проверить запись")),
      m_forceCheckbox(new QCheckBox("Принудительная запись")),
      m_progressBar(new QProgressBar),
      m_logView(new QTextEdit),
      m_writeBtn(new QPushButton("Записать образ")),
      m_cancelBtn(new QPushButton("Отмена")),
      m_refreshBtn(new QPushButton("Обновить")),
      m_browseBtn(new QPushButton("Обзор...")),
      m_formatBtn(new QPushButton("Форматировать")),
      m_writeTimer(new QElapsedTimer),
      m_speedLabel(new QLabel),
      m_timeLeftLabel(new QLabel),
      m_formatManager(&FormatManager::instance())
{
    setWindowTitle("C-mile v0.9.5");
    resize(450, 700);  // Увеличили размер для новых элементов
    
    setupUi();
    setupConnections();
    
    refreshDevices();
    
    // Автообновление устройств каждые 5 сек
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refreshDevices);
    m_refreshTimer->start(5000);
}

void MainWindow::setupUi() {
    auto central = new QWidget(this);
    setCentralWidget(central);
    auto mainLayout = new QVBoxLayout(central);
    
    // Заголовок
    auto title = new QLabel("C-mile v0.9.5");
    title->setAlignment(Qt::AlignCenter);
    QFont f;
    f.setPointSize(16); f.setBold(true);
    title->setFont(f);
    mainLayout->addWidget(title);
    
    // Группа выбора образа
    auto imgGroup = new QGroupBox("Выбор образа");
    auto imgLay = new QVBoxLayout;
    auto imgTop = new QHBoxLayout;
    
    imgTop->addWidget(new QLabel("Образ:"));
    imgTop->addWidget(m_imageCombo, 1);
    imgTop->addWidget(m_browseBtn);
    
    m_imageInfoLabel = new QLabel("Выберите файл образа (IMG, ISO, GZ, XZ, BZ2, ZIP, etc.)");
    m_imageInfoLabel->setWordWrap(true);
    m_imageInfoLabel->setStyleSheet("color: gray;");
    
    imgLay->addLayout(imgTop);
    imgLay->addWidget(m_imageInfoLabel);
    imgGroup->setLayout(imgLay);
    
    // Группа выбора устройства
    auto devGroup = new QGroupBox("Выбор устройства");
    auto devLay = new QVBoxLayout;
    auto devTop = new QHBoxLayout;
    auto devButtons = new QHBoxLayout;
    
    devTop->addWidget(new QLabel("Устройство:"));
    devTop->addWidget(m_deviceCombo, 1);
    
    // Кнопки для устройства
    devButtons->addWidget(m_refreshBtn);
    devButtons->addWidget(m_formatBtn);
    devButtons->addStretch();
    
    m_deviceInfoLabel = new QLabel("Выберите устройство");
    m_deviceInfoLabel->setWordWrap(true);
    m_deviceInfoLabel->setStyleSheet("color: gray;");
    
    devLay->addLayout(devTop);
    devLay->addLayout(devButtons);
    devLay->addWidget(m_deviceInfoLabel);
    devGroup->setLayout(devLay);
    
    // Группа настроек
    auto settingsGroup = new QGroupBox("Настройки записи");
    auto settingsLay = new QVBoxLayout;
    
    // Строка с выбором размера буфера (блока)
    auto blockSizeLayout = new QHBoxLayout;
    blockSizeLayout->addWidget(new QLabel("Размер буфера:"));
    
    // Добавляем варианты размеров буферов (оптимальные для записи)
    m_blockSizeCombo->addItems({
        "64 KB", "128 KB", "256 KB", "512 KB", 
        "1 MB", "2 MB", "4 MB", "8 MB",
        "16 MB", "32 MB", "64 MB", "128 MB", "256 MB"
    });
    m_blockSizeCombo->setCurrentText("64 MB");  // Оптимальный для USB 3.0
    
    blockSizeLayout->addWidget(m_blockSizeCombo);
    blockSizeLayout->addStretch();
    
    // Строка с выбором размера кластера (если форматируем)
    auto clusterSizeLayout = new QHBoxLayout;
    clusterSizeLayout->addWidget(new QLabel("Размер кластера (для форматирования):"));
    
    m_clusterSizeCombo->addItems({
        "4 KB", "8 KB", "16 KB", "32 KB", "64 KB", "128 KB",
        "256 KB", "512 KB", "1 MB", "2 MB"
    });
    m_clusterSizeCombo->setCurrentText("32 KB");  // Оптимальный для USB
    m_clusterSizeCombo->setToolTip("Размер кластера для файловой системы (если будете форматировать после записи)");
    
    clusterSizeLayout->addWidget(m_clusterSizeCombo);
    clusterSizeLayout->addStretch();
    
    m_verifyCheckbox->setChecked(true);
    
    settingsLay->addLayout(blockSizeLayout);
    settingsLay->addLayout(clusterSizeLayout);
    settingsLay->addWidget(m_verifyCheckbox);
    settingsLay->addWidget(m_forceCheckbox);
    settingsGroup->setLayout(settingsLay);
    
    // Добавляем группы в основной layout
    mainLayout->addWidget(imgGroup);
    mainLayout->addWidget(devGroup);
    mainLayout->addWidget(settingsGroup);
    
    // Прогресс
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    mainLayout->addWidget(m_progressBar);
    
    // Информация о скорости и времени
    auto infoLayout = new QHBoxLayout;
    m_speedLabel->setText("Скорость: -");
    m_timeLeftLabel->setText("Осталось: -");
    m_speedLabel->setStyleSheet("font-weight: bold; color: #0066cc;");
    m_timeLeftLabel->setStyleSheet("font-weight: bold; color: #cc6600;");
    infoLayout->addWidget(m_speedLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_timeLeftLabel);
    mainLayout->addLayout(infoLayout);
    
    // Лог
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(150);
    m_logView->setFont(QFont("Monospace", 9));
    mainLayout->addWidget(new QLabel("Лог:"));
    mainLayout->addWidget(m_logView);
    
    // Кнопки
    auto btnLayout = new QHBoxLayout;
    btnLayout->addWidget(m_writeBtn);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_refreshBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // Начальное состояние
    m_cancelBtn->setEnabled(false);
    m_speedLabel->setVisible(false);
    m_timeLeftLabel->setVisible(false);
}

void MainWindow::setupConnections() {
    connect(m_imageCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onImageSelected);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onDeviceSelected);
    connect(m_writeBtn, &QPushButton::clicked, this, &MainWindow::onStartWrite);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelWrite);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshDevices);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::browseImage);
    connect(m_formatBtn, &QPushButton::clicked, this, &MainWindow::onShowFormatDialog);
}

void MainWindow::refreshDevices() {
    // Получаем список устройств
    auto newDevices = DeviceManager::scanDevices();
    
    // Если список не изменился, не обновляем UI
    if (m_devices == newDevices) {
        return;
    }
    
    m_devices = newDevices;
    
    // Сохраняем текущий выбор
    QString currentDevicePath;
    if (m_deviceCombo->currentIndex() >= 0) {
        QVariant var = m_deviceCombo->currentData();
        if (var.isValid()) {
            DeviceInfo currentDevice = var.value<DeviceInfo>();
            currentDevicePath = currentDevice.path;
        }
    }
    
    m_deviceCombo->clear();
    
    int selectIndex = -1;
    for (int i = 0; i < m_devices.size(); ++i) {
        const auto& dev = m_devices[i];
        
        // Получаем информацию о файловой системе
        QString fsType = Utils::getFilesystemType(dev.path);
        
        QString displayText;
        if (fsType != "unknown" && fsType != "empty") {
            displayText = QString("%1 (%2, %3)").arg(dev.path).arg(dev.sizeStr).arg(fsType);
        } else {
            displayText = dev.path + " (" + dev.sizeStr + ")";
        }
        
        m_deviceCombo->addItem(displayText, QVariant::fromValue(dev));
        
        if (dev.path == currentDevicePath) {
            selectIndex = i;
        }
    }
    
    if (selectIndex >= 0) {
        m_deviceCombo->setCurrentIndex(selectIndex);
    } else if (!m_devices.isEmpty()) {
        m_deviceCombo->setCurrentIndex(0);
    }
    
    logMessage("INFO", QString("Найдено устройств: %1").arg(m_devices.size()));
}

void MainWindow::browseImage() {
    QString path = QFileDialog::getOpenFileName(this, 
        "Выберите образ", 
        QDir::homePath() + "/Загрузки",
        "Все поддерживаемые образы (*.img *.iso *.gz *.xz *.bz2 *.zip *.raw *.dd *.bin *.7z *.tar.gz *.tar.xz *.tar.bz2);;"
        "Все файлы (*)");
    
    if (!path.isEmpty()) {
        // Проверяем, есть ли уже этот образ в списке
        for (int i = 0; i < m_imageCombo->count(); ++i) {
            if (m_imageCombo->itemData(i).value<ImageInfo>().path == path) {
                m_imageCombo->setCurrentIndex(i);
                return;
            }
        }
        
        // Добавляем новый образ
        ImageInfo img;
        img.path = path;
        img.size = QFileInfo(path).size();
        img.fileType = Utils::detectFileType(path);
        
        QString displayName = QFileInfo(path).fileName() + " (" + Utils::formatSize(img.size) + ")";
        m_imageCombo->insertItem(0, displayName, QVariant::fromValue(img));
        m_imageCombo->setCurrentIndex(0);
    }
}

qint64 MainWindow::parseBlockSize(const QString& sizeStr) {
    QStringList parts = sizeStr.split(' ');
    if (parts.size() != 2) return 64 * 1024 * 1024;  // 64MB по умолчанию
    
    qint64 size = parts[0].toLongLong();
    QString unit = parts[1].toUpper();
    
    if (unit == "KB") return size * 1024;
    if (unit == "MB") return size * 1024 * 1024;
    if (unit == "GB") return size * 1024 * 1024 * 1024;
    
    return size;  // Байты по умолчанию
}

void MainWindow::onImageSelected(int index) {
    if (index < 0) {
        m_selectedImage = ImageInfo();
        m_imageInfoLabel->setText("Выберите файл образа");
        checkReadyState();
        return;
    }
    
    QVariant var = m_imageCombo->itemData(index);
    if (!var.isValid()) return;
    
    m_selectedImage = var.value<ImageInfo>();
    
    QString name = QFileInfo(m_selectedImage.path).fileName();
    m_imageInfoLabel->setText(
        QString("<b>%1</b><br>Размер: %2<br>Тип: %3")
        .arg(name)
        .arg(Utils::formatSize(m_selectedImage.size))
        .arg(m_selectedImage.fileType)
    );
    checkReadyState();
}

void MainWindow::onDeviceSelected(int index) {
    if (index < 0) {
        m_selectedDevice = DeviceInfo();
        m_deviceInfoLabel->setText("Выберите устройство");
        checkReadyState();
        return;
    }
    
    QVariant var = m_deviceCombo->itemData(index);
    if (!var.isValid()) return;
    
    m_selectedDevice = var.value<DeviceInfo>();
    
    // Получаем информацию о файловой системе
    QString fsType = Utils::getFilesystemType(m_selectedDevice.path);
    QString mounts = m_selectedDevice.mountPoints.isEmpty() ? 
        "Не смонтировано" : 
        "Смонтировано: " + m_selectedDevice.mountPoints.join(", ");
    
    m_deviceInfoLabel->setText(
        QString("<b>%1</b><br>Размер: %2<br>Модель: %3<br>Файловая система: %4<br>%5")
        .arg(m_selectedDevice.path)
        .arg(m_selectedDevice.sizeStr)
        .arg(m_selectedDevice.model)
        .arg(fsType)
        .arg(mounts)
    );
    checkReadyState();
}

void MainWindow::checkReadyState() {
    bool ready = !m_selectedImage.path.isEmpty() && !m_selectedDevice.path.isEmpty();
    m_writeBtn->setEnabled(ready);
}

void MainWindow::updateSpeedInfo(double speedMBps, const QString& timeLeft) {
    m_speedLabel->setText(QString("Скорость: %1 МБ/с").arg(speedMBps, 0, 'f', 1));
    m_timeLeftLabel->setText(QString("Осталось: %1").arg(timeLeft));
}

void MainWindow::onStartWrite() {
    if (m_selectedImage.path.isEmpty() || m_selectedDevice.path.isEmpty()) {
        logMessage("ERROR", "Не выбран образ или устройство!");
        return;
    }

    // Предварительная проверка
    if (!validateWriteSettings()) {
        if (!m_forceCheckbox->isChecked()) {
            logMessage("ERROR", "Проверка не пройдена. Используйте 'Принудительная запись' для продолжения.");
            return;
        }
        logMessage("WARNING", "Предупреждения проигнорированы (принудительная запись)");
    }

    QString msg = QString(
        "<b>ВНИМАНИЕ! Все данные на %1 будут уничтожены!</b><br><br>"
        "Образ: <b>%2</b><br>"
        "Устройство: <b>%1</b><br>"
        "Размер буфера: <b>%3</b><br><br>"
        "Продолжить?")
    .arg(m_selectedDevice.path)
    .arg(QFileInfo(m_selectedImage.path).fileName())
    .arg(m_blockSizeCombo->currentText());

    if (QMessageBox::question(this, "Подтверждение", msg, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        logMessage("INFO", "Операция отменена");
        return;
    }

    m_writeBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    // Показываем информацию о скорости
    m_speedLabel->setVisible(true);
    m_timeLeftLabel->setVisible(true);
    m_speedLabel->setText("Скорость: -");
    m_timeLeftLabel->setText("Осталось: -");

    ImageWriter::Config cfg;
    cfg.imagePath = m_selectedImage.path;
    cfg.devicePath = m_selectedDevice.path;
    cfg.verify = m_verifyCheckbox->isChecked();
    cfg.force = m_forceCheckbox->isChecked();
    cfg.blockSize = parseBlockSize(m_blockSizeCombo->currentText());
    cfg.clusterSize = parseBlockSize(m_clusterSizeCombo->currentText());

    // Сохраняем размер образа для расчета скорости
    m_totalImageSize = QFileInfo(m_selectedImage.path).size();
    m_writeTimer->restart();

    m_writer = new ImageWriter(cfg, this);
    connect(m_writer, &ImageWriter::progress, this, &MainWindow::onWriteProgress);
    connect(m_writer, &ImageWriter::finished, this, &MainWindow::onWriteFinished);
    m_writer->start();

    logMessage("INFO", QString("Начало записи образа: %1 на %2")
        .arg(QFileInfo(m_selectedImage.path).fileName())
        .arg(m_selectedDevice.path));
}

void MainWindow::onCancelWrite() {
    if (m_writer) {
        logMessage("WARNING", "Отмена операции...");
        m_cancelBtn->setEnabled(false);
        m_cancelled = true;
        
        m_writer->cancel();
        
        if (!m_writer->wait(3000)) {
            logMessage("ERROR", "Не удалось безопасно завершить операцию");
            m_writer->terminate();
            m_writer->wait();
        }
        
        m_writer->deleteLater();
        m_writer = nullptr;
        m_cancelled = false;
        m_writeBtn->setEnabled(true);
        m_progressBar->setVisible(false);
        m_progressBar->setValue(0);
        m_speedLabel->setVisible(false);
        m_timeLeftLabel->setVisible(false);
    }
}

void MainWindow::onWriteProgress(int percent, const QString& status, double speedMBps, const QString& timeLeft) {
    m_progressBar->setValue(percent);
    
    // Обновляем информацию о скорости и времени
    updateSpeedInfo(speedMBps, timeLeft);
    
    // Обновляем текст прогресс-бара
    m_progressBar->setFormat(QString("%1%").arg(percent));
    
    // Логируем сообщение только если оно новое
    if (status != m_lastProgressMessage) {
        logMessage("PROGRESS", status);
        m_lastProgressMessage = status;
    }
}

void MainWindow::onWriteFinished(bool success, const QString& message) {
    ImageWriter* finishedWriter = qobject_cast<ImageWriter*>(sender());
    
    if (finishedWriter && finishedWriter != m_writer) {
        finishedWriter->deleteLater();
        return;
    }
    
    m_progressBar->setValue(success ? 100 : 0);
    m_writeBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    
    if (m_writer) {
        disconnect(m_writer, nullptr, this, nullptr);
        
        if (m_writer->isRunning()) {
            m_writer->wait(100);
        }
        
        m_writer->deleteLater();
        m_writer = nullptr;
    }
    
    if (success) {
        logMessage("SUCCESS", message);
        
        // Рассчитываем общее время записи
        qint64 elapsed = m_writeTimer->elapsed();
        QString timeStr;
        if (elapsed < 1000) {
            timeStr = QString("%1 мс").arg(elapsed);
        } else if (elapsed < 60000) {
            timeStr = QString("%1 сек").arg(elapsed / 1000.0, 0, 'f', 1);
        } else {
            int minutes = elapsed / 60000;
            int seconds = (elapsed % 60000) / 1000;
            timeStr = QString("%1 мин %2 сек").arg(minutes).arg(seconds);
        }
        
        double speedMBps = (m_totalImageSize / 1024.0 / 1024.0) / (elapsed / 1000.0);
        
        // Обновляем финальную скорость
        m_speedLabel->setText(QString("Средняя скорость: %1 МБ/с").arg(speedMBps, 0, 'f', 1));
        m_timeLeftLabel->setText(QString("Время записи: %1").arg(timeStr));
        
        QMessageBox::information(this, "Успех", 
            QString("Запись образа успешно завершена!\n\n"
                   "Образ: %1\n"
                   "Устройство: %2\n"
                   "Время: %3\n"
                   "Средняя скорость: %4 МБ/с")
            .arg(QFileInfo(m_selectedImage.path).fileName())
            .arg(m_selectedDevice.path)
            .arg(timeStr)
            .arg(speedMBps, 0, 'f', 1));
        
        // Сбрасываем выбранное устройство (для безопасности)
        m_deviceCombo->setCurrentIndex(-1);
        m_selectedDevice = DeviceInfo();
        m_deviceInfoLabel->setText("Выберите устройство");
        
        // Через 3 секунды скрываем информацию о скорости
        QTimer::singleShot(3000, this, [this]() {
            m_speedLabel->setVisible(false);
            m_timeLeftLabel->setVisible(false);
        });
    } else {
        logMessage("ERROR", message);
        QMessageBox::critical(this, "Ошибка", 
            "Ошибка при записи образа:\n" + message);
        
        // Сразу скрываем информацию о скорости
        m_speedLabel->setVisible(false);
        m_timeLeftLabel->setVisible(false);
    }
    
    refreshDevices();
    m_progressBar->setVisible(false);
    m_lastProgressMessage.clear();
}

bool MainWindow::validateWriteSettings() {
    // Проверка размера образа относительно устройства
    if (!Utils::checkImageFitsDevice(m_selectedImage.path, m_selectedDevice.path)) {
        logMessage("ERROR", "Размер образа превышает размер устройства!");
        return false;
    }
    
    // Проверка доступности файла
    QFileInfo fi(m_selectedImage.path);
    if (!fi.exists() || !fi.isReadable()) {
        logMessage("ERROR", "Файл образа недоступен для чтения!");
        return false;
    }
    
    if (fi.size() == 0) {
        logMessage("ERROR", "Файл образа пустой!");
        return false;
    }
    
    // Проверяем, что устройство не является системным диском
    if (m_selectedDevice.path == "/dev/sda" || m_selectedDevice.path.startsWith("/dev/nvme0n1")) {
        logMessage("WARNING", "Выбрано устройство, которое может быть системным диском!");
        return m_forceCheckbox->isChecked();
    }
    
    return true;
}

void MainWindow::onShowFormatDialog() {
    if (m_selectedDevice.path.isEmpty()) {
        QMessageBox::warning(this, "Внимание", "Сначала выберите устройство!");
        return;
    }
    
    showFormatDialog();
}

void MainWindow::showFormatDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Интеллектуальное форматирование");
    dialog.resize(600, 500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    
    // Заголовок
    QLabel* title = new QLabel(QString("Форматирование устройства: %1").arg(m_selectedDevice.path));
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    mainLayout->addWidget(title);
    
    // Информация об устройстве
    QGroupBox* infoGroup = new QGroupBox("Информация об устройстве");
    QFormLayout* infoLayout = new QFormLayout;
    
    infoLayout->addRow("Устройство:", new QLabel(m_selectedDevice.path));
    infoLayout->addRow("Размер:", new QLabel(m_selectedDevice.sizeStr));
    infoLayout->addRow("Модель:", new QLabel(m_selectedDevice.model.isEmpty() ? "Неизвестно" : m_selectedDevice.model));
    
    QString fsType = Utils::getFilesystemType(m_selectedDevice.path);
    infoLayout->addRow("Текущая ФС:", new QLabel(fsType));
    
    QString mounts = m_selectedDevice.mountPoints.isEmpty() ? 
        "Не смонтировано" : m_selectedDevice.mountPoints.join(", ");
    infoLayout->addRow("Состояние:", new QLabel(mounts));
    
    infoGroup->setLayout(infoLayout);
    mainLayout->addWidget(infoGroup);
    
    // Предполагаемое использование
    QGroupBox* usageGroup = new QGroupBox("Предполагаемое использование");
    QVBoxLayout* usageLayout = new QVBoxLayout;
    
    QComboBox* usageCombo = new QComboBox;
    usageCombo->addItems({
        "Автоматический выбор",
        "Windows",
        "Linux",
        "macOS",
        "Android",
        "Игровые приставки",
        "Мультиплатформенное использование",
        "Резервное копирование",
        "Хранение данных"
    });
    
    usageLayout->addWidget(new QLabel("Как планируете использовать устройство?"));
    usageLayout->addWidget(usageCombo);
    usageGroup->setLayout(usageLayout);
    mainLayout->addWidget(usageGroup);
    
    // Рекомендации системы
    QGroupBox* recommendationGroup = new QGroupBox("Рекомендации системы");
    QVBoxLayout* recLayout = new QVBoxLayout;
    
    FormatRecommendation recommendation = m_formatManager->getRecommendation(
        m_selectedDevice.path, 
        m_selectedDevice.sizeBytes,
        usageCombo->currentText()
    );
    
    QLabel* recLabel = new QLabel(recommendation.explanation);
    recLabel->setWordWrap(true);
    recLabel->setStyleSheet("color: #0066cc; font-weight: bold;");
    
    QLabel* fsLabel = new QLabel(QString("Рекомендуемая файловая система: <b>%1</b>").arg(recommendation.filesystem));
    QLabel* clusterLabel = new QLabel(QString("Рекомендуемый размер кластера: <b>%1</b>").arg(
        Utils::formatSize(recommendation.clusterSize)));
    
    recLayout->addWidget(recLabel);
    recLayout->addWidget(fsLabel);
    recLayout->addWidget(clusterLabel);
    
    QLabel* partLabel = nullptr;
    if (recommendation.needsPartitioning) {
        partLabel = new QLabel("⚠ Рекомендуется создание разделов для больших дисков");
        partLabel->setStyleSheet("color: #ff6600;");
        recLayout->addWidget(partLabel);
    }
    
    recommendationGroup->setLayout(recLayout);
    mainLayout->addWidget(recommendationGroup);
    
    // Настройки форматирования
    QGroupBox* settingsGroup = new QGroupBox("Настройки форматирования");
    QFormLayout* settingsLayout = new QFormLayout;
    
    QComboBox* fsCombo = new QComboBox;
    QList<FilesystemInfo> allFS = m_formatManager->getAllFilesystems();
    
    for (const auto& fs : allFS) {
        if (m_formatManager->isFilesystemSupported(fs.name, m_selectedDevice.sizeBytes)) {
            QString display = QString("%1 (%2)").arg(fs.name).arg(fs.description.left(40) + "...");
            fsCombo->addItem(display, fs.name);
        }
    }
    
    // Устанавливаем рекомендуемую файловую систему
    int recommendedIndex = fsCombo->findData(recommendation.filesystem);
    if (recommendedIndex >= 0) {
        fsCombo->setCurrentIndex(recommendedIndex);
    }
    
    QComboBox* clusterCombo = new QComboBox;
    QString selectedFS = fsCombo->currentData().toString();
    FilesystemInfo fsInfo = m_formatManager->getFilesystemInfo(selectedFS);
    
    for (int clusterSize : fsInfo.clusterSizes) {
        clusterCombo->addItem(Utils::formatSize(clusterSize), clusterSize);
    }
    
    // Устанавливаем рекомендуемый размер кластера
    int recommendedClusterIndex = clusterCombo->findData(recommendation.clusterSize);
    if (recommendedClusterIndex >= 0) {
        clusterCombo->setCurrentIndex(recommendedClusterIndex);
    }
    
    QLineEdit* labelEdit = new QLineEdit;
    labelEdit->setPlaceholderText("Метка устройства (не обязательно)");
    
    QCheckBox* quickFormatCheck = new QCheckBox("Быстрое форматирование");
    quickFormatCheck->setChecked(true);
    
    settingsLayout->addRow("Файловая система:", fsCombo);
    settingsLayout->addRow("Размер кластера:", clusterCombo);
    settingsLayout->addRow("Метка:", labelEdit);
    settingsLayout->addRow("", quickFormatCheck);
    
    settingsGroup->setLayout(settingsLayout);
    mainLayout->addWidget(settingsGroup);
    
    // Обновляем размеры кластеров при изменении файловой системы
    connect(fsCombo, &QComboBox::currentIndexChanged, [&, fsCombo, clusterCombo]() {
        QString fsName = fsCombo->currentData().toString();
        FilesystemInfo fsInfo = m_formatManager->getFilesystemInfo(fsName);
        
        clusterCombo->clear();
        for (int clusterSize : fsInfo.clusterSizes) {
            clusterCombo->addItem(Utils::formatSize(clusterSize), clusterSize);
        }
        
        // Устанавливаем оптимальный размер
        int optimalSize = m_formatManager->getOptimalClusterSize(fsName, m_selectedDevice.sizeBytes);
        int optimalIndex = clusterCombo->findData(optimalSize);
        if (optimalIndex >= 0) {
            clusterCombo->setCurrentIndex(optimalIndex);
        }
    });
    
    // Обновляем рекомендации при изменении использования
    connect(usageCombo, &QComboBox::currentTextChanged, [&, usageCombo, recLabel, fsLabel, clusterLabel, 
                                                          fsCombo, partLabel, recommendationGroup]() {
        FormatRecommendation newRec = m_formatManager->getRecommendation(
            m_selectedDevice.path, 
            m_selectedDevice.sizeBytes,
            usageCombo->currentText()
        );
        
        recLabel->setText(newRec.explanation);
        fsLabel->setText(QString("Рекомендуемая файловая система: <b>%1</b>").arg(newRec.filesystem));
        clusterLabel->setText(QString("Рекомендуемый размер кластера: <b>%1</b>").arg(
            Utils::formatSize(newRec.clusterSize)));
        
        // Обновляем выбор файловой системы
        int recIndex = fsCombo->findData(newRec.filesystem);
        if (recIndex >= 0) {
            fsCombo->setCurrentIndex(recIndex);
        }
        
        // Показываем/скрываем предупреждение о разделах
        if (partLabel) {
            partLabel->setVisible(newRec.needsPartitioning);
        }
    });
    
    // Кнопки
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    QPushButton* formatBtn = new QPushButton("Форматировать");
    QPushButton* cancelBtn = new QPushButton("Отмена");
    
    formatBtn->setStyleSheet("background-color: #ff4444; color: white; font-weight: bold;");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(formatBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // Соединения
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(formatBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Получаем выбранные параметры
        QString filesystem = fsCombo->currentData().toString();
        int clusterSize = clusterCombo->currentData().toInt();
        QString label = labelEdit->text().trimmed();
        bool quickFormat = quickFormatCheck->isChecked();
        
        // Подтверждение
        QString warning = QString(
            "<b>ВНИМАНИЕ! Все данные на %1 будут уничтожены!</b><br><br>"
            "Параметры форматирования:<br>"
            "• Файловая система: <b>%2</b><br>"
            "• Размер кластера: <b>%3</b><br>"
            "• Метка: <b>%4</b><br>"
            "• Быстрое форматирование: <b>%5</b><br><br>"
            "Продолжить?"
        ).arg(m_selectedDevice.path)
         .arg(filesystem)
         .arg(Utils::formatSize(clusterSize))
         .arg(label.isEmpty() ? "(нет)" : label)
         .arg(quickFormat ? "Да" : "Нет");
        
        if (QMessageBox::warning(this, "Подтверждение", warning, 
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            formatDeviceIntelligently(m_selectedDevice.path, m_selectedDevice.sizeBytes,
                                     filesystem, clusterSize, label, quickFormat);
        }
    }
}

void MainWindow::formatDeviceIntelligently(const QString& devicePath, qint64 sizeBytes,
                                          const QString& filesystem, int clusterSize,
                                          const QString& label, bool quickFormat) {
    m_currentFormatDevice = devicePath;
    
    // Создаем диалог прогресса
    m_formatProgressDialog = new QProgressDialog(this);
    m_formatProgressDialog->setWindowTitle("Форматирование");
    m_formatProgressDialog->setLabelText("Подготовка к форматированию...");
    m_formatProgressDialog->setRange(0, 100);
    m_formatProgressDialog->setValue(0);
    m_formatProgressDialog->setCancelButtonText("Отмена");
    m_formatProgressDialog->setMinimumDuration(0);
    
    // Запускаем форматирование в отдельном потоке
    QFuture<void> future = QtConcurrent::run([this, devicePath, sizeBytes, filesystem, clusterSize, label, quickFormat]() {
        updateFormatProgress("Размонтирование устройства...", 10);
        
        // Размонтируем устройство
        auto [success, message] = DeviceManager::unmountAll(devicePath);
        if (!success && !m_selectedDevice.mountPoints.isEmpty()) {
            updateFormatProgress("Ошибка: не удалось размонтировать устройство", -1);
            return;
        }
        
        updateFormatProgress("Начало форматирования...", 20);
        
        // Форматируем устройство
        bool formatSuccess = m_formatManager->formatDevice(devicePath, filesystem, 
                                                          clusterSize, label, quickFormat);
        
        if (formatSuccess) {
            updateFormatProgress("Форматирование успешно завершено!", 100);
            QThread::msleep(1000);
            
            QMetaObject::invokeMethod(this, [this, devicePath]() {
                if (m_formatProgressDialog) {
                    m_formatProgressDialog->close();
                    m_formatProgressDialog->deleteLater();
                    m_formatProgressDialog = nullptr;
                }
                
                QMessageBox::information(this, "Успех", 
                    QString("Устройство %1 успешно отформатировано!").arg(devicePath));
                
                // Обновляем информацию об устройстве
                refreshDevices();
            }, Qt::QueuedConnection);
        } else {
            updateFormatProgress("Ошибка форматирования", -1);
            
            QMetaObject::invokeMethod(this, [this]() {
                if (m_formatProgressDialog) {
                    m_formatProgressDialog->close();
                    m_formatProgressDialog->deleteLater();
                    m_formatProgressDialog = nullptr;
                }
                
                QMessageBox::critical(this, "Ошибка", "Не удалось отформатировать устройство");
            }, Qt::QueuedConnection);
        }
    });
    
    // Явно игнорируем возвращаемое значение, чтобы избежать предупреждения
    (void)future;
    
    // Обработка отмены
    connect(m_formatProgressDialog, &QProgressDialog::canceled, [this]() {
        // Можно добавить логику отмены форматирования
        QMessageBox::information(this, "Отмена", "Операция форматирования отменена");
    });
    
    m_formatProgressDialog->show();
}

void MainWindow::updateFormatProgress(const QString& message, int percent) {
    QMetaObject::invokeMethod(this, [this, message, percent]() {
        if (m_formatProgressDialog) {
            m_formatProgressDialog->setLabelText(message);
            if (percent >= 0) {
                m_formatProgressDialog->setValue(percent);
            }
        }
    }, Qt::QueuedConnection);
}

// Реализация слота onFormatDevice()
void MainWindow::onFormatDevice() {
    // Это устаревший слот, перенаправляем на новый диалог
    qDebug() << "onFormatDevice called, redirecting to onShowFormatDialog";
    onShowFormatDialog();
}

// Реализация слота onFormatProgress()
void MainWindow::onFormatProgress(const QString& message, int percent) {
    // Это устаревший слот, перенаправляем на новую функцию
    qDebug() << QString("onFormatProgress: %1 (%2%)").arg(message).arg(percent);
    updateFormatProgress(message, percent);
}

// Реализация слота onFormatFinished()
void MainWindow::onFormatFinished(bool success, const QString& message) {
    qDebug() << QString("onFormatFinished: success=%1, message=%2").arg(success).arg(message);
    
    if (success) {
        logMessage("SUCCESS", QString("Форматирование завершено успешно: %1").arg(message));
        QMessageBox::information(this, "Успех", 
            QString("Устройство успешно отформатировано!\n\n%1").arg(message));
    } else {
        logMessage("ERROR", QString("Ошибка форматирования: %1").arg(message));
        QMessageBox::critical(this, "Ошибка", 
            QString("Не удалось отформатировать устройство:\n\n%1").arg(message));
    }
    
    // Обновляем список устройств
    refreshDevices();
    
    // Закрываем диалог прогресса, если он открыт
    if (m_formatProgressDialog) {
        m_formatProgressDialog->close();
        m_formatProgressDialog->deleteLater();
        m_formatProgressDialog = nullptr;
    }
}

void MainWindow::logMessage(const QString& level, const QString& msg) {
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString color;
    
    if (level == "ERROR") color = "#ff4444";
    else if (level == "SUCCESS") color = "#44ff44";
    else if (level == "WARNING") color = "#ffaa44";
    else if (level == "INFO") color = "#4488ff";
    else if (level == "PROGRESS") color = "#ff88ff";
    else color = "#ffffff";
    
    // Для прогресса не добавляем время (чтобы не дублировать)
    if (level == "PROGRESS") {
        m_logView->append(QString("<font color=\"%1\">%2</font>").arg(color).arg(msg));
    } else {
        m_logView->append(QString("<font color=\"%1\">[%2] %3</font>").arg(color).arg(time).arg(msg));
    }
    
    m_logView->moveCursor(QTextCursor::End);
}

MainWindow::~MainWindow() {
    if (m_refreshTimer && m_refreshTimer->isActive()) {
        m_refreshTimer->stop();
    }
    
    if (m_writer && m_writer->isRunning()) {
        m_writer->cancel();
        if (!m_writer->wait(2000)) {
            m_writer->terminate();
            m_writer->wait(1000);
        }
        delete m_writer;
        m_writer = nullptr;
    }
    
    if (m_refreshTimer) {
        delete m_refreshTimer;
        m_refreshTimer = nullptr;
    }
    
    if (m_writeTimer) {
        delete m_writeTimer;
        m_writeTimer = nullptr;
    }
    
    if (m_formatProgressDialog) {
        delete m_formatProgressDialog;
        m_formatProgressDialog = nullptr;
    }
}
