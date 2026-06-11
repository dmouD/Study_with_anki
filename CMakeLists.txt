cmake_minimum_required(VERSION 3.16)

project(DStudy VERSION 1.0 LANGUAGES CXX)

include(GNUInstallDirs)

# 使用 C++17，和 Qt Creator 中常见的 Qt6/CMake 项目配置保持一致。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启 AUTOMOC 后，包含 Q_OBJECT 的类会自动运行 Qt 的 moc 工具。
set(CMAKE_AUTOMOC ON)

if(WIN32)
    enable_language(RC)
endif()

# Widgets 提供传统桌面控件，Sql 提供 QSqlDatabase/QSqlQuery，Network 用于访问 DeepSeek API。
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Network)
find_package(ZLIB REQUIRED)

set(DSTUDY_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    task.h
    task.cpp
    taskdialog.h
    taskdialog.cpp
    flashcard.h
    ankipackageimporter.h
    ankipackageimporter.cpp
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

if(WIN32)
    set(DSTUDY_WINDOWS_RC resources/windows/DStudy.rc)
    set_source_files_properties(${DSTUDY_WINDOWS_RC}
        PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows"
    )
    list(APPEND DSTUDY_SOURCES ${DSTUDY_WINDOWS_RC})
endif()

# 项目源码列表。没有使用 .ui 文件，所有界面都在 C++ 代码里创建。
add_executable(DStudy ${DSTUDY_SOURCES})

if(WIN32)
    set_target_properties(DStudy PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# 链接 Qt Widgets、Qt SQL 和 Qt Network 模块。
target_link_libraries(DStudy PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    ZLIB::ZLIB
)

install(TARGETS DStudy
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

if(UNIX AND NOT APPLE)
    install(FILES packaging/dstudy.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES resources/icons/dstudy.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    )
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_target(deploy-qt
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:DStudy>"
            DEPENDS DStudy
            COMMENT "Deploying Qt runtime DLLs with windeployqt"
        )

        install(CODE "
            message(STATUS \"Deploying Qt runtime DLLs with windeployqt\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/DStudy.exe\"
                RESULT_VARIABLE DSTUDY_WINDEPLOYQT_RESULT
            )
            if(NOT DSTUDY_WINDEPLOYQT_RESULT EQUAL 0)
                message(FATAL_ERROR \"windeployqt failed with code \${DSTUDY_WINDEPLOYQT_RESULT}\")
            endif()
        ")
    else()
        message(WARNING "windeployqt was not found. Windows packages will need Qt DLL deployment before distribution.")
    endif()
endif()

set(CPACK_PACKAGE_NAME "DStudy")
set(CPACK_PACKAGE_VENDOR "DStudy")
set(CPACK_PACKAGE_CONTACT "DStudy Maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Local study planner and Anki-style review tool")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "DStudy")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "DStudy")
    set(CPACK_NSIS_PACKAGE_NAME "DStudy")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/DStudy.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/DStudy.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\DStudy.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "DStudy Maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "education")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
