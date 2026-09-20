cmake_minimum_required(VERSION 3.19)
project(auto_code LANGUAGES CXX)
# Qt 安装路径：优先读环境变量 QT_HOME（跨机器），未设置时回退到本机默认安装路径；
# 也可在命令行显式 -DCMAKE_PREFIX_PATH=... 覆盖
if(NOT DEFINED CMAKE_PREFIX_PATH)
    if(DEFINED ENV{QT_HOME})
        set(CMAKE_PREFIX_PATH "$ENV{QT_HOME}")
    else()
        set(CMAKE_PREFIX_PATH "D:/tool/Qt/6.12.0/msvc2022_64")
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network Svg Concurrent)

qt_standard_project_setup()

# 本项目不使用 .ui 文件，关闭 AutoUic 避免将 aui_mgr.h 误判为 UI 表单头文件
set(CMAKE_AUTOUIC OFF)

# ════════════════════════════════════════════════════════════
#  库边界拆分
#  - ac_core  : accore JSON 值模型（顺序敏感，无 QJson 语义依赖）
#  - ac_engine: 脚本引擎/模板/语义/内置函数 + 非界面工具（util/common）
#  主程序只保留 UI 层（src/ui + src/util/ui），链接库复用引擎；
#  auto_code_tests 直接链接同一份库，消除手工摘抄源文件的双份维护。
# ════════════════════════════════════════════════════════════

add_library(ac_core STATIC
    src/core/json/ac_json_value.h
    src/core/json/ac_json_value.cpp
    src/core/json/ac_json_parse.cpp
    src/core/json/ac_json_write.cpp
)
target_include_directories(ac_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ac_core PUBLIC Qt::Core)

add_library(ac_engine STATIC
    # ── 脚本引擎 ──
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
    src/engine/script/ac_debugger.h
    src/engine/script/ac_debugger.cpp
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
    src/engine/script/ac_builtin_loader.h
    src/engine/script/ac_builtin_loader.cpp
    src/engine/script/ac_symbol_table.h
    src/engine/script/ac_symbol_table.cpp
    # ── 模板引擎 ──
    src/engine/tpl/tpl_engine.h
    src/engine/tpl/tpl_engine.cpp
    src/engine/tpl/tpl_validator.h
    src/engine/tpl/tpl_validator.cpp
    src/engine/tpl/tpl_ast.h
    src/engine/tpl/tpl_lexer.h
    src/engine/tpl/tpl_lexer.cpp
    src/engine/tpl/tpl_parser.h
    src/engine/tpl/tpl_parser.cpp
    src/engine/tpl/tpl_renderer.h
    src/engine/tpl/tpl_renderer.cpp
    # ── 校验 / 重命名 / 语义 ──
    src/engine/schema_validator.h
    src/engine/schema_validator.cpp
    src/engine/validation_result.h
    src/engine/json_validator.h
    src/engine/json_validator.cpp
    src/engine/rename/symbol_rename.h
    src/engine/rename/symbol_rename.cpp
    src/engine/semantic/workspace_index.h
    src/engine/semantic/workspace_index.cpp
    # ── 内置函数 ──
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
    src/engine/function/fun_args.h
    src/engine/function/fun_builtin.h
    src/engine/function/fun_builtin.cpp
    src/engine/ac_language.h
    # ── 非界面工具（util/common）──
    src/util/common/util_json.h
    src/util/common/util_json.cpp
    src/util/common/util_file.h
    src/util/common/util_file.cpp
    src/util/common/path_resolver.h
    src/util/common/path_resolver.cpp
    src/util/common/http_client.h
    src/util/common/http_client.cpp
    src/util/common/ac_log.h
    src/util/common/workspace_diag.h
    src/util/common/workspace_diag.cpp
    src/util/common/workspace_iter.h
)
target_include_directories(ac_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-connector-c/include
)
target_compile_definitions(ac_engine PUBLIC
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    # 调试模式宏：需要调试 ac 引擎 / 模板引擎日志时取消注释
    # AC_DEBUG
)
target_link_libraries(ac_engine
    PUBLIC
        ac_core
        Qt::Core
        Qt::Network
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-connector-c/lib/libmysql.lib
)

qt_add_executable(auto_code
    WIN32 MACOSX_BUNDLE
    main.cpp
    res/app_icon.rc
    # ── UI 工具层（src/util/ui + util/design）──
    src/util/ui/code/tree_dir.h
    src/util/ui/code/tree_dir.cpp
    src/util/ui/code/tree_state_store.h
    src/util/ui/code/tree_state_store.cpp
    src/util/ui/code/bracket_matcher.h
    src/util/ui/code/bracket_matcher.cpp
    src/util/ui/code/indent_guide.h
    src/util/ui/code/indent_guide.cpp
    src/util/ui/code/code_editor.h
    src/util/ui/code/code_editor.cpp
    src/util/ui/code/code_visual_sync_widget.h
    src/util/ui/code/code_visual_sync_widget.cpp
    src/util/ui/code/code_editor_debug.cpp
    src/util/ui/code/code_editor_fold.cpp
    src/util/ui/code/code_editor_validate.cpp
    src/util/ui/code/code_editor_navigate.cpp
    src/util/ui/code/code_editor_complete.cpp
    src/util/ui/code/code_find_bar.h
    src/util/ui/code/code_find_bar.cpp
    src/util/ui/code/symbol_navigator.h
    src/util/ui/code/symbol_navigator.cpp
    src/util/ui/code/code_log.h
    src/util/ui/code/code_log.cpp
    src/util/ui/code/problem_panel.h
    src/util/ui/code/problem_panel.cpp
    src/util/ui/code/search_panel.h
    src/util/ui/code/search_panel.cpp
    src/util/ui/code/reference_panel.h
    src/util/ui/code/reference_panel.cpp
    src/util/ui/code/vsc_result_tree.h
    src/util/ui/code/vsc_result_tree.cpp
    src/util/ui/code/vsc_result_panel.h
    src/util/ui/code/vsc_result_panel.cpp
    src/util/ui/code/comment_scan.h
    src/util/ui/highlighter/light_color.h
    src/util/ui/highlighter/light_tpl.h
    src/util/ui/highlighter/light_tpl.cpp
    src/util/ui/highlighter/light_json.h
    src/util/ui/highlighter/light_json.cpp
    src/util/ui/highlighter/light_ts.h
    src/util/ui/highlighter/light_ts.cpp
    src/util/ui/highlighter/light_ac.h
    src/util/ui/highlighter/light_ac.cpp
    src/util/design/singleton.h
    src/util/ui/aui_mgr.h
    src/util/ui/setting_store.h
    src/util/ui/setting_store.cpp
    src/util/ui/component/aui_style.h
    src/util/ui/component/aui_style.cpp
    src/util/ui/component/aui_multi_check_combo.h
    src/util/ui/component/aui_multi_check_combo.cpp
    src/util/ui/component/aui_tab_bar.h
    src/util/ui/component/aui_tab_bar.cpp
    src/util/ui/component/aui_code_tab_bar.h
    src/util/ui/component/aui_code_tab_bar.cpp
    src/util/ui/component/aui_button.h
    src/util/ui/component/aui_button.cpp
    src/util/ui/component/aui_icon.h
    src/util/ui/component/aui_icon.cpp
    src/util/ui/component/aui_tree.h
    src/util/ui/component/aui_tree.cpp
    src/util/ui/component/aui_combo_box.h
    src/util/ui/component/aui_combo_box.cpp
    src/util/ui/component/aui_combo_delete.h
    src/util/ui/component/aui_combo_delete.cpp
    src/util/ui/aui_window.h
    src/util/ui/aui_window.cpp
    src/util/ui/rename_dialog.h
    src/util/ui/rename_dialog.cpp
    src/util/ui/component/aui_error_tool_tip.h
    src/util/ui/component/aui_error_tool_tip.cpp
    src/util/ui/component/aui_message_box.h
    src/util/ui/component/aui_message_box.cpp
    src/util/ui/component/aui_input_dialog.h
    src/util/ui/component/aui_input_dialog.cpp
    src/util/ui/code/format_code.h
    src/util/ui/code/format_code.cpp
    src/util/ui/code/guess_code.h
    src/util/ui/code/guess_code.cpp
    # ── UI 业务层（src/ui）──
    src/ui/main_dev/main_dev_ui.h
    src/ui/main_dev/main_dev_ui.cpp
    src/ui/main_dev/main_dev_ui_ext.h
    src/ui/main_dev/main_dev_ui_ext.cpp
    src/ui/main_dev/debug_panel.h
    src/ui/main_dev/debug_panel.cpp
    src/ui/main_dev/debug_controller.h
    src/ui/main_dev/debug_controller.cpp
    src/ui/main_dev/editor_lookup.h
    src/ui/main_dev/main_dev_mgr.h
    src/ui/main_dev/main_dev_mgr.cpp
    src/ui/main_dev/main_dev_mgr_file.cpp
    src/ui/main_dev/main_dev_mgr_tab.cpp
    src/ui/main_dev/main_dev_mgr_connect.cpp
    src/ui/main_dev/main_dev_mgr_navigate.cpp
    src/ui/main_dev/main_dev_mgr_rename.cpp
    src/ui/main_dev/main_dev_mgr_session.cpp
    src/ui/main_dev/main_dev_mgr_theme.cpp
    src/ui/main_dev/main_dev_model.h
    src/ui/main_dev/main_dev_model.cpp
    src/ui/demo/demo_ui.h
    src/ui/demo/demo_ui.cpp
    src/ui/demo/demo_mgr.h
    src/ui/demo/demo_mgr.cpp
    src/ui/demo/demo_model.h
    src/ui/demo/demo_model.cpp
    src/ui/help/help_doc_data.h
    src/ui/help/help_doc_model.h
    src/ui/help/help_doc_ui.h
    src/ui/help/help_doc_ui.cpp
    src/ui/help/help_doc_mgr.h
    src/ui/help/help_doc_mgr.cpp
    src/ui/help/about_dialog.h
    src/ui/help/about_dialog.cpp
    src/ui/create/create_ui.h
    src/ui/create/create_ui.cpp
    src/ui/create/create_mgr.h
    src/ui/create/create_mgr.cpp
    src/ui/create/create_model.h
    src/ui/create/create_model.cpp
    src/ui/json_vue/json_vue_model.h
    src/ui/json_vue/json_vue_model.cpp
    src/ui/json_vue/json_vue_editor.h
    src/ui/json_vue/json_vue_editor.cpp
    src/ui/json_vue/json_vue_editor_http.cpp
    src/ui/json_vue/json_vue_editor_columns.cpp
    src/ui/json_vue/json_vue_editor_config.cpp
    src/ui/json_vue/json_vue_editor_helpers.h
    src/ui/json_vue/combobox_config_dialog.h
    src/ui/json_vue/combobox_config_dialog.cpp
    src/ui/json_vue/config_dialog_common.h
    src/ui/json_vue/config_dialog_common.cpp
    src/ui/json_vue/style_config_dialog.h
    src/ui/json_vue/style_config_dialog.cpp
    src/ui/json_vue/style_config_query_dialog.cpp
    src/ui/json_vue/button_config_dialog.h
    src/ui/json_vue/button_config_dialog.cpp
    src/ui/json_vue/icon_loader.h
    src/ui/json_vue/icon_loader.cpp
    src/ui/json_vue/icon_picker_dialog.h
    src/ui/json_vue/icon_picker_dialog.cpp
    src/ui/json_vue/icon_picker_data.cpp
    src/ui/json_vue/json_vue_widget.h
    src/ui/json_vue/json_vue_widget.cpp
    src/ui/json_vue/select_source_panel.h
    src/ui/json_vue/select_source_panel.cpp
    src/ui/json_source/json_source_model.h
    src/ui/json_source/json_source_model.cpp
    src/ui/json_source/json_source_finder.h
    src/ui/json_source/json_source_dialog.h
    src/ui/json_source/json_source_dialog.cpp
    src/ui/json_source/json_source_editor.h
    src/ui/json_source/json_source_editor.cpp
    src/ui/json_source/json_source_widget.h
    src/ui/json_source/json_source_widget.cpp
    src/ui/json_source/json_upload_model.h
    src/ui/json_source/json_upload_model.cpp
    src/ui/json_source/json_upload_dialog.h
    src/ui/json_source/json_upload_dialog.cpp
    src/ui/json_source/json_upload_editor.h
    src/ui/json_source/json_upload_editor.cpp
    src/ui/json_source/json_upload_widget.h
    src/ui/json_source/json_upload_widget.cpp
    src/ui/schema_json/schema_form_editor.h
    src/ui/schema_json/schema_form_editor.cpp
    src/ui/schema_json/schema_json_widget.h
    src/ui/schema_json/schema_json_widget.cpp
    src/ui/setting/setting_model.h
    src/ui/setting/setting_model.cpp
    src/ui/setting/setting_ui.h
    src/ui/setting/setting_ui.cpp
    src/ui/setting/setting_mgr.h
    src/ui/setting/setting_mgr.cpp
)

target_include_directories(auto_code PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(auto_code
    PRIVATE
        ac_core
        ac_engine
        Qt::Widgets
        Qt::Sql
        Qt::Svg
        Qt::Concurrent
        dwmapi.lib
)

# ── MySQL 运行时依赖自动部署（libmysql.dll + OpenSSL） ──
# DLL 文件存放在 third_party/mysql-connector-c/ 下，不依赖本机 MySQL 安装
set(MYSQL_THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mysql-connector-c")
set(MYSQL_LIB_DIR "${MYSQL_THIRDPARTY_DIR}/lib")
set(MYSQL_BIN_DIR "${MYSQL_THIRDPARTY_DIR}/bin")

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

# ── Qt 运行时自动部署：windeployqt 将 Qt DLL/插件复制到 exe 所在目录 ──
# 使 exe 目录自包含，脱离 IDE 注入的 PATH 也能直接启动（CMake Tools 三角按钮/资源管理器双击均可）
find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin" REQUIRED)
add_custom_command(TARGET auto_code POST_BUILD
    COMMAND ${WINDEPLOYQT_EXE} $<TARGET_FILE:auto_code>
    COMMENT "Deploying Qt runtime to $<TARGET_FILE_DIR:auto_code>"
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

# ════════════════════════════════════════════════════════════
#  单元测试（EXCLUDE_FROM_ALL：不随主程序构建）
#  构建：cmake --build <build-dir> --target auto_code_tests --config Debug
#  运行：<build-dir>/Debug/auto_code_tests.exe（返回 0 = 全部通过）
# ════════════════════════════════════════════════════════════
add_executable(auto_code_tests EXCLUDE_FROM_ALL
    tests/test_json_utils.cpp
    tests/test_ac_param_default.cpp
    tests/test_ac_json_value.cpp
    tests/test_ac_ident_pool.cpp
    tests/test_ac_interpreter.cpp
    # 端到端 golden 脚本测试（QProcess 调主程序 --run，无引擎链接依赖）
    tests/test_golden_script.cpp
    # JSON 上传模型测试所需（UI 层纯模型文件，测试直接编译）
    src/ui/json_source/json_upload_model.cpp
)
target_include_directories(auto_code_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(auto_code_tests PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
# 直接链接引擎库（与主程序同一份产物），不再手工摘抄引擎源文件
target_link_libraries(auto_code_tests PRIVATE
    ac_core
    ac_engine
    Qt::Core
)