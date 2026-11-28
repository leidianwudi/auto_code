/** @file test_editor_ui.cpp
 * @brief 编辑器工厂和代码/可视化切换的数据保护测试（offscreen，无窗口交互）。
 */
#include <QApplication>
#include <QEventLoop>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <memory>

#include "src/ui/json_source/json_source_editor.h"
#include "src/ui/json_source/json_source_widget.h"
#include "src/ui/json_source/json_table_model.h"
#include "src/ui/json_source/admin_page_widget.h"
#include "src/ui/json_source/json_table_designer.h"
#include "src/ui/json_source/view_field_config_dialog.h"
#include "src/ui/json_source/table_schema_service.h"
#include "src/ui/main_dev/editor_factory.h"
#include "src/ui/main_dev/main_dev_model.h"
#include "src/ui/schema_json/schema_form_editor.h"
#include "src/ui/schema_json/schema_json_widget.h"
#include "src/util/common/util_file.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/code_visual_sync_widget.h"
#include "src/util/ui/component/aui_code_tab_bar.h"
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

/// 数据库注释与前端标题分别保存，编辑物理列不能丢失开关等扩展配置。
void testPhysicalColumnMetadata() {
  const QString source = UtilFile::readUtf8(QString::fromUtf8(PROJECT_SOURCE_DIR) + "/file/go_pure/shop/game/game_region.jsontable");
  auto table = JsonTableTable::fromJsonObject(UtilJson::fromJson(source).object().value("table").toObject());
  for (auto &column : table.columns) {
    if (column.name != "enable") continue;
    column.comment = QStringLiteral("数据库原始注释");
    column.title = QStringLiteral("启用状态");
    CHECK(column.extra.value("boolTrueText").toString() == QStringLiteral("启用"));
    CHECK(column.extra.value("boolFalseText").toString() == QStringLiteral("停用"));
    CHECK(JsonTableColumn::fromJsonObject(column.toJsonObject()) == column);
  }
  JsonTableDesigner designer(table, {});
  auto *presentation = designer.findChild<QTableWidget *>("fieldPresentation");
  CHECK(presentation != nullptr);
  if (presentation) {
    // 来源列已并入字段引用单元格的悬停提示，不再单独占列。
    CHECK(presentation->columnCount() == 8);
    CHECK(presentation->horizontalHeaderItem(0)->text() == QStringLiteral("字段引用"));
    CHECK(presentation->horizontalHeaderItem(1)->text() == QStringLiteral("标题"));
  }
  auto *fields = designer.findChild<QTableWidget *>("databaseFields");
  CHECK(fields != nullptr);
  if (!fields) return;
  CHECK(fields->horizontalHeaderItem(7)->text() == QStringLiteral("数据库注释"));
  int enableRow = -1;
  for (int row = 0; row < fields->rowCount(); ++row)
    if (fields->item(row, 0)->text() == "enable") enableRow = row;
  CHECK(enableRow >= 0);
  if (enableRow < 0) return;
  CHECK(fields->item(enableRow, 7)->text() == QStringLiteral("数据库原始注释"));
  fields->item(enableRow, 7)->setText(QStringLiteral("更新的数据库注释"));
  for (const auto &column : designer.collectTable().columns) {
    if (column.name != "enable") continue;
    CHECK(column.comment == QStringLiteral("更新的数据库注释"));
    CHECK(column.title == QStringLiteral("启用状态"));
    CHECK(column.toJsonObject().value("boolTrueText").toString() == QStringLiteral("启用"));
    CHECK(column.toJsonObject().value("boolFalseText").toString() == QStringLiteral("停用"));
  }
}

/// 页面设置只保留自定义保存接口与页面说明；未知属性保真。
void testAdminPageCustomSave() {
  AdminPageWidget empty;
  empty.setConfig({});
  CHECK(empty.config().isEmpty());

  AdminPageWidget widget;
  int changes = 0;
  QObject::connect(&widget, &AdminPageWidget::configChanged, [&changes]() { ++changes; });
  widget.setConfig({
      {"futureSetting", 7},
      {"customSave", true},
      {"saveUrl", QStringLiteral("/api/market/base-config")},
      {"keyField", QStringLiteral("code")},
      {"editFields", QJsonArray{QStringLiteral("value")}},
      {"switchField", QStringLiteral("enable")}});
  CHECK(changes == 0);
  const QJsonObject loaded = widget.config();
  CHECK(loaded.value("futureSetting").toInt() == 7);
  QString error;
  CHECK(widget.validate(&error));
  auto *customSave = widget.findChild<QCheckBox *>("adminPage_customSave");
  CHECK(customSave != nullptr && customSave->isChecked());
  auto *saveUrl = widget.findChild<QLineEdit *>("adminPage_saveUrl");
  CHECK(saveUrl != nullptr && saveUrl->text() == QStringLiteral("/api/market/base-config"));
  auto *fields = widget.findChild<QLineEdit *>("adminPage_editFields");
  CHECK(fields != nullptr && fields->text() == QStringLiteral("value"));
  if (!fields) return;
  fields->setText(QStringLiteral("value, other"));
  const QJsonObject edited = widget.config();
  CHECK(changes > 0);
  CHECK(edited.value("futureSetting").toInt() == 7);
  CHECK(edited.value("editFields").toArray().size() == 2);
  CHECK(widget.findChildren<QTableWidget *>().isEmpty());

  // 关闭自定义保存后，相关键全部移除，回到标准 CRUD 配置。
  if (customSave) customSave->setChecked(false);
  const QJsonObject standard = widget.config();
  CHECK(!standard.contains("customSave"));
  CHECK(!standard.contains("saveUrl"));
  CHECK(!standard.contains("keyField"));
  CHECK(!standard.contains("editFields"));
  CHECK(!standard.contains("switchField"));
}

/// 字段设置：Tabs 分组、自动推导提示、列表开关归并、按数据库类型过滤控件。
void testViewFieldDialogRefactor() {
  ViewFieldConfigDialog dialog(QStringLiteral("value"), false);
  dialog.setMysqlType(QStringLiteral("int(11)"));
  JsonTableSelCol sc;
  JsonTableEditCol ec;
  dialog.setCurrent(sc, ec);

  auto *type = dialog.findChild<QLabel *>(QStringLiteral("field_mysqlType"));
  CHECK(type != nullptr && type->text() == QStringLiteral("int(11)"));
  auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("field_tabs"));
  CHECK(tabs != nullptr && tabs->count() == 3);
  auto *tabBar = qobject_cast<AuiCodeTabBar *>(tabs ? tabs->tabBar() : nullptr);
  CHECK(tabBar != nullptr && !tabBar->showCloseButton());
  auto *editTab = dialog.findChild<QWidget *>(QStringLiteral("field_editTab"));
  CHECK(editTab != nullptr && tabs && tabs->isTabVisible(tabs->indexOf(editTab)));
  auto *summary = dialog.findChild<QLabel *>(QStringLiteral("field_summary"));
  CHECK(summary != nullptr && summary->text().contains(QStringLiteral("编辑：数字输入")));
  auto *listHint = dialog.findChild<QLabel *>(QStringLiteral("field_listHint"));
  auto *widgetHint = dialog.findChild<QLabel *>(QStringLiteral("field_widgetHint"));
  CHECK(listHint != nullptr && listHint->text() == QStringLiteral("自动 → 文本"));
  CHECK(widgetHint != nullptr &&
        widgetHint->text().startsWith(QStringLiteral("自动 → 数字输入")));
  auto *widget = dialog.findChild<QComboBox *>(QStringLiteral("field_widget"));
  CHECK(widget != nullptr && widget->findData(QStringLiteral("number")) >= 0);
  CHECK(widget != nullptr && widget->findData(QStringLiteral("image")) < 0);
  // 非图片列表：高级设置只有缩略图，入口整体隐藏；物理列取值域常显。
  auto *listAdvanced = dialog.findChild<QToolButton *>(QStringLiteral("field_listAdvanced"));
  CHECK(listAdvanced != nullptr && !listAdvanced->isVisibleTo(&dialog));
  auto *domainTab = dialog.findChild<QWidget *>(QStringLiteral("field_domainTab"));
  CHECK(domainTab != nullptr && tabs && tabs->isTabVisible(tabs->indexOf(domainTab)));

  // 图片呈现方式：高级设置入口出现（缩略图可配）。
  JsonTableSelCol imageCol;
  imageCol.displayType = QStringLiteral("image");
  JsonTableEditCol imageEdit;
  dialog.setCurrent(imageCol, imageEdit);
  CHECK(listAdvanced->isVisibleTo(&dialog));

  // 下拉控件：取值域 Tab 出现，供配置数据源。
  ViewFieldConfigDialog selectDlg(QStringLiteral("kind"), false);
  selectDlg.setMysqlType(QStringLiteral("varchar(32)"));
  JsonTableSelCol selectCol;
  JsonTableEditCol selectEdit;
  selectEdit.widget = QStringLiteral("select");
  selectDlg.setCurrent(selectCol, selectEdit);
  auto *selectTabs = selectDlg.findChild<QTabWidget *>(QStringLiteral("field_tabs"));
  auto *selectDomainTab = selectDlg.findChild<QWidget *>(QStringLiteral("field_domainTab"));
  CHECK(selectTabs != nullptr && selectDomainTab != nullptr &&
        selectTabs->isTabVisible(selectTabs->indexOf(selectDomainTab)));

  // 列表开关归并到「呈现方式」：displayType 留空，switchEditable 承接。
  ViewFieldConfigDialog switchDlg(QStringLiteral("enable"), false);
  switchDlg.setMysqlType(QStringLiteral("tinyint(1)"));
  JsonTableSelCol switchCol;
  JsonTableEditCol switchEdit;
  switchEdit.switchEditable = true;
  switchDlg.setCurrent(switchCol, switchEdit);
  switchDlg.fillListResult(&switchCol);
  switchDlg.fillEditResult(&switchEdit);
  CHECK(switchCol.displayType.isEmpty());
  CHECK(switchEdit.switchEditable);
  auto *switchListHint = switchDlg.findChild<QLabel *>(QStringLiteral("field_listHint"));
  CHECK(switchListHint != nullptr && switchListHint->text() == QStringLiteral("指定 → 开关"));

  // 字符串列不能选数字控件，但保留文本/多行/图片/上传。
  ViewFieldConfigDialog textDlg(QStringLiteral("name"), false);
  textDlg.setMysqlType(QStringLiteral("varchar(64)"));
  JsonTableSelCol textCol;
  JsonTableEditCol textEdit;
  textEdit.widget = QStringLiteral("input");
  textDlg.setCurrent(textCol, textEdit);
  auto *textWidget = textDlg.findChild<QComboBox *>(QStringLiteral("field_widget"));
  CHECK(textWidget != nullptr && textWidget->findData(QStringLiteral("number")) < 0);
  CHECK(textWidget != nullptr && textWidget->findData(QStringLiteral("textarea")) >= 0);
  auto *textWidgetHint = textDlg.findChild<QLabel *>(QStringLiteral("field_widgetHint"));
  CHECK(textWidgetHint != nullptr && textWidgetHint->text() == QStringLiteral("指定 → 单行文本"));

  // 表内枚举列：自动推导为标签/下拉，不再提供会冲突的输入控件。
  ViewFieldConfigDialog enumDlg(QStringLiteral("status"), false);
  enumDlg.setMysqlType(QStringLiteral("tinyint(1)"), true);
  JsonTableSelCol enumCol;
  JsonTableEditCol enumEdit;
  enumDlg.setCurrent(enumCol, enumEdit);
  auto *enumWidget = enumDlg.findChild<QComboBox *>(QStringLiteral("field_widget"));
  CHECK(enumWidget != nullptr && enumWidget->findData(QStringLiteral("input")) < 0);
  auto *enumSummary = enumDlg.findChild<QLabel *>(QStringLiteral("field_summary"));
  CHECK(enumSummary != nullptr && enumSummary->text().contains(QStringLiteral("列表：标签")));
  auto *enumListHint = enumDlg.findChild<QLabel *>(QStringLiteral("field_listHint"));
  CHECK(enumListHint != nullptr &&
        enumListHint->text() == QStringLiteral("自动 → 标签（表内枚举）"));
  auto *enumTabs = enumDlg.findChild<QTabWidget *>(QStringLiteral("field_tabs"));
  auto *enumDomainTab = enumDlg.findChild<QWidget *>(QStringLiteral("field_domainTab"));
  CHECK(enumTabs != nullptr && enumDomainTab != nullptr &&
        !enumTabs->isTabVisible(enumTabs->indexOf(enumDomainTab)));

  // 多语言平铺字段没有物理类型：只保留列表展示，编辑/取值域和 Tab 栏隐藏。
  ViewFieldConfigDialog i18nDlg(QStringLiteral("title"), false);
  i18nDlg.setMysqlType(QString());
  JsonTableSelCol i18nCol;
  JsonTableEditCol i18nEdit;
  i18nDlg.setCurrent(i18nCol, i18nEdit);
  auto *i18nTabs = i18nDlg.findChild<QTabWidget *>(QStringLiteral("field_tabs"));
  auto *i18nEditTab = i18nDlg.findChild<QWidget *>(QStringLiteral("field_editTab"));
  auto *i18nDomainTab = i18nDlg.findChild<QWidget *>(QStringLiteral("field_domainTab"));
  CHECK(i18nTabs != nullptr && i18nEditTab != nullptr && i18nDomainTab != nullptr);
  CHECK(i18nTabs && !i18nTabs->isTabVisible(i18nTabs->indexOf(i18nEditTab)));
  CHECK(i18nTabs && !i18nTabs->isTabVisible(i18nTabs->indexOf(i18nDomainTab)));
  CHECK(i18nTabs && !i18nTabs->tabBar()->isVisibleTo(&i18nDlg));

  // 联表列只有列表展示 Tab。
  ViewFieldConfigDialog joinDlg(QStringLiteral("shop_name"), true);
  joinDlg.setMysqlType(QStringLiteral("varchar(64)"));
  JsonTableSelCol joinCol;
  JsonTableEditCol joinEdit;
  joinDlg.setCurrent(joinCol, joinEdit);
  auto *joinTabs = joinDlg.findChild<QTabWidget *>(QStringLiteral("field_tabs"));
  CHECK(joinTabs != nullptr && joinTabs->count() == 1);
}

/// 字段设置列宽：非预设宽度（如 260）不能被下拉预设 60 覆盖，空值保持自适应。
void testViewFieldWidthRoundTrip() {
  ViewFieldConfigDialog dialog("desc", false);
  JsonTableSelCol sc;
  sc.width = 260;
  sc.displayType.clear();
  JsonTableEditCol ec;
  ec.widget = QStringLiteral("textarea");
  ec.editEditable = false;
  dialog.setCurrent(sc, ec);
  CHECK(dialog.width() == 260);

  sc.width = 0;
  dialog.setCurrent(sc, ec);
  CHECK(dialog.width() == 0);
}

/// 联表输出别名自动继承目标表表内枚举：设计器 ⚙ 标记与生成端 aliasMeta 一致。
void testJoinAliasEnumFlag() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  // 目标表 A：列 a 配置表内枚举
  CHECK(UtilFile::writeUtf8(
      dir.filePath(QStringLiteral("A.jsontable")),
      QStringLiteral("{table:{tableName:'A',columns:[{name:'a',mysqlType:'varchar(32)'}],"
                     "enums:[{column:'a',items:[{key:'on',label:'启用',value:'1'}]}]}}")));
  // B 表：joinTables 输出 A.a（别名 A_a），自身无枚举
  const QString bText = QStringLiteral(
      "{table:{tableName:'B',columns:[{name:'id',mysqlType:'bigint'}],"
      "joinTables:[{table:'A',cols:[{col:'a'}]}]}}");
  auto bTable = JsonTableTable::fromJsonObject(
      UtilJson::fromJson(bText).object().value(QStringLiteral("table")).toObject());
  JsonTableDesigner designer(bTable, {}, dir.filePath(QStringLiteral("B.jsontable")));
  auto *presentation = designer.findChild<QTableWidget *>("fieldPresentation");
  CHECK(presentation != nullptr);
  if (!presentation) return;
  int aliasRow = -1;
  int idRow = -1;
  for (int row = 0; row < presentation->rowCount(); ++row) {
    const QString ref = presentation->item(row, 0)->text();
    if (ref == QStringLiteral("A_a")) aliasRow = row;
    if (ref == QStringLiteral("id")) idRow = row;
  }
  CHECK(aliasRow >= 0);
  CHECK(idRow >= 0);
  if (aliasRow < 0 || idRow < 0) return;
  CHECK(presentation->item(aliasRow, 0)->data(Qt::UserRole + 29).toInt() == 1);
  CHECK(presentation->item(idRow, 0)->data(Qt::UserRole + 29).toInt() == 0);

  // 显式输出别名：字段呈现 ref = 自定义别名，枚举继承照常生效
  const QString b2Text = QStringLiteral(
      "{table:{tableName:'B2',columns:[{name:'id',mysqlType:'bigint'}],"
      "joinTables:[{table:'A',cols:[{col:'a',alias:'a_custom'}]}]}}");
  auto b2Table = JsonTableTable::fromJsonObject(
      UtilJson::fromJson(b2Text).object().value(QStringLiteral("table")).toObject());
  JsonTableDesigner designer2(b2Table, {}, dir.filePath(QStringLiteral("B2.jsontable")));
  auto *presentation2 = designer2.findChild<QTableWidget *>("fieldPresentation");
  CHECK(presentation2 != nullptr);
  int customRow = -1;
  if (presentation2) {
    for (int row = 0; row < presentation2->rowCount(); ++row) {
      if (presentation2->item(row, 0)->text() == QStringLiteral("a_custom")) customRow = row;
    }
  }
  CHECK(customRow >= 0);
  if (customRow >= 0)
    CHECK(presentation2->item(customRow, 0)->data(Qt::UserRole + 29).toInt() == 1);
}

/// 联表输出别名：显式 alias 可避开主表列名冲突，冲突配置必须被校验拒绝。
void testJoinAliasValidation() {
  const QString aText = QStringLiteral(
      "{table:{tableName:'ta',tableComment:'A表',columns:[{name:'a',mysqlType:'varchar(32)',"
      "comment:'A列'}]}}");
  const QString bText = QStringLiteral(
      "{table:{tableName:'tb',tableComment:'B表',columns:[{name:'id',mysqlType:'bigint',"
      "comment:'主键'},{name:'a',mysqlType:'varchar(32)',comment:'A列'}],"
      "joinTables:[{table:'ta',on:['a','a'],cols:[{col:'a',alias:'a'}]}]}}");
  auto aTable = JsonTableTable::fromJsonObject(
      UtilJson::fromJson(aText).object().value(QStringLiteral("table")).toObject());
  auto bTable = JsonTableTable::fromJsonObject(
      UtilJson::fromJson(bText).object().value(QStringLiteral("table")).toObject());
  CHECK(bTable.joinTables.size() == 1);
  CHECK(bTable.joinTables.value(0).cols.value(0).alias == QStringLiteral("a"));
  // 模型 round-trip 保真显式别名
  const JsonTableJoinCol round =
      JsonTableJoinCol::fromJsonObject(bTable.joinTables.value(0).cols.value(0).toJsonObject());
  CHECK(round == bTable.joinTables.value(0).cols.value(0));

  JsonTableConfig cfg;
  cfg.tables = {aTable, bTable};
  QString err;
  CHECK(!TableSchema::validateCrossRefs(bTable, cfg, cfg.tables, bTable.tableName, &err));
  CHECK(err.contains(QStringLiteral("冲突")));

  // 指定唯一别名后校验通过
  bTable.joinTables[0].cols[0].alias = QStringLiteral("a_joined");
  CHECK(TableSchema::validateCrossRefs(bTable, cfg, cfg.tables, bTable.tableName, &err));
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
  testAdminPageCustomSave();
  testViewFieldDialogRefactor();
  testViewFieldWidthRoundTrip();
  testJoinAliasEnumFlag();
  testJoinAliasValidation();
  testPhysicalColumnMetadata();
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
