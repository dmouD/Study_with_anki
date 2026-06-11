cmake_minimum_required(VERSION 3.16)

project(DStudy VERSION 1.0 LANGUAGES CXX)

# 使用 C++17，和 Qt Creator 中常见的 Qt6/CMake 项目配置保持一致。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启 AUTOMOC 后，包含 Q_OBJECT 的类会自动运行 Qt 的 moc 工具。
set(CMAKE_AUTOMOC ON)

# Widgets 提供传统桌面控件，Sql 提供 QSqlDatabase/QSqlQuery，Network 用于访问 DeepSeek API。
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network)

# 项目源码列表。没有使用 .ui 文件，所有界面都在 C++ 代码里创建。
add_executable(DStudy
    main.cpp
    mainwindow.h
    mainwindow.cpp
    task.h
    task.cpp
    taskdialog.h
    taskdialog.cpp
    flashcard.h
    admindebugdialog.h
    admindebugdialog.cpp
    carddialog.h
    carddialog.cpp
    reviewwidget.h
    reviewwidget.cpp
    databasemanager.h
    databasemanager.cpp
    deepseekclient.h
    deepseekclient.cpp
)

# 链接 Qt Widgets、Qt SQL 和 Qt Network 模块。
target_link_libraries(DStudy PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)
