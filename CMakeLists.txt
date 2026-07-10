cmake_minimum_required(VERSION 3.19)
project(auto_code LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql)

qt_standard_project_setup()

# 本项目不使用 .ui 文件，关闭 AutoUic 避免将 aui_mgr.h 误判为 UI 表单头文件
set(CMAKE_AUTOUIC OFF)

qt_add_executable(auto_code
    WIN32 MACOSX_BUNDLE
    main.cpp
    src/engine/script/ac_type.h
    src/engine/script/ac_lexer.h
    src/engine/script/ac_lexer.cpp
    src/engine/script/ac_parser.h
    src/engine/script/ac_parser.cpp
    src/engine/script/ac_interpreter.h
    src/engine/script/ac_interpreter.cpp
    src/engine/script/ac_executor.h
    src/engine/script/ac_executor.cpp
    src/engine/script/ac_engine.h
    src/engine/script/ac_engine.cpp
    src/engine/tpl/tpl_engine.h
    src/engine/tpl/tpl_engine.cpp
    src/engine/tpl/handlers/tpl_block.h
    src/engine/tpl/handlers/tpl_block.cpp
    src/engine/tpl/handlers/tpl_factory.h
    src/engine/tpl/handlers/tpl_factory.cpp
    src/engine/tpl/handlers/block_each.h
    src/engine/tpl/handlers/block_each.cpp
    src/engine/tpl/handlers/block_if.h
    src/engine/tpl/handlers/block_if.cpp
    src/engine/tpl/handlers/block_expression.h
    src/engine/tpl/handlers/block_expression.cpp
    src/engine/validator_json.h
    src/engine/validator_json.cpp
    src/engine/function/fun_str.h
    src/engine/function/fun_str.cpp
    src/engine/function/fun_db.h
    src/engine/function/fun_db.cpp
    src/engine/function/fun_file.h
    src/engine/function/fun_file.cpp
    src/engine/function/fun_json.h
    src/engine/function/fun_json.cpp
    src/engine/function/fun_mgr.h
    src/engine/function/fun_mgr.cpp
    src/engine/function/fun_builtin.h
    src/engine/function/fun_builtin.cpp
    src/engine/ac_language.h
    src/tool/ui/tree_dir.h
    src/tool/ui/tree_dir.cpp
    src/tool/ui/code_editor.h
    src/tool/ui/code_editor.cpp
    src/tool/ui/code_log.h
    src/tool/ui/code_log.cpp
    src/tool/ui/highlighter/light_color.h
    src/tool/ui/highlighter/light_tpl.h
    src/tool/ui/highlighter/light_tpl.cpp
    src/tool/ui/highlighter/light_json.h
    src/tool/ui/highlighter/light_json.cpp
    src/tool/ui/highlighter/light_ts.h
    src/tool/ui/highlighter/light_ts.cpp
    src/tool/ui/highlighter/light_ac.h
    src/tool/ui/highlighter/light_ac.cpp
    src/tool/ui/aui_mgr.h
    src/tool/ui/aui_style.h
    src/tool/ui/aui_style.cpp
    src/tool/ui/aui_button.h
    src/tool/ui/aui_button.cpp
    src/tool/ui/aui_icon.h
    src/tool/ui/aui_icon.cpp
    src/tool/ui/aui_combo_box.h
    src/tool/ui/aui_combo_box.cpp
    src/tool/ui/aui_error_tool_tip.h
    src/tool/ui/aui_error_tool_tip.cpp
    src/tool/ui/format/format_code.h
    src/tool/ui/format/format_code.cpp
    src/tool/ui/guess/guess_code.h
    src/tool/ui/guess/guess_code.cpp
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
    src/ui/create/create_ui.h
    src/ui/create/create_ui.cpp
    src/ui/create/create_mgr.h
    src/ui/create/create_mgr.cpp
    src/ui/create/create_model.h
    src/ui/create/create_model.cpp
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