# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(AdvancedImageWriter VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)

# Исходники
set(SOURCES
    main.cpp
    mainwindow.cpp
    devicemanager.cpp
    imagewriter.cpp
)

set(HEADERS
    mainwindow.h
    devicemanager.h
    imagewriter.h
    utils.h
)

add_executable(AdvancedImageWriter ${SOURCES} ${HEADERS})

target_link_libraries(AdvancedImageWriter
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
)

# Убедитесь, что все заголовки видны
target_include_directories(AdvancedImageWriter PRIVATE .)

# Установка правильных путей
install(TARGETS AdvancedImageWriter DESTINATION bin)
