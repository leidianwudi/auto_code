cmake_minimum_required(VERSION 3.19)
project(auto_code LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql)

qt_standard_project_setup()

# 本项目不使用 .ui 文件，关闭 AutoUic 避免将 aui_mgr.h 误判为 UI 表单头文件
set(CMAKE_AUTOUIC OFF)

qt_add_executable(auto_code
    WIN32 MACOSX_BUNDLE
    main.cpp
    res/app_icon.rc
    src/engine/script/ac_type.h
    src/engine/script/ac_type.cpp
    src/engine/script/ac_lexer.h
    src/engine/script/ac_lexer.cpp
    src/engine/script/ac_parser.h
    src/engine/script/ac_parser.cpp
    src/engine/script/ac_parser_expr.cpp
    src/engine/script/ac_parser_stmt.cpp
    src/engine/script/ac_interpreter.h
    src/engine/script/ac_interpreter.cpp
    src/engine/script/ac_interpreter_expr.cpp
    src/engine/script/ac_interpreter_stmt.cpp
    src/engine/script/ac_builtin_eval.h
    src/engine/script/ac_builtin_eval.cpp
    src/engine/script/ac_object_manager.h
    src/engine/script/ac_object_manager.cpp
    src/engine/script/ac_executor.h
    src/engine/script/ac_executor.cpp
    src/engine/script/ac_type_checker.h
    src/engine/script/ac_type_checker.cpp
    src/engine/script/ac_engine.h
    src/engine/script/ac_engine.cpp
    src/engine/script/ast_visitor.h
    src/engine/script/ast_visitor.cpp
    src/engine/script/undeclared_ident_validator.h
    src/engine/script/undeclared_ident_validator.cpp
    src/engine/script/ac_validator.h
    src/engine/script/ac_validator.cpp
    src/engine/script/ac_symbol_table.h
    src/engine/script/ac_symbol_table.cpp
    src/engine/tpl/tpl_engine.h
    src/engine/tpl/tpl_engine.cpp
    src/engine/tpl/tpl_validator.h
    src/engine/tpl/tpl_validator.cpp
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
    src/engine/schema_validator.h
    src/engine/schema_validator.cpp
    src/engine/validation_result.h
    src/engine/json_validator.h
    src/engine/json_validator.cpp
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
    src/util/common/util_json.h
    src/util/common/util_json.cpp
    src/util/common/util_file.h
    src/util/common/util_file.cpp
    src/util/ui/code/tree_dir.h
    src/util/ui/code/tree_dir.cpp
    src/util/ui/code/bracket_matcher.h
    src/util/ui/code/bracket_matcher.cpp
    src/util/ui/code/indent_guide.h
    src/util/ui/code/indent_guide.cpp
    src/util/ui/code/code_validator.h
    src/util/ui/code/code_validator.cpp
    src/util/ui/code/code_editor.h
    src/util/ui/code/code_editor.cpp
    src/util/ui/code/symbol_navigator.h
    src/util/ui/code/symbol_navigator.cpp
    src/util/ui/code/code_log.h
    src/util/ui/code/code_log.cpp
    src/util/ui/highlighter/light_color.h
    src/util/ui/highlighter/light_tpl.h
    src/util/ui/highlighter/light_tpl.cpp
    src/util/ui/highlighter/light_json.h
    src/util/ui/highlighter/light_json.cpp
    src/util/ui/highlighter/light_ts.h
    src/util/ui/highlighter/light_ts.cpp
    src/util/ui/highlighter/light_ac.h
    src/util/ui/highlighter/light_ac.cpp
    src/util/ui/aui_mgr.h
    src/util/ui/component/aui_style.h
    src/util/ui/component/aui_style.cpp
    src/util/ui/component/aui_button.h
    src/util/ui/component/aui_button.cpp
    src/util/ui/component/aui_icon.h
    src/util/ui/component/aui_icon.cpp
    src/util/ui/component/aui_combo_box.h
    src/util/ui/component/aui_combo_box.cpp
    src/util/ui/aui_window.h
    src/util/ui/aui_window.cpp
    src/util/ui/rename_dialog.h
    src/util/ui/rename_dialog.cpp
    src/util/ui/component/aui_error_tool_tip.h
    src/util/ui/component/aui_error_tool_tip.cpp
    src/util/ui/component/aui_message_box.h
    src/util/ui/component/aui_message_box.cpp
    src/util/ui/code/format_code.h
    src/util/ui/code/format_code.cpp
    src/util/ui/code/guess_code.h
    src/util/ui/code/guess_code.cpp
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
        dwmapi.lib
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

# ════════════════════════════════════════════════════════════
#  构建时生成 app_icon.ico（嵌入到 exe 中，影响 Windows 任务管理器图标）
# ════════════════════════════════════════════════════════════

# gen_icon 工具：编译后手动运行一次生成 res/app_icon.ico
#   build> gen_icon.exe D:/work/github/auto_code/res/app_icon.ico
# 生成后 .rc 文件会自动将其嵌入到 auto_code.exe 中
qt_add_executable(gen_icon EXCLUDE_FROM_ALL gen_icon.cpp)
target_include_directories(gen_icon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gen_icon PRIVATE Qt::Core Qt::Widgets)