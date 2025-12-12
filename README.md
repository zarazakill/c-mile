# C-mile - Professional Disk Imaging Utility

## English Description

**C-mile** is a professional cross-platform disk imaging utility designed for writing disk images to USB drives and SD cards. It provides a user-friendly graphical interface for creating bootable USB devices and formatting storage media.

### Features:
- **Image Writing**: Write ISO, IMG, and other image formats to USB drives and SD cards
- **Device Management**: Automatic detection and listing of connected storage devices
- **Progress Tracking**: Real-time progress indication with speed and time estimation
- **Verification Options**: Optional verification after writing to ensure data integrity
- **Force Writing**: Option to force write operations even on protected devices
- **Formatting Capabilities**: Built-in formatting functionality for various file systems
- **Block Size Configuration**: Customizable block sizes for optimized performance
- **Safety Checks**: Validation of selected devices to prevent accidental data loss

### Requirements:
- Linux operating system
- Root privileges (required for direct device access)
- Qt6 libraries
- Modern C++17 compatible compiler

### Usage:
1. Run the application with root privileges: `sudo ./c-mile`
2. Select your target device from the dropdown menu
3. Choose your image file using the browse button
4. Configure additional settings (block size, verification, etc.)
5. Click "Write Image" to start the process

---

## Русское описание

**C-mile** — это профессиональная кроссплатформенная утилита для создания дисковых образов, предназначенная для записи образов на USB-устройства и SD-карты. Приложение предоставляет удобный графический интерфейс для создания загрузочных USB-устройств и форматирования носителей данных.

### Возможности:
- **Запись образов**: Запись образов ISO, IMG и других форматов на USB-устройства и SD-карты
- **Управление устройствами**: Автоматическое обнаружение и список подключенных устройств хранения
- **Отслеживание прогресса**: Отображение реального времени с оценкой скорости и времени
- **Параметры проверки**: Дополнительная проверка после записи для обеспечения целостности данных
- **Принудительная запись**: Возможность принудительно выполнить операции записи даже на защищенных устройствах
- **Функции форматирования**: Встроенная функция форматирования для различных файловых систем
- **Настройка размера блока**: Настраиваемые размеры блоков для оптимальной производительности
- **Проверки безопасности**: Проверка выбранных устройств для предотвращения случайной потери данных

### Требования:
- Операционная система Linux
- Права root (необходимы для прямого доступа к устройствам)
- Библиотеки Qt6
- Современный компилятор, совместимый с C++17

### Использование:
1. Запустите приложение с правами root: `sudo ./c-mile`
2. Выберите целевое устройство из выпадающего меню
3. Выберите файл образа с помощью кнопки обзора
4. Настройте дополнительные параметры (размер блока, проверка и т.д.)
5. Нажмите "Записать образ", чтобы начать процесс