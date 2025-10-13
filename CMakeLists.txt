cmake_minimum_required(VERSION 3.16)

project(PMPassThrough LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Bluetooth Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Bluetooth Core)

add_executable(PMPassThrough
  main.cpp
)
target_link_libraries(PMPassThrough Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Bluetooth)

include(GNUInstallDirs)
install(TARGETS PMPassThrough
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
