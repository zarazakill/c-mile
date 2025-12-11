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
    // Явное удаление всех элементов перед очисткой
    for (int row = m_devicesTable->rowCount() - 1; row >= 0; --row) {
        for (int col = 0; col < m_devicesTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_devicesTable->takeItem(row, col);
            delete item;
        }
    }

    // Теперь можно безопасно очистить
    m_devicesTable->clearContents();
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
    // Сохраняем текущий выбор
    ImageInfo currentImage;
    if (m_imageCombo->currentIndex() >= 0) {
        QVariant var = m_imageCombo->currentData();
        if (var.isValid()) {
            currentImage = var.value<ImageInfo>();
        }
    }

    // Очищаем комбобокс
    m_imageCombo->clear();

    // Явное удаление всех элементов таблицы перед очисткой
    for (int row = m_imagesTable->rowCount() - 1; row >= 0; --row) {
        for (int col = 0; col < m_imagesTable->columnCount(); ++col) {
            QTableWidgetItem* item = m_imagesTable->takeItem(row, col);
            delete item;
        }
    }

    // Теперь можно безопасно очистить
    m_imagesTable->clearContents();
    m_imagesTable->setRowCount(images.size());

    int selectIndex = -1;

    for (int i = 0; i < images.size(); ++i) {
        const auto& img = images[i];
        QString name = QFileInfo(img.path).fileName();
        QString sizeStr = Utils::formatSize(img.size);

        // Добавляем в комбобокс
        m_imageCombo->addItem(name + " (" + sizeStr + ")", QVariant::fromValue(img));

        // Находим сохраненный выбор
        if (img.path == currentImage.path) {
            selectIndex = i;
        }

        // Создаем элементы для таблицы
        m_imagesTable->setItem(i, 0, new QTableWidgetItem(name));
        m_imagesTable->setItem(i, 1, new QTableWidgetItem(sizeStr));
        m_imagesTable->setItem(i, 2, new QTableWidgetItem(img.fileType));
        m_imagesTable->setItem(i, 3, new QTableWidgetItem(img.path));
    }

    // Восстанавливаем выбор
    if (selectIndex >= 0) {
        m_imageCombo->setCurrentIndex(selectIndex);
        m_imagesTable->setCurrentCell(selectIndex, 0);
    } else if (!images.isEmpty()) {
        m_imageCombo->setCurrentIndex(0);
    }

    checkReadyState();
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
        logMessage("WARNING", "Отмена операции...");
        m_cancelBtn->setEnabled(false);

        // Безопасная отмена
        m_writer->cancel();

        // Устанавливаем флаг отмены в UI потоке
        m_cancelled = true;

        // Даем время на безопасное завершение
        if (!m_writer->wait(3000)) {
            logMessage("ERROR", "Не удалось безопасно завершить операцию");
            m_writer->terminate();
            m_writer->wait();
        }

        m_writer->deleteLater();
        m_writer = nullptr;
        m_cancelled = false;
    }
}

void MainWindow::onWriteProgress(int percent, const QString& status) {
    m_progressBar->setValue(percent);
    logMessage("PROGRESS", status);
}

void MainWindow::onWriteFinished(bool success, const QString& message) {
    // Сохраняем указатель на writer для безопасного удаления
    ImageWriter* finishedWriter = qobject_cast<ImageWriter*>(sender());

    // Проверяем, что сигнал пришел от текущего writer'а
    if (finishedWriter && finishedWriter != m_writer) {
        // Сигнал от старого writer'а, игнорируем
        finishedWriter->deleteLater();
        return;
    }

    // Обновляем UI состояние
    m_progressBar->setValue(success ? 100 : 0);
    m_writeBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_progressBar->setVisible(false);

    // Останавливаем и удаляем writer
    if (m_writer) {
        // Отключаем все соединения с writer'ом
        disconnect(m_writer, nullptr, this, nullptr);

        // Ждем безопасного завершения потока
        if (m_writer->isRunning()) {
            m_writer->wait(100); // Короткое ожидание для завершения
        }

        // Удаляем writer
        m_writer->deleteLater();
        m_writer = nullptr;
    }

    // Обрабатываем результат операции
    if (success) {
        handleWriteSuccess(message);
    } else {
        handleWriteError(message);
    }

    // Обновляем список устройств
    refreshDevices();

    // Дополнительные действия при успешной записи
    if (success) {
        // Возможность записать еще один образ
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Операция завершена",
            "Запись образа успешно завершена!\n\nЗаписать еще один образ?",
            QMessageBox::Yes | QMessageBox::No
        );

        if (reply == QMessageBox::Yes) {
            // Подготовка к новой записи
            prepareForNewWrite();
        }
    }
}

void MainWindow::handleWriteSuccess(const QString& message) {
    // Логируем успех
    logMessage("SUCCESS", message);

    // Показываем информационное сообщение
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle("Операция завершена успешно");
    msgBox.setText("Запись образа завершена успешно");

    // Форматируем детальное сообщение
    QString formattedDetails = formatMessageForDisplay(message);
    if (!formattedDetails.isEmpty()) {
        msgBox.setDetailedText(formattedDetails);
    }

    // Добавляем полезную информацию
    QString infoText = QString(
        "Образ: %1\n"
        "Устройство: %2\n"
        "Время: %3")
    .arg(QFileInfo(m_selectedImage.path).fileName())
    .arg(m_selectedDevice.path)
    .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss"));

    msgBox.setInformativeText(infoText);
    msgBox.setStandardButtons(QMessageBox::Ok);

    // Настраиваем размер окна
    msgBox.setMinimumSize(400, 200);

    msgBox.exec();

    // Очищаем выбранные данные
    clearSelection();
}

void MainWindow::handleWriteError(const QString& message) {
    // Логируем ошибку
    logMessage("ERROR", message);

    // Форматируем сообщение об ошибке
    QString displayMessage = message;
    QString detailedMessage = message;

    // Если сообщение содержит переносы строк, форматируем для отображения
    if (message.contains('\n')) {
        QStringList lines = message.split('\n', Qt::SkipEmptyParts);
        if (lines.size() > 1) {
            // Первая строка - краткое сообщение
            displayMessage = lines.first();

            // Остальные строки - детали
            detailedMessage = lines.join("\n");
        }
    }

    // Создаем диалог ошибки
    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setWindowTitle("Ошибка записи");
    msgBox.setText("Произошла ошибка при записи образа");

    // Устанавливаем информативный текст
    QString informativeText = formatErrorMessage(displayMessage);
    msgBox.setInformativeText(informativeText);

    // Добавляем детальный текст если нужно
    if (detailedMessage.length() > 100 || detailedMessage.contains('\n')) {
        msgBox.setDetailedText(detailedMessage);
    }

    // Добавляем полезные кнопки
    msgBox.addButton("Повторить", QMessageBox::AcceptRole);
    msgBox.addButton(QMessageBox::Ok);
    msgBox.addButton("Подробности", QMessageBox::HelpRole);

    // Обработка нажатия кнопок
    int result = msgBox.exec();

    if (msgBox.clickedButton() &&
        msgBox.clickedButton()->text() == "Повторить") {
        // Пользователь хочет повторить операцию
        QTimer::singleShot(100, this, [this]() {
            onStartWrite();
        });
        } else if (msgBox.clickedButton() &&
            msgBox.clickedButton()->text() == "Подробности") {
            // Показываем дополнительные детали
            showErrorDetails(detailedMessage);
            }

            // Сохраняем лог ошибки
            saveErrorLog(message);
}

void MainWindow::prepareForNewWrite() {
    // Очищаем прогресс
    m_progressBar->setValue(0);

    // Очищаем лог
    m_logView->clear();

    // Обновляем списки
    refreshDevices();
    refreshImages();

    // Сбрасываем выбор устройства (для безопасности)
    if (m_deviceCombo->count() > 0) {
        m_deviceCombo->setCurrentIndex(-1);
    }

    logMessage("INFO", "Готово к новой операции записи");
}

void MainWindow::clearSelection() {
    // Сбрасываем выбранные данные
    m_selectedImage = ImageInfo();
    m_selectedDevice = DeviceInfo();

    // Обновляем UI
    m_imageInfoLabel->setText("Выберите образ");
    m_deviceInfoLabel->setText("Выберите устройство");

    // Очищаем комбобоксы
    if (m_imageCombo->count() > 0) {
        m_imageCombo->setCurrentIndex(-1);
    }
    if (m_deviceCombo->count() > 0) {
        m_deviceCombo->setCurrentIndex(-1);
    }

    checkReadyState();
}

QString MainWindow::formatMessageForDisplay(const QString& message) {
    if (message.isEmpty()) return QString();

    QString formatted = message;

    // Заменяем HTML-теги если они есть
    formatted.replace("<br>", "\n");
    formatted.replace("</b>", "");
    formatted.replace("<b>", "");

    // Ограничиваем длину
    const int maxLength = 500;
    if (formatted.length() > maxLength) {
        formatted = formatted.left(maxLength) + "\n\n... (сообщение обрезано)";
    }

    return formatted.trimmed();
}

QString MainWindow::formatErrorMessage(const QString& error) {
    // Преобразуем системные ошибки в понятные сообщения
    static const QMap<QString, QString> errorTranslations = {
        {"Permission denied", "Отказано в доступе. Запустите программу от имени администратора."},
        {"No space left on device", "Недостаточно места на устройстве."},
        {"Input/output error", "Ошибка ввода/вывода. Проверьте подключение устройства."},
        {"Device or resource busy", "Устройство занято другим процессом."},
        {"No such file or directory", "Файл или устройство не найдено."}
    };

    QString formatted = error;

    // Ищем известные ошибки
    for (auto it = errorTranslations.constBegin(); it != errorTranslations.constEnd(); ++it) {
        if (error.contains(it.key(), Qt::CaseInsensitive)) {
            formatted = it.value() + "\n\nОригинальное сообщение:\n" + error;
            break;
        }
    }

    return formatted;
}

void MainWindow::showErrorDetails(const QString& details) {
    QDialog detailsDialog(this);
    detailsDialog.setWindowTitle("Детали ошибки");
    detailsDialog.setMinimumSize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(&detailsDialog);

    // Текстовое поле с деталями
    QTextEdit* textEdit = new QTextEdit(&detailsDialog);
    textEdit->setPlainText(details);
    textEdit->setReadOnly(true);
    textEdit->setFont(QFont("Monospace", 9));

    // Кнопки
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* copyButton = new QPushButton("Копировать", &detailsDialog);
    QPushButton* closeButton = new QPushButton("Закрыть", &detailsDialog);

    connect(copyButton, &QPushButton::clicked, [textEdit]() {
        QApplication::clipboard()->setText(textEdit->toPlainText());
    });
    connect(closeButton, &QPushButton::clicked, &detailsDialog, &QDialog::accept);

    buttonLayout->addWidget(copyButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    layout->addWidget(new QLabel("Подробная информация об ошибке:"));
    layout->addWidget(textEdit);
    layout->addLayout(buttonLayout);

    detailsDialog.exec();
}

void MainWindow::saveErrorLog(const QString& error) {
    // Сохраняем лог ошибки в файл
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);

    QString logFile = logDir + "/error_log.txt";
    QFile file(logFile);

    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "=== Ошибка записи ===\n";
        stream << "Время: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
        stream << "Образ: " << m_selectedImage.path << "\n";
        stream << "Устройство: " << m_selectedDevice.path << "\n";
        stream << "Сообщение: " << error << "\n";
        stream << "=================================\n\n";
        file.close();
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
    bool allChecksPassed = true;

    // Проверка размера образа относительно устройства
    if (!Utils::checkImageFitsDevice(m_selectedImage.path, m_selectedDevice.path)) {
        logMessage("ERROR", "Размер образа превышает размер устройства!");
        allChecksPassed = false;
    }

    // Проверка свободного места для временных файлов
    qint64 freeSpace = Utils::getFreeSpace("/tmp");
    qint64 imageSize = QFileInfo(m_selectedImage.path).size();
    if (freeSpace > 0 && freeSpace < imageSize * 2) {
        logMessage("ERROR",
                   QString("Недостаточно свободного места в /tmp: %1 (требуется примерно %2)")
                   .arg(Utils::formatSize(freeSpace))
                   .arg(Utils::formatSize(imageSize * 2)));
        allChecksPassed = false;
    } else if (freeSpace > 0 && freeSpace < imageSize * 3) {
        logMessage("WARNING",
                   QString("Мало свободного места в /tmp: %1 (рекомендуется не менее %2)")
                   .arg(Utils::formatSize(freeSpace))
                   .arg(Utils::formatSize(imageSize * 3)));
    }

    // Проверка целостности архива
    if (Utils::isCompressedArchive(m_selectedImage.path)) {
        logMessage("INFO", "Проверка целостности архива...");
        if (!Utils::verifyArchiveIntegrity(m_selectedImage.path)) {
            logMessage("ERROR", "Образ поврежден!");
            allChecksPassed = false;
        } else {
            logMessage("SUCCESS", "Целостность архива проверена");
        }
    }

    // Проверка доступности файла
    QFileInfo fi(m_selectedImage.path);
    if (!fi.exists() || !fi.isReadable()) {
        logMessage("ERROR", "Файл образа недоступен для чтения!");
        allChecksPassed = false;
    }

    // Проверка размера
    if (fi.size() == 0) {
        logMessage("ERROR", "Файл образа пустой!");
        allChecksPassed = false;
    }

    return allChecksPassed;
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

MainWindow::~MainWindow() {
    qDebug() << "Начало уничтожения MainWindow...";

    try {
        // 1. Останавливаем все активные операции
        stopAllActiveOperations();

        // 2. Отключаем все соединения
        disconnectAllConnections();

        // 3. Очищаем UI элементы
        cleanupUI();

        // 4. Очищаем данные
        cleanupData();

        // 5. Освобождаем ресурсы
        cleanupResources();

        qDebug() << "MainWindow уничтожен успешно";
    }
    catch (const std::exception& e) {
        qCritical() << "Ошибка при уничтожении MainWindow:" << e.what();
    }
    catch (...) {
        qCritical() << "Неизвестная ошибка при уничтожении MainWindow";
    }
}

void MainWindow::stopAllActiveOperations() {
    qDebug() << "Остановка активных операций...";

    // Останавливаем таймер обновления
    if (m_refreshTimer && m_refreshTimer->isActive()) {
        qDebug() << "Остановка таймера обновления...";
        m_refreshTimer->stop();
    }

    // Останавливаем рабочий поток
    if (m_writer && m_writer->isRunning()) {
        qDebug() << "Остановка рабочего потока записи...";

        // Отправляем сигнал отмены
        emit cancelAllOperations();

        // Устанавливаем флаг отмены в потоке
        m_writer->cancel();

        // Ждем завершения с таймаутом
        const int timeoutMs = 5000;
        if (m_writer->wait(timeoutMs)) {
            qDebug() << "Рабочий поток завершен корректно";
        } else {
            qWarning() << "Рабочий поток не завершился за" << timeoutMs << "мс";

            // Пытаемся принудительно завершить
            m_writer->terminate();

            if (!m_writer->wait(1000)) {
                qCritical() << "Не удалось завершить рабочий поток";
            }
        }
    }

    // Останавливаем все фоновые операции QtConcurrent
    qDebug() << "Ожидание завершения фоновых операций...";
    QThreadPool::globalInstance()->waitForDone(3000);
}

void MainWindow::disconnectAllConnections() {
    qDebug() << "Отключение всех соединений...";

    // Отключаем сигналы от таймера
    if (m_refreshTimer) {
        disconnect(m_refreshTimer, nullptr, nullptr, nullptr);
    }

    // Отключаем сигналы от рабочего потока
    if (m_writer) {
        disconnect(m_writer, nullptr, nullptr, nullptr);
    }

    // Отключаем все сигналы MainWindow
    disconnect(this, nullptr, nullptr, nullptr);

    // Отключаем сигналы от виджетов
    if (m_devicesTable) disconnect(m_devicesTable, nullptr, nullptr, nullptr);
    if (m_imagesTable) disconnect(m_imagesTable, nullptr, nullptr, nullptr);
    if (m_imageCombo) disconnect(m_imageCombo, nullptr, nullptr, nullptr);
    if (m_deviceCombo) disconnect(m_deviceCombo, nullptr, nullptr, nullptr);
}

void MainWindow::cleanupUI() {
    qDebug() << "Очистка UI элементов...";

    // Очищаем таблицы
    clearTable(m_devicesTable, "Таблица устройств");
    clearTable(m_imagesTable, "Таблица образов");

    // Очищаем комбобоксы
    if (m_imageCombo) {
        m_imageCombo->clear();
        m_imageCombo->setEnabled(false);
    }
    if (m_deviceCombo) {
        m_deviceCombo->clear();
        m_deviceCombo->setEnabled(false);
    }

    // Очищаем другие виджеты
    if (m_logView) {
        m_logView->clear();
        m_logView->setEnabled(false);
    }
    if (m_progressBar) {
        m_progressBar->setValue(0);
        m_progressBar->setEnabled(false);
    }

    // Отключаем кнопки
    if (m_writeBtn) m_writeBtn->setEnabled(false);
    if (m_cancelBtn) m_cancelBtn->setEnabled(false);
    if (m_refreshBtn) m_refreshBtn->setEnabled(false);
}

void MainWindow::clearTable(QTableWidget* table, const QString& tableName) {
    if (!table) return;

    qDebug() << "Очистка" << tableName << "...";

    // Сохраняем ссылки на элементы перед удалением
    QList<QTableWidgetItem*> items;
    const int rows = table->rowCount();
    const int cols = table->columnCount();

    // Собираем все элементы
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            QTableWidgetItem* item = table->item(row, col);
            if (item) {
                items.append(item);
                table->setItem(row, col, nullptr);
            }
        }
    }

    // Очищаем таблицу
    table->clear();
    table->setRowCount(0);

    // Удаляем элементы
    qDeleteAll(items);

    qDebug() << tableName << "очищена, удалено элементов:" << items.size();
}

void MainWindow::cleanupData() {
    qDebug() << "Очистка данных...";

    m_devices.clear();
    m_images.clear();

    // Сбрасываем выбранные элементы
    m_selectedDevice = DeviceInfo();
    m_selectedImage = ImageInfo();

    qDebug() << "Данные очищены";
}

void MainWindow::cleanupResources() {
    qDebug() << "Освобождение ресурсов...";

    // Удаляем таймер
    if (m_refreshTimer) {
        delete m_refreshTimer;
        m_refreshTimer = nullptr;
        qDebug() << "Таймер удален";
    }

    // Удаляем рабочий поток
    if (m_writer) {
        // Проверяем, что поток завершен
        if (m_writer->isRunning()) {
            qWarning() << "Попытка удаления работающего потока, ожидание...";
            m_writer->wait(1000);
        }

        delete m_writer;
        m_writer = nullptr;
        qDebug() << "Рабочий поток удален";
    }

    // Очищаем указатели (они удаляются автоматически как дети MainWindow)
    m_tabWidget = nullptr;
    m_deviceCombo = nullptr;
    m_imageCombo = nullptr;
    m_blockSizeCombo = nullptr;
    m_verifyCheckbox = nullptr;
    m_forceCheckbox = nullptr;
    m_devicesTable = nullptr;
    m_imagesTable = nullptr;
    m_imagesDirEdit = nullptr;
    m_deviceInfoLabel = nullptr;
    m_imageInfoLabel = nullptr;
    m_progressBar = nullptr;
    m_logView = nullptr;
    m_writeBtn = nullptr;
    m_cancelBtn = nullptr;
    m_refreshBtn = nullptr;

    qDebug() << "Ресурсы освобождены";
}
