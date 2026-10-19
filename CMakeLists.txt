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
    src/core/ConfigIssue.h
    src/core/SystemConfig.h
    src/core/ConfigLoader.h
    src/core/ConfigLoader.cpp
)

target_link_libraries(DataMonitorPro
    PRIVATE
        Qt::Core
        Qt::Widgets
)

# 把源码树里的 config/ 目录路径编译进程序。
# 现在程序从源码目录读配置；等发布形态定下来，再加"优先读可执行文件
# 旁边的 config/" 那一层。
# Windows 路径统一换成正斜杠，避免反斜杠在字符串字面量里被当成转义符。
string(REPLACE "\\" "/" DM_CONFIG_DIR_NORMALIZED "${CMAKE_SOURCE_DIR}/config")
target_compile_definitions(DataMonitorPro PRIVATE
    DM_SOURCE_CONFIG_DIR="${DM_CONFIG_DIR_NORMALIZED}"
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
