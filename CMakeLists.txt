cmake_minimum_required(VERSION 3.19)
project(auto_code LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(auto_code
    WIN32 MACOSX_BUNDLE
    main.cpp
    main_window.cpp
    main_window.h
    main_window.ui
    src/engine/template_engine.h
    src/engine/template_engine.cpp
    src/engine/handler/block_handler.h
    src/engine/handler/block_handler.cpp
    src/engine/handler/each_block_handler.h
    src/engine/handler/each_block_handler.cpp
    src/engine/handler/if_block_handler.h
    src/engine/handler/if_block_handler.cpp
    src/engine/handler/expression_handler.h
    src/engine/handler/expression_handler.cpp
    src/ui/tool_ui/code_editor.h
    src/ui/tool_ui/code_editor.cpp
    src/ui/tool_ui/highlighter/template_highlighter.h
    src/ui/tool_ui/highlighter/template_highlighter.cpp
    src/ui/tool_ui/highlighter/json_highlighter.h
    src/ui/tool_ui/highlighter/json_highlighter.cpp
    src/ui/tool_ui/highlighter/ts_highlighter.h
    src/ui/tool_ui/highlighter/ts_highlighter.cpp
    src/ui/main_dev/main_dev_ui.h
    src/ui/main_dev/main_dev_ui.cpp
    src/ui/main_dev/main_dev_mgr.h
    src/ui/main_dev/main_dev_mgr.cpp
    src/ui/main_dev/main_dev_model.h
    src/ui/main_dev/main_dev_model.cpp
)

target_include_directories(auto_code PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 注入项目源码路径，使运行时能准确找到 file/ 目录
target_compile_definitions(auto_code PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

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