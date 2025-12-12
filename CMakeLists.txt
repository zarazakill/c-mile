# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(cmile VERSION 0.7.4 LANGUAGES CXX)

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
    formatmanager.cpp
)

set(HEADERS
    mainwindow.h
    devicemanager.h
    imagewriter.h
    utils.h
    formatmanager.h
)

add_executable(cmile ${SOURCES} ${HEADERS})
set_target_properties(cmile PROPERTIES OUTPUT_NAME "c-mile")

target_link_libraries(cmile
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
)

# Убедитесь, что все заголовки видны
target_include_directories(cmile PRIVATE .)

# Установка правильных путей
install(TARGETS cmile DESTINATION bin)
