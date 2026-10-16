cmake_minimum_required(VERSION 3.19)
project(DataMonitorPro LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

# 把 src/ 设为头文件包含根，于是所有模块统一写成：
#     #include "core/Types.h"
#     #include "ui/MainWindow.h"
# 包含路径本身就体现了分层。
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

qt_add_executable(DataMonitorPro
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.ui
    src/core/Types.h
    src/core/Types.cpp
)

target_link_libraries(DataMonitorPro
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS DataMonitorPro
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET DataMonitorPro
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
