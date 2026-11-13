/** @file test_editor_ui.cpp
 * @brief 编辑器工厂和代码/可视化切换的数据保护测试（offscreen，无窗口交互）。
 */
#include <QApplication>
#include <QEventLoop>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <memory>

#include "src/ui/json_source/json_source_editor.h"
#include "src/ui/json_source/json_source_widget.h"
#include "src/ui/json_source/json_table_model.h"
#include "src/ui/main_dev/editor_factory.h"
#include "src/ui/main_dev/main_dev_model.h"
#include "src/ui/schema_json/schema_form_editor.h"
#include "src/ui/schema_json/schema_json_widget.h"
#include "src/util/common/util_file.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/code_visual_sync_widget.h"
#include "src/util/ui/setting_store.h"

namespace {
int total = 0;
int failed = 0;
#define CHECK(condition)                                              \
  do {                                                                \
    ++total;                                                          \
    if (!(condition)) {                                               \
      ++failed;                                                       \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); \
    }                                                                 \
  } while (0)

/// 处理回填、焦点和防抖信号，避免只检查同步调用时漏掉误标修改。
void waitForVisualSync() {
  QEventLoop loop;
  QTimer::singleShot(350, &loop, &QEventLoop::quit);
  loop.exec();
}

/// 仅打开、无值变化通知、保存前同步和切换视图，都必须保留原文及未修改状态。
void testUnchangedVisualContent() {
  QTemporaryDir dir;
  const QList<QPair<QString, QString>> cases{
      {QStringLiteral(".json"), QStringLiteral("// 原文注释\n{z: 1, a: '原样保留',}\n")},
      {QStringLiteral(".jsonvue"), QStringLiteral("{extra: '保留'}")},
      {QStringLiteral(".jsonsource"), QStringLiteral("{sources: [], extra: '保留'}")},
      {QStringLiteral(".jsonupload"), QStringLiteral("{uploads: [], extra: '保留'}")},
      {QStringLiteral(".jsonglobalenum"), QStringLiteral("{enums: [], extra: '保留'}")},
      {QStringLiteral(".acproj"), QStringLiteral("{name: 'demo', extra: '保留'}")},
      {QStringLiteral(".jsontable"),
       QStringLiteral(
           "{table:{tableName:'people',columns:[{name:'id',mysqlType:'bigint'}]},extra:'保留'}")}};
  for (const auto &item : cases) {
    CodeEditor *editor = nullptr;
    std::unique_ptr<QWidget> tab(EditorFactory::create(
        dir.filePath(QStringLiteral("unchanged") + item.first), item.second, {}, &editor));
    auto *visual = qobject_cast<CodeVisualSyncWidget *>(tab.get());
    CHECK(visual != nullptr);
    if (!visual) continue;
    int changes = 0;
    QObject::connect(visual, &CodeVisualSyncWidget::contentChanged, visual, [&changes, editor]() {
      ++changes;
      editor->document()->setModified(true);
    });
    editor->document()->setModified(false);
    visual->switchToVisual();
    CHECK(visual->isVisualMode());
    visual->onVisualContentChanged();  // 模拟点击或失焦触发的无值变化通知
    waitForVisualSync();
    visual->syncVisualToCode();
    visual->switchToCode();
    visual->switchToVisual();
    visual->switchToCode();
    CHECK(changes == 0);
    CHECK(!editor->document()->isModified());
    CHECK(editor->toPlainText() == item.second);
  }
}

/// 真实字段修改仍需写回；同值信号及防抖内改回原值不能制造修改标记。
void testSchemaFieldEditing() {
  QTemporaryDir dir;
  CHECK(UtilFile::writeUtf8(dir.filePath(QStringLiteral("sample.schema.json")),
                            QStringLiteral("{root:'Data',definitions:{Data:{properties:{name:{type:"
                                           "'string'}},required:['name']}}}")));
  const QString original =
      QStringLiteral("// 不编辑时保留注释\n{$schema: 'sample.schema.json', name: '原值'}\n");
  CodeEditor *editor = nullptr;
  std::unique_ptr<QWidget> tab(
      EditorFactory::create(dir.filePath(QStringLiteral("sample.json")), original, {}, &editor));
  auto *visual = qobject_cast<SchemaJsonWidget *>(tab.get());
  editor->document()->setModified(false);
  visual->switchToVisual();
  const auto fields = visual->visualEditor()->findChildren<QLineEdit *>();
  QLineEdit *name = nullptr;
  for (auto *field : fields)
    if (field->text() == QStringLiteral("原值")) name = field;
  CHECK(name != nullptr);
  if (!name) return;
  auto editName = [name](const QString &value) {
    name->setText(value);
    QMetaObject::invokeMethod(name, "textEdited", Qt::DirectConnection, Q_ARG(QString, value));
  };
  editName(QStringLiteral("原值"));
  waitForVisualSync();
  CHECK(!editor->document()->isModified());
  CHECK(editor->toPlainText() == original);
  editName(QStringLiteral("临时值"));
  editName(QStringLiteral("原值"));
  waitForVisualSync();
  CHECK(!editor->document()->isModified());
  CHECK(editor->toPlainText() == original);
  editName(QStringLiteral("新值"));
  waitForVisualSync();
  CHECK(editor->document()->isModified());
  QJsonObject saved;
  CHECK(UtilJson::parseObject(editor->toPlainText(), &saved));
  CHECK(saved.value(QStringLiteral("name")).toString() == QStringLiteral("新值"));
  editor->document()->setModified(false);  // 模拟保存成功后再次点击同一个值
  editName(QStringLiteral("新值"));
  waitForVisualSync();
  CHECK(!editor->document()->isModified());
}

/// 所有 JSON 包装器统一拒绝非法文本，强制调用回写也不得破坏原文。
void testInvalidConfig(const QString &suffix, const QString &text) {
  QTemporaryDir dir;
  const QString path = dir.filePath(QStringLiteral("invalid") + suffix);
  CodeEditor *editor = nullptr;
  std::unique_ptr<QWidget> tab(EditorFactory::create(path, text, {}, &editor));
  auto *visual = qobject_cast<CodeVisualSyncWidget *>(tab.get());
  CHECK(visual != nullptr);
  if (!visual) return;
  int errors = 0;
  QObject::connect(visual, &CodeVisualSyncWidget::synchronizationFailed, visual,
                   [&errors](const QString &) { ++errors; });
  visual->switchToVisual();
  CHECK(!visual->isVisualMode());
  CHECK(!visual->syncError().isEmpty());
  CHECK(errors == 1);
  CHECK(editor->toPlainText() == text);
  visual->syncVisualToCode();
  visual->setCurrentIndex(1);
  visual->syncVisualToCode();
  CHECK(editor->toPlainText() == text);
  visual->switchToCode();
  CHECK(editor->toPlainText() == text);
}

/// 打开和拆分使用同一个工厂，文件类型、路径与可视化能力一致。
void testFactoryAndRecovery() {
  QTemporaryDir dir;
  for (const QString &suffix :
       {QStringLiteral(".jsonvue"), QStringLiteral(".jsonsource"), QStringLiteral(".jsonupload"),
        QStringLiteral(".jsonglobalenum"), QStringLiteral(".acproj"), QStringLiteral(".json")}) {
    CodeEditor *editor = nullptr;
    const QString path = dir.filePath(QStringLiteral("test") + suffix);
    std::unique_ptr<QWidget> tab(EditorFactory::create(path, QStringLiteral("{"), {}, &editor));
    auto *visual = qobject_cast<CodeVisualSyncWidget *>(tab.get());
    CHECK(visual != nullptr);
    CHECK(editor->objectName() == path);
    visual->switchToVisual();
    CHECK(!visual->isVisualMode());
    editor->setPlainText(QStringLiteral("{}"));
    visual->switchToVisual();
    CHECK(visual->isVisualMode());
    CHECK(visual->syncError().isEmpty());
    visual->reloadStyle();
    editor->reloadColors();
    visual->switchToCode();
  }
  CodeEditor *plain = nullptr;
  std::unique_ptr<QWidget> schema(EditorFactory::create(
      dir.filePath(QStringLiteral("template.schema.json")), QStringLiteral("{}"), {}, &plain));
  CHECK(qobject_cast<CodeVisualSyncWidget *>(schema.get()) == nullptr);
  CHECK(schema.get() == plain);

  CodeEditor *source = nullptr;
  std::unique_ptr<QWidget> tab(
      EditorFactory::create(dir.filePath(QStringLiteral("test.jsonsource")),
                            QStringLiteral("{sources: [], extra: '保留'}"), {}, &source));
  auto *visual = qobject_cast<CodeVisualSyncWidget *>(tab.get());
  visual->switchToVisual();
  visual->switchToCode();
  CHECK(source->toPlainText().contains(QStringLiteral("保留")));

  // 主题通过公共接口刷新，数据源包装器不能再漏掉自身的可视化页。
  SettingStore::ins().setColor(QStringLiteral("ui.textColor"), QColor(QStringLiteral("#123456")));
  visual->reloadStyle();
  auto *sourceWidget = qobject_cast<JsonSourceWidget *>(tab.get());
  CHECK(sourceWidget->visualEditor()->styleSheet().contains(QStringLiteral("#123456")));
  SettingStore::ins().resetColor(QStringLiteral("ui.textColor"));

  const QString tablePath = dir.filePath(QStringLiteral("people.jsontable"));
  const QString tableText = QStringLiteral(
      "{table:{tableName:'people',columns:[{name:'id',mysqlType:'bigint'}]},extra:'保留'}");
  CodeEditor *tableEditor = nullptr;
  std::unique_ptr<QWidget> table(EditorFactory::create(tablePath, tableText, {}, &tableEditor));
  auto *tableVisual = qobject_cast<CodeVisualSyncWidget *>(table.get());
  tableVisual->switchToVisual();
  CHECK(tableVisual->isVisualMode());
  tableVisual->switchToCode();
  QString error;
  const auto parsed = jsonTableFromString(tableEditor->toPlainText(), &error);
  CHECK(error.isEmpty());
  CHECK(parsed.tableName == QStringLiteral("people"));
  CHECK(parsed.columns.size() == 1);
  CHECK(parsed.columns.first().name == QStringLiteral("id"));
  CHECK(tableEditor->toPlainText().contains(QStringLiteral("保留")));
}
}  // namespace

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::printf("[editor UI] initializing application\n");
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QStandardPaths::setTestModeEnabled(true);
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("auto_code_ui_tests"));
  testUnchangedVisualContent();
  testSchemaFieldEditing();
  for (const QString &suffix :
       {QStringLiteral(".jsonvue"), QStringLiteral(".jsonsource"), QStringLiteral(".jsonupload"),
        QStringLiteral(".jsonglobalenum"), QStringLiteral(".jsontable"), QStringLiteral(".acproj"),
        QStringLiteral(".json")}) {
    testInvalidConfig(suffix, QStringLiteral("{broken:"));
    testInvalidConfig(suffix, QStringLiteral("[]"));
  }
  testInvalidConfig(QStringLiteral(".jsontable"), QStringLiteral("{table: []}"));
  testInvalidConfig(QStringLiteral(".jsontable"), QStringLiteral("{tables: []}"));
  testFactoryAndRecovery();
  {
    QTemporaryDir dir;
    MainDevModel model;
    CodeEditor editor;
    editor.setObjectName(dir.filePath(QStringLiteral("missing/test.txt")));
    editor.setPlainText(QStringLiteral("待保存"));
    editor.document()->setModified(true);
    QString error;
    CHECK(!model.saveEditor(&editor, &error));
    CHECK(!error.isEmpty());
    CHECK(editor.document()->isModified());
    editor.setObjectName(dir.filePath(QStringLiteral("test.txt")));
    CHECK(model.saveEditor(&editor, &error));
    CHECK(!editor.document()->isModified());
    CHECK(UtilFile::readUtf8(editor.objectName()) == QStringLiteral("待保存"));
  }
  std::printf("[editor UI] %d checks, %d failed\n", total, failed);
  return failed == 0 ? 0 : 1;
}
