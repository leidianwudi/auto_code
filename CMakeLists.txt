cmake_minimum_required(VERSION 3.19)
project(auto_code LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql)

qt_standard_project_setup()

# 本项目不使用 .ui 文件，关闭 AutoUic 避免将 aui_mgr.h 误判为 UI 表单头文件
set(CMAKE_AUTOUIC OFF)

qt_add_executable(auto_code
    WIN32 MACOSX_BUNDLE
    main.cpp
    src/engine/engine_tpl.h
    src/engine/engine_tpl.cpp
    src/engine/engine_ac.h
    src/engine/engine_ac.cpp
    src/engine/script_parser.h
    src/engine/script_parser.cpp
    src/engine/schema_validator.h
    src/engine/schema_validator.cpp
    src/engine/handler/block_handler.h
    src/engine/handler/block_handler.cpp
    src/engine/handler/each_block_handler.h
    src/engine/handler/each_block_handler.cpp
    src/engine/handler/if_block_handler.h
    src/engine/handler/if_block_handler.cpp
    src/engine/handler/expression_handler.h
    src/engine/handler/expression_handler.cpp
    src/engine/function/fun_str.h
    src/engine/function/fun_str.cpp
    src/engine/function/fun_db.h
    src/engine/function/fun_db.cpp
    src/engine/function/fun_file.h
    src/engine/function/fun_file.cpp
    src/engine/function/fun_mgr.h
    src/engine/function/fun_mgr.cpp
    src/engine/function/fun_const.h
    src/engine/function/fun_const.cpp
    src/tool/ui/tree_dir.h
    src/tool/ui/tree_dir.cpp
    src/tool/ui/code_editor.h
    src/tool/ui/code_editor.cpp
    src/tool/ui/highlighter/lighter_color.h
    src/tool/ui/highlighter/tpl_highlighter.h
    src/tool/ui/highlighter/tpl_highlighter.cpp
    src/tool/ui/highlighter/json_highlighter.h
    src/tool/ui/highlighter/json_highlighter.cpp
    src/tool/ui/highlighter/ts_highlighter.h
    src/tool/ui/highlighter/ts_highlighter.cpp
    src/tool/ui/highlighter/ac_highlighter.h
    src/tool/ui/highlighter/ac_highlighter.cpp
    src/tool/ui/aui_mgr.h
    src/tool/ui/aui_style.h
    src/tool/ui/aui_style.cpp
    src/tool/ui/aui_button.h
    src/tool/ui/aui_button.cpp
    src/tool/ui/aui_icon.h
    src/tool/ui/aui_icon.cpp
    src/ui/main_dev/main_dev_ui.h
    src/ui/main_dev/main_dev_ui.cpp
    src/ui/main_dev/main_dev_ui_ext.h
    src/ui/main_dev/main_dev_ui_ext.cpp
    src/ui/main_dev/main_dev_mgr.h
    src/ui/main_dev/main_dev_mgr.cpp
    src/ui/main_dev/main_dev_model.h
    src/ui/main_dev/main_dev_model.cpp
    src/ui/demo/demo_ui.h
    src/ui/demo/demo_ui.cpp
    src/ui/demo/demo_mgr.h
    src/ui/demo/demo_mgr.cpp
    src/ui/demo/demo_model.h
    src/ui/demo/demo_model.cpp
)

target_include_directories(auto_code PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    D:/tool/mysql9_0/include
)

# 注入项目源码路径，使运行时能准确找到 file/ 目录
target_compile_definitions(auto_code PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(auto_code
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Sql
        D:/tool/mysql9_0/lib/libmysql.lib
)

# ── MySQL 运行时依赖自动部署（libmysql.dll + OpenSSL） ──
# 以下是 MySQL 本机的安装目录
set(MYSQL_LIB_DIR "D:/tool/mysql9_0/lib")
set(MYSQL_BIN_DIR "D:/tool/mysql9_0/bin")

set(MYSQL_RUNTIME_DLLS
    "${MYSQL_LIB_DIR}/libmysql.dll"
    "${MYSQL_BIN_DIR}/libssl-3-x64.dll"
    "${MYSQL_BIN_DIR}/libcrypto-3-x64.dll"
)

# ① CMake Configure 时复制到构建根目录
foreach(dll ${MYSQL_RUNTIME_DLLS})
    file(COPY "${dll}" DESTINATION "${CMAKE_BINARY_DIR}")
endforeach()

# ② 每次编译后复制到 exe 输出目录
add_custom_command(TARGET auto_code POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MYSQL_LIB_DIR}/libmysql.dll"
        "$<TARGET_FILE_DIR:auto_code>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MYSQL_BIN_DIR}/libssl-3-x64.dll"
        "$<TARGET_FILE_DIR:auto_code>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MYSQL_BIN_DIR}/libcrypto-3-x64.dll"
        "$<TARGET_FILE_DIR:auto_code>"
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