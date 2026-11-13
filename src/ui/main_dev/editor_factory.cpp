#include "editor_factory.h"

#include "src/engine/ac_language.h"
#include "src/ui/json_global_enum/json_global_enum_widget.h"
#include "src/ui/json_source/json_source_widget.h"
#include "src/ui/json_source/json_table_widget.h"
#include "src/ui/json_source/json_upload_widget.h"
#include "src/ui/json_source/project_acproj_widget.h"
#include "src/ui/json_vue/json_vue_editor.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/ui/schema_json/schema_json_widget.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/highlighter/light_ac.h"
#include "src/util/ui/highlighter/light_json.h"
#include "src/util/ui/highlighter/light_tpl.h"

CodeEditor *EditorFactory::createCodeEditor(const QString &filePath) {
  auto *editor = new CodeEditor;
  editor->setObjectName(filePath);

  if (filePath.endsWith(AcFileSuffix::kJson, Qt::CaseInsensitive)) {
    editor->setSyntaxHighlighter(new LightJson(editor->document()));
    editor->setValidationMode(CodeEditor::JsonValidation);
  } else if (filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
    editor->setSyntaxHighlighter(new LightAc(editor->document()));
    editor->setValidationMode(CodeEditor::AcValidation);
  } else {
    // .tpl 及其他未知类型统一按模板处理
    editor->setSyntaxHighlighter(new LightTpl(editor->document()));
    editor->setValidationMode(CodeEditor::TemplateValidation);
  }

  return editor;
}

QWidget *EditorFactory::create(const QString &filePath, const QString &content,
                               const QString &httpConfigPath, CodeEditor **editorOut) {
  CodeEditor *editor = nullptr;
  QWidget *tabWidget = nullptr;

  // .jsonvue / .jsonsource / .jsonupload / .jsontable / .jsonglobalenum / .acproj 文件
  // 使用可视化包装器（CodeEditor + 可视化编辑器）
  const bool isJsonVue = filePath.endsWith(AcFileSuffix::kJsonvue, Qt::CaseInsensitive);
  const bool isJsonSource = filePath.endsWith(AcFileSuffix::kJsonsource, Qt::CaseInsensitive);
  const bool isJsonUpload = filePath.endsWith(AcFileSuffix::kJsonupload, Qt::CaseInsensitive);
  const bool isJsonTable = filePath.endsWith(AcFileSuffix::kJsontable, Qt::CaseInsensitive);
  const bool isJsonGlobalEnum =
      filePath.endsWith(AcFileSuffix::kJsonglobalenum, Qt::CaseInsensitive);
  const bool isAcProj = filePath.endsWith(AcFileSuffix::kAcProj, Qt::CaseInsensitive);
  const QString acPath = httpConfigPath;

  if (isJsonVue) {
    auto *jvw = new JsonVueWidget;
    editor = jvw->codeEditor();
    editor->setObjectName(filePath);
    editor->setPlainText(content);
    // 缓存磁盘原文：可视化写回时以它为底做保真合并，防止未加载时保存把 dataName 数据清空
    jvw->setPreservedSource(content);
    // 记录文件路径：供可视化编辑器推导 .jsonsource 数据源搜索根目录
    jvw->setSourceFilePath(filePath);
    tabWidget = jvw;
    // 加载 HTTP 配置（供动态数据源"测试"按钮使用）
    if (!acPath.isEmpty()) jvw->loadHttpConfigFromAcFile(acPath);
  } else if (isJsonSource) {
    auto *jdw = new JsonSourceWidget;
    editor = jdw->codeEditor();
    editor->setObjectName(filePath);
    editor->setPlainText(content);
    jdw->setPreservedSource(content);
    tabWidget = jdw;
    // 加载 HTTP 配置（供动态数据源"测试"按钮使用）
    if (!acPath.isEmpty()) {
      QString baseUrl, authHeader, postData;
      JsonVueEditor::loadHttpConfigFromAcFile(acPath, &baseUrl, &authHeader, &postData);
      jdw->setHttpConfig(baseUrl, authHeader, postData);
    }
  } else if (isJsonUpload) {
    auto *juw = new JsonUploadWidget;
    editor = juw->codeEditor();
    editor->setObjectName(filePath);
    editor->setPlainText(content);
    juw->setPreservedSource(content);
    tabWidget = juw;
    // 注：上传预设无"测试请求"按钮，不需要 HTTP 配置
  } else if (isJsonTable) {
    auto *jtw = new JsonTableWidget;
    editor = jtw->codeEditor();
    editor->setObjectName(filePath);
    editor->setPlainText(content);
    editor->setObjectName(filePath);  // 供表设计器定位项目 meta 与数据源作用域
    jtw->setPreservedSource(content);
    tabWidget = jtw;
    // 注：表结构配置无"测试请求"按钮，不需要 HTTP 配置
  } else if (isJsonGlobalEnum) {
    auto *gew = new JsonGlobalEnumWidget;
    editor = gew->codeEditor();
    editor->setObjectName(filePath);
    editor->setPlainText(content);
    gew->setPreservedSource(content);
    tabWidget = gew;
    // 注：全局枚举为纯静态选项，无"测试请求"按钮，不需要 HTTP 配置
  } else if (isAcProj) {
    // .acproj 项目配置：代码 + 项目设置表单双视图
    auto *pcw = new ProjectAcprojWidget;
    editor = pcw->codeEditor();
    editor->setObjectName(filePath);
    editor->setPlainText(content);
    tabWidget = pcw;
    // 注：项目设置无"测试请求"按钮，不需要 HTTP 配置
  } else {
    CodeEditor *plain = createCodeEditor(filePath);
    plain->setPlainText(content);
    editor = plain;

    // 所有 .json 统一用 SchemaJsonWidget 包装：schema 由包装器按 $schema 解析加载；
    // 空文件/无 $schema 也可视化（表单顶部模板下拉选择 .schema.json 后编辑）。
    // 例外：*.schema.json 本身就是模板定义，没有 $schema 概念，
    // 套无关 schema 做表单编辑可能改写 schema 结构 —— 只保留代码模式
    const bool isJson = filePath.endsWith(AcFileSuffix::kJson, Qt::CaseInsensitive);
    const bool isSchemaFile = filePath.endsWith(AcFileSuffix::kSchemaJson, Qt::CaseInsensitive);
    if (isJson && !isSchemaFile) {
      auto *sjw = new SchemaJsonWidget;
      sjw->setSourceFilePath(filePath);
      sjw->codeEditor()->setObjectName(filePath);
      sjw->codeEditor()->setPlainText(content);
      editor = sjw->codeEditor();
      tabWidget = sjw;
      // 普通 editor 已被包装器内的 CodeEditor 取代，释放避免泄漏
      delete plain;
      plain = nullptr;
    }
    if (!tabWidget) tabWidget = editor;
  }

  // 所有入口（包括拆分）采用一致的路径与验证防抖策略。
  editor->setObjectName(filePath);
  editor->setValidationDebounce(
      filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive) || content.size() > 100 * 1024 ? 300
                                                                                               : 0);
  if (editorOut) *editorOut = editor;
  return tabWidget;
}
