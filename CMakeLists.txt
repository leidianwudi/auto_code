cmake_minimum_required(VERSION 3.19)
project(auto_code LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(auto_code
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    templateengine.h
    templateengine.cpp
    codeeditor.h
    codeeditor.cpp
    templatehighlighter.h
    templatehighlighter.cpp
    jsonhighlighter.h
    jsonhighlighter.cpp
    tshighlighter.h
    tshighlighter.cpp
)

target_include_directories(auto_code PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(auto_code
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS auto_code
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET auto_code
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
