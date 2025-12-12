#!/bin/bash

# build_release.sh - Универсальный скрипт сборки c-mile
# Включает установку зависимостей и создание пакетов
# Версия: 2.0

set -e  # Выход при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Конфигурация
PROJECT_NAME="c-mile"
VERSION="0.9.5"
QT_VERSION="6"
DESCRIPTION="Профессиональный инструмент для записи образов и форматирования устройств"
MAINTAINER="$(whoami)@$(hostname)"
WEBSITE="https://github.com/$(whoami)/c-mile"

# Директории
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_DIR/build"
RELEASE_DIR="$PROJECT_DIR/release"
PACKAGE_DIR="$RELEASE_DIR/packages"
TEMP_DIR="$RELEASE_DIR/temp"
DEPENDENCIES_DIR="$PROJECT_DIR/.deps"

# Функции
print_header() {
    echo -e "\n${CYAN}================================${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

check_command() {
    if ! command -v $1 &> /dev/null; then
        print_warning "Команда '$1' не найдена."
        return 1
    fi
    return 0
}

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    elif [ -f /etc/redhat-release ]; then
        echo "rhel"
    elif [ -f /etc/arch-release ]; then
        echo "arch"
    else
        echo "unknown"
    fi
}

install_dependencies_debian() {
    print_header "Установка зависимостей для Debian/Ubuntu"

    local distro=$(lsb_release -is 2>/dev/null | tr '[:upper:]' '[:lower:]' || echo "debian")
    local version=$(lsb_release -cs 2>/dev/null || echo "trixie")

    print_info "Дистрибутив: $distro $version"

    # Обновление пакетов
    sudo apt update

    # Базовые инструменты сборки
    print_info "Установка базовых инструментов сборки..."
    sudo apt install -y \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        git \
        wget \
        curl \
        file \
        desktop-file-utils \
        libfuse2 \
        libglib2.0-bin \
        zsync

    # Qt6 зависимости
    print_info "Установка Qt6 зависимостей..."
    if [ "$distro" = "ubuntu" ] && [ "$version" = "jammy" ]; then
        # Ubuntu 22.04
        sudo apt install -y \
            qt6-base-dev \
            qt6-tools-dev \
            qt6-declarative-dev \
            libqt6core6 \
            libqt6widgets6 \
            libqt6concurrent6 \
            libqt6dbus6 \
            libgl1-mesa-dev
    else
        # Debian Trixie и другие
        sudo apt install -y \
            qt6-base-dev \
            qt6-tools-dev \
            qt6-declarative-dev \
            libqt6core6t64 \
            libqt6widgets6t64 \
            libqt6concurrent6t64 \
            libqt6dbus6t64 \
            libgl1-mesa-dev
    fi

    # Инструменты для пакетирования
    print_info "Установка инструментов для пакетирования..."
    sudo apt install -y \
        rpm \
        dpkg-dev \
        fakeroot \
        lintian

    # Проверка и установка linuxdeploy
    if ! check_command "linuxdeploy"; then
        print_info "Установка linuxdeploy..."
        mkdir -p "$DEPENDENCIES_DIR"

        # Скачиваем linuxdeploy
        wget -q -O "$DEPENDENCIES_DIR/linuxdeploy-x86_64.AppImage" \
            "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        chmod +x "$DEPENDENCIES_DIR/linuxdeploy-x86_64.AppImage"

        # Скачиваем плагин Qt
        wget -q -O "$DEPENDENCIES_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" \
            "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
        chmod +x "$DEPENDENCIES_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

        # Создаем симлинки
        sudo ln -sf "$DEPENDENCIES_DIR/linuxdeploy-x86_64.AppImage" /usr/local/bin/linuxdeploy
        sudo ln -sf "$DEPENDENCIES_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" /usr/local/bin/linuxdeploy-plugin-qt

        print_success "linuxdeploy установлен"
    else
        print_success "linuxdeploy уже установлен"
    fi

    # Проверка и установка snapcraft
    if ! check_command "snapcraft"; then
        print_info "Установка snapcraft..."
        if ! check_command "snap"; then
            sudo apt install -y snapd
            sudo systemctl enable --now snapd.socket
        fi
        sudo snap install snapcraft --classic
        print_success "snapcraft установлен"
    else
        print_success "snapcraft уже установлен"
    fi

    # Дополнительные зависимости для Flatpak
    if [ "$1" = "full" ]; then
        print_info "Установка зависимостей для Flatpak..."
        sudo apt install -y \
            flatpak \
            flatpak-builder \
            gnome-software-plugin-flatpak

        flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
    fi

    print_success "Все зависимости установлены!"
}

install_dependencies_fedora() {
    print_header "Установка зависимостей для Fedora/RHEL"

    # Базовые инструменты
    sudo dnf install -y \
        gcc-c++ \
        cmake \
        ninja-build \
        git \
        wget \
        file \
        desktop-file-utils \
        fuse-libs

    # Qt6 зависимости
    sudo dnf install -y \
        qt6-qtbase-devel \
        qt6-qtdeclarative-devel \
        qt6-qttools-devel \
        mesa-libGL-devel

    # Инструменты для пакетирования
    sudo dnf install -y \
        rpm-build \
        rpmdevtools \
        fakeroot

    # linuxdeploy
    if ! check_command "linuxdeploy"; then
        print_info "Установка linuxdeploy..."
        mkdir -p "$DEPENDENCIES_DIR"

        wget -q -O "$DEPENDENCIES_DIR/linuxdeploy-x86_64.AppImage" \
            "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        chmod +x "$DEPENDENCIES_DIR/linuxdeploy-x86_64.AppImage"

        wget -q -O "$DEPENDENCIES_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" \
            "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
        chmod +x "$DEPENDENCIES_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

        sudo ln -sf "$DEPENDENCIES_DIR/linuxdeploy-x86_64.AppImage" /usr/local/bin/linuxdeploy
        sudo ln -sf "$DEPENDENCIES_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" /usr/local/bin/linuxdeploy-plugin-qt
    fi

    # snapcraft
    if ! check_command "snapcraft" && [ "$1" = "full" ]; then
        sudo dnf install -y snapd
        sudo ln -s /var/lib/snapd/snap /snap
        sudo snap install snapcraft --classic
    fi
}

install_dependencies_arch() {
    print_header "Установка зависимостей для Arch Linux"

    sudo pacman -S --needed --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        wget \
        qt6-base \
        qt6-tools \
        qt6-declarative \
        mesa \
        fuse2 \
        desktop-file-utils

    # Инструменты для пакетирования
    sudo pacman -S --needed --noconfirm \
        rpm-tools \
        dpkg

    # linuxdeploy (из AUR)
    if ! check_command "linuxdeploy"; then
        print_info "Установка linuxdeploy из AUR..."
        if check_command "yay"; then
            yay -S --noconfirm linuxdeploy linuxdeploy-plugin-qt
        elif check_command "paru"; then
            paru -S --noconfirm linuxdeploy linuxdeploy-plugin-qt
        else
            print_warning "Установите yay или paru для установки linuxdeploy из AUR"
        fi
    fi
}

install_dependencies() {
    local mode="${1:-basic}"

    print_header "Проверка и установка зависимостей"

    # Создаем директорию для зависимостей
    mkdir -p "$DEPENDENCIES_DIR"

    # Определяем дистрибутив
    local distro=$(detect_distro)

    case $distro in
        debian|ubuntu)
            install_dependencies_debian "$mode"
            ;;
        fedora|rhel|centos)
            install_dependencies_fedora "$mode"
            ;;
        arch|manjaro)
            install_dependencies_arch
            ;;
        *)
            print_error "Неподдерживаемый дистрибутив: $distro"
            print_info "Установите зависимости вручную:"
            echo "  - CMake 3.16+"
            echo "  - Qt6 (Core, Widgets, Concurrent)"
            echo "  - GCC/Clang с поддержкой C++17"
            echo "  - linuxdeploy (для AppImage)"
            echo "  - snapcraft (для Snap пакетов)"
            return 1
            ;;
    esac

    # Проверяем основные команды
    print_info "Проверка установленных команд..."

    local missing=()
    for cmd in cmake g++ make; do
        if ! check_command "$cmd"; then
            missing+=("$cmd")
        fi
    done

    if [ ${#missing[@]} -ne 0 ]; then
        print_error "Не установлены: ${missing[*]}"
        return 1
    fi

    print_success "Все зависимости проверены и установлены"
    return 0
}

clean_build() {
    print_info "Очистка предыдущих сборок..."
    rm -rf "$BUILD_DIR"
    rm -rf "$TEMP_DIR"
    mkdir -p "$BUILD_DIR" "$RELEASE_DIR" "$PACKAGE_DIR" "$TEMP_DIR"
    print_success "Очистка завершена"
}

build_project() {
    local platform=$1
    local build_type=$2

    print_header "Сборка проекта ($platform - $build_type)"

    cd "$BUILD_DIR"

    case $platform in
        linux)
            if [ "$build_type" = "debug" ]; then
                cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr
            else
                cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
            fi
            make -j$(nproc)
            ;;
        windows)
            print_warning "Сборка для Windows требует настройки кросс-компиляции"
            print_info "Установите mingw-w64 и настройте CMake toolchain"
            return 1
            ;;
        macos)
            print_warning "Сборка для macOS возможна только на macOS"
            return 1
            ;;
    esac

    print_success "Сборка завершена успешно"
}

create_appimage() {
    print_header "Создание AppImage для Linux"

    if ! check_command "linuxdeploy"; then
        print_error "linuxdeploy не найден. Установите зависимости."
        return 1
    fi

    if ! check_command "linuxdeploy-plugin-qt"; then
        print_error "linuxdeploy-plugin-qt не найден. Установите зависимости."
        return 1
    fi

    # Создаем структуру AppDir
    APPDIR="$TEMP_DIR/AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR"

    # Устанавливаем приложение
    cd "$BUILD_DIR"
    make install DESTDIR="$APPDIR"

    # Создаем необходимые директории
    mkdir -p "$APPDIR/usr/share/applications"
    mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

    # Создаем desktop файл
    cat > "$APPDIR/usr/share/applications/$PROJECT_NAME.desktop" << EOF
[Desktop Entry]
Type=Application
Name=$PROJECT_NAME
Comment=$DESCRIPTION
Exec=$PROJECT_NAME
Icon=$PROJECT_NAME
Categories=Utility;System;
Terminal=false
EOF

    # Создаем временную иконку если нет существующей
    if [ ! -f "$PROJECT_DIR/icons/$PROJECT_NAME.png" ]; then
        print_info "Создание временной иконки..."
        if check_command "convert"; then
            convert -size 256x256 xc:#4CAF50 -pointsize 48 -fill white -gravity center \
                -draw "text 0,0 'CM'" "$APPDIR/usr/share/icons/hicolor/256x256/apps/$PROJECT_NAME.png"
        else
            # Простой fallback - создаем пустую иконку
            echo "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==" | \
                base64 -d > "$APPDIR/usr/share/icons/hicolor/256x256/apps/$PROJECT_NAME.png"
        fi
    else
        cp "$PROJECT_DIR/icons/$PROJECT_NAME.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/$PROJECT_NAME.png"
    fi

    # Запускаем linuxdeploy
    print_info "Запуск linuxdeploy..."
    cd "$TEMP_DIR"

    # Экспортируем переменные для linuxdeploy
    export ARCH=x86_64
    export OUTPUT="c-mile-$VERSION.AppImage"

    linuxdeploy \
        --appdir AppDir \
        --plugin qt \
        --output appimage \
        --icon-file "AppDir/usr/share/icons/hicolor/256x256/apps/$PROJECT_NAME.png" \
        --desktop-file "AppDir/usr/share/applications/$PROJECT_NAME.desktop"

    # Проверяем результат
    if [ -f "$PROJECT_NAME-$VERSION-x86_64.AppImage" ]; then
        mv "$PROJECT_NAME-$VERSION-x86_64.AppImage" "$PACKAGE_DIR/"
        print_success "AppImage создан: $PACKAGE_DIR/$PROJECT_NAME-$VERSION-x86_64.AppImage"
        return 0
    else
        print_error "Не удалось создать AppImage"
        return 1
    fi
}

create_deb_package() {
    print_header "Создание DEB пакета"

    # Создаем структуру DEB пакета
    DEB_DIR="$TEMP_DIR/deb/$PROJECT_NAME-$VERSION"
    rm -rf "$DEB_DIR"
    mkdir -p "$DEB_DIR/DEBIAN"
    mkdir -p "$DEB_DIR/usr/bin"
    mkdir -p "$DEB_DIR/usr/share/applications"
    mkdir -p "$DEB_DIR/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$DEB_DIR/usr/share/doc/$PROJECT_NAME"

    # Копируем бинарник
    cp "$BUILD_DIR/$PROJECT_NAME" "$DEB_DIR/usr/bin/"
    chmod 755 "$DEB_DIR/usr/bin/$PROJECT_NAME"

    # Копируем desktop файл и иконку из AppDir
    if [ -d "$TEMP_DIR/AppDir" ]; then
        cp "$TEMP_DIR/AppDir/usr/share/applications/$PROJECT_NAME.desktop" \
           "$DEB_DIR/usr/share/applications/"
        cp "$TEMP_DIR/AppDir/usr/share/icons/hicolor/256x256/apps/$PROJECT_NAME.png" \
           "$DEB_DIR/usr/share/icons/hicolor/256x256/apps/"
    else
        # Создаем минимальный desktop файл
        cat > "$DEB_DIR/usr/share/applications/$PROJECT_NAME.desktop" << EOF
[Desktop Entry]
Type=Application
Name=$PROJECT_NAME
Comment=$DESCRIPTION
Exec=$PROJECT_NAME
Categories=Utility;
Terminal=false
EOF
    fi

    # Создаем control файл
    cat > "$DEB_DIR/DEBIAN/control" << EOF
Package: $PROJECT_NAME
Version: $VERSION
Section: utils
Priority: optional
Architecture: amd64
Depends: libqt6core6t64, libqt6widgets6t64, libqt6concurrent6t64, libc6 (>= 2.34), libstdc++6 (>= 11)
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 $PROJECT_NAME - инструмент для записи образов на USB-накопители
 и SD-карты с поддержкой различных форматов образов.
EOF

    # Создаем скрипты
    cat > "$DEB_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q
fi
if command -v update-icon-caches >/dev/null 2>&1; then
    update-icon-caches /usr/share/icons/hicolor
fi
EOF
    chmod 755 "$DEB_DIR/DEBIAN/postinst"

    # Собираем DEB пакет
    cd "$TEMP_DIR/deb"
    dpkg-deb --build "$PROJECT_NAME-$VERSION"

    if [ -f "$PROJECT_NAME-$VERSION.deb" ]; then
        mv "$PROJECT_NAME-$VERSION.deb" "$PACKAGE_DIR/"
        print_success "DEB пакет создан: $PACKAGE_DIR/$PROJECT_NAME-$VERSION.deb"
        return 0
    else
        print_error "Не удалось создать DEB пакет"
        return 1
    fi
}

create_rpm_package() {
    print_header "Создание RPM пакета"

    if ! check_command "rpmbuild"; then
        print_error "rpmbuild не найден"
        return 1
    fi

    # Создаем структуру для RPM
    RPM_DIR="$HOME/rpmbuild"
    mkdir -p "$RPM_DIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

    # Создаем spec файл
    cat > "$RPM_DIR/SPECS/$PROJECT_NAME.spec" << EOF
Name:       $PROJECT_NAME
Version:    $VERSION
Release:    1%{?dist}
Summary:    $DESCRIPTION
License:    GPLv3+
URL:        $WEBSITE
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  gcc-c++
Requires:       qt6-qtbase
Requires:       qt6-qtdeclarative

%description
$PROJECT_NAME - инструмент для записи образов на USB-накопители.

%prep
%setup -q

%build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make %{?_smp_mflags}

%install
cd build
make install DESTDIR=%{buildroot}

%files
%license LICENSE
%doc README.md
/usr/bin/%{name}

%changelog
* $(date "+%a %b %d %Y") $MAINTAINER - $VERSION-1
- Первый релиз
EOF

    # Архивируем исходники
    cd "$PROJECT_DIR"
    tar -czf "$RPM_DIR/SOURCES/$PROJECT_NAME-$VERSION.tar.gz" \
        --exclude="build" --exclude="release" --exclude=".git" .

    # Собираем RPM
    cd "$RPM_DIR"
    rpmbuild -bb "SPECS/$PROJECT_NAME.spec"

    # Находим созданный RPM
    RPM_FILE=$(find "$RPM_DIR/RPMS" -name "*.rpm" | head -1)
    if [ -n "$RPM_FILE" ]; then
        cp "$RPM_FILE" "$PACKAGE_DIR/"
        print_success "RPM пакет создан: $(basename "$RPM_FILE")"
        return 0
    else
        print_error "Не удалось создать RPM пакет"
        return 1
    fi
}

create_portable_linux() {
    print_header "Создание переносной версии для Linux"

    PORTABLE_DIR="$TEMP_DIR/portable/$PROJECT_NAME-$VERSION-linux"
    rm -rf "$PORTABLE_DIR"
    mkdir -p "$PORTABLE_DIR"

    # Копируем бинарник
    cp "$BUILD_DIR/$PROJECT_NAME" "$PORTABLE_DIR/"

    # Создаем скрипт запуска
    cat > "$PORTABLE_DIR/run.sh" << 'EOF'
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Проверка прав root
if [ "$EUID" -ne 0 ]; then
    echo "ВНИМАНИЕ: Программа запущена без прав root."
    echo "Для записи на USB запустите: sudo ./run.sh"
    echo ""
fi

# Запуск приложения
exec "./c-mile" "$@"
EOF

    chmod +x "$PORTABLE_DIR/run.sh"

    # Создаем README
    cat > "$PORTABLE_DIR/README.txt" << EOF
$PROJECT_NAME v$VERSION - Переносная версия для Linux

Использование:
  ./run.sh

Для записи на USB:
  sudo ./run.sh

Зависимости:
  - Qt6 библиотеки
  - Современный дистрибутив Linux
EOF

    # Архивируем
    cd "$TEMP_DIR/portable"
    tar -czf "$PACKAGE_DIR/$PROJECT_NAME-$VERSION-linux-portable.tar.gz" \
        "$(basename "$PORTABLE_DIR")"

    print_success "Переносная версия создана: $PROJECT_NAME-$VERSION-linux-portable.tar.gz"
    return 0
}

create_source_package() {
    print_header "Создание исходного пакета"

    SRC_DIR="$TEMP_DIR/source/$PROJECT_NAME-$VERSION"
    rm -rf "$SRC_DIR"
    mkdir -p "$SRC_DIR"

    # Копируем исходные файлы
    cp -r "$PROJECT_DIR"/* "$SRC_DIR/" 2>/dev/null || true

    # Удаляем временные файлы
    rm -rf "$SRC_DIR"/{build,release,.deps,.git} 2>/dev/null

    # Создаем минимальный CMakeLists.txt если нет
    if [ ! -f "$SRC_DIR/CMakeLists.txt" ]; then
        cat > "$SRC_DIR/CMakeLists.txt" << EOF
cmake_minimum_required(VERSION 3.16)
project($PROJECT_NAME VERSION $VERSION)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(\${PROJECT_NAME} \${SOURCES} \${HEADERS})
target_link_libraries(\${PROJECT_NAME} Qt6::Core Qt6::Widgets Qt6::Concurrent)

install(TARGETS \${PROJECT_NAME} DESTINATION bin)
EOF
    fi

    # Архивируем
    cd "$TEMP_DIR/source"
    tar -czf "$PACKAGE_DIR/$PROJECT_NAME-$VERSION-src.tar.gz" \
        "$PROJECT_NAME-$VERSION"

    print_success "Исходный пакет создан: $PROJECT_NAME-$VERSION-src.tar.gz"
    return 0
}

create_snap_package() {
    print_header "Создание Snap пакета"

    if ! check_command "snapcraft"; then
        print_error "snapcraft не найден. Установите snapcraft."
        return 1
    fi

    # Создаем snapcraft.yaml
    mkdir -p "$PROJECT_DIR/snap"
    cat > "$PROJECT_DIR/snap/snapcraft.yaml" << EOF
name: $PROJECT_NAME
version: '$VERSION'
summary: $DESCRIPTION
description: |
  $PROJECT_NAME - инструмент для записи образов на USB-накопители.

grade: stable
confinement: strict
base: core22

apps:
  $PROJECT_NAME:
    command: usr/bin/$PROJECT_NAME
    plugs:
      - hardware-observe
      - raw-usb
      - home
      - x11

parts:
  $PROJECT_NAME:
    plugin: cmake
    source: .
    build-packages:
      - qt6-base-dev
    stage-packages:
      - libqt6core6t64
      - libqt6widgets6t64
      - libqt6concurrent6t64
EOF

    # Собираем Snap
    cd "$PROJECT_DIR"
    if snapcraft; then
        mv "$PROJECT_NAME"*.snap "$PACKAGE_DIR/" 2>/dev/null
        print_success "Snap пакет создан"
        return 0
    else
        print_error "Ошибка при создании Snap пакета"
        return 1
    fi
}

show_menu() {
    clear
    echo -e "${CYAN}================================${NC}"
    echo -e "${CYAN}  Сборщик релизов $PROJECT_NAME  ${NC}"
    echo -e "${CYAN}================================${NC}"
    echo -e "${CYAN}Версия: $VERSION${NC}"
    echo -e "${CYAN}Дистрибутив: $(detect_distro)${NC}"
    echo -e "${CYAN}Директория пакетов: $PACKAGE_DIR${NC}"
    echo ""
    echo "Выберите действие:"
    echo " 1) Установить зависимости"
    echo " 2) Собрать все пакеты Linux"
    echo " 3) Собрать AppImage"
    echo " 4) Собрать DEB пакет"
    echo " 5) Собрать RPM пакет"
    echo " 6) Собрать переносную версию"
    echo " 7) Собрать Snap пакет"
    echo " 8) Собрать исходный пакет"
    echo " 9) Выполнить полный цикл (зависимости + все пакеты)"
    echo "10) Проверить зависимости"
    echo "11) Очистить все"
    echo "12) Выход"
    echo ""
    read -p "Выберите вариант (1-12): " choice
}

check_dependencies() {
    print_header "Проверка зависимостей"

    local deps_ok=true

    # Проверяем основные команды
    for cmd in cmake g++ make; do
        if check_command "$cmd"; then
            print_success "$cmd"
        else
            print_error "$cmd"
            deps_ok=false
        fi
    done

    # Проверяем Qt
    if [ -f "/usr/include/qt6/QtCore/QtCore" ] || [ -f "/usr/include/qt6/QtCore" ]; then
        print_success "Qt6 development files"
    else
        print_error "Qt6 development files"
        deps_ok=false
    fi

    # Проверяем инструменты пакетирования
    for tool in linuxdeploy dpkg-deb rpmbuild; do
        if check_command "$tool"; then
            print_success "$tool"
        else
            print_warning "$tool (опционально)"
        fi
    done

    if $deps_ok; then
        print_success "Все основные зависимости установлены"
        return 0
    else
        print_error "Некоторые зависимости отсутствуют"
        return 1
    fi
}

full_cycle() {
    print_header "Полный цикл сборки"

    # Устанавливаем зависимости
    if ! install_dependencies "full"; then
        print_error "Ошибка установки зависимостей"
        return 1
    fi

    # Очищаем и собираем
    clean_build
    if ! build_project "linux" "release"; then
        print_error "Ошибка сборки проекта"
        return 1
    fi

    # Создаем пакеты
    local success_count=0
    local total_count=0

    for pkg_func in create_appimage create_deb_package create_portable_linux create_source_package; do
        total_count=$((total_count + 1))
        if $pkg_func; then
            success_count=$((success_count + 1))
        fi
    done

    print_header "Результаты сборки"
    echo -e "${GREEN}Успешно: $success_count из $total_count пакетов${NC}"
    echo -e "${BLUE}Пакеты находятся в: $PACKAGE_DIR${NC}"

    if [ $success_count -gt 0 ]; then
        ls -la "$PACKAGE_DIR"
    fi

    return 0
}

main() {
    # Создаем необходимые директории
    mkdir -p "$BUILD_DIR" "$RELEASE_DIR" "$PACKAGE_DIR" "$TEMP_DIR" "$DEPENDENCIES_DIR"

    # Основной цикл
    while true; do
        show_menu

        case $choice in
            1)
                # Установка зависимостей
                if install_dependencies "full"; then
                    read -p "Нажмите Enter для продолжения..." -n 1
                fi
                ;;
            2)
                # Все пакеты Linux
                clean_build
                if build_project "linux" "release"; then
                    create_appimage
                    create_deb_package
                    create_portable_linux
                    create_source_package
                fi
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            3)
                # AppImage
                clean_build
                if build_project "linux" "release"; then
                    create_appimage
                fi
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            4)
                # DEB пакет
                clean_build
                if build_project "linux" "release"; then
                    create_deb_package
                fi
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            5)
                # RPM пакет
                clean_build
                if build_project "linux" "release"; then
                    create_rpm_package
                fi
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            6)
                # Переносная версия
                clean_build
                if build_project "linux" "release"; then
                    create_portable_linux
                fi
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            7)
                # Snap пакет
                clean_build
                if build_project "linux" "release"; then
                    create_snap_package
                fi
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            8)
                # Исходный пакет
                create_source_package
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            9)
                # Полный цикл
                full_cycle
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            10)
                # Проверка зависимостей
                check_dependencies
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            11)
                # Очистка
                print_info "Очистка всех временных файлов..."
                rm -rf "$BUILD_DIR" "$RELEASE_DIR" "$DEPENDENCIES_DIR"
                print_success "Очистка завершена"
                read -p "Нажмите Enter для продолжения..." -n 1
                ;;
            12)
                # Выход
                print_info "Выход..."
                exit 0
                ;;
            *)
                print_error "Неверный выбор"
                sleep 2
                ;;
        esac
    done
}

# Обработка аргументов командной строки
if [ $# -gt 0 ]; then
    case $1 in
        --install-deps|-i)
            install_dependencies "full"
            exit $?
            ;;
        --build|-b)
            clean_build
            build_project "linux" "release"
            exit $?
            ;;
        --appimage|-a)
            clean_build
            build_project "linux" "release"
            create_appimage
            exit $?
            ;;
        --deb|-d)
            clean_build
            build_project "linux" "release"
            create_deb_package
            exit $?
            ;;
        --portable|-p)
            clean_build
            build_project "linux" "release"
            create_portable_linux
            exit $?
            ;;
        --full|-f)
            full_cycle
            exit $?
            ;;
        --help|-h)
            echo "Использование: $0 [опция]"
            echo ""
            echo "Опции:"
            echo "  -i, --install-deps  Установить зависимости"
            echo "  -b, --build         Собрать проект"
            echo "  -a, --appimage      Собрать AppImage"
            echo "  -d, --deb           Собрать DEB пакет"
            echo "  -p, --portable      Собрать переносную версию"
            echo "  -f, --full          Полный цикл сборки"
            echo "  -h, --help          Показать эту справку"
            echo ""
            echo "Без аргументов: запуск интерактивного меню"
            exit 0
            ;;
        *)
            print_error "Неизвестная опция: $1"
            echo "Используйте $0 --help для справки"
            exit 1
            ;;
    esac
fi

# Запуск основного меню
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
