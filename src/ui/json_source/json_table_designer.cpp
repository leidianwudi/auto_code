/**
 * @file json_table_designer.cpp
 * @brief .jsontable 表设计器面板实现（可视化编辑器内嵌，逐编辑广播）
 */

#include "json_table_designer.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include "table_database_service.h"
#include "table_import_dialog.h"
#include "table_schema_service.h"

using namespace TableSchema;

#include <QAbstractItemView>
#include <QAction>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QHelpEvent>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QStringListModel>
#include <QStyleOptionHeader>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include "src/engine/ac_language.h"
#include "src/ui/json_vue/button_config_dialog.h"
#include "src/ui/json_vue/json_vue_model.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_cell_edit_combo.h"
#include "src/util/ui/component/aui_cell_read_box.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_config_dialog.h"
#include "src/util/ui/component/aui_icon.h"
#include "src/util/ui/component/aui_input_dialog.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/component/aui_splitter.h"
#include "src/util/ui/component/aui_style.h"
#include "view_field_config_dialog.h"

// ════════════════════════════════════════════════════════════
//  表格列索引 / 局部工具
// ════════════════════════════════════════════════════════════

namespace {

/// 字段表格列索引（页面呈现全部由三视图接管，字段表格只留纯物理属性；
/// 历史 列表/搜索/表单角色/必填 列均已退役）
enum ColCols {
  ColColName = 0,  ///< 列名
  ColColType,      ///< MySQL 类型（可编辑下拉）
  ColColUnsigned,  ///< 无符号（UNSIGNED）复选框
  ColColNullable,  ///< 可空 复选框
  ColColPrimary,   ///< 主键 复选框
  ColColAutoInc,   ///< 自增 复选框
  ColColDefault,   ///< 默认值
  ColColComment,   ///< 注释
  ColColCount
};

/// MySQL 类型列预设（可编辑下拉，仍可自由输入）
QStringList colTypePresets() {
  return {QStringLiteral("bigint"),   QStringLiteral("int"),
          QStringLiteral("tinyint"),  QStringLiteral("varchar(64)"),
          QStringLiteral("text"),     QStringLiteral("decimal(10,2)"),
          QStringLiteral("datetime"), QStringLiteral("json"),
          QStringLiteral("boolean")};
}

/// 查询模式下拉候选（显示文本 ↔ 配置值双向映射；下拉只显示文本，收集时转回值）
QStringList queryModePresets() {
  return {QStringLiteral("不查询"), QStringLiteral("精确"), QStringLiteral("模糊"),
          QStringLiteral("范围"),   QStringLiteral("仅接口")};
}

QString queryModeData(const QString &display) {
  if (display == QStringLiteral("精确")) return QStringLiteral("eq");
  if (display == QStringLiteral("模糊")) return QStringLiteral("like");
  if (display == QStringLiteral("范围")) return QStringLiteral("gap");
  if (display == QStringLiteral("仅接口")) return QStringLiteral("api");
  return QStringLiteral("none");
}

QString queryModeDisplay(const QString &data) {
  if (data == QStringLiteral("eq")) return QStringLiteral("精确");
  if (data == QStringLiteral("like")) return QStringLiteral("模糊");
  if (data == QStringLiteral("gap")) return QStringLiteral("范围");
  if (data == QStringLiteral("api")) return QStringLiteral("仅接口");
  return QStringLiteral("不查询");
}

/// 联表声明表格列索引（joinTable；输出列已迁至「输出字段」主从表格）
enum JoinCols {
  JoinColTable = 0,  ///< 被联表名（joinTables[].table，可编辑下拉=项目表名）
  JoinColType,       ///< JOIN 类型（LEFT/INNER 下拉）
  JoinColOnMain,     ///< on[0] 主表列（可编辑下拉=本表列名）
  JoinColOnJoin,     ///< on[1] 被联表列（目标表列名）
  JoinColCount
};

/// 联表输出字段表格列索引（m_joinColTable：选中声明行的输出列；筛选能力统一
/// 由字段呈现页的查询模式驱动（selColsEq/Like 别名通道），此处不再单列）
enum JoinColCols {
  JColCol = 0,   ///< 输出列名（∈ 被联表列；输出别名 = 表名_列名）
  JColLabel,     ///< 显示标题（空 = 回退目标列注释/列名）
  JColCount
};

/// 字段呈现统一表格列索引（一行 = 一个字段：列表/查询/编辑三视图合一）
enum UCols {
  UColRef = 0,   ///< 字段引用（本表列名 或 引用名.目标列，可编辑下拉）
  UColTitle,     ///< 标题（生效值 = selCols.label > 列注释 > ref；写回 selCols.label）
  UColWritable,  ///< 写入参数勾（Go InUpd 包含该字段）
  UColOutput,    ///< 输出勾（Go 实体 json:"-"：取消后 API 响应不含该列）
  UColQuery,     ///< 查询模式（cell widget：不查询/精确/模糊/范围/仅接口）
  UColList,      ///< 列表勾选（在 selCols 中）
  UColVisible,   ///< 表单展示勾（Vue 渲染表单项）
  UColCfg,       ///< 配置（⚙ + 一行式摘要合成一列：双击打开字段设置面板；必填在面板内）
  UColCount
};

/// 索引表格列索引
enum IndexCols {
  IndexColName = 0,  ///< 索引名
  IndexColCols,      ///< 列（逗号分隔文本）
  IndexColUnique,    ///< unique 复选框
  IndexColCount
};

/// 逗号分隔多值补全器：按最后一个逗号后的片段匹配，选中后拼回前缀
/// （全局枚举列 / 多语言字段等 "a,b,c" 形态输入框共用）
class CsvCompleter : public QCompleter {
public:
  explicit CsvCompleter(QLineEdit *edit) : QCompleter(edit), m_model(new QStringListModel(this)) {
    setModel(m_model);
  }

  /// 更新候选列表
  void setCandidates(const QStringList &items) { m_model->setStringList(items); }

  /// 匹配片段 = 最后一个逗号之后的文本（去空格）
  QStringList splitPath(const QString &path) const override {
    return QStringList{path.mid(path.lastIndexOf(QLatin1Char(',')) + 1).trimmed()};
  }

  /// 选中候选后拼回逗号前缀（"a,b," + 候选）
  QString pathFromIndex(const QModelIndex &index) const override {
    const QString tail = index.data(Qt::EditRole).toString();
    QString prefix;
    if (auto *edit = qobject_cast<QLineEdit *>(widget())) {
      const int pos = edit->text().lastIndexOf(QLatin1Char(','));
      if (pos >= 0) prefix = edit->text().left(pos + 1) + QStringLiteral(" ");
    }
    return prefix + tail;
  }

private:
  QStringListModel *m_model;  ///< 候选模型（父对象为本补全器，随构造销毁）
};

/// 复选框列条目（无可编辑文本，点击勾选框直接切换）

/// 单元格常驻控件文本（AuiCellEditCombo 等；无控件返回空）
QString cellWidgetText(const QTableWidget *table, int row, int col) {
  if (!table) return QString();
  if (const auto *c = dynamic_cast<const AuiCellEditCombo *>(table->cellWidget(row, col)))
    return c->text().trimmed();
  return QString();
}

/// 通用表格单元格文本（空表/缺条目返回空串；截断空白）
QString cellOf(const QTableWidget *table, int row, int col) {
  const QTableWidgetItem *it = table ? table->item(row, col) : nullptr;
  return it ? it->text().trimmed() : QString();
}

/// 类型徽标小图标：联表 = 链条、多语言 = 地球，两者同一蓝色（自绘而非 emoji：
/// emoji 颜色由字体决定——🔗 灰棕、🌐 蓝，无法保证一致；自绘图标颜色固定）
QIcon viewBadgeIcon(bool join) {
  constexpr int kSize = 12;      ///< 逻辑尺寸（px）
  constexpr qreal kScale = 2.0;  ///< 2x 采样，缩小显示时清晰
  QPixmap pm(static_cast<int>(kSize * kScale), static_cast<int>(kSize * kScale));
  pm.setDevicePixelRatio(kScale);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor(0x4A, 0x90, 0xE2));
  pen.setWidthF(1.2);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  if (join) {
    // 联表：两个相交的圆环（链条）
    constexpr qreal d = 6.5;
    p.drawEllipse(QRectF(1.2, (kSize - d) / 2.0, d, d));
    p.drawEllipse(QRectF(kSize - 1.2 - d, (kSize - d) / 2.0, d, d));
  } else {
    // 多语言：地球（外圆 + 赤道 + 经线椭圆）
    const QRectF r(1.2, 1.2, kSize - 2.4, kSize - 2.4);
    p.drawEllipse(r);
    p.drawLine(QPointF(r.left(), kSize / 2.0), QPointF(r.right(), kSize / 2.0));
    p.drawEllipse(QRectF(kSize / 2.0 - r.width() / 4.5, r.top(), r.width() / 2.25, r.height()));
  }
  p.end();
  return QIcon(pm);
}

}  // namespace

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonTableDesigner::JsonTableDesigner(const JsonTableTable &table, const JsonTableConfig &config,
                                     const QString &filePath, QWidget *parent)
    : QWidget(parent), m_table(table), m_config(config), m_filePath(filePath) {
  m_buttons = table.buttons;
  setupUI();
  refill();
  connectChangeSignals();
  connect(this, &JsonTableDesigner::configChanged, this, [this]() { ++m_revision; });
}

void JsonTableDesigner::setupUI() {
  // 外围间距对齐 jsonvue 可视化编辑器（margins 2 / spacing 2）
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(2, 2, 2, 2);
  rootLayout->setSpacing(2);
  auto *layout = rootLayout;

  // 显式最小宽度（覆盖布局的 minimumSizeHint 聚合）：IDE 左右拆分时可视化
  // 面板允许拖到较窄——行内输入框已设水平 Ignored（可被压窄、内容横向滚动），
  // 窄于此值时按钮/标签等不可压缩项会被截断显示
  setMinimumWidth(300);

  // ── 页签容器（对齐 jsonvue「一行=一字段」心智）：
  //    字段(物理) / 字段呈现(生成设置+三视图合一+排序) / 联表 / 枚举 / 多语言 /
  //    索引 / DDL。索引是物理结构（决定 DDL 预览与执行建表内容），独立成页放
  //    DDL 前：配索引 → 预览 DDL → 执行建表一条线；联表是查询声明，与索引领域分离
  auto *tabs = new QTabWidget(this);
  auto makeTab = [tabs](const QString &title) {
    auto *w = new QWidget(tabs);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    tabs->addTab(w, title);
    return lay;
  };
  // 可拖动分隔页内的「分区」：标题行 + 表格装进同一个 QWidget，
  // 作为 QSplitter 的独立段，拖动分隔条即可调整各段高度
  auto makeSection = [](QSplitter *owner) {
    auto *w = new QWidget(owner);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    return lay;
  };
  auto *fieldTab = makeTab(QStringLiteral("字段"));
  auto *presentTab = makeTab(QStringLiteral("字段呈现"));
  auto *joinTab = makeTab(QStringLiteral("联表"));
  auto *enumTab = makeTab(QStringLiteral("枚举"));
  auto *i18nTab = makeTab(QStringLiteral("多语言"));
  auto *indexTab = makeTab(QStringLiteral("索引"));
  auto *ddlTab = makeTab(QStringLiteral("DDL"));

  // ── 顶部：表名 / 模型名 / 表注释 / manualUpdateTime（字段页）──
  auto *topRow = new QHBoxLayout;
  topRow->setSpacing(6);
  auto addField = [&topRow](const QString &label, QLineEdit **edit, const QString &placeholder) {
    topRow->addWidget(new QLabel(label, topRow->parentWidget()));
    auto *e = new QLineEdit(topRow->parentWidget());
    e->setPlaceholderText(placeholder);
    // 水平 Ignored：布局计算忽略 minimumSizeHint——窄面板（IDE 左右拆分）下
    // 输入框可被压到很窄（内容横向滚动），行最小宽度不再被 3 组输入框撑大；
    // 宽面板下空间仍按 stretch 均分，视觉与默认一致
    e->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    topRow->addWidget(e, 1);
    *edit = e;
  };
  addField(QStringLiteral("表名:"), &m_nameEdit, QStringLiteral("如 shop"));
  addField(QStringLiteral("模型名:"), &m_modelEdit, QStringLiteral("生成代码用的标识"));
  addField(QStringLiteral("表注释:"), &m_commentEdit, QStringLiteral("如 商品表"));
  m_manualUpdateCheck = new QCheckBox(QStringLiteral("手动维护更新时间"), this);
  m_manualUpdateCheck->setToolTip(
      QStringLiteral("勾选后生成端不由数据库 ON UPDATE 维护更新时间字段"));
  topRow->addWidget(m_manualUpdateCheck);
  fieldTab->addLayout(topRow);

  // ── 字段表格工具行 ──
  auto *colHeader = new QHBoxLayout;
  colHeader->addWidget(new QLabel(QStringLiteral("字段:"), this));
  colHeader->addStretch();
  auto *addColBtn = makeCompactButton(QStringLiteral("+ 加列"), this);
  auto *delColBtn = makeCompactButton(QStringLiteral("- 删列"), this);
  auto *upColBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downColBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  m_refreshFieldsBtn = makeCompactButton(QStringLiteral("从数据库刷新字段"), this);
  colHeader->addWidget(addColBtn);
  colHeader->addWidget(delColBtn);
  colHeader->addWidget(upColBtn);
  colHeader->addWidget(downColBtn);
  colHeader->addSpacing(8);
  colHeader->addWidget(m_refreshFieldsBtn);
  fieldTab->addLayout(colHeader);

  // ── 字段表格（纯物理属性，独占字段页；页面呈现由三视图接管）──
  m_colsTable = makeConfigTable({{QStringLiteral("列名"), QHeaderView::Interactive, 110},
                                 {QStringLiteral("MySQL类型"), QHeaderView::Interactive, 120},
                                 {QStringLiteral("无符号"), QHeaderView::Interactive, 62},
                                 {QStringLiteral("可空"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("主键"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("自增"), QHeaderView::Interactive, 50},
                                 {QStringLiteral("默认值"), QHeaderView::Interactive, 90},
                                 {QStringLiteral("注释"), QHeaderView::Stretch, 0}},
                                this, 300, 0, QAbstractItemView::SelectItems);
  fieldTab->addWidget(m_colsTable, 1);
  // 「无符号」列头提示（MySQL UNSIGNED 语义）
  m_colsTable->horizontalHeaderItem(ColColUnsigned)
      ->setToolTip(QStringLiteral(
          "无符号整数（UNSIGNED）：不允许负数，取值范围从 0 开始，仅对整数类型有效"));

  // MySQL 类型列用常驻可编辑下拉（cellWidget，点箭头展开），不走委托编辑：
  // 委托编辑器的弹层会因失焦被 closeEditor 连带销毁，且透明化后会透出单元格文字

  // ── 字段呈现统一表格（一行 = 一个字段：列表/查询/编辑三视图合一）──
  // 行集完全派生（物理列跟随字段表格 / 联表行跟随联表页输出字段 / i18n 行跟随
  // 多语言页），无手动增删按钮，仅提供行序调整
  auto *viewHeader = new QHBoxLayout;
  viewHeader->addWidget(new QLabel(QStringLiteral("字段呈现:"), this));
  viewHeader->addStretch();
  auto *upViewBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downViewBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  viewHeader->addWidget(upViewBtn);
  viewHeader->addWidget(downViewBtn);
  // 三段式垂直分隔：字段呈现 / 默认排序 / 操作按钮 各占一段，
  // 拖动任意分隔条即可调整高度；短窗口下排序/按钮区不会被压到只剩一两行。
  auto *presentSplit = new AuiSplitter(Qt::Vertical, this);
  presentSplit->setObjectName(QStringLiteral("presentSplit"));
  presentSplit->setChildrenCollapsible(false);

  // 上段：字段呈现
  auto *viewLay = makeSection(presentSplit);
  viewLay->addLayout(viewHeader);
  presentSplit->addWidget(viewLay->parentWidget());

  // 列模式：文本列与勾选列全部 Interactive（可拖拽调宽），末列 ⚙ Stretch 吸收
  // 剩余宽度——前两列原为 Stretch 会被 Qt 禁用拖拽柄，导致列宽拖不动
  m_viewTable = makeConfigTable({{QStringLiteral("字段引用"), QHeaderView::Interactive, 150},
                                 {QStringLiteral("标题"), QHeaderView::Interactive, 130},
                                 {QStringLiteral("写入"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("输出"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("查询"), QHeaderView::Interactive, 88},
                                 {QStringLiteral("列表"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("展示"), QHeaderView::Interactive, 56},
                                 {QStringLiteral("配置"), QHeaderView::Stretch, 0}},
                                this, 240, 0, QAbstractItemView::SelectItems);
  // 功能列表头：问号并入标题（「列表 ?」），悬停整格显示对应说明
  m_headerTips = {
      {UColList, QStringLiteral("勾选后该字段出现在列表页表格中\n（列标题 / 列宽在 ⚙ 列配置）")},
      {UColQuery, QStringLiteral("查询方式：\n"
                                 "· 不查询：列表页无该字段的查询控件\n"
                                 "· 精确 / 模糊 / 范围：前端渲染输入控件，后端生成对应查询条件\n"
                                 "· 仅接口：后端生成查询条件，前端不渲染输入控件\n"
                                 "枚举列 / 外键列的查询控件自动为下拉框")},
      {UColVisible, QStringLiteral("☑ 编辑/新增/详情表单显示该字段\n"
                                  "☐ 上述表单不显示该字段（不影响「列表」页与「查询」条件）\n"
                                  "表单内是否可输入由 ⚙ 里的「表单可编辑」控制")},
      {UColWritable, QStringLiteral("☑ 进入写参数 InUpd，接口可写入该字段\n"
                                   "☐ 不写入（只展示或完全不参与）；与「展示」独立组合")},
      {UColOutput, QStringLiteral("☑ 接口正常输出该列\n"
                                 "☐ 不输出（如密码）：实体 json 标记为 json:\"-\"，"
                                 "所有 API 响应都不含该列\n"
                                 "写参数 InUpd 不受影响（编辑表单仍可提交该字段）")},
      {UColCfg, QStringLiteral("配置：⚙ 列表样式=列表展示类型、表单控件/表单可编辑/必填；"
                               "双击单元格打开设置")}};
  m_viewHeader = m_viewTable->horizontalHeader();
  for (auto tipIt = m_headerTips.constBegin(); tipIt != m_headerTips.constEnd(); ++tipIt) {
    if (auto *h = m_viewTable->horizontalHeaderItem(tipIt.key())) {
      h->setText(h->text() + QStringLiteral(" ?"));
      h->setToolTip(tipIt.value());
    }
  }
  viewLay->addWidget(m_viewTable, 1);
  // 字段引用列用常驻可编辑下拉（cellWidget，单击即展开），不走委托编辑：
  // 编辑器失焦会 closeEditor 连带销毁弹层，表现为「下拉一出现就收起」

  // 中段：默认排序（列表呈现的附属配置）
  auto *sortLay = makeSection(presentSplit);
  presentSplit->addWidget(sortLay->parentWidget());
  auto *sortHeader = new QHBoxLayout;
  sortHeader->addWidget(new QLabel(QStringLiteral("默认排序:"), this));
  sortHeader->addStretch();
  auto *addSortBtn = makeCompactButton(QStringLiteral("+ 加排序"), this);
  auto *delSortBtn = makeCompactButton(QStringLiteral("- 删排序"), this);
  auto *upSortBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downSortBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  sortHeader->addWidget(addSortBtn);
  sortHeader->addWidget(delSortBtn);
  sortHeader->addWidget(upSortBtn);
  sortHeader->addWidget(downSortBtn);
  sortLay->addLayout(sortHeader);
  connect(upSortBtn, &QPushButton::clicked, this, [this]() { moveSortRow(-1); });
  connect(downSortBtn, &QPushButton::clicked, this, [this]() { moveSortRow(1); });

  m_sortTable = makeConfigTable({{QStringLiteral("排序列"), QHeaderView::Interactive, 140},
                                 {QStringLiteral("方向"), QHeaderView::Stretch, 0}},
                                this, 60, 0, QAbstractItemView::SelectItems);
  sortLay->addWidget(m_sortTable, 1);
  // 排序列用常驻下拉（cellWidget，点右侧箭头展开、Alt+↓/F4 亦可），
  // 不走委托编辑：编辑器生命周期会让弹层连带销毁（双击后一闪即收）

  // 下段：操作按钮（buttons：键名与 .jsonvue ButtonConfig 完全一致）
  auto *btnLay = makeSection(presentSplit);
  presentSplit->addWidget(btnLay->parentWidget());
  auto *btnHeader = new QHBoxLayout;
  btnHeader->addWidget(new QLabel(QStringLiteral("操作按钮:"), this));
  btnHeader->addStretch();
  auto *addBtnBtn = makeCompactButton(QStringLiteral("+ 加按钮"), this);
  auto *delBtnBtn = makeCompactButton(QStringLiteral("- 删按钮"), this);
  auto *upBtnBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downBtnBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  btnHeader->addWidget(addBtnBtn);
  btnHeader->addWidget(delBtnBtn);
  btnHeader->addWidget(upBtnBtn);
  btnHeader->addWidget(downBtnBtn);
  btnLay->addLayout(btnHeader);

  m_buttonTable = makeConfigTable({{QStringLiteral("按钮文字"), QHeaderView::Interactive, 140},
                                   {QStringLiteral("位置"), QHeaderView::Interactive, 90},
                                   {QStringLiteral("行为"), QHeaderView::Interactive, 90},
                                   {QStringLiteral("动作"), QHeaderView::Stretch, 0}},
                                  this, 0, 0, QAbstractItemView::SelectRows);
  m_buttonTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_buttonTable->setToolTip(
      QStringLiteral("列表页自定义按钮（工具栏/行操作列），行为支持 直接调 API / 二次确认 / "
                     "对话框表单 / 跳转；\n双击数据行或「+ 加按钮」打开按钮配置对话框"));
  btnLay->addWidget(m_buttonTable, 1);
  connect(addBtnBtn, &QPushButton::clicked, this, &JsonTableDesigner::onAddButton);
  connect(delBtnBtn, &QPushButton::clicked, this, &JsonTableDesigner::onRemoveButton);
  connect(upBtnBtn, &QPushButton::clicked, this, [this]() { moveButtonRow(-1); });
  connect(downBtnBtn, &QPushButton::clicked, this, [this]() { moveButtonRow(1); });
  connect(m_buttonTable, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) { onEditButton(row); });
  fillButtons();

  // 操作按钮段容器：显式给定最小尺寸，允许默认收缩到只剩标题行（≈26px），
  // 让出的高度全部给「字段呈现」正表。注意必须 setMinimumSize(0, 26)（两维
  // 均有效才覆盖 minimumSizeHint）；只 setMinimumHeight(0) 时 minimumSize
  // 仍是无效值（宽=-1），QSplitter 会回退用 minimumSizeHint（QTableWidget
  // 自带 ≈60px），导致 setSizes 的小值被钳制到 90 左右"无效"。
  btnLay->parentWidget()->setMinimumSize(0, 26);

  // 三段高度比例：字段呈现 3 份，排序 / 按钮各 1 份（可拖动调整）
  presentSplit->setStretchFactor(0, 3);
  presentSplit->setStretchFactor(1, 1);
  presentSplit->setStretchFactor(2, 1);
  // setSizes 是期望值，总和高出/不足部分按比例缩放：{320,90,10} 实际把多余
  // 空间分给前两段，第三段被上方 26px 最小尺寸钳到标题行
  presentSplit->setSizes({320, 90, 60});
  // ── 顶部：生成设置（表级生成开关，从字段页迁入；门禁下方三视图/按钮产物）──
  //    一行三段，段间竖线分隔：
  //      生成前端页面 │ 按钮: 新增 编辑 删除 详情 │ C端模块: [不生成/只读查询/空接口] [鉴权]
  //    生成前端页面 = 三视图总开关（false = 纯 API 模块，下方配置不生效）；
  //    按钮组同属页面呈现范畴；删除勾选同时控制工具栏批量删除与行内删除图标
  //    （两者同生同灭，避免"能批量删但不能单删"的怪状态）
  auto *genSep = new QLabel(QStringLiteral("── 生成设置 ──"), this);
  genSep->setAlignment(Qt::AlignCenter);
  presentTab->addWidget(genSep);
  auto *genRow = new QHBoxLayout;
  genRow->setSpacing(6);
  // 段间竖线（VLine + Sunken 阴影，跟随主题边框色）
  auto addVSep = [this, genRow]() {
    auto *v = new QFrame(this);
    v->setFrameShape(QFrame::VLine);
    v->setFrameShadow(QFrame::Sunken);
    genRow->addWidget(v);
  };
  m_frontendCheck = new QCheckBox(QStringLiteral("生成前端页面"), this);
  m_frontendCheck->setChecked(m_table.frontend);
  m_frontendCheck->setToolTip(
      QStringLiteral("取消勾选 = 纯 API 模块：后端链照常生成 "
                     "API，前端链跳过此表（下方三视图/排序/按钮配置不生效）"));
  genRow->addWidget(m_frontendCheck);
  addVSep();
  genRow->addWidget(new QLabel(QStringLiteral("按钮:"), this));
  m_genAddCheck = new QCheckBox(QStringLiteral("新增"), this);
  m_genAddCheck->setToolTip(QStringLiteral("工具栏「新增」按钮（打开编辑弹窗新增）"));
  genRow->addWidget(m_genAddCheck);
  m_genEditCheck = new QCheckBox(QStringLiteral("编辑"), this);
  m_genEditCheck->setToolTip(QStringLiteral("行操作列「编辑」图标（打开编辑弹窗修改）"));
  genRow->addWidget(m_genEditCheck);
  m_genDeleteCheck = new QCheckBox(QStringLiteral("删除"), this);
  m_genDeleteCheck->setToolTip(
      QStringLiteral("行操作列「删除」图标 + 工具栏「删除」批量按钮（两者同一开关）"));
  genRow->addWidget(m_genDeleteCheck);
  m_genDetailCheck = new QCheckBox(QStringLiteral("详情"), this);
  m_genDetailCheck->setToolTip(QStringLiteral("行操作列「详情」图标（只读弹窗查看）"));
  genRow->addWidget(m_genDetailCheck);
  // 「生成前端页面」开关联动按钮组可用状态：未勾选（纯 API 模块）时按钮配置
  // 不生效，置灰提示；仅改 enabled 不改勾选值，重新勾上后原配置原样恢复
  connect(m_frontendCheck, &QCheckBox::toggled, this,
          [this](bool) { updateGenButtonsEnabled(); });
  addVSep();
  // C 端模块（modules/app）：三态（不生成/只读查询/空接口），鉴权默认需登录
  genRow->addWidget(new QLabel(QStringLiteral("C端模块:"), this));
  // ? 帮助：解释「C端」概念与本段三个控件的含义（悬停看摘要、点击看完整说明）
  genRow->addWidget(AuiButton::createHelpButton(
      QStringLiteral("C端模块说明"),
      QStringLiteral(
          "C端 = 面向终端用户的客户端（App / 小程序 / H5 / 前台网站），\n"
          "本段控制生成到 modules/app/<模块目录> 的只读接口（永不生成写接口）；\n"
          "后台管理端页面与写接口走 modules/admin，两端共用 data 共享层。\n\n"
          "不生成：该表只在后台管理端使用\n"
          "只读查询：生成 selectByIn / selectById 两个查询接口\n"
          "空接口：只生成 service/handler/router 骨架与手写扩展三件套，\n"
          "        业务接口写在 *_ext_* 三件套（RegisterExtRoutes 自动挂载）\n\n"
          "鉴权：公开浏览 = 不挂 UserAuth（如商品列表）；需登录 = 默认\n"
          "C端出参：出参白名单，只输出所选列（out_<表>_.go）；留空 = 返回完整实体"),
      this));
  m_appQueryCombo = AuiComboBox::create(this);
  m_appQueryCombo->addItem(QStringLiteral("不生成"), QStringLiteral("none"));
  m_appQueryCombo->addItem(QStringLiteral("只读查询"), QStringLiteral("query"));
  m_appQueryCombo->addItem(QStringLiteral("空接口"), QStringLiteral("empty"));
  m_appQueryCombo->setToolTip(QStringLiteral(
      "C端 = 客户端（App/小程序/H5/前台网站），生成到 modules/app\n"
      "只读查询 = 生成 selectByIn/selectById 两接口（永不生成写接口）\n"
      "空接口 = 仅生成 service/handler/router 骨架与手写扩展三件套，"
      "不生成只读接口；自定义接口写在 *_ext_* 三件套，RegisterExtRoutes 自动挂载\n"
      "service 的查询编排方法在两种形态下均保留（供扩展接口内部复用，不暴露 HTTP）"));
  genRow->addWidget(m_appQueryCombo);
  m_appAuthCombo = AuiComboBox::create(this);
  m_appAuthCombo->addItem(QStringLiteral("公开浏览"), QStringLiteral("public"));
  m_appAuthCombo->addItem(QStringLiteral("需登录"), QStringLiteral("user"));
  m_appAuthCombo->setToolTip(QStringLiteral(
      "public=不挂 UserAuth（如商品列表）；user=需登录（默认，宁可漏开放不可误开放）\n"
      "作用于 mount 路由组（含扩展路由）"));
  genRow->addWidget(m_appAuthCombo);
  m_appOutBtn = makeCompactButton(QStringLiteral("C端出参: 全部"), this);
  m_appOutBtn->setToolTip(QStringLiteral(
      "C 端出参白名单：勾选后生成 out_<表>_.go，只输出所选列\n"
      "支持主表列、联表输出列（表名_列名）、多语言字段\n"
      "留空 = 直接返回完整实体（「输出」未勾选的列始终不出现）"));
  genRow->addWidget(m_appOutBtn);
  addVSep();
  // 表缓存骨架（data 层手写件）：仅首次生成，之后重新生成不覆盖
  m_genCacheCheck = new QCheckBox(QStringLiteral("表缓存"), this);
  m_genCacheCheck->setChecked(m_table.genCache);
  m_genCacheCheck->setToolTip(QStringLiteral(
      "勾选后首次生成 data/<前缀>/<表>/<表>_cache.go 骨架（手写件，重新生成不覆盖）\n"
      "只给读多写少、且本表是缓存主数据源的表勾选；admin/user_ext 等强一致表不要勾\n"
      "取消勾选不会删除已生成的 *_cache.go，需要手动删除"));
  genRow->addWidget(m_genCacheCheck);
  genRow->addStretch();
  connect(m_appQueryCombo, &QComboBox::activated, this, [this](int) {
    const bool appOn =
        m_appQueryCombo->currentData().toString() != QStringLiteral("none");
    m_appAuthCombo->setEnabled(appOn);
    updateAppOutBtn();
    if (!m_loading) emit configChanged();
  });
  connect(m_appOutBtn, &QPushButton::clicked, this, [this]() { editAppOutCols(); });
  presentTab->addLayout(genRow);

  presentTab->addWidget(presentSplit, 1);

  // ── 联表查询声明表格（joinTables 新格式：声明式 JOIN → 生成 QuerySpec 流水线）──
  auto *joinHeader = new QHBoxLayout;
  joinHeader->addWidget(new QLabel(QStringLiteral("联表(joinTables):"), this));
  joinHeader->addStretch();
  auto *addJoinBtn = makeCompactButton(QStringLiteral("+ 加联表"), this);
  auto *delJoinBtn = makeCompactButton(QStringLiteral("- 删联表"), this);
  auto *upJoinBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downJoinBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  joinHeader->addWidget(addJoinBtn);
  joinHeader->addWidget(delJoinBtn);
  joinHeader->addWidget(upJoinBtn);
  joinHeader->addWidget(downJoinBtn);
  connect(upJoinBtn, &QPushButton::clicked, this, [this]() { moveJoinRow(-1); });
  connect(downJoinBtn, &QPushButton::clicked, this, [this]() { moveJoinRow(1); });

  // 联表声明独占「联表」页上半（索引已拆到独立页签；下半为输出字段主从表格）
  joinTab->addLayout(joinHeader);

  m_joinTable = makeConfigTable({{QStringLiteral("联表表"), QHeaderView::Interactive, 110},
                                 {QStringLiteral("类型"), QHeaderView::Interactive, 76},
                                 {QStringLiteral("主表列"), QHeaderView::Interactive, 100},
                                 {QStringLiteral("联表列"), QHeaderView::Interactive, 100}},
                                this, 110, 0, QAbstractItemView::SelectRows);
  m_joinTable->setToolTip(
      QStringLiteral("联表表=被联表名（项目内 .jsontable）；类型=LEFT/INNER（缺省 LEFT）；\n"
                     "主表列/联表列=ON 两列（on:[主表列,联表列]，表名由规格自带）；\n"
                     "输出列=联表输出列名，逗号分隔（只管列表输出哪些列）；\n"
                     "输出别名固定为 表名_列名（前端统一字段名），查询能力在「字段呈现」页的\n"
                     "查询视图（精确/模糊）里按输出别名配置，与主表列同源。"
                     "生成端产出 EnXxxView 视图 + querySpec 联表流水线"));
  joinTab->addWidget(m_joinTable);

  // ── 联表输出字段（主从联动，对齐表内枚举/多语言：选中上方声明行，下方编辑
  //    该声明的输出字段 + 显示标题；标题空 = 回退目标列注释/列名）──
  auto *joinColHeader = new QHBoxLayout;
  joinColHeader->setSpacing(6);
  joinColHeader->addWidget(new QLabel(QStringLiteral("输出字段（选中上方联表后编辑）:"), this));
  joinColHeader->addStretch();
  m_joinColAddBtn = makeCompactButton(QStringLiteral("+ 加字段"), this);
  m_joinColDelBtn = makeCompactButton(QStringLiteral("- 删字段"), this);
  joinColHeader->addWidget(m_joinColAddBtn);
  joinColHeader->addWidget(m_joinColDelBtn);

  m_joinColTable = makeConfigTable({{QStringLiteral("字段"), QHeaderView::Interactive, 140},
                                    {QStringLiteral("显示标题"), QHeaderView::Stretch, 0}},
                                   this, 110, 0, QAbstractItemView::SelectItems);
  m_joinColTable->setToolTip(
      QStringLiteral("字段=被联表输出列名（输出别名固定 表名_列名，字段呈现自动出现）；\n"
                     "显示标题=列表页列标题（空 = 回退目标列注释/列名）；\n"
                     "联表列的查询能力在「字段呈现」页的查询模式下拉配置（精确/模糊）"));
  joinTab->addLayout(joinColHeader);
  joinTab->addWidget(m_joinColTable, 1);
  connect(m_joinColAddBtn, &QPushButton::clicked, this, [this]() {
    syncJoinColsToDraft(m_joinTable->currentRow());
    appendJoinColRow(QString(), QString());
    // 新行绑定字段名候选 = 被联表列名（appendJoinColRow 用空候选，此处按目标表补）
    const int r = m_joinTable->currentRow();
    if (r >= 0 && r < m_joinDrafts.size()) {
      const QStringList cands = joinTargetCandidates(m_joinDrafts.at(r).table);
      if (auto *combo = dynamic_cast<AuiCellEditCombo *>(m_joinColTable->cellWidget(r, JColCol)))
        combo->setPresets(cands);
    }
    syncJoinColsToDraft(m_joinTable->currentRow());  // 新行入 draft
  });
  connect(m_joinColDelBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_joinColTable->currentRow();
    if (row >= 0) m_joinColTable->removeRow(row);
    syncJoinColsToDraft(m_joinTable->currentRow());
  });
  // 主从联动：声明行切换 → 旧行字段表格内容入 draft、新行重建字段表格
  connect(m_joinTable, &QTableWidget::currentCellChanged, this,
          [this](int curRow, int, int prevRow, int) {
            if (m_loading) return;
            if (prevRow >= 0 && prevRow < m_joinDrafts.size()) syncJoinColsToDraft(prevRow);
            if (curRow >= 0 && curRow < m_joinDrafts.size()) fillJoinCols(curRow);
            m_joinCurRow = curRow;
          });
  // ── 索引页（独立页签，DDL 前：索引直接决定 DDL 预览与执行建表内容）──
  auto *idxHeader = new QHBoxLayout;
  idxHeader->addWidget(new QLabel(QStringLiteral("索引:"), this));
  idxHeader->addStretch();
  auto *addIdxBtn = makeCompactButton(QStringLiteral("+ 加索引"), this);
  auto *delIdxBtn = makeCompactButton(QStringLiteral("- 删索引"), this);
  idxHeader->addWidget(addIdxBtn);
  idxHeader->addWidget(delIdxBtn);

  m_indexTable = makeConfigTable({{QStringLiteral("索引名"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("列(逗号分隔)"), QHeaderView::Stretch, 0},
                                  {QStringLiteral("unique"), QHeaderView::Interactive, 70}},
                                 this, 200, 0, QAbstractItemView::SelectItems);
  m_indexTable->setToolTip(QStringLiteral("unique 索引自动生成 Insert/Update 唯一性预检（后端）"));
  indexTab->addLayout(idxHeader);
  indexTab->addWidget(m_indexTable, 1);

  // ── 表内枚举区（主从表：主表选枚举行，从表编辑选项）──
  auto *enumHeader = new QHBoxLayout;
  enumHeader->addWidget(new QLabel(QStringLiteral("表内枚举:"), this));
  enumHeader->addStretch();
  auto *addEnumBtn = makeCompactButton(QStringLiteral("+ 加枚举"), this);
  auto *delEnumBtn = makeCompactButton(QStringLiteral("- 删枚举"), this);
  auto *upEnumBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downEnumBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  enumHeader->addWidget(addEnumBtn);
  enumHeader->addWidget(delEnumBtn);
  enumHeader->addWidget(upEnumBtn);
  enumHeader->addWidget(downEnumBtn);

  // 上下两段可拖动：枚举主表 / 枚举选项
  auto *enumSplit = new AuiSplitter(Qt::Vertical, this);
  enumSplit->setObjectName(QStringLiteral("enumSplit"));
  enumSplit->setChildrenCollapsible(false);
  auto *enumLay = makeSection(enumSplit);
  enumLay->addLayout(enumHeader);
  enumSplit->addWidget(enumLay->parentWidget());

  m_enumTable = makeConfigTable({{QStringLiteral("枚举列"), QHeaderView::Interactive, 150},
                                 {QStringLiteral("说明"), QHeaderView::Stretch, 0}},
                                this, 72, 0, QAbstractItemView::SelectRows);
  m_enumTable->setToolTip(QStringLiteral("枚举列需与字段表格中的列名一致（如 status）"));
  enumLay->addWidget(m_enumTable, 1);
  // 枚举列候选（预设 = 字段表格列名）由 refreshColumnCandidates 遍历行刷新
  m_enumTable->verticalHeader()->setDefaultSectionSize(30);

  auto *optHeader = new QHBoxLayout;
  optHeader->addWidget(new QLabel(QStringLiteral("枚举选项（选中上方枚举后编辑）:"), this));
  optHeader->addStretch();
  auto *addOptBtn = makeCompactButton(QStringLiteral("+ 加选项"), this);
  auto *delOptBtn = makeCompactButton(QStringLiteral("- 删选项"), this);
  auto *upOptBtn = makeCompactButton(QStringLiteral("↑ 上移"), this);
  auto *downOptBtn = makeCompactButton(QStringLiteral("↓ 下移"), this);
  optHeader->addWidget(addOptBtn);
  optHeader->addWidget(delOptBtn);
  optHeader->addWidget(upOptBtn);
  optHeader->addWidget(downOptBtn);
  connect(upOptBtn, &QPushButton::clicked, this, [this]() { moveEnumOptRow(-1); });
  connect(downOptBtn, &QPushButton::clicked, this, [this]() { moveEnumOptRow(1); });
  auto *optLay = makeSection(enumSplit);
  optLay->addLayout(optHeader);
  enumSplit->addWidget(optLay->parentWidget());

  m_enumOptTable = makeConfigTable({{QStringLiteral("成员名(key)"), QHeaderView::Interactive, 120},
                                    {QStringLiteral("说明(label)"), QHeaderView::Stretch, 0},
                                    {QStringLiteral("值(value)"), QHeaderView::Interactive, 90},
                                    {QStringLiteral("值类型"), QHeaderView::Interactive, 96}},
                                   this, 100, 0, QAbstractItemView::SelectItems);
  optLay->addWidget(m_enumOptTable, 1);
  enumSplit->setStretchFactor(0, 1);
  enumSplit->setStretchFactor(1, 2);
  enumSplit->setSizes({160, 320});
  enumTab->addWidget(enumSplit, 1);

  // 各配置表统一行高，避免默认行高偏挤
  for (auto *tw : {m_colsTable, m_viewTable, m_sortTable, m_buttonTable, m_joinTable, m_indexTable,
                   m_enumTable, m_enumOptTable}) {
    tw->verticalHeader()->setDefaultSectionSize(30);
  }

  // ── 全局枚举列 ──
  auto *geRow = new QHBoxLayout;
  geRow->setSpacing(6);
  geRow->addWidget(new QLabel(QStringLiteral("全局枚举列:"), this));
  m_globalEnumEdit = new QLineEdit(this);
  m_globalEnumEdit->setPlaceholderText(
      QStringLiteral("逗号分隔，如 is_enable（列名 = .jsonglobalenum 中的枚举名）"));
  m_globalEnumEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);  // 窄面板可压缩
  m_globalEnumEdit->setToolTip(QStringLiteral(
      "命中列在 Go 侧生成对应 const 块，前端标准页合成全局枚举取值域；与表内枚举二选一"));
  geRow->addWidget(m_globalEnumEdit, 1);
  enumTab->addLayout(geRow);
  // 全局枚举列补全：候选 = 字段列名（约定列名 = .jsonglobalenum 枚举名）
  m_globalEnumCompleter = new CsvCompleter(m_globalEnumEdit);
  m_globalEnumCompleter->setCaseSensitivity(Qt::CaseInsensitive);
  m_globalEnumCompleter->setFilterMode(Qt::MatchContains);
  m_globalEnumEdit->setCompleter(m_globalEnumCompleter);

  // ── 多语言翻译表配置（多语言页）──
  m_i18nCheck =
      new QCheckBox(QStringLiteral("启用多语言（主表 + 翻译表联合查询/事务写入/级联删除）"), this);
  i18nTab->addWidget(m_i18nCheck);
  auto *i18nRow1 = new QHBoxLayout;
  i18nRow1->setSpacing(6);
  i18nRow1->addWidget(new QLabel(QStringLiteral("翻译表:"), this));
  m_i18nTableEdit = new QLineEdit(this);
  m_i18nTableEdit->setPlaceholderText(QStringLiteral("如 shop0"));
  // 水平 Ignored：窄面板下可压缩（同顶部行，见 addField 注释）
  m_i18nTableEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  i18nRow1->addWidget(m_i18nTableEdit, 1);
  i18nRow1->addWidget(new QLabel(QStringLiteral("外键列:"), this));
  m_i18nExtEdit = new QLineEdit(this);
  m_i18nExtEdit->setPlaceholderText(QStringLiteral("如 ext_id"));
  m_i18nExtEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  i18nRow1->addWidget(m_i18nExtEdit, 1);
  i18nRow1->addWidget(new QLabel(QStringLiteral("语言列:"), this));
  m_i18nLangKeyEdit = new QLineEdit(this);
  m_i18nLangKeyEdit->setPlaceholderText(QStringLiteral("如 lang"));
  m_i18nLangKeyEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  i18nRow1->addWidget(m_i18nLangKeyEdit, 1);
  i18nTab->addLayout(i18nRow1);
  auto *i18nRow2 = new QHBoxLayout;
  i18nRow2->setSpacing(6);
  i18nRow2->addWidget(new QLabel(QStringLiteral("默认语言:"), this));
  m_i18nDefaultLangEdit = new QLineEdit(this);
  m_i18nDefaultLangEdit->setMaximumWidth(80);
  m_i18nDefaultLangEdit->setPlaceholderText(QStringLiteral("zh"));
  i18nRow2->addWidget(m_i18nDefaultLangEdit);
  i18nRow2->addWidget(new QLabel(QStringLiteral("语言来源:"), this));
  m_i18nLangFromCombo = AuiComboBox::create(this);
  m_i18nLangFromCombo->addItem(QStringLiteral("body（请求体 lang 字段）"), QStringLiteral("body"));
  m_i18nLangFromCombo->addItem(QStringLiteral("header（请求头，预留）"), QStringLiteral("header"));
  m_i18nLangFromCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  i18nRow2->addWidget(m_i18nLangFromCombo);
  i18nTab->addLayout(i18nRow2);
  // ── 多语言字段表格（列 = 字段名 + 显示标题；标题空 = 回退链）──
  auto *i18nFieldHeader = new QHBoxLayout;
  i18nFieldHeader->setSpacing(6);
  auto *i18nFieldTip = new QLabel(
      QStringLiteral("多语言字段（显示标题空 = 回退：标题映射 → 翻译表列注释 → 字段名）"), this);
  i18nFieldTip->setToolTip(
      QStringLiteral("字段为翻译表的文本列（不能与主表列重名）；\n"
                     "显示标题为生成页面的列标题，留空时按回退链取名"));
  i18nFieldHeader->addWidget(i18nFieldTip);
  i18nFieldHeader->addStretch();
  m_i18nFieldAddBtn = makeCompactButton(QStringLiteral("+ 加字段"), this);
  m_i18nFieldDelBtn = makeCompactButton(QStringLiteral("- 删字段"), this);
  i18nFieldHeader->addWidget(m_i18nFieldAddBtn);
  i18nFieldHeader->addWidget(m_i18nFieldDelBtn);
  i18nTab->addLayout(i18nFieldHeader);
  m_i18nFieldTable = makeConfigTable(
      {{QStringLiteral("字段"), QHeaderView::Interactive, 160},
       {QStringLiteral("显示标题"), QHeaderView::Stretch, 0}},
      this, 90, 0, QAbstractItemView::SelectItems);
  i18nTab->addWidget(m_i18nFieldTable);
  i18nTab->addStretch(1);
  applyI18nEnabled(false);

  // ── 刷新时忽略的列（排除列）──
  // 从数据库刷新结构时，这些列不会新增也不会被修改；用于 token/locked 等
  // 不希望进入生成配置的认证列。持久化在 table.extra.excludeColumns。
  auto *excludeRow = new QHBoxLayout;
  excludeRow->setSpacing(6);
  excludeRow->addWidget(new QLabel(QStringLiteral("排除列:"), this));
  m_excludeEdit = new QLineEdit(this);
  m_excludeEdit->setPlaceholderText(
      QStringLiteral("逗号分隔，如 token,locked（从数据库刷新时忽略这些列）"));
  m_excludeEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);  // 窄面板可压缩
  m_excludeEdit->setToolTip(
      QStringLiteral("从数据库刷新结构时忽略的列：既不新增、也不修改；\n"
                     "适合 token/locked 等不想进入生成配置的认证列"));
  excludeRow->addWidget(m_excludeEdit, 1);
  ddlTab->addLayout(excludeRow);

  // ── DDL 预览 + 执行区（DDL 页）──
  auto *ddlHeader = new QHBoxLayout;
  ddlHeader->addWidget(new QLabel(QStringLiteral("DDL:"), this));
  ddlHeader->addStretch();
  m_importBtn = makeCompactButton(QStringLiteral("从数据库刷新结构"), this);
  auto *previewBtn = makeCompactButton(QStringLiteral("DDL 预览"), this);
  auto *execBtn = makeCompactButton(QStringLiteral("执行建表"), this);
  ddlHeader->addWidget(m_importBtn);
  ddlHeader->addWidget(previewBtn);
  ddlHeader->addWidget(execBtn);
  ddlTab->addLayout(ddlHeader);

  m_ddlView = new QPlainTextEdit(this);
  m_ddlView->setReadOnly(true);
  m_ddlView->setPlaceholderText(QStringLiteral("点击「DDL 预览」生成 CREATE TABLE 语句"));
  m_ddlView->setMinimumHeight(160);
  ddlTab->addWidget(m_ddlView, 1);

  // meta.db 就绪性由 updateDbButtonStates 统一处理（构造时 config 可能为空，
  // 真正的连接信息在 load() 后才到位，refill 末尾会重新评估）
  updateDbButtonStates();

  // ── 连接 ──
  connect(m_refreshFieldsBtn, &QPushButton::clicked, this, &JsonTableDesigner::importFromDatabase);
  connect(addColBtn, &QPushButton::clicked, this, [this]() { addColumn(); });
  connect(delColBtn, &QPushButton::clicked, this, [this]() { deleteColumn(); });
  connect(upColBtn, &QPushButton::clicked, this, [this]() { moveColumn(-1); });
  connect(downColBtn, &QPushButton::clicked, this, [this]() { moveColumn(1); });
  connect(addIdxBtn, &QPushButton::clicked, this, [this]() { appendIndexRow(); });
  connect(delIdxBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_indexTable->currentRow();
    if (row >= 0) m_indexTable->removeRow(row);
  });
  connect(m_importBtn, &QPushButton::clicked, this, &JsonTableDesigner::importFromDatabase);
  connect(previewBtn, &QPushButton::clicked, this, &JsonTableDesigner::previewDdl);
  m_executeBtn = execBtn;
  connect(execBtn, &QPushButton::clicked, this, &JsonTableDesigner::executeCreateTable);
  connect(addSortBtn, &QPushButton::clicked, this,
          [this]() { appendSortRow(QString(), QStringLiteral("DESC")); });
  connect(delSortBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_sortTable->currentRow();
    if (row >= 0) m_sortTable->removeRow(row);
  });
  connect(m_i18nFieldAddBtn, &QPushButton::clicked, this,
          [this]() { appendI18nFieldRow(QString(), QString()); });
  connect(m_i18nFieldDelBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_i18nFieldTable->currentRow();
    if (row >= 0) m_i18nFieldTable->removeRow(row);
  });
  connect(addJoinBtn, &QPushButton::clicked, this, [this]() {
    m_joinDrafts.append(JsonTableJoinSpec());
    appendJoinRow();
    // 新行候选（联表表=项目表名；主表列=本表列名；联表列=目标表列名）
    refreshJoinCandidates();
  });
  connect(delJoinBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_joinTable->currentRow();
    if (row >= 0) {
      m_joinTable->removeRow(row);
      if (row < m_joinDrafts.size()) m_joinDrafts.removeAt(row);
      m_joinCurRow = -1;
      m_joinColTable->setRowCount(0);
    }
  });
  // 联表行自动跟随「联表」页的输出字段（不再手动添加）；物理列行跟随字段表格
  connect(upViewBtn, &QPushButton::clicked, this, [this]() { moveViewRow(-1); });
  connect(downViewBtn, &QPushButton::clicked, this, [this]() { moveViewRow(1); });
  connect(addEnumBtn, &QPushButton::clicked, this, [this]() {
    syncEnumDrafts();
    JsonTableEnum e;
    m_enums.append(e);
    rebuildEnumMaster();
    const int newRow = m_enumTable->rowCount() - 1;
    m_enumTable->selectRow(newRow);
    m_enumTable->setCurrentCell(newRow, 0);
    // 枚举列为常驻可编辑下拉（cellWidget），直接聚焦输入框进入编辑
    if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_enumTable->cellWidget(newRow, 0)))
      c->lineEdit()->setFocus(Qt::OtherFocusReason);
  });
  connect(delEnumBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_enumTable->currentRow();
    if (row < 0 || row >= m_enums.size()) return;
    syncEnumDrafts();
    m_enums.removeAt(row);
    rebuildEnumMaster();
    const int next = qMin(row, m_enumTable->rowCount() - 1);
    if (next >= 0) m_enumTable->selectRow(next);
  });
  connect(upEnumBtn, &QPushButton::clicked, this, [this]() { moveEnumRow(-1); });
  connect(downEnumBtn, &QPushButton::clicked, this, [this]() { moveEnumRow(1); });
  connect(addOptBtn, &QPushButton::clicked, this, [this]() {
    if (m_enumCurRow < 0) return;
    appendEnumOptRow();
  });
  connect(delOptBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_enumOptTable->currentRow();
    if (row >= 0) m_enumOptTable->removeRow(row);
  });
  // 枚举主从表联动：行切换前把主表/选项表的编辑内容存回 m_enums
  connect(m_enumTable, &QTableWidget::currentCellChanged, this,
          [this](int cur, int, int prev, int) {
            if (m_enumSyncing) return;
            syncEnumDrafts();
            fillEnumOptions(cur);
          });
  connect(m_i18nCheck, &QCheckBox::toggled, this, [this](bool on) { applyI18nEnabled(on); });
  // 字段表格编辑 → 列名候选刷新（枚举列下拉/联表外键列/编辑列等）
  connect(m_colsTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
    if (!it) return;
    refreshColumnCandidates();
  });
  // 字段呈现表「配置」列双击（⚙ 与摘要同一单元格）→ 字段设置面板
  // （单击不弹：避免只想点选单元格/复制摘要时误开对话框）
  connect(m_viewTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
    if (col == UColCfg) openViewFieldConfig(row);
  });
  // 翻译表名输入 → 多语言字段候选切换
  connect(m_i18nTableEdit, &QLineEdit::textChanged, this,
          [this]() { refreshI18nFieldCandidates(); });

  layout->addWidget(tabs, 1);

  // ── 交叉校验状态行（被动：随编辑自动刷新，替代原弹窗确定拦截与校验按钮）──
  // 文字可选中复制（错误详情常需贴给他人排查）：QLabel 默认不可选，
  // 开启鼠标/键盘选择后右键自动带「复制」菜单，光标同步换 IBeam 提示可复制
  m_validateLabel = new QLabel(this);
  m_validateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
  m_validateLabel->setCursor(Qt::IBeamCursor);
  m_validateLabel->setContentsMargins(4, 0, 4, 0);
  layout->addWidget(m_validateLabel);
  applyStyle();
}

// ════════════════════════════════════════════════════════════
//  refill（loadConfig 后全量回填各表格）
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::refill() {
  m_loading = true;
  m_nameEdit->setText(m_table.tableName);
  m_modelEdit->setText(m_table.modelName);
  m_commentEdit->setText(m_table.tableComment);
  m_manualUpdateCheck->setChecked(m_table.manualUpdateTime);
  m_frontendCheck->setChecked(m_table.frontend);
  m_genAddCheck->setChecked(m_table.genAdd);
  m_genEditCheck->setChecked(m_table.genEdit);
  m_genDeleteCheck->setChecked(m_table.genDelete);
  m_genDetailCheck->setChecked(m_table.genDetail);
  // 回填后同步按钮组可用状态（仅置灰联动，勾选值保持 m_table 原样）
  updateGenButtonsEnabled();
  m_genCacheCheck->setChecked(m_table.genCache);
  {
    // C 端形态：bool + mode → 下拉（"none"/"query"/"empty"）
    const QString appMode =
        m_table.appQuery ? (m_table.appQueryMode == QStringLiteral("empty")
                                ? QStringLiteral("empty")
                                : QStringLiteral("query"))
                         : QStringLiteral("none");
    const int appIdx = m_appQueryCombo->findData(appMode);
    m_appQueryCombo->setCurrentIndex(appIdx >= 0 ? appIdx : 0);
    const int authIdx = m_appAuthCombo->findData(m_table.appAuth);
    m_appAuthCombo->setCurrentIndex(authIdx >= 0 ? authIdx : 1);
    m_appAuthCombo->setEnabled(appMode != QStringLiteral("none"));
    m_appOutCols = m_table.appOutCols;
    updateAppOutBtn();
  }
  fillColumns(m_table.columns);
  fillIndexes(m_table.indexes);
  fillJoins(m_table.joinTables);
  fillViewTable();
  // 记录回填后的行序基线：保存时只有用户真正移动过行才落 viewOrder，
  // 避免打开文件后没动就凭空新增键（那会让「单击/失焦」被误判成内容变化）
  m_viewBaselineOrder.clear();
  for (int r = 0; r < m_viewTable->rowCount(); ++r) {
    const QString ref = viewRefText(r);
    if (!ref.isEmpty()) m_viewBaselineOrder << ref;
  }
  fillSortTable(m_table.selColsSort);
  m_buttons = m_table.buttons;
  fillButtons();
  m_enums = m_table.enums;
  rebuildEnumMaster();
  if (m_enumTable->rowCount() > 0) m_enumTable->selectRow(0);
  m_globalEnumEdit->setText(joinCsv(m_table.globalEnumCols));
  m_i18nCheck->setChecked(m_table.i18n.isEnabled());
  m_i18nTableEdit->setText(m_table.i18n.table);
  m_i18nExtEdit->setText(m_table.i18n.extKey);
  m_i18nLangKeyEdit->setText(m_table.i18n.langKey);
  m_i18nDefaultLangEdit->setText(m_table.i18n.defaultLang);
  comboSelectData(m_i18nLangFromCombo, m_table.i18n.langFrom);
  m_i18nFieldTable->setRowCount(0);
  for (const auto &f : m_table.i18n.fields) appendI18nFieldRow(f.name, f.label);
  applyI18nEnabled(m_i18nCheck->isChecked());
  // 排除列（设计器专属键，存 table.extra，生成期不消费）
  QStringList excludedColumns;
  const QJsonArray excludedArray =
      m_table.extra.value(QString::fromLatin1(JsonTableKey::kExcludeColumns)).toArray();
  for (const auto &value : excludedArray) {
    const QString name = value.toString().trimmed();
    if (!name.isEmpty()) excludedColumns << name;
  }
  m_excludeEdit->setText(joinCsv(excludedColumns));
  refreshColumnCandidates();
  refreshI18nFieldCandidates();
  m_loading = false;
  updateDbButtonStates();
  updateValidateStatus();
}

void JsonTableDesigner::updateDbButtonStates() {
  // 密码允许为空（本机 MySQL 免密场景），只要求连接定位三要素齐全
  const bool ready =
      !m_config.dbHost.isEmpty() && !m_config.dbUser.isEmpty() && !m_config.dbDatabase.isEmpty();
  const QString readyTip = QStringLiteral("按当前表名拉取数据库结构，先展示差异再按勾选应用");
  const QString missingTip =
      QStringLiteral("meta.db 未配置完整（host/user/database），无法从数据库刷新结构");
  if (m_executeBtn) m_executeBtn->setEnabled(ready && !m_dbBusy);
  for (QPushButton *button : {m_importBtn, m_refreshFieldsBtn}) {
    if (!button) continue;
    button->setEnabled(ready && !m_dbBusy);
    button->setToolTip(ready ? readyTip : missingTip);
  }
}

void JsonTableDesigner::load(const JsonTableTable &table, const JsonTableConfig &config,
                             const QString &filePath) {
  ++m_revision;
  m_table = table;
  m_config = config;
  m_filePath = filePath;
  m_buttons = table.buttons;
  refill();
}

void JsonTableDesigner::updateValidateStatus() {
  if (!m_validateLabel) return;
  QString err;
  if (validateCrossRefs(&err)) {
    m_validateLabel->setText(QStringLiteral("✓ 交叉引用校验通过"));
    m_validateLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::mutedTextColor().name()));
  } else {
    m_validateLabel->setText(QStringLiteral("⚠ %1").arg(err));
    m_validateLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(AuiStyle::warningColor().name()));
  }
}

void JsonTableDesigner::applyI18nEnabled(bool on) {
  m_i18nTableEdit->setEnabled(on);
  m_i18nExtEdit->setEnabled(on);
  m_i18nLangKeyEdit->setEnabled(on);
  m_i18nDefaultLangEdit->setEnabled(on);
  m_i18nLangFromCombo->setEnabled(on);
  m_i18nFieldTable->setEnabled(on);
  m_i18nFieldAddBtn->setEnabled(on);
  m_i18nFieldDelBtn->setEnabled(on);
}

void JsonTableDesigner::updateGenButtonsEnabled() {
  // 仅联动可用状态（置灰），不改勾选值：collect 仍按原勾选保存，
  // 「生成前端页面」重新勾上后按钮配置原样恢复
  const bool on = m_frontendCheck->isChecked();
  m_genAddCheck->setEnabled(on);
  m_genEditCheck->setEnabled(on);
  m_genDeleteCheck->setEnabled(on);
  m_genDetailCheck->setEnabled(on);
}

void JsonTableDesigner::connectChangeSignals() {
  // 表格：模型级变更（条目编辑/行增删/搬运）全部覆盖；行内下拉在创建点单独接线
  auto onChange = [this]() {
    if (m_loading) return;
    // 枚举主从联动（fillEnumOptions/rebuildEnumMaster）会重建表格，产生的
    // dataChanged/rowsInserted 是程序性刷新而非用户编辑，不能误报「已修改」
    //（典型路径：点表内枚举任意一行 → 联动重建选项表 → 若不挡会立即亮保存）
    if (m_enumSyncing) return;
    // 联表输出字段表格程序性重建（fillJoinCols）同理：接线在 model 层
    //（dataChanged/rowsRemoved/rowsInserted），表格自身 blockSignals 挡不住；
    // 不挡的话重建中 setRowCount(0) 会同步跑 updateValidateStatus → collectJoins
    // → syncJoinColsToDraft，把此刻的空表格内容写进 drafts 造成输出字段丢失
    if (m_joinColRebuilding) return;
    emit configChanged();
    updateValidateStatus();
  };
  for (QTableWidget *tw : findChildren<QTableWidget *>()) {
    connect(tw->model(), &QAbstractItemModel::dataChanged, this, onChange);
    connect(tw->model(), &QAbstractItemModel::rowsInserted, this, onChange);
    connect(tw->model(), &QAbstractItemModel::rowsRemoved, this, onChange);
  }
  // 静态输入控件（setupUI 构建的顶层控件；行内下拉的内部输入框跳过：
  // 后者随 combo 一起单独接线，避免「点一下单元格」就误报变化）
  for (QLineEdit *le : findChildren<QLineEdit *>()) {
    if (qobject_cast<QComboBox *>(le->parentWidget())) continue;
    connect(le, &QLineEdit::textChanged, this, onChange);
  }
  for (QCheckBox *cb : findChildren<QCheckBox *>()) {
    connect(cb, &QCheckBox::toggled, this, onChange);
  }
  for (QComboBox *combo : findChildren<QComboBox *>()) {
    connect(combo, &QComboBox::currentIndexChanged, this, onChange);
  }
  // 「表单展示」「输出」勾选变化 → 刷新该行配置摘要
  connect(m_viewTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
    if (!it || m_loading || it->row() < 0) return;
    if (it->column() == UColVisible || it->column() == UColOutput)
      refreshViewCfgSummary(it->row());
  });
  // 多语言页「显示标题」编辑 → 实时同步字段呈现 i18n 行标题（两处联动；
  // 数据落盘仍走 collectTable，此处仅刷新界面，不重复广播 configChanged）
  connect(m_i18nFieldTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
    if (!it || m_loading || it->column() != 1 || it->row() < 0) return;
    auto *nameCombo = dynamic_cast<AuiCellEditCombo *>(m_i18nFieldTable->cellWidget(it->row(), 0));
    const QString name = nameCombo ? nameCombo->text().trimmed() : QString();
    if (name.isEmpty()) return;
    const QString label = it->text().trimmed();
    const QString title = label.isEmpty() ? name : label;  // 空 = 回退字段名
    for (int r = 0; r < m_viewTable->rowCount(); ++r) {
      if (viewRefText(r) != name) continue;
      auto *titleIt = m_viewTable->item(r, UColTitle);
      // i18n 行标题条目无 ItemIsEditable（appendRow 置只读）；物理列行不动
      if (titleIt && titleIt->flags().testFlag(Qt::ItemIsEditable)) break;
      if (titleIt) titleIt->setText(title);
      // 只读标题行挂有 AuiCellReadBox，同步其显示文本（item 是数据源）
      if (auto *rb = dynamic_cast<AuiCellReadBox *>(m_viewTable->cellWidget(r, UColTitle)))
        rb->setText(title);
      break;
    }
  });
  // 联表页「显示标题」编辑 → 实时同步字段呈现联表行标题（对齐多语言页两处联动；
  // 数据落盘仍走 collectTable，此处仅刷新界面）。标题链与 fillViewTable 一致：
  // 编辑态 label > 目标列注释（m_aliasComments 回填缓存）> 别名
  connect(m_joinColTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
    if (!it || m_loading || m_joinColRebuilding) return;
    if (it->column() != JColLabel || it->row() < 0) return;
    // alias = 被联表名_输出列名（表名取主从联动的当前声明行编辑态）
    const int jr = m_joinTable->currentRow();
    if (jr < 0 || jr >= m_joinDrafts.size()) return;
    auto *colCombo = dynamic_cast<AuiCellEditCombo *>(m_joinColTable->cellWidget(it->row(), JColCol));
    const QString col = colCombo ? colCombo->text().trimmed() : QString();
    const QString table = m_joinDrafts.at(jr).table.trimmed();
    if (col.isEmpty() || table.isEmpty()) return;
    const QString alias = table + QLatin1Char('_') + col;
    const QString label = it->text().trimmed();
    QString fallback = alias;
    if (m_aliasComments.contains(alias) && !m_aliasComments.value(alias).isEmpty())
      fallback = m_aliasComments.value(alias);
    const QString title = label.isEmpty() ? fallback : label;
    for (int r = 0; r < m_viewTable->rowCount(); ++r) {
      if (viewRefText(r) != alias) continue;
      auto *titleIt = m_viewTable->item(r, UColTitle);
      // 联表行标题条目无 ItemIsEditable（appendRow 置只读）；物理列行不动
      if (titleIt && titleIt->flags().testFlag(Qt::ItemIsEditable)) break;
      if (titleIt) titleIt->setText(title);
      // 只读标题行挂有 AuiCellReadBox，同步其显示文本（item 是数据源）
      if (auto *rb = dynamic_cast<AuiCellReadBox *>(m_viewTable->cellWidget(r, UColTitle)))
        rb->setText(title);
      break;
    }
  });
}

void JsonTableDesigner::applyStyle() {
  const auto fontPx = QString::number(AuiStyle::dialogFontSize());
  const auto text = AuiStyle::textColor().name();
  const auto border = AuiStyle::borderColor().name();
  const auto bg = AuiStyle::panelBackground().name();
  const auto headerBg = AuiStyle::background().name();
  const auto alt = AuiStyle::listAlternateBackground().name();

  QString qss = QStringLiteral(
                    "QLabel { font-size: %1px; color: %2; background: transparent; }"
                    "QPushButton { padding: 2px 8px; font-size: %1px; color: %2; }"
                    "QCheckBox { font-size: %1px; color: %2; }"
                    // 禁用态降为弱色：控件级样式表优先于全局样式表，
                    // 全局的 QCheckBox:disabled 在此被覆盖，必须本地补
                    "QCheckBox:disabled { color: %7; }"
                    "QTabWidget::pane { border: 1px solid %3; border-radius: 4px;"
                    "  background: %4; top: -1px; }"
                    "QTabBar::tab { font-size: %1px; color: %2; padding: 4px 10px;"
                    "  border: 1px solid %3; border-bottom: none;"
                    "  background: transparent; border-top-left-radius: 4px;"
                    "  border-top-right-radius: 4px; }"
                    "QTabBar::tab:selected { background: %4; color: %2; }"
                    "QTableWidget { font-size: %1px; background: %4; color: %2;"
                    "  gridline-color: %3; alternate-background-color: %5; }"
                    "QTableWidget::item { color: %2; }"
                    "QHeaderView::section { padding: 2px; font-size: %1px;"
                    "  background: %6; color: %2; border: none;"
                    "  border-right: 1px solid %3; border-bottom: 1px solid %3; }"
                    "QComboBox { font-size: %1px; color: %2; background: %4;"
                    "  border: 1px solid %3; border-radius: 3px; padding: 1px 4px; }"
                    "QLineEdit { font-size: %1px; color: %2; background: %4;"
                    "  border: 1px solid %3; border-radius: 3px; padding: 1px 4px; }"
                    "QPlainTextEdit { font-size: %1px; color: %2; background: %4;"
                    "  border: 1px solid %3; }"
                    // 分隔条由 AuiSplitterHandle 自绘（1px 细线 + 三点抓手），
                    // 不走 QSS，避免样式表与自绘重叠
                    "QScrollBar:vertical, QScrollBar:horizontal { background: %4; }")
                    .arg(fontPx, text, border, bg, alt, headerBg,
                         AuiStyle::mutedTextColor().name());
  setStyleSheet(qss);
}

// ════════════════════════════════════════════════════════════
//  字段表格填充 / 收集 / 行操作
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillColumns(const QVector<JsonTableColumn> &cols) {
  m_colsTable->setRowCount(0);
  for (const auto &c : cols) appendColumnRow(c);
}

void JsonTableDesigner::appendColumnRow(const JsonTableColumn &c) {
  const int row = m_colsTable->rowCount();
  m_colsTable->insertRow(row);
  m_colsTable->setItem(row, ColColName, new QTableWidgetItem(c.name));
  // 类型列文本由常驻下拉显示，条目只占位（避免条目文字与下拉文字叠成重影）
  m_colsTable->setItem(row, ColColType, new QTableWidgetItem);
  bindColTypeCombo(row, c.mysqlType);
  m_colsTable->setItem(row, ColColUnsigned, makeConfigCheckItem(c.isUnsigned));
  m_colsTable->setItem(row, ColColNullable, makeConfigCheckItem(c.nullable));
  m_colsTable->setItem(row, ColColPrimary, makeConfigCheckItem(c.isPrimary));
  m_colsTable->setItem(row, ColColAutoInc, makeConfigCheckItem(c.isAutoInc));
  m_colsTable->setItem(row, ColColDefault, new QTableWidgetItem(c.defaultValue));
  m_colsTable->setItem(row, ColColComment, new QTableWidgetItem(c.comment));
}

/// @brief 为字段表格某行绑定 MySQL 类型列的常驻输入框 + ▾ 下拉
void JsonTableDesigner::bindColTypeCombo(int row, const QString &type) {
  auto *cell = new AuiCellEditCombo(m_colsTable, ColColType, colTypePresets(), m_colsTable);
  cell->setText(type.isEmpty() ? QStringLiteral("bigint") : type);  // 与收集时兜底一致
  // 值变化统一上报：点候选走 valueCommitted（值变化才触发）；手输走 textEdited。
  // cellWidget 在构造之后才创建，connectChangeSignals 的 findChildren 覆盖不到，
  // 必须在此接线
  cell->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  connect(cell->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
    if (!m_loading) emit configChanged();
  });
  m_colsTable->setCellWidget(row, ColColType, cell);
}

QString JsonTableDesigner::cellText(int row, int col) const {
  // 类型列是常驻编辑控件（AuiCellEditCombo：输入框 + ▾ 下拉），文本以控件为准
  // （AuiCellEditCombo 无 Q_OBJECT，不能用 qobject_cast，走 dynamic_cast）
  if (auto *cell = dynamic_cast<AuiCellEditCombo *>(m_colsTable->cellWidget(row, col)))
    return cell->text().trimmed();
  const QTableWidgetItem *it = m_colsTable->item(row, col);
  return it ? it->text().trimmed() : QString();
}

bool JsonTableDesigner::cellChecked(int row, int col) const {
  const QTableWidgetItem *it = m_colsTable->item(row, col);
  return it && it->checkState() == Qt::Checked;
}

JsonTableColumn JsonTableDesigner::columnFromRow(int row) const {
  JsonTableColumn c;
  c.name = cellText(row, ColColName);
  c.mysqlType = cellText(row, ColColType);
  if (c.mysqlType.isEmpty()) c.mysqlType = QStringLiteral("bigint");
  c.isUnsigned = cellChecked(row, ColColUnsigned);
  c.nullable = cellChecked(row, ColColNullable);
  c.isPrimary = cellChecked(row, ColColPrimary);
  c.isAutoInc = cellChecked(row, ColColAutoInc);
  c.defaultValue = cellText(row, ColColDefault);
  c.comment = cellText(row, ColColComment);
  return c;
}

QVector<JsonTableColumn> JsonTableDesigner::collectColumns() const {
  QVector<JsonTableColumn> out;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    JsonTableColumn c = columnFromRow(r);
    if (c.name.isEmpty()) continue;  // 跳过未填列名的行
    out.append(c);
  }
  return out;
}

void JsonTableDesigner::addColumn() {
  appendColumnRow(JsonTableColumn());  // 模型缺省：bigint / list=true / form=input
  const int row = m_colsTable->rowCount() - 1;
  m_colsTable->setCurrentCell(row, ColColName);
  m_colsTable->editItem(m_colsTable->item(row, ColColName));  // 直接进入列名编辑
}

void JsonTableDesigner::deleteColumn() {
  const int row = m_colsTable->currentRow();
  if (row < 0) return;
  const QString name = cellText(row, ColColName);
  if (name.isEmpty()) {
    m_colsTable->removeRow(row);
    refreshColumnCandidates();
    return;
  }

  // 在模型副本上删除并清理引用；用户取消时界面不受影响
  JsonTableTable table = collectTable();
  for (int i = 0; i < table.columns.size(); ++i) {
    if (table.columns[i].name.compare(name, Qt::CaseInsensitive) == 0) {
      table.columns.removeAt(i);
      break;
    }
  }
  const RefCleanupStats cleanup = cleanRemovedColumnRefs(&table, QStringList() << name);
  if (!cleanup.details.isEmpty()) {
    const QString message =
        QStringLiteral("删除列「%1」将同时清理以下前端引用：\n\n%2\n\n是否继续？")
            .arg(name, cleanup.details.join(QStringLiteral("\n")));
    if (!AuiMessageBox::confirm(this, QStringLiteral("删除列并清理引用"), message)) return;
  }

  load(table, m_config, m_filePath);
  emit configChanged();
  updateValidateStatus();
}

void JsonTableDesigner::moveColumn(int delta) {
  const int row = m_colsTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_colsTable->rowCount()) return;

  // 整行搬运：先摘下条目，删行后按新位置插回（复选框状态随条目保留）
  // 类型列的常驻下拉随 removeRow 销毁，搬完按保存的文本重新绑定
  const QString typeText = cellText(row, ColColType);
  QVector<QTableWidgetItem *> items(ColColCount);
  for (int c = 0; c < ColColCount; ++c) items[c] = m_colsTable->takeItem(row, c);
  m_colsTable->removeRow(row);
  m_colsTable->insertRow(target);
  for (int c = 0; c < ColColCount; ++c) m_colsTable->setItem(target, c, items[c]);
  bindColTypeCombo(target, typeText);
  m_colsTable->setCurrentCell(target, ColColName);
  refreshColumnCandidates();  // 行搬运不触发 itemChanged，显式刷新候选
}

/// @brief 字段呈现表格行上移/下移（delta=-1 上移，1 下移）
/// 查询模式值持久化在引用条目 +26，其余列（含只读的「字段引用」文本条目）
/// 整行 takeItem 搬运（复选框/⚙/行内元数据随条目保留），全程块信号
void JsonTableDesigner::moveViewRow(int delta) {
  const int row = m_viewTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_viewTable->rowCount()) return;
  // 交换全程块信号：逐列 setItem 触发的 itemChanged 会中途跑
  // refreshViewCfgSummary（把 i18n 行的「🌐 多语言」徽标覆盖成普通摘要）、
  // 连发 configChanged；成功后由本函数末尾显式广播一次
  m_viewTable->blockSignals(true);
  for (int c = 0; c < UColCount; ++c) {
    if (c == UColQuery) {
      // 查询模式列：cellWidget 固定在行位置，模式值持久化在引用条目 +26，
      // 循环外按条目数据重置显示（见下方），此处跳过
      continue;
    }
    // 含「字段引用」只读文本条目：整条 takeItem 搬运（文本+元数据一起走；
    // 引用列已无常驻下拉，旧版「item 与 indexWidget 冲突」不复存在）
    QTableWidgetItem *it1 = m_viewTable->takeItem(row, c);
    QTableWidgetItem *it2 = m_viewTable->takeItem(target, c);
    m_viewTable->setItem(row, c, it2);
    m_viewTable->setItem(target, c, it1);
  }
  m_viewTable->blockSignals(false);
  // 查询模式下拉固定在行位置（cellWidget 不随条目走），模式值已随引用条目
  // （+26）交换 → 按条目数据重置两行显示（i18n 行 +26 恒 none = 不查询）
  for (int r : {row, target}) {
    auto *mc = dynamic_cast<AuiCellEditCombo *>(m_viewTable->cellWidget(r, UColQuery));
    if (!mc) continue;
    const QString m26 = m_viewTable->item(r, UColRef)
                            ? m_viewTable->item(r, UColRef)->data(Qt::UserRole + 26).toString()
                            : QString();
    mc->setText(queryModeDisplay(m26.isEmpty() ? QStringLiteral("none") : m26));
  }
  // 字段引用列只读文本框同样固定在行位置（cellWidget 不随条目走），条目已随
  // takeItem 交换 → 按条目文本重置两行显示
  for (int r : {row, target}) {
    auto *e = dynamic_cast<AuiCellReadBox *>(m_viewTable->cellWidget(r, UColRef));
    if (!e) continue;
    const auto *it = m_viewTable->item(r, UColRef);
    if (it) e->setText(it->text());
  }
  // 标题列只读行（i18n / 联表）同样挂只读控件：交换后按每行的只读性校正
  //（只读 item 无 ItemIsEditable；cellWidget 固定行位，须移除/重建/同步文本）
  for (int r : {row, target}) {
    auto *titleIt = m_viewTable->item(r, UColTitle);
    const bool ro = titleIt && !titleIt->flags().testFlag(Qt::ItemIsEditable);
    auto *rb = dynamic_cast<AuiCellReadBox *>(m_viewTable->cellWidget(r, UColTitle));
    if (ro && !rb) {
      if (m_viewTable->cellWidget(r, UColTitle)) m_viewTable->removeCellWidget(r, UColTitle);
      auto *fresh = new AuiCellReadBox(m_viewTable, r, UColTitle, titleIt->text());
      fresh->setTip(titleIt->toolTip());
      m_viewTable->setCellWidget(r, UColTitle, fresh);
    } else if (!ro && rb) {
      m_viewTable->removeCellWidget(r, UColTitle);
    } else if (ro && rb) {
      rb->setText(titleIt->text());
    }
  }
  m_viewTable->setCurrentCell(target, UColRef);
  if (!m_loading) emit configChanged();
}

// ════════════════════════════════════════════════════════════
//  其余配置表行上移/下移（排序 / 操作按钮 / 联表 / 枚举选项）
// ════════════════════════════════════════════════════════════

/// @brief 通用两行整行交换：普通条目 takeItem 搬运；cellWidget 下拉逐列交换文本。
/// 交换全程块信号（表格 model 信号 + 下拉自身信号），避免逐项触发 configChanged，
/// 由调用方在成功后显式广播一次。可编辑下拉用 setEditText，固定项下拉按文本回选。
bool JsonTableDesigner::swapConfigRows(QTableWidget *tw, int a, int b) {
  if (!tw || a < 0 || b < 0 || a >= tw->rowCount() || b >= tw->rowCount() || a == b) {
    return false;
  }
  tw->blockSignals(true);
  for (int c = 0; c < tw->columnCount(); ++c) {
    // 普通条目交换（widget 列取到的是 nullptr，setItem(nullptr) 等价清位）
    QTableWidgetItem *ia = tw->takeItem(a, c);
    QTableWidgetItem *ib = tw->takeItem(b, c);
    tw->setItem(a, c, ib);
    tw->setItem(b, c, ia);
    // cellWidget 下拉交换当前文本（程序性 setText 不触发用户信号，无需块信号）
    auto *wa = dynamic_cast<AuiCellEditCombo *>(tw->cellWidget(a, c));
    auto *wb = dynamic_cast<AuiCellEditCombo *>(tw->cellWidget(b, c));
    if (!wa || !wb) continue;
    const QString ta = wa->text();
    const QString tb = wb->text();
    wa->setText(tb);
    wb->setText(ta);
  }
  tw->blockSignals(false);
  return true;
}

void JsonTableDesigner::moveSortRow(int delta) {
  const int row = m_sortTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_sortTable->rowCount()) return;
  if (swapConfigRows(m_sortTable, row, target)) {
    m_sortTable->setCurrentCell(target, 0);
    if (!m_loading) emit configChanged();
  }
}

void JsonTableDesigner::moveJoinRow(int delta) {
  const int row = m_joinTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_joinTable->rowCount()) return;
  if (swapConfigRows(m_joinTable, row, target)) {
    // 联表行含 cellWidget（类型/输出字段下拉）：条目交换不动 cellWidget，
    // 交换后按 m_joinDrafts 重建两行 + drafts 同步交换
    syncJoinColsToDraft(row);
    syncJoinColsToDraft(target);
    m_joinDrafts.swapItemsAt(row, target);
    // 类型下拉文本互换（cellWidget 固定在行位置）
    for (int r : {row, target}) {
      auto *tc = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColType));
      if (tc) tc->setText(m_joinDrafts.at(r).type == QStringLiteral("INNER")
                              ? QStringLiteral("INNER")
                              : QStringLiteral("LEFT"));
      // 联表表/ON 两列文本已随 swapConfigRows 互换；联表列候选随目标表刷新
      refreshJoinRowCandidates(r);
    }
    m_joinTable->setCurrentCell(target, JoinColTable);
    if (!m_loading) emit configChanged();
  }
}

void JsonTableDesigner::moveButtonRow(int delta) {
  const int row = m_buttonTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_buttons.size()) return;
  m_buttons.swapItemsAt(row, target);
  fillButtons();  // 按钮表格条目是摘要（真实数据在 m_buttons），整表重建最可靠
  m_buttonTable->selectRow(target);
  if (!m_loading) emit configChanged();
}

/// @brief 表内枚举主表行上移/下移（delta=-1 上移，1 下移）
void JsonTableDesigner::moveEnumRow(int delta) {
  const int row = m_enumTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_enumTable->rowCount()) return;
  syncEnumDrafts();  // 主表/选项表的当前编辑内容先落 draft（切行同款语义）
  m_enums.swapItemsAt(row, target);
  rebuildEnumMaster();  // 按新顺序重建主表（内部 m_enumSyncing 守卫）
  m_enumTable->selectRow(target);  // 触发主从联动：选项表跟随新选中枚举
  if (!m_loading) emit configChanged();
}

void JsonTableDesigner::moveEnumOptRow(int delta) {
  if (m_enumCurRow < 0 || m_enumCurRow >= m_enums.size()) return;
  syncEnumDrafts();  // 选项表的当前编辑内容先落 draft（切行同款语义）
  auto &opts = m_enums[m_enumCurRow].options;
  const int row = m_enumOptTable->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= opts.size()) return;
  opts.swapItemsAt(row, target);
  fillEnumOptions(m_enumCurRow);  // 按新顺序重建选项表（含值类型下拉）
  m_enumOptTable->setCurrentCell(target, 0);
  if (!m_loading) emit configChanged();
}

// ════════════════════════════════════════════════════════════
//  索引表格
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillIndexes(const QVector<JsonTableIndex> &indexes) {
  m_indexTable->setRowCount(0);
  for (const auto &ix : indexes) {
    const int row = m_indexTable->rowCount();
    m_indexTable->insertRow(row);
    m_indexTable->setItem(row, IndexColName, new QTableWidgetItem(ix.name));
    m_indexTable->setItem(row, IndexColCols,
                          new QTableWidgetItem(ix.cols.join(QStringLiteral(","))));
    m_indexTable->setItem(row, IndexColUnique, makeConfigCheckItem(ix.unique));
  }
}

void JsonTableDesigner::appendIndexRow() {
  const int row = m_indexTable->rowCount();
  m_indexTable->insertRow(row);
  m_indexTable->setItem(row, IndexColName, new QTableWidgetItem(QString()));
  m_indexTable->setItem(row, IndexColCols, new QTableWidgetItem(QString()));
  m_indexTable->setItem(row, IndexColUnique, makeConfigCheckItem(false));
  m_indexTable->setCurrentCell(row, IndexColName);
}

QVector<JsonTableIndex> JsonTableDesigner::collectIndexes() const {
  QVector<JsonTableIndex> out;
  for (int r = 0; r < m_indexTable->rowCount(); ++r) {
    JsonTableIndex ix;
    const QTableWidgetItem *nameIt = m_indexTable->item(r, IndexColName);
    ix.name = nameIt ? nameIt->text().trimmed() : QString();
    if (ix.name.isEmpty()) continue;  // 跳过未填索引名的行
    const QTableWidgetItem *colsIt = m_indexTable->item(r, IndexColCols);
    const QString colsStr = colsIt ? colsIt->text() : QString();
    for (const QString &c : colsStr.split(QLatin1Char(','))) {
      const QString col = c.trimmed();
      if (!col.isEmpty()) ix.cols.append(col);
    }
    const QTableWidgetItem *uniqIt = m_indexTable->item(r, IndexColUnique);
    ix.unique = uniqIt && uniqIt->checkState() == Qt::Checked;
    out.append(ix);
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  联表查询声明（joinTables 新格式）/ 列表读模型（selCols）
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillJoins(const QVector<JsonTableJoinSpec> &joins) {
  // 编辑态整体重建（主从联动数据侧）
  m_joinDrafts = joins;
  m_joinTable->setRowCount(0);
  for (const auto &j : joins) appendJoinRow();
  // 逐行回填（appendJoinRow 建的是空行；三列均为 AuiCellEditCombo 常驻控件）
  for (int r = 0; r < joins.size() && r < m_joinTable->rowCount(); ++r) {
    const JsonTableJoinSpec &j = joins[r];
    if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColTable)))
      c->setText(j.table);
    auto *typeCombo = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColType));
    if (typeCombo)
      typeCombo->setText(j.type == QStringLiteral("INNER") ? QStringLiteral("INNER")
                                                           : QStringLiteral("LEFT"));
    if (j.on.size() == 2) {
      if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColOnMain)))
        c->setText(j.on.at(0));
      if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColOnJoin)))
        c->setText(j.on.at(1));
    }
  }
  // 候选刷新（联表表=项目表名；主表列=本表列名；联表列=各行目标表列名）
  refreshJoinCandidates();
  // 选中首行并回填其输出字段（无声明时字段表格清空）
  if (m_joinTable->rowCount() > 0) {
    m_joinTable->selectRow(0);
    m_joinCurRow = 0;
    fillJoinCols(0);
  } else {
    m_joinCurRow = -1;
    m_joinColTable->setRowCount(0);
  }
}

void JsonTableDesigner::appendJoinRow() {
  // 仅建表格行（m_joinDrafts 由调用方管理：addJoinBtn 先 append，fillJoins 整体重建）
  const int row = m_joinTable->rowCount();
  m_joinTable->insertRow(row);
  // 联表表 / 主表列 / 联表列：可编辑常驻下拉（候选由 refreshJoinCandidates 刷新；
  // 与类型列同一控件族——单击整格选中变色、双击进编辑态可输入/拖选复制、
  // 点箭头展开候选；旧 item+委托在双击编辑器下会重影且无法选中复制）
  auto makeEditableCombo = [this, row](int col) {
    auto *c = new AuiCellEditCombo(m_joinTable, col, QStringList(), this);
    c->valueCommitted = [this] {
      if (!m_loading) emit configChanged();
    };
    connect(c->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
      if (!m_loading) emit configChanged();
    });
    m_joinTable->setCellWidget(row, col, c);
    return c;
  };
  auto *tableCombo = makeEditableCombo(JoinColTable);
  // 联表表变化 → 同步该行「联表列」候选（目标表列名随表切换）
  auto refreshTarget = [this, row] {
    if (!m_loading) refreshJoinRowCandidates(row);
  };
  tableCombo->valueCommitted = [this, refreshTarget] {
    refreshTarget();
    if (!m_loading) emit configChanged();
  };
  connect(tableCombo->lineEdit(), &QLineEdit::textEdited, this,
          [this, refreshTarget](const QString &) {
            refreshTarget();
            if (!m_loading) emit configChanged();
          });
  // JOIN 类型：常驻只读下拉（文字可拖选/双击全选/Ctrl+C 复制，点箭头展开候选）
  auto *typeCombo = new AuiCellEditCombo(m_joinTable, JoinColType,
                                         {QStringLiteral("LEFT"), QStringLiteral("INNER")}, this,
                                         true);
  typeCombo->setText(QStringLiteral("LEFT"));
  typeCombo->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  m_joinTable->setCellWidget(row, JoinColType, typeCombo);
  makeEditableCombo(JoinColOnMain);
  makeEditableCombo(JoinColOnJoin);
  m_joinTable->setCurrentCell(row, JoinColTable);
}

void JsonTableDesigner::refreshJoinCandidates() {
  // 候选来源：联表表=项目内表名（跨文件，按当前文件目录搜索）；主表列=本表列名；
  // 联表列=该行目标表列名
  const QString searchDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                 : QFileInfo(m_filePath).absolutePath();
  QStringList tableNames;
  for (const auto &t : listProjectTables(searchDir)) {
    if (!t.tableName.isEmpty()) tableNames.append(t.tableName);
  }
  QStringList mainCols;
  // 主表列候选取字段表格实时列名（含新增未保存的列）
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    const QString n = cellText(r, ColColName);
    if (!n.isEmpty()) mainCols.append(n);
  }
  if (mainCols.isEmpty()) {
    for (const auto &c : m_table.columns) {
      if (!c.name.isEmpty()) mainCols.append(c.name);
    }
  }
  for (int r = 0; r < m_joinTable->rowCount(); ++r) {
    if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColTable)))
      c->setPresets(tableNames);
    if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColOnMain)))
      c->setPresets(mainCols);
    refreshJoinRowCandidates(r);
  }
}

void JsonTableDesigner::refreshJoinRowCandidates(int row) {
  if (row < 0 || row >= m_joinTable->rowCount()) return;
  auto *joinCol = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(row, JoinColOnJoin));
  if (!joinCol) return;
  QString target = cellWidgetText(m_joinTable, row, JoinColTable);
  if (target.isEmpty()) target = cellOf(m_joinTable, row, JoinColTable);
  joinCol->setPresets(joinTargetCandidates(target));
}

QVector<JsonTableJoinSpec> JsonTableDesigner::collectJoins() const {
  // 当前选中行的输出字段表格内容先入 draft（未切行前用户可能刚编辑）
  if (m_joinTable->currentRow() >= 0 && m_joinTable->currentRow() < m_joinDrafts.size())
    syncJoinColsToDraft(m_joinTable->currentRow());
  QVector<JsonTableJoinSpec> out;
  for (int r = 0; r < m_joinTable->rowCount() && r < m_joinDrafts.size(); ++r) {
    JsonTableJoinSpec j = m_joinDrafts.at(r);  // cols（含 label）来自编辑态
    // 表名/ON 两列均为常驻可编辑下拉控件，从控件读值
    j.table = cellWidgetText(m_joinTable, r, JoinColTable);
    auto *typeCombo = dynamic_cast<AuiCellEditCombo *>(m_joinTable->cellWidget(r, JoinColType));
    j.type = typeCombo ? typeCombo->text().trimmed() : QStringLiteral("LEFT");
    if (j.type.isEmpty()) j.type = QStringLiteral("LEFT");
    j.on = {cellWidgetText(m_joinTable, r, JoinColOnMain),
            cellWidgetText(m_joinTable, r, JoinColOnJoin)};
    // 全空行（表/ON/输出列均未填）跳过
    if (j.table.isEmpty() && j.on.value(0).isEmpty() && j.on.value(1).isEmpty() &&
        j.cols.isEmpty()) {
      continue;
    }
    out.append(j);
  }
  return out;
}

void JsonTableDesigner::syncJoinColsToDraft(int row) const {
  if (row < 0 || row >= m_joinDrafts.size()) return;
  m_joinDrafts[row].cols = collectJoinCols();
}

void JsonTableDesigner::fillJoinCols(int row) {
  if (row < 0 || row >= m_joinDrafts.size()) return;
  // 字段名下拉候选 = 被联表列名（目标表名从联表表控件读，缺省回退 draft）
  QString target = cellWidgetText(m_joinTable, row, JoinColTable);
  if (target.isEmpty()) target = m_joinDrafts.at(row).table;
  const QStringList candidates = joinTargetCandidates(target);
  // 重建全程置重建标记：connectChangeSignals 的接线挂在 model 层
  //（dataChanged/rowsRemoved/rowsInserted），表格自身 blockSignals 挡不住；
  // 不挡的话 setRowCount(0) 会同步触发 onChange → updateValidateStatus →
  // collectTable → collectJoins → syncJoinColsToDraft，把此刻的空表格内容写进
  // m_joinDrafts[row].cols，随后重建循环读到空列表 → 输出字段内容丢失
  m_joinColRebuilding = true;
  m_joinColTable->setRowCount(0);
  for (const auto &c : m_joinDrafts.at(row).cols) {
    appendJoinColRow(c.col, c.label);
  }
  // 逐行绑定下拉候选（appendJoinColRow 用的是空候选，此处按目标表补）
  for (int r = 0; r < m_joinColTable->rowCount(); ++r) {
    if (auto *combo = dynamic_cast<AuiCellEditCombo *>(m_joinColTable->cellWidget(r, JColCol)))
      combo->setPresets(candidates);
  }
  m_joinColRebuilding = false;
}

QStringList JsonTableDesigner::joinTargetCandidates(const QString &table) const {
  QStringList candidates;
  if (table.isEmpty()) return candidates;
  for (const auto &t : m_config.tables) {
    if (t.tableName == table) {
      for (const auto &c : t.columns) {
        if (!c.name.isEmpty()) candidates.append(c.name);
      }
      break;
    }
  }
  return candidates;
}

void JsonTableDesigner::appendJoinColRow(const QString &col, const QString &label) {
  const int row = m_joinColTable->rowCount();
  m_joinColTable->insertRow(row);
  // 字段：常驻可编辑下拉（候选由 fillJoinCols 按被联表补齐；可自由输入）
  m_joinColTable->setItem(row, JColCol, new QTableWidgetItem);
  auto *colCombo = new AuiCellEditCombo(m_joinColTable, JColCol, QStringList(), this);
  colCombo->setText(col);
  colCombo->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  connect(colCombo->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
    if (!m_loading) emit configChanged();
  });
  m_joinColTable->setCellWidget(row, JColCol, colCombo);
  // 显示标题：普通文本条目（空 = 回退目标列注释/列名）
  m_joinColTable->setItem(row, JColLabel, new QTableWidgetItem(label));
}

QVector<JsonTableJoinCol> JsonTableDesigner::collectJoinCols() const {
  QVector<JsonTableJoinCol> out;
  for (int r = 0; r < m_joinColTable->rowCount(); ++r) {
    auto *colCombo = dynamic_cast<AuiCellEditCombo *>(m_joinColTable->cellWidget(r, JColCol));
    const QString col = colCombo ? colCombo->text().trimmed() : QString();
    if (col.isEmpty()) continue;  // 跳过字段名为空的行
    JsonTableJoinCol c;
    c.col = col;
    c.label = cellOf(m_joinColTable, r, JColLabel);
    // filter 恒空（只输出）：联表列的查询能力统一由字段呈现页的查询模式
    //（selColsEq/Like 别名通道）驱动，避免双通道重复生成查询条件
    out.append(c);
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  字段呈现（统一表格：三视图双向映射 + ⚙ 字段设置面板）
// ════════════════════════════════════════════════════════════

/// @brief 配置摘要（对齐 jsonvue 配置列：一行式只读展示关键设置）
/// 形如 "取值域数据源 dropurl, 显示:image, 编辑:select, 列宽:120"；
/// 空值按缺省语义显示（displayType 空=text、widget 空=text、未配置不显示该段）；
/// domainType 来自列级取值域（物理列）——⚙ 面板取值域区
static QString viewCfgSummary(const JsonTableSelCol &sc, const JsonTableEditCol &ec,
                              const QString &domainType, bool inForm = true,
                              bool noOutput = false) {
  QStringList parts;
  if (noOutput) {
    parts << QStringLiteral("不输出");
  }
  if (!domainType.isEmpty()) {
    parts << (QStringLiteral("取值域:") + domainType);
  }
  parts << (QStringLiteral("列表样式:") +
            (sc.displayType.isEmpty() ? QStringLiteral("text") : sc.displayType));
  parts << (QStringLiteral("表单控件:") + (ec.widget.isEmpty() ? QStringLiteral("text") : ec.widget));
  // 表单可编辑状态（仅进编辑表单的列显示；联表输出列固定只读展示）
  if (inForm) {
    parts << (ec.editEditable ? QStringLiteral("表单可编辑") : QStringLiteral("表单只读"));
  }
  if (sc.width > 0) {
    parts << (QStringLiteral("列宽:") + QString::number(sc.width));
  }
  return parts.join(QStringLiteral(", "));
}

void JsonTableDesigner::fillViewTable() {
  m_viewTable->setRowCount(0);
  // 联表上下文：引用名 → {目标表名, 主键, 列类型, 列注释}（行填充/⚙面板共用）
  struct JoinCtx {
    QString tableName;
    QString pk;
    QHash<QString, QString> types;
    QHash<QString, QString> comments;
  };
  QHash<QString, JoinCtx> joins;
  for (const auto &j : m_table.joinTable) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne") ||
        j.joinEnClassPro.isEmpty()) {
      continue;
    }
    for (const auto &t : m_config.tables) {
      if (t.modelName != j.joinEnClass) continue;
      JoinCtx ctx;
      ctx.tableName = t.tableName;
      ctx.pk = j.joinPk.isEmpty() ? QStringLiteral("id") : j.joinPk;
      for (const auto &c : t.columns) {
        ctx.types.insert(c.name, c.mysqlType);
        ctx.comments.insert(c.name, c.comment);
      }
      joins.insert(j.joinEnClassPro, ctx);
      break;
    }
  }
  // 新格式联表输出别名：ref（表名_列名）→ 显示标题（联表页配置，单一来源；
  // 空 = 回退目标列注释）。字段呈现的联表行自动跟随此处声明
  QSet<QString> aliasRefs;
  QHash<QString, QString> aliasTitles;   // 别名 → 联表页标题（label；空 = 未配置）
  m_aliasComments.clear();               // 别名 → 目标列注释（标题回退；成员缓存，
                                         // 供联表页显示标题编辑时的实时联动回退查询）
  if (!m_table.joinTables.isEmpty()) {
    const QString joinDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                 : QFileInfo(m_filePath).absolutePath();
    QHash<QString, QHash<QString, QString>> targetComments;  // 表名 → 列注释
    for (const auto &t : listProjectTables(joinDir)) {
      QHash<QString, QString> cm;
      for (const auto &c : t.columns) cm.insert(c.name, c.comment);
      targetComments.insert(t.tableName, cm);
    }
    for (const auto &js : m_table.joinTables) {
      const auto tc = targetComments.constFind(js.table);
      for (const auto &c : js.cols) {
        const QString alias = js.table + QLatin1Char('_') + c.col;
        aliasRefs.insert(alias);
        aliasTitles.insert(alias, c.label);
        if (tc != targetComments.constEnd() && tc.value().contains(c.col)) {
          m_aliasComments.insert(alias, tc.value().value(c.col));
        }
      }
    }
  }
  // 三视图配置索引（ref → 项）
  QHash<QString, JsonTableSelCol> selOf;
  for (const auto &c : m_table.selCols) selOf.insert(c.ref, c);
  QHash<QString, JsonTableQueryCol> queryOf;  // 命中模式（后写覆盖，同 ref 多模式罕见）
  for (const auto &c : m_table.selColsEq) queryOf.insert(c.ref, c);
  for (const auto &c : m_table.selColsLike) queryOf.insert(c.ref, c);
  for (const auto &c : m_table.selColsGap) queryOf.insert(c.ref, c);
  QHash<QString, JsonTableEditCol> editOf;
  for (const auto &e : m_table.editCols) editOf.insert(e.ref, e);
  // i18n 平铺字段（多语言行）：字段名 → 自定义标题（标题回退链与生成端一致）
  QHash<QString, QString> i18nLabels;
  for (const auto &f : m_table.i18n.fields) {
    if (!f.name.isEmpty()) i18nLabels.insert(f.name, f.label);
  }
  // 物理列注释（标题回退用）
  QHash<QString, QString> colComments;
  for (const auto &c : m_table.columns) colComments.insert(c.name, c.comment);
  QHash<QString, QString> colTitles;
  for (const auto &c : m_table.columns) colTitles.insert(c.name, c.title);
  QHash<QString, bool> colHidden;  ///< 列是否「不输出」（实体 json:"-"）
  for (const auto &c : m_table.columns) colHidden.insert(c.name, c.hidden);

  // 模式判定需区分键归属，循环三键填 mode2（ref → {mode, visible}）
  QHash<QString, QPair<QString, bool>> mode2;
  for (const auto &c : m_table.selColsEq) mode2.insert(c.ref, {QStringLiteral("eq"), c.visible});
  for (const auto &c : m_table.selColsLike)
    mode2.insert(c.ref, {QStringLiteral("like"), c.visible});
  for (const auto &c : m_table.selColsGap) mode2.insert(c.ref, {QStringLiteral("gap"), c.visible});

  auto appendRow = [&](const QString &ref, bool isJoinRow, bool isI18nRow) {
    const int row = m_viewTable->rowCount();
    m_viewTable->insertRow(row);
    // 标题（生效值）：i18n 行 = i18n.fields[].label（单一来源：多语言页「显示标题」，
    // 不查 selCols.label——历史固化值已废弃）；新格式联表行 = 联表页标题（单一来源，
    // aliasTitles）> 目标列注释 > 别名；其余行 = selCols.label > 列定义 title
    // > 列注释(本表/联表目标列) > ref
    QString title;
    const auto selIt = selOf.constFind(ref);
    const bool isAliasRow = aliasRefs.contains(ref);
    if (isI18nRow) {
      title = (i18nLabels.contains(ref) && !i18nLabels.value(ref).isEmpty())
                  ? i18nLabels.value(ref)
                  : ref;
    } else if (isAliasRow) {
      // 联表页标题优先（不查 selCols.label——字段呈现的联表行标题不固化）
      title = (aliasTitles.contains(ref) && !aliasTitles.value(ref).isEmpty())
                  ? aliasTitles.value(ref)
                  : QString();
      if (title.isEmpty() && m_aliasComments.contains(ref)) {
        title = m_aliasComments.value(ref);
      }
      if (title.isEmpty()) title = ref;
    } else if (selIt != selOf.constEnd() && !selIt.value().label.isEmpty()) {
      title = selIt.value().label;
    } else if (colTitles.contains(ref) && !colTitles.value(ref).isEmpty()) {
      title = colTitles.value(ref);
    } else if (isJoinRow) {
      const int dot = ref.indexOf(QLatin1Char('.'));
      if (dot > 0) {
        const QString pro = ref.left(dot);
        const QString cn = ref.mid(dot + 1);
        const auto jc = joins.constFind(pro);
        if (jc != joins.constEnd() && jc.value().comments.contains(cn)) {
          title = jc.value().comments.value(cn);
        }
      }
      if (title.isEmpty()) title = ref;
    } else if (colComments.contains(ref) && !colComments.value(ref).isEmpty()) {
      title = colComments.value(ref);
    } else {
      title = ref;
    }
    // 条目存文本（行身份，只读）+ 元数据槽位（⚙ 面板读写，collectViewTable 落三视图）
    auto *refItem = new QTableWidgetItem;
    refItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);  // 行身份标识，只读
    const auto selMeta = selOf.value(ref);
    const auto editMeta = editOf.value(ref);
    refItem->setData(Qt::UserRole + 1, selMeta.width > 0 ? selMeta.width : 0);  // 列宽
    refItem->setData(Qt::UserRole + 2, editMeta.widget);                        // 控件类型
    // 列表展示/上传绑定/缩略图（⚙ 面板读写；collectViewTable 落三视图）
    refItem->setData(Qt::UserRole + 4, selMeta.displayType);  // 展示类型
    refItem->setData(Qt::UserRole + 5,
                     selMeta.listThumbWidth > 0 ? selMeta.listThumbWidth : 0);  // 列表缩略图宽
    refItem->setData(Qt::UserRole + 6,
                     selMeta.listThumbHeight > 0 ? selMeta.listThumbHeight : 0);  // 列表缩略图高
    refItem->setData(Qt::UserRole + 7, editMeta.uploadSourceFile);                // 上传预设文件
    refItem->setData(Qt::UserRole + 8, editMeta.uploadSourceId);                  // 上传预设 id
    refItem->setData(Qt::UserRole + 9,
                     editMeta.editThumbWidth > 0 ? editMeta.editThumbWidth : 0);  // 编辑缩略图宽
    refItem->setData(Qt::UserRole + 10,
                     editMeta.editThumbHeight > 0 ? editMeta.editThumbHeight : 0);  // 编辑缩略图高
    // 列级取值域（物理列；+11..+14 暂存，collectTable 回写 columns）
    QString dType, dFile, dId, dUrl;
    for (const auto &c : m_table.columns) {
      if (c.name == ref) {
        dType = c.domainType;
        dFile = c.domainSourceFile;
        dId = c.domainSourceId;
        dUrl = c.domainUrl;
        break;
      }
    }
    refItem->setData(Qt::UserRole + 11, dType);
    refItem->setData(Qt::UserRole + 12, dFile);
    refItem->setData(Qt::UserRole + 13, dId);
    refItem->setData(Qt::UserRole + 14, dUrl);
    // 编辑表单精细参数（+15..+23；⚙ 面板读写，collectViewTable 落 editCols）
    refItem->setData(Qt::UserRole + 15, editMeta.placeholder);
    refItem->setData(Qt::UserRole + 16, editMeta.maxlength > 0 ? editMeta.maxlength : 0);
    refItem->setData(Qt::UserRole + 17, editMeta.textareaRows > 0 ? editMeta.textareaRows : 0);
    refItem->setData(Qt::UserRole + 18, editMeta.minValue != 0 ? editMeta.minValue : 0);
    refItem->setData(Qt::UserRole + 19, editMeta.maxValue != 0 ? editMeta.maxValue : 0);
    refItem->setData(Qt::UserRole + 20, editMeta.precision > 0 ? editMeta.precision : 0);
    refItem->setData(Qt::UserRole + 21, editMeta.dateFormat);
    refItem->setData(Qt::UserRole + 22, editMeta.formSpan > 0 ? editMeta.formSpan : 0);
    refItem->setData(Qt::UserRole + 23, editMeta.formDefaultValue);
    m_viewTable->setItem(row, UColRef, refItem);
    // 引用列 = 行身份标识（行集由字段表格/联表声明/多语言 fields 派生），
    // 纯文本不可编辑（可选中复制）：改 ref 会让行元数据错位、配置丢失
    refItem->setText(ref);
    // 引用列常驻只读文本框（通用只读单元格控件 AuiCellReadBox）：item 仍是
    // 数据源（元数据/查询模式 +26 等全在条目上），widget 纯显示——无边框透
    // 明贴合、单击聚焦背景变色、拖选/双击选词/Ctrl+C 与可编辑输入框一致
    m_viewTable->setCellWidget(row, UColRef, new AuiCellReadBox(m_viewTable, row, UColRef, ref));
    // i18n 行标题只读（单一来源 = 多语言页「显示标题」）；新格式联表行标题只读
    //（单一来源 = 联表页「显示标题」）——此处改动均不会落盘
    auto *titleItem = new QTableWidgetItem(title);
    const bool titleReadOnly = isI18nRow || isAliasRow;
    if (isI18nRow) {
      titleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      titleItem->setToolTip(QStringLiteral("多语言字段标题在「多语言」页的显示标题列配置"));
    } else if (isAliasRow) {
      titleItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      titleItem->setToolTip(QStringLiteral("联表列标题在「联表」页的显示标题列配置"));
    }
    m_viewTable->setItem(row, UColTitle, titleItem);
    // 只读标题行（i18n / 联表）：与引用列同一通用只读控件——单击整格选中变色、
    // 双击进入可拖选复制的编辑态（只读 item 在 QSS 下无选中高亮，须挂控件）
    if (titleReadOnly) {
      auto *rb = new AuiCellReadBox(m_viewTable, row, UColTitle, title);
      rb->setTip(titleItem->toolTip());
      m_viewTable->setCellWidget(row, UColTitle, rb);
    }
    // 列表勾选
    auto *listItem = new QTableWidgetItem;
    listItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    listItem->setCheckState(selOf.contains(ref) ? Qt::Checked : Qt::Unchecked);
    listItem->setTextAlignment(Qt::AlignCenter);
    m_viewTable->setItem(row, UColList, listItem);
    // 查询模式下拉（不查询/精确/模糊/范围/仅接口；只读，文字可拖选复制）；
    // i18n 行无候选（跨表查询第一版不支持，配置区恒显「不查询」）。
    // 模式值持久化在引用条目 +26（cellWidget 固定在行位置，随条目交换/搬运
    // 才不会在行交换时错位/丢失；下拉文本仅显示用，改值时同步回写）
    auto *modeCombo =
        new AuiCellEditCombo(m_viewTable, UColQuery,
                             isI18nRow ? QStringList() : queryModePresets(), this, true);
    QString mode = QStringLiteral("none");
    bool qVisible = true;
    if (const auto mIt = mode2.constFind(ref); mIt != mode2.constEnd()) {
      mode = mIt.value().first;
      qVisible = mIt.value().second;
    }
    if (!qVisible) mode = QStringLiteral("api");
    modeCombo->setText(queryModeDisplay(mode));
    refItem->setData(Qt::UserRole + 26, mode);
    if (isI18nRow) {
      modeCombo->setToolTip(QStringLiteral("多语言字段不支持查询（跨表查询第一版未支持）"));
    }
    modeCombo->valueCommitted = [this, modeCombo] {
      // 按 widget 实例反查所在行（行交换后行号会变，不能捕获 row）
      for (int i = 0; i < m_viewTable->rowCount(); ++i) {
        if (m_viewTable->cellWidget(i, UColQuery) == modeCombo) {
          if (auto *refIt = m_viewTable->item(i, UColRef))
            refIt->setData(Qt::UserRole + 26, queryModeData(modeCombo->text().trimmed()));
          break;
        }
      }
      if (!m_loading) emit configChanged();
    };
    m_viewTable->setCellWidget(row, UColQuery, modeCombo);
    // 展示 / 写入：未配置 editCols 时缺省 true（全列可写可展示）；
    // 联表行固定只读展示（visible=true, writable=false）；
    // i18n 行两勾均不适用（恒进列表、不进编辑表单）→ 灰显提示
    bool showField = true;
    bool writeField = true;
    if (isJoinRow || isI18nRow) {
      writeField = false;
    } else if (const auto eIt = editOf.constFind(ref); eIt != editOf.constEnd()) {
      showField = eIt.value().visible;
      writeField = eIt.value().writable;
    }
    auto makeCheck = [](bool on) {
      auto *it = new QTableWidgetItem;
      it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
      it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
      it->setTextAlignment(Qt::AlignCenter);
      return it;
    };
    if (isI18nRow) {
      auto *sItem = makeCheck(true);
      sItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      sItem->setToolTip(QStringLiteral("多语言字段恒进列表（生成端按 i18n 配置输出）"));
      m_viewTable->setItem(row, UColVisible, sItem);
      auto *wItem = makeCheck(false);
      wItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      wItem->setToolTip(QStringLiteral("多语言字段不进编辑表单（由 write 页语言 Tab 承接）"));
      m_viewTable->setItem(row, UColWritable, wItem);
    } else if (isJoinRow) {
      // 联表行写入列灰显「—」不可点（与输出列「—」视觉一致）：写入是主表列
      // 能力，联表输出列只读展示、输出内容由「联表」页配置 → 勾选无意义；
      // 原未勾选复选框可点，误点会把联表别名收进 editCols.writable
      m_viewTable->setItem(row, UColVisible, makeCheck(showField));
      auto *wItem = new QTableWidgetItem(QStringLiteral("—"));
      wItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      wItem->setTextAlignment(Qt::AlignCenter);
      wItem->setToolTip(
          QStringLiteral("联表输出列不进写入参数 InUpd（输出内容由「联表」页配置）"));
      m_viewTable->setItem(row, UColWritable, wItem);
    } else {
      m_viewTable->setItem(row, UColVisible, makeCheck(showField));
      m_viewTable->setItem(row, UColWritable, makeCheck(writeField));
    }
    // 输出（实体 json）：联表输出列由 joinTables、i18n 平铺字段由翻译表配置
    // 控制 → 本列不适用 → 灰显「—」
    const bool noOutput = (isJoinRow || isI18nRow) ? false : colHidden.value(ref, false);
    if (isJoinRow || isI18nRow) {
      auto *outItem = new QTableWidgetItem(QStringLiteral("—"));
      outItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      outItem->setTextAlignment(Qt::AlignCenter);
      outItem->setToolTip(isI18nRow ? QStringLiteral("多语言平铺字段：响应内容由翻译表 i18n 配置控制")
                                    : QStringLiteral("联表输出列由「联表」页的输出字段控制"));
      m_viewTable->setItem(row, UColOutput, outItem);
    } else {
      m_viewTable->setItem(row, UColOutput, makeCheck(!noOutput));
    }
    // 配置列（⚙ 与摘要合成一列，对齐 jsonvue；双击单元格打开设置面板）；
    // i18n 行无 ⚙ 面板（不进编辑表单），摘要固定为多语言徽标；联表行加联表
    // 徽标（自绘同色小图标，见 viewBadgeIcon）
    auto *cfgItem = new QTableWidgetItem(
        isI18nRow ? QStringLiteral("⚙ 多语言")
                  : QStringLiteral("⚙ ") +
                        (isJoinRow ? QStringLiteral("联表 · ") : QString()) +
                        viewCfgSummary(selMeta, editMeta, dType, showField, noOutput));
    if (isI18nRow) {
      cfgItem->setIcon(viewBadgeIcon(false));
    } else if (isJoinRow) {
      cfgItem->setIcon(viewBadgeIcon(true));
    }
    cfgItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_viewTable->setItem(row, UColCfg, cfgItem);
    // 编辑可编辑/必填槽位（+24/+25；必填在 ⚙ 字段设置面板编辑，联表/i18n 行
    // 不进编辑表单，固定只读不校验）
    refItem->setData(Qt::UserRole + 24,
                     (isJoinRow || isI18nRow)
                         ? 0
                         : (editOf.contains(ref) ? (editOf.value(ref).editEditable ? 1 : 0) : 1));
    refItem->setData(Qt::UserRole + 25,
                     (isJoinRow || isI18nRow)
                         ? 0
                         : (editOf.contains(ref) ? (editOf.value(ref).required ? 1 : 0) : 0));
    // 列表开关（+26；与编辑控件解耦，联表/i18n 行只读）
    refItem->setData(Qt::UserRole + 26,
                     (isJoinRow || isI18nRow)
                         ? 0
                         : (editOf.contains(ref) ? (editOf.value(ref).switchEditable ? 1 : 0) : 0));
  };

  // 行序 = 上次保存的字段呈现行序 > selCols 顺序 > columns 顺序 > i18n 字段；
  // 未列出的 ref（未勾选列表的物理列/仅查询 ref/旧格式联表路径/输出别名）追加在后
  QStringList orderedRefs;
  auto pushRef = [&](const QString &r) {
    if (r.isEmpty() || orderedRefs.contains(r)) return;
    orderedRefs.append(r);
  };
  const QJsonArray viewOrderArr =
      m_table.extra.value(QString::fromLatin1(JsonTableKey::kViewOrder)).toArray();
  for (const auto &v : viewOrderArr) pushRef(v.toString().trimmed());
  for (const auto &sc : m_table.selCols) pushRef(sc.ref);
  for (const auto &c : m_table.columns) pushRef(c.name);
  for (const auto &f : m_table.i18n.fields) pushRef(f.name);
  // 新格式联表输出别名自动入列（跟随「联表」页的输出字段，不再手动添加）
  for (const QString &alias : aliasRefs) pushRef(alias);
  for (auto it = selOf.constBegin(); it != selOf.constEnd(); ++it) pushRef(it.key());
  for (auto it = queryOf.constBegin(); it != queryOf.constEnd(); ++it) pushRef(it.key());
  for (const QString &ref : orderedRefs) {
    const bool isJoin = ref.contains(QLatin1Char('.')) || aliasRefs.contains(ref);
    const bool isI18n = i18nLabels.contains(ref);
    appendRow(ref, isJoin, isI18n);
  }
}

void JsonTableDesigner::collectViewTable(JsonTableTable *out) const {
  out->selCols.clear();
  out->selColsEq.clear();
  out->selColsLike.clear();
  out->selColsGap.clear();
  out->editCols.clear();
  // i18n 平铺字段行：仅进 selCols（列表展示），不进 editCols / 物理列回写
  const bool hasI18n = !m_table.i18n.fields.isEmpty();
  // 新格式联表输出别名集（联表行标题不固化进 selCols.label，单一来源=联表页）
  QSet<QString> aliasRefs;
  for (const auto &js : m_table.joinTables) {
    for (const auto &c : js.cols) {
      if (!c.col.isEmpty()) aliasRefs.insert(js.table + QLatin1Char('_') + c.col);
    }
  }
  for (int r = 0; r < m_viewTable->rowCount(); ++r) {
    const QString ref = viewRefText(r);
    if (ref.isEmpty()) continue;
    const bool isI18nRef = hasI18n && [&]() {
      for (const auto &f : m_table.i18n.fields) {
        if (f.name == ref) return true;
      }
      return false;
    }();
    const QString title = cellOf(m_viewTable, r, UColTitle);
    QString widthStr;
    QString widget;
    QString displayType;
    int listThumbW = 0;
    int listThumbH = 0;
    QString uploadFile;
    QString uploadId;
    int editThumbW = 0;
    int editThumbH = 0;
    QString domainType;
    QString domainFile;
    QString domainId;
    QString domainUrl;
    QString placeholder;
    int maxlength = 0;
    int textareaRows = 0;
    double minValue = 0;
    double maxValue = 0;
    int precision = 0;
    QString dateFormat;
    int formSpan = 0;
    QString formDefaultValue;
    if (const QTableWidgetItem *refIt = m_viewTable->item(r, UColRef)) {
      widthStr = refIt->data(Qt::UserRole + 1).toString();
      widget = refIt->data(Qt::UserRole + 2).toString();
      displayType = refIt->data(Qt::UserRole + 4).toString();
      listThumbW = refIt->data(Qt::UserRole + 5).toInt();
      listThumbH = refIt->data(Qt::UserRole + 6).toInt();
      uploadFile = refIt->data(Qt::UserRole + 7).toString();
      uploadId = refIt->data(Qt::UserRole + 8).toString();
      editThumbW = refIt->data(Qt::UserRole + 9).toInt();
      editThumbH = refIt->data(Qt::UserRole + 10).toInt();
      domainType = refIt->data(Qt::UserRole + 11).toString();
      domainFile = refIt->data(Qt::UserRole + 12).toString();
      domainId = refIt->data(Qt::UserRole + 13).toString();
      domainUrl = refIt->data(Qt::UserRole + 14).toString();
      placeholder = refIt->data(Qt::UserRole + 15).toString();
      maxlength = refIt->data(Qt::UserRole + 16).toInt();
      textareaRows = refIt->data(Qt::UserRole + 17).toInt();
      minValue = refIt->data(Qt::UserRole + 18).toDouble();
      maxValue = refIt->data(Qt::UserRole + 19).toDouble();
      precision = refIt->data(Qt::UserRole + 20).toInt();
      dateFormat = refIt->data(Qt::UserRole + 21).toString();
      formSpan = refIt->data(Qt::UserRole + 22).toInt();
      formDefaultValue = refIt->data(Qt::UserRole + 23).toString();
    }
    // 列表视图
    const QTableWidgetItem *listIt = m_viewTable->item(r, UColList);
    if (listIt && listIt->checkState() == Qt::Checked) {
      JsonTableSelCol c;
      c.ref = ref;
      // i18n/联表行标题不固化进 selCols.label（单一来源分别在多语言页/联表页；
      // 固化会让 selCols.label 恒覆盖页面配置，标题改不动）
      if (!isI18nRef && !aliasRefs.contains(ref) && !title.isEmpty() && title != ref)
        c.label = title;
      c.width = widthStr.toInt();
      c.displayType = displayType;
      c.listThumbWidth = listThumbW;
      c.listThumbHeight = listThumbH;
      out->selCols.append(c);
    }
    // 查询视图（模式下拉）：模式值持久化在引用条目 +26（cellWidget 固定在行
    // 位置，行交换/搬运后会错位；条目数据才是权威）。+26 空 = 旧会话兼容回退下拉文本
    QString mode;
    if (const QTableWidgetItem *refIt = m_viewTable->item(r, UColRef))
      mode = refIt->data(Qt::UserRole + 26).toString();
    if (mode.isEmpty()) {
      auto *modeCombo = dynamic_cast<AuiCellEditCombo *>(m_viewTable->cellWidget(r, UColQuery));
      mode = modeCombo ? queryModeData(modeCombo->text().trimmed()) : QStringLiteral("none");
    }
    if (mode != QStringLiteral("none")) {
      JsonTableQueryCol qc;
      qc.ref = ref;
      qc.visible = (mode != QStringLiteral("api"));  // 仅接口 = 后端生成、前端不渲染
      if (mode == QStringLiteral("eq"))
        out->selColsEq.append(qc);
      else if (mode == QStringLiteral("like"))
        out->selColsLike.append(qc);
      else if (mode == QStringLiteral("gap"))
        out->selColsGap.append(qc);
    }
    // 展示 / 写入（列表勾选）与可编辑（⚙ 弹窗配置）三者独立：
    //   展示=false → Vue 不渲染；写入=false → 不进 InUpd；可编辑=false → 表单只读
    const QTableWidgetItem *showIt = m_viewTable->item(r, UColVisible);
    const QTableWidgetItem *writeIt = m_viewTable->item(r, UColWritable);
    const bool showField = showIt ? (showIt->checkState() == Qt::Checked) : true;
    const bool writeField = writeIt ? (writeIt->checkState() == Qt::Checked) : true;
    bool editableField = true;
    if (const QTableWidgetItem *eeIt = m_viewTable->item(r, UColRef))
      editableField = eeIt->data(Qt::UserRole + 24).toInt() != 0;
    const bool isJoinRef = ref.contains(QLatin1Char('.'));
    if (isJoinRef) editableField = false;
    // 必填槽位（+25）：原「必填」列已并入 ⚙ 字段设置面板
    bool requiredField = false;
    if (const QTableWidgetItem *refIt = m_viewTable->item(r, UColRef))
      requiredField = refIt->data(Qt::UserRole + 25).toInt() != 0;
    // 列表开关槽位（+26）：列表渲染 0/1 开关，与编辑控件解耦
    bool switchField = false;
    if (const QTableWidgetItem *refIt = m_viewTable->item(r, UColRef))
      switchField = refIt->data(Qt::UserRole + 26).toInt() != 0;
    if (isJoinRef) switchField = false;
    // 方案 B：editCols 覆盖所有字段（默认 visible/writable/editEditable=true），
    // 由「展示/写入」勾选与 ⚙ 配置显式表达每个字段状态；不再用“是否列出”做白名单。
    // i18n 平铺字段行不进 editCols（不进编辑表单，语言 Tab 承接；否则触发
    // 「编辑列不在字段列表中」交叉校验）
    if (!isI18nRef) {
      JsonTableEditCol ec;
      ec.ref = ref;
      ec.visible = showField;
      ec.writable = writeField;
      ec.editEditable = editableField;
      // 编辑标题：唯一来源 = 「字段呈现」的标题列（空 = 生成端回退列注释）
      if (!title.isEmpty() && title != ref) {
        ec.label = title;
      }
      ec.widget = widget;
      ec.uploadSourceFile = uploadFile;
      ec.uploadSourceId = uploadId;
      ec.editThumbWidth = editThumbW;
      ec.editThumbHeight = editThumbH;
      ec.placeholder = placeholder;
      ec.maxlength = maxlength;
      ec.textareaRows = textareaRows;
      ec.minValue = minValue;
      ec.maxValue = maxValue;
      ec.precision = precision;
      ec.dateFormat = dateFormat;
      ec.formSpan = formSpan;
      ec.formDefaultValue = formDefaultValue;
      ec.required = requiredField;
      ec.switchEditable = switchField;
      out->editCols.append(ec);
    }
    // 列级取值域暂存 → 回写物理列定义（联表/i18n 行无域；域随列存储）
    if (!ref.contains(QLatin1Char('.')) && !isI18nRef && !domainType.isEmpty()) {
      for (auto &c : out->columns) {
        if (c.name == ref) {
          c.domainType = domainType;
          c.domainSourceFile = domainFile;
          c.domainSourceId = domainId;
          c.domainUrl = domainUrl;
          break;
        }
      }
    }
    // 列级标题 → 回写物理列定义（三视图共用一个标题；与注释/引用相同的值
    // 视为未覆写不落键，避免批量固化注释导致后续注释修改无法传导）
    if (!ref.contains(QLatin1Char('.')) && !isI18nRef) {
      const QTableWidgetItem *outIt = m_viewTable->item(r, UColOutput);
      const bool noOutput = outIt && outIt->flags().testFlag(Qt::ItemIsUserCheckable) &&
                            outIt->checkState() != Qt::Checked;
      for (auto &c : out->columns) {
        if (c.name == ref) {
          c.title = (title.isEmpty() || title == ref || title == c.comment) ? QString() : title;
          c.hidden = noOutput;
          break;
        }
      }
    }
  }
}

void JsonTableDesigner::openViewFieldConfig(int row) {
  if (row < 0 || row >= m_viewTable->rowCount()) return;
  const QString ref = viewRefText(row);
  if (ref.isEmpty()) return;
  // i18n 平铺字段行：无 ⚙ 面板（不进编辑表单，摘要列固定徽标）
  for (const auto &f : m_table.i18n.fields) {
    if (f.name == ref) return;
  }
  // 联表行 = 旧格式 pro.col 或 新格式输出别名（编辑表单区整区折叠：别名只读展示）
  bool isJoinRow = ref.contains(QLatin1Char('.'));
  if (!isJoinRow) {
    for (const auto &js : m_table.joinTables) {
      for (const auto &c : js.cols) {
        if (ref == js.table + QLatin1Char('_') + c.col) {
          isJoinRow = true;
          break;
        }
      }
      if (isJoinRow) break;
    }
  }
  QTableWidgetItem *refIt = m_viewTable->item(row, UColRef);
  if (!refIt) return;

  // 行槽位 → 双视图结构（列表侧 sc / 编辑侧 ec）
  JsonTableSelCol sc;
  sc.ref = ref;
  sc.width = refIt->data(Qt::UserRole + 1).toInt();
  sc.displayType = refIt->data(Qt::UserRole + 4).toString();
  sc.listThumbWidth = refIt->data(Qt::UserRole + 5).toInt();
  sc.listThumbHeight = refIt->data(Qt::UserRole + 6).toInt();
  JsonTableEditCol ec;
  ec.widget = refIt->data(Qt::UserRole + 2).toString();
  ec.editEditable = refIt->data(Qt::UserRole + 24).toInt() != 0;
  ec.required = refIt->data(Qt::UserRole + 25).toInt() != 0;
  ec.switchEditable = refIt->data(Qt::UserRole + 26).toInt() != 0;
  ec.uploadSourceFile = refIt->data(Qt::UserRole + 7).toString();
  ec.uploadSourceId = refIt->data(Qt::UserRole + 8).toString();
  ec.editThumbWidth = refIt->data(Qt::UserRole + 9).toInt();
  ec.editThumbHeight = refIt->data(Qt::UserRole + 10).toInt();
  ec.placeholder = refIt->data(Qt::UserRole + 15).toString();
  ec.maxlength = refIt->data(Qt::UserRole + 16).toInt();
  ec.textareaRows = refIt->data(Qt::UserRole + 17).toInt();
  ec.minValue = refIt->data(Qt::UserRole + 18).toDouble();
  ec.maxValue = refIt->data(Qt::UserRole + 19).toDouble();
  ec.precision = refIt->data(Qt::UserRole + 20).toInt();
  ec.dateFormat = refIt->data(Qt::UserRole + 21).toString();
  ec.formSpan = refIt->data(Qt::UserRole + 22).toInt();
  ec.formDefaultValue = refIt->data(Qt::UserRole + 23).toString();

  ViewFieldConfigDialog dlg(ref, isJoinRow, this);
  dlg.setSearchRoot(m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).absolutePath());
  dlg.setCurrent(sc, ec);
  if (!isJoinRow) {
    // 列级取值域初值（+11..+14 暂存槽位）
    JsonTableColumn dc;
    dc.domainType = refIt->data(Qt::UserRole + 11).toString();
    dc.domainSourceFile = refIt->data(Qt::UserRole + 12).toString();
    dc.domainSourceId = refIt->data(Qt::UserRole + 13).toString();
    dc.domainUrl = refIt->data(Qt::UserRole + 14).toString();
    dlg.setDomain(dc);
  }
  if (dlg.exec() != QDialog::Accepted) return;
  // 确定后写回行槽位（collectViewTable 读取落三视图/列定义），并刷新配置摘要
  dlg.fillListResult(&sc);
  dlg.fillEditResult(&ec);
  refIt->setData(Qt::UserRole + 1, sc.width);
  refIt->setData(Qt::UserRole + 2, ec.widget);
  refIt->setData(Qt::UserRole + 4, sc.displayType);
  refIt->setData(Qt::UserRole + 5, sc.listThumbWidth);
  refIt->setData(Qt::UserRole + 6, sc.listThumbHeight);
  refIt->setData(Qt::UserRole + 7, ec.uploadSourceFile);
  refIt->setData(Qt::UserRole + 8, ec.uploadSourceId);
  refIt->setData(Qt::UserRole + 9, ec.editThumbWidth);
  refIt->setData(Qt::UserRole + 10, ec.editThumbHeight);
  refIt->setData(Qt::UserRole + 15, ec.placeholder);
  refIt->setData(Qt::UserRole + 16, ec.maxlength);
  refIt->setData(Qt::UserRole + 17, ec.textareaRows);
  refIt->setData(Qt::UserRole + 18, ec.minValue);
  refIt->setData(Qt::UserRole + 19, ec.maxValue);
  refIt->setData(Qt::UserRole + 20, ec.precision);
  refIt->setData(Qt::UserRole + 21, ec.dateFormat);
  refIt->setData(Qt::UserRole + 22, ec.formSpan);
  refIt->setData(Qt::UserRole + 23, ec.formDefaultValue);
  refIt->setData(Qt::UserRole + 24, ec.editEditable ? 1 : 0);
  refIt->setData(Qt::UserRole + 25, ec.required ? 1 : 0);
  refIt->setData(Qt::UserRole + 26, ec.switchEditable ? 1 : 0);
  refIt->setData(Qt::UserRole + 11, dlg.domainType());
  refIt->setData(Qt::UserRole + 12, dlg.domainSourceFile());
  refIt->setData(Qt::UserRole + 13, dlg.domainSourceId());
  refIt->setData(Qt::UserRole + 14, dlg.domainUrl());
  // 刷新配置摘要（含表单可编辑状态；inForm 取当前「表单展示」勾选）
  refreshViewCfgSummary(row);
}

/// @brief 按行槽位与「表单展示」勾选重建配置摘要（⚙ 列）
void JsonTableDesigner::refreshViewCfgSummary(int row) {
  if (row < 0 || row >= m_viewTable->rowCount()) return;
  auto *cfgIt = m_viewTable->item(row, UColCfg);
  auto *refIt = m_viewTable->item(row, UColRef);
  if (!cfgIt || !refIt) return;
  // i18n 行摘要恒为多语言徽标（无 ⚙ 面板）；任何刷新路径不得覆盖成普通摘要
  const QString ref = viewRefText(row);
  for (const auto &f : m_table.i18n.fields) {
    if (f.name == ref) {
      cfgIt->setText(QStringLiteral("⚙ 多语言"));
      cfgIt->setIcon(viewBadgeIcon(false));
      return;
    }
  }
  JsonTableSelCol sc;
  JsonTableEditCol ec;
  sc.width = refIt->data(Qt::UserRole + 1).toInt();
  sc.displayType = refIt->data(Qt::UserRole + 4).toString();
  ec.widget = refIt->data(Qt::UserRole + 2).toString();
  ec.editEditable = refIt->data(Qt::UserRole + 24).toInt() != 0;
  const QString domainType = refIt->data(Qt::UserRole + 11).toString();
  const QTableWidgetItem *showIt = m_viewTable->item(row, UColVisible);
  const bool inForm = showIt && showIt->checkState() != Qt::Unchecked;
  // 输出勾：不勾 = 实体 json:"-"（联表行的「—」不可勾选，按正常输出处理）
  const QTableWidgetItem *outIt = m_viewTable->item(row, UColOutput);
  const bool noOutput = outIt && outIt->flags().testFlag(Qt::ItemIsUserCheckable) &&
                        outIt->checkState() != Qt::Checked;
  // 联表行摘要加「联表」徽标（与 appendRow 首次填充保持一致；i18n 行
  // 摘要固定「⚙ 多语言」且 ⚙ 面板不可开，不会走到本函数）。联表行判定 =
  // 输出格灰显「—」（无 UserCheckable），覆盖旧格式 pro.col 与新格式别名两类
  QString prefix;
  if (outIt && !outIt->flags().testFlag(Qt::ItemIsUserCheckable)) {
    prefix = QStringLiteral("联表 · ");
  }
  cfgIt->setText(QStringLiteral("⚙ ") + prefix +
                 viewCfgSummary(sc, ec, domainType, inForm, noOutput));
  cfgIt->setIcon(prefix.isEmpty() ? QIcon() : viewBadgeIcon(true));
}
// ════════════════════════════════════════════════════════════
//  表级进阶键：查询配置 / 排序 / 表内枚举 / i18n
// ════════════════════════════════════════════════════════════

QStringList JsonTableDesigner::splitCsv(const QString &text) {
  QStringList out;
  for (const QString &part : text.split(QLatin1Char(','))) {
    const QString item = part.trimmed();
    if (!item.isEmpty()) out.append(item);
  }
  return out;
}

QString JsonTableDesigner::joinCsv(const QStringList &list) {
  return list.join(QStringLiteral(","));
}

void JsonTableDesigner::updateAppOutBtn() {
  if (!m_appOutBtn) return;
  const bool appOn = m_appQueryCombo &&
                     m_appQueryCombo->currentData().toString() != QStringLiteral("none");
  m_appOutBtn->setEnabled(appOn);
  if (!appOn) {
    m_appOutBtn->setText(QStringLiteral("C端出参: 不适用"));
    return;
  }
  m_appOutBtn->setText(m_appOutCols.isEmpty()
                           ? QStringLiteral("C端出参: 全部")
                           : QStringLiteral("C端出参: %1列").arg(m_appOutCols.size()));
  const QString current = m_appOutCols.isEmpty()
                              ? QStringLiteral("未选择（返回完整实体）")
                              : m_appOutCols.join(QStringLiteral(", "));
  m_appOutBtn->setToolTip(QStringLiteral(
      "C 端出参白名单：勾选后生成 out_<表>_.go，只输出所选列\n"
      "支持主表列、联表输出列（表名_列名）、多语言字段\n"
      "留空 = 直接返回完整实体（「输出」未勾选的列始终不出现）\n"
      "当前：%1").arg(current));
}

void JsonTableDesigner::editAppOutCols() {
  QDialog dlg(this);
  dlg.resize(560, 460);
  auto frame = beginConfigDialog(
      &dlg, QStringLiteral("C端出参白名单 - %1").arg(m_nameEdit->text().trimmed()));

  auto *tip = new QLabel(
      QStringLiteral("勾选 C 端接口（App / 小程序等客户端）允许输出的列：主表列、"
                     "联表输出列（表名_列名）、多语言字段均可勾选；"
                     "留空 = 返回完整实体。标记「不输出」的主表列不在此列出。"),
      &dlg);
  tip->setWordWrap(true);
  frame.contentLayout->addWidget(tip);

  auto *toolRow = new QHBoxLayout;
  auto *allBtn = makeCompactButton(QStringLiteral("全选"), &dlg);
  auto *noneBtn = makeCompactButton(QStringLiteral("全不选"), &dlg);
  auto *summary = new QLabel(&dlg);
  toolRow->addWidget(allBtn);
  toolRow->addWidget(noneBtn);
  toolRow->addStretch();
  toolRow->addWidget(summary);
  frame.contentLayout->addLayout(toolRow);

  // 统一字段配置表格：隐藏行号、纯勾选单元格居中、表头不可点选
  auto *table = makeConfigTable(
      {{QStringLiteral("选"), QHeaderView::ResizeToContents, 0},
       {QStringLiteral("列名"), QHeaderView::ResizeToContents, 0},
       {QStringLiteral("注释"), QHeaderView::Stretch, 0}},
      &dlg, 200);
  // 允许选中（只读列挂 AuiCellReadBox：单击整格选中变色、双击可拖选复制）；
  // 表格本身禁止编辑，勾选列点击切换不受影响
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);

  // 勾选行（列名 + 灰显注释；legacyChecked = 旧配置迁移预勾；badge = 注释列
  // 类型图标，联表链条 / 多语言地球同色）。联表/多语言行在注释列行内标记类型
  // （不对齐分组标题行，保持单一列表心智）
  auto appendCheckRow = [this, table](const QString &name, const QString &comment,
                                      bool legacyChecked, const QIcon &badge) {
    const int row = table->rowCount();
    table->insertRow(row);
    auto *check = new QTableWidgetItem;
    check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    check->setCheckState(m_appOutCols.contains(name) || legacyChecked ? Qt::Checked
                                                                      : Qt::Unchecked);
    table->setItem(row, 0, check);
    auto *nameItem = new QTableWidgetItem(name);
    nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, 1, nameItem);
    // 列名为只读文本：挂只读控件（单击整格选中变色、双击可拖选复制）
    table->setCellWidget(row, 1, new AuiCellReadBox(table, row, 1, name));
    auto *commentItem = new QTableWidgetItem(comment);
    commentItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    commentItem->setForeground(QBrush(QColor(0x80, 0x80, 0x80)));
    commentItem->setIcon(badge);  // 空图标 = 主表列（无类型徽标）
    table->setItem(row, 2, commentItem);
  };

  // 按字段呈现行序构建白名单列表（用户在字段呈现页调整的行序 = 保存后的
  // appOutCols 顺序 = 生成端出参 JSON 字段顺序，三者一致）；行类型按 ref
  // 命中集合判定：主表列 / 联表输出别名 / i18n 平铺字段
  QSet<QString> hiddenCols;  // 主表行「输出」未勾：白名单不列出（生成侧也会过滤）
  for (int r = 0; r < m_viewTable->rowCount(); ++r) {
    const QTableWidgetItem *outIt = m_viewTable->item(r, UColOutput);
    if (outIt && outIt->flags().testFlag(Qt::ItemIsUserCheckable) &&
        outIt->checkState() != Qt::Checked) {
      hiddenCols.insert(viewRefText(r));
    }
  }
  QSet<QString> colNames;  // 主表列名（字段页）
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    const QString n = cellText(r, ColColName);
    if (!n.isEmpty()) colNames.insert(n);
  }
  QHash<QString, QString> aliasCmt;  // 联表别名 → 标记注释（标题链：联表页标题 >
                                     // 目标列注释缓存 > 别名）
  for (const auto &js : m_joinDrafts) {
    for (const auto &jc : js.cols) {
      if (jc.col.isEmpty() || js.table.isEmpty()) continue;
      const QString alias = js.table + QLatin1Char('_') + jc.col;
      QString cmt = jc.label;
      if (cmt.isEmpty()) cmt = m_aliasComments.value(alias);
      if (cmt.isEmpty()) cmt = alias;
      aliasCmt.insert(alias, QStringLiteral("联表 · %1 · %2").arg(js.table, cmt));
    }
  }
  // i18n 字段名在多语言页第 0 列的下拉控件里（item 文本为空），不能直接读
  // item；取控件文本，回退 item 文本
  auto i18nFieldName = [this](int row) -> QString {
    if (auto *combo = dynamic_cast<AuiCellEditCombo *>(m_i18nFieldTable->cellWidget(row, 0))) {
      return combo->text().trimmed();
    }
    return cellOf(m_i18nFieldTable, row, 0);
  };
  QHash<QString, QString> i18nCmt;  // i18n 字段名 → 标记注释
  for (int r = 0; r < m_i18nFieldTable->rowCount(); ++r) {
    const QString n = i18nFieldName(r);
    if (n.isEmpty()) continue;
    i18nCmt.insert(n, QStringLiteral("多语言 · %1").arg(cellOf(m_i18nFieldTable, r, 1)));
  }
  // i18n 旧配置迁移：白名单非空但不含任何 i18n 字段名 = 旧配置（生成端自动
  // 全量追加）→ 打开时预勾，确定后落盘完成迁移
  bool i18nLegacyAll = !m_appOutCols.isEmpty();
  for (int r = 0; r < m_i18nFieldTable->rowCount() && i18nLegacyAll; ++r) {
    const QString n = i18nFieldName(r);
    if (!n.isEmpty() && m_appOutCols.contains(n)) i18nLegacyAll = false;
  }
  for (int r = 0; r < m_viewTable->rowCount(); ++r) {
    const QString ref = viewRefText(r);
    if (ref.isEmpty()) continue;
    if (colNames.contains(ref)) {
      if (hiddenCols.contains(ref)) continue;
      // 注释列显示字段呈现页的标题（注意不能用 cellText——它固定读字段
      // 表格，会取到 MySQL 类型列的 int/varchar 之类）
      appendCheckRow(ref, cellOf(m_viewTable, r, UColTitle), false, QIcon());
    } else if (aliasCmt.contains(ref)) {
      appendCheckRow(ref, aliasCmt.value(ref), false, viewBadgeIcon(true));
    } else if (i18nCmt.contains(ref)) {
      appendCheckRow(ref, i18nCmt.value(ref), i18nLegacyAll, viewBadgeIcon(false));
    }
  }
  frame.contentLayout->addWidget(table, 1);

  auto *warn = new QLabel(&dlg);
  warn->setWordWrap(true);
  frame.contentLayout->addWidget(warn);

  const auto refreshSummary = [table, summary, warn]() {
    QStringList selected;
    for (int r = 0; r < table->rowCount(); ++r) {
      const QTableWidgetItem *check = table->item(r, 0);
      const QTableWidgetItem *name = table->item(r, 1);
      if (check && name && check->checkState() == Qt::Checked) selected << name->text();
    }
    if (selected.isEmpty()) {
      summary->setText(QStringLiteral("已选 0 列"));
      warn->setText(
          QStringLiteral("⚠ 未选择：C 端接口将返回完整实体（coin/up1 等会全部输出）。"));
      warn->setStyleSheet(QStringLiteral("color:#b26a00;"));
    } else {
      summary->setText(QStringLiteral("已选 %1 列").arg(selected.size()));
      QString shown = selected.mid(0, 8).join(QStringLiteral(", "));
      if (selected.size() > 8) shown += QStringLiteral(" …");
      warn->setText(QStringLiteral("输出列：%1").arg(shown));
      warn->setStyleSheet(QString());
    }
  };
  connect(table, &QTableWidget::itemChanged, &dlg,
          [refreshSummary](QTableWidgetItem *) { refreshSummary(); });
  connect(allBtn, &QPushButton::clicked, &dlg, [table]() {
    for (int r = 0; r < table->rowCount(); ++r) {
      if (auto *it = table->item(r, 0)) it->setCheckState(Qt::Checked);
    }
  });
  connect(noneBtn, &QPushButton::clicked, &dlg, [table]() {
    for (int r = 0; r < table->rowCount(); ++r) {
      if (auto *it = table->item(r, 0)) it->setCheckState(Qt::Unchecked);
    }
  });
  refreshSummary();

  // 框架封装：无边框 + 自定义标题栏 + 统一样式确定/取消
  finishConfigDialog(&dlg, frame);

  if (dlg.exec() != QDialog::Accepted) return;
  QStringList selected;
  for (int r = 0; r < table->rowCount(); ++r) {
    const QTableWidgetItem *check = table->item(r, 0);
    const QTableWidgetItem *name = table->item(r, 1);
    if (check && name && check->checkState() == Qt::Checked) selected << name->text();
  }
  m_appOutCols = selected;
  updateAppOutBtn();
  emit configChanged();
}

void JsonTableDesigner::fillSortTable(const QVector<QPair<QString, QString>> &sort) {
  m_sortTable->setRowCount(0);
  for (const auto &p : sort) appendSortRow(p.first, p.second);
}

void JsonTableDesigner::appendSortRow(const QString &col, const QString &dir) {
  const int row = m_sortTable->rowCount();
  m_sortTable->insertRow(row);
  // 排序列：常驻可编辑下拉（候选 = 字段列 + 联表 ref，可自由输入）
  m_sortTable->setItem(row, 0, new QTableWidgetItem);
  auto *refCombo = new AuiCellEditCombo(m_sortTable, 0, m_sortColOptions, this);
  refCombo->setText(col);
  refCombo->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  connect(refCombo->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
    if (!m_loading) emit configChanged();
  });
  m_sortTable->setCellWidget(row, 0, refCombo);
  // 方向：常驻只读下拉（ASC/DESC，文字可拖选复制，点箭头展开候选）
  auto *dirCombo = new AuiCellEditCombo(m_sortTable, 1,
                                        {QStringLiteral("ASC"), QStringLiteral("DESC")}, this,
                                        true);
  dirCombo->setText(dir.isEmpty() ? QStringLiteral("DESC") : dir);
  dirCombo->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  m_sortTable->setCellWidget(row, 1, dirCombo);
}

QVector<QPair<QString, QString>> JsonTableDesigner::collectSort() const {
  QVector<QPair<QString, QString>> out;
  for (int r = 0; r < m_sortTable->rowCount(); ++r) {
    auto *refCombo = dynamic_cast<AuiCellEditCombo *>(m_sortTable->cellWidget(r, 0));
    const QString col = refCombo ? refCombo->text().trimmed() : QString();
    if (col.isEmpty()) continue;  // 跳过排序列为空的行
    auto *dirCombo = dynamic_cast<AuiCellEditCombo *>(m_sortTable->cellWidget(r, 1));
    QString dir = dirCombo ? dirCombo->text().trimmed() : QString();
    if (dir.isEmpty()) dir = QStringLiteral("DESC");
    out.append({col, dir});
  }
  return out;
}

void JsonTableDesigner::appendI18nFieldRow(const QString &name, const QString &label) {
  const int row = m_i18nFieldTable->rowCount();
  m_i18nFieldTable->insertRow(row);
  // 字段：常驻可编辑下拉（候选 = 翻译表列名，随翻译表名实时切换；可自由输入）
  m_i18nFieldTable->setItem(row, 0, new QTableWidgetItem);
  auto *nameCombo = new AuiCellEditCombo(m_i18nFieldTable, 0, m_i18nFieldOptions, this);
  nameCombo->setText(name);
  nameCombo->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  connect(nameCombo->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
    if (!m_loading) emit configChanged();
  });
  m_i18nFieldTable->setCellWidget(row, 0, nameCombo);
  // 显示标题：普通文本条目（空 = 回退链：标题映射 → 翻译表列注释 → 字段名）
  auto *labelItem = new QTableWidgetItem(label);
  m_i18nFieldTable->setItem(row, 1, labelItem);
}

/// @brief 读取字段呈现表某行的字段引用（优先下拉当前文本）
QString JsonTableDesigner::viewRefText(int row) const {
  if (row < 0 || row >= m_viewTable->rowCount()) return QString();
  if (auto *cell = dynamic_cast<AuiCellEditCombo *>(m_viewTable->cellWidget(row, UColRef)))
    return cell->text().trimmed();
  return cellOf(m_viewTable, row, UColRef);
}

/// @brief 刷新全部排序行的候选列表（字段表格列名变化时调用，保留已填值）
void JsonTableDesigner::setSortOptions(const QStringList &opts) {
  m_sortColOptions = opts;
  for (int r = 0; r < m_sortTable->rowCount(); ++r) {
    if (auto *cell = dynamic_cast<AuiCellEditCombo *>(m_sortTable->cellWidget(r, 0)))
      cell->setPresets(opts);
  }
}

// ════════════════════════════════════════════════════════════
//  操作按钮（buttons：键名与 .jsonvue ButtonConfig 一致）
// ════════════════════════════════════════════════════════════

void JsonTableDesigner::fillButtons() {
  m_buttonTable->setRowCount(0);
  for (int i = 0; i < m_buttons.size(); ++i) {
    appendButtonRow(i);
  }
}

void JsonTableDesigner::appendButtonRow(int idx) {
  if (idx < 0 || idx >= m_buttons.size()) return;
  const ButtonConfig &b = m_buttons[idx];
  const int row = m_buttonTable->rowCount();
  m_buttonTable->insertRow(row);
  m_buttonTable->setItem(row, 0, new QTableWidgetItem(b.label));
  m_buttonTable->setItem(
      row, 1,
      new QTableWidgetItem(b.position == ButtonPosition::Toolbar ? QStringLiteral("工具栏")
                                                                 : QStringLiteral("行操作")));
  QString action;
  switch (b.actionType) {
    case ButtonActionType::Ajax:
      action = QStringLiteral("调 API");
      break;
    case ButtonActionType::Confirm:
      action = QStringLiteral("二次确认");
      break;
    case ButtonActionType::Dialog:
      action = QStringLiteral("对话框");
      break;
    case ButtonActionType::Link:
      action = QStringLiteral("跳转");
      break;
  }
  m_buttonTable->setItem(row, 2, new QTableWidgetItem(action));
  m_buttonTable->setItem(row, 3, new QTableWidgetItem(b.actionKey));
}

void JsonTableDesigner::onAddButton() {
  ButtonConfigDialog dlg(this);
  dlg.setSearchRoot(m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).absolutePath());
  if (dlg.exec() != QDialog::Accepted) return;
  m_buttons.append(dlg.getData());
  appendButtonRow(m_buttons.size() - 1);
}

void JsonTableDesigner::onEditButton(int row) {
  if (row < 0 || row >= m_buttons.size()) return;
  ButtonConfigDialog dlg(m_buttons[row], this);
  dlg.setSearchRoot(m_filePath.isEmpty() ? QString() : QFileInfo(m_filePath).absolutePath());
  if (dlg.exec() != QDialog::Accepted) return;
  m_buttons[row] = dlg.getData();
  // 摘要刷新：重建该行文本（行数不变）
  const ButtonConfig &b = m_buttons[row];
  m_buttonTable->item(row, 0)->setText(b.label);
  m_buttonTable->item(row, 1)->setText(
      b.position == ButtonPosition::Toolbar ? QStringLiteral("工具栏") : QStringLiteral("行操作"));
  QString action;
  switch (b.actionType) {
    case ButtonActionType::Ajax:
      action = QStringLiteral("调 API");
      break;
    case ButtonActionType::Confirm:
      action = QStringLiteral("二次确认");
      break;
    case ButtonActionType::Dialog:
      action = QStringLiteral("对话框");
      break;
    case ButtonActionType::Link:
      action = QStringLiteral("跳转");
      break;
  }
  m_buttonTable->item(row, 2)->setText(action);
  m_buttonTable->item(row, 3)->setText(b.actionKey);
}

void JsonTableDesigner::onRemoveButton() {
  int row = m_buttonTable->currentRow();
  if (row < 0 || row >= m_buttons.size()) return;
  m_buttonTable->removeRow(row);
  m_buttons.removeAt(row);
}

void JsonTableDesigner::syncEnumDrafts() {
  // 主表编辑内容 → m_enums（枚举列从常驻下拉控件读，说明读表格条目）
  for (int r = 0; r < m_enumTable->rowCount() && r < m_enums.size(); ++r) {
    m_enums[r].column = cellWidgetText(m_enumTable, r, 0);
    const QTableWidgetItem *cmtIt = m_enumTable->item(r, 1);
    if (cmtIt) m_enums[r].comment = cmtIt->text().trimmed();
  }
  // 选项表格 → 当前行
  if (m_enumCurRow >= 0 && m_enumCurRow < m_enums.size()) {
    m_enums[m_enumCurRow].options = collectEnumOptions();
  }
}

void JsonTableDesigner::rebuildEnumMaster() {
  m_enumSyncing = true;
  m_enumTable->setRowCount(0);
  for (const auto &e : m_enums) {
    const int row = m_enumTable->rowCount();
    m_enumTable->insertRow(row);
    // 枚举列：常驻可编辑下拉（候选=字段列名，与联表页同一控件族——单击整格
    // 选中变色、双击进编辑态可输入/拖选复制、点箭头展开候选；旧 item+委托
    // 在双击编辑器下会重影且无法选中复制）
    auto *colCombo = new AuiCellEditCombo(m_enumTable, 0, QStringList(), this);
    colCombo->setText(e.column);
    colCombo->valueCommitted = [this] {
      if (!m_loading) emit configChanged();
    };
    connect(colCombo->lineEdit(), &QLineEdit::textEdited, this, [this](const QString &) {
      if (!m_loading) emit configChanged();
    });
    m_enumTable->setCellWidget(row, 0, colCombo);
    m_enumTable->setItem(row, 1, new QTableWidgetItem(e.comment));
  }
  m_enumSyncing = false;
  // 候选 = 字段表格列名
  QStringList names;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    const QString n = cellText(r, ColColName);
    if (!n.isEmpty()) names.append(n);
  }
  for (int r = 0; r < m_enumTable->rowCount(); ++r) {
    if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_enumTable->cellWidget(r, 0)))
      c->setPresets(names);
  }
}

void JsonTableDesigner::fillEnumOptions(int row) {
  m_enumSyncing = true;
  m_enumCurRow = (row >= 0 && row < m_enums.size()) ? row : -1;
  m_enumOptTable->setRowCount(0);
  if (m_enumCurRow >= 0) {
    for (const auto &o : m_enums[m_enumCurRow].options) {
      const int r = m_enumOptTable->rowCount();
      m_enumOptTable->insertRow(r);
      m_enumOptTable->setItem(r, 0, new QTableWidgetItem(o.key));
      m_enumOptTable->setItem(r, 1, new QTableWidgetItem(o.label));
      m_enumOptTable->setItem(r, 2, new QTableWidgetItem(o.value));
      // 值类型：常驻只读下拉（string/number，文字可拖选复制，点箭头展开候选）
      auto *typeCombo = new AuiCellEditCombo(m_enumOptTable, 3,
                                             {QStringLiteral("string"), QStringLiteral("number")},
                                             this, true);
      typeCombo->setText(o.valueType.isEmpty() ? QStringLiteral("string") : o.valueType);
      typeCombo->valueCommitted = [this] {
        if (!m_loading) emit configChanged();
      };
      m_enumOptTable->setCellWidget(r, 3, typeCombo);
    }
  }
  m_enumSyncing = false;
}

void JsonTableDesigner::appendEnumOptRow() {
  const int row = m_enumOptTable->rowCount();
  m_enumOptTable->insertRow(row);
  m_enumOptTable->setItem(row, 0, new QTableWidgetItem(QString()));
  m_enumOptTable->setItem(row, 1, new QTableWidgetItem(QString()));
  m_enumOptTable->setItem(row, 2, new QTableWidgetItem(QString()));
  // 值类型：常驻只读下拉（string/number，文字可拖选复制，点箭头展开候选）
  auto *typeCombo = new AuiCellEditCombo(m_enumOptTable, 3,
                                         {QStringLiteral("string"), QStringLiteral("number")},
                                         this, true);
  typeCombo->setText(QStringLiteral("string"));
  typeCombo->valueCommitted = [this] {
    if (!m_loading) emit configChanged();
  };
  m_enumOptTable->setCellWidget(row, 3, typeCombo);
  m_enumOptTable->setCurrentCell(row, 0);
}

QVector<JsonTableEnumOption> JsonTableDesigner::collectEnumOptions() const {
  QVector<JsonTableEnumOption> out;
  for (int r = 0; r < m_enumOptTable->rowCount(); ++r) {
    JsonTableEnumOption o;
    const QTableWidgetItem *keyIt = m_enumOptTable->item(r, 0);
    const QTableWidgetItem *labelIt = m_enumOptTable->item(r, 1);
    const QTableWidgetItem *valIt = m_enumOptTable->item(r, 2);
    o.key = keyIt ? keyIt->text().trimmed() : QString();
    o.label = labelIt ? labelIt->text().trimmed() : QString();
    o.value = valIt ? valIt->text().trimmed() : QString();
    auto *typeCombo = dynamic_cast<AuiCellEditCombo *>(m_enumOptTable->cellWidget(r, 3));
    o.valueType = typeCombo ? typeCombo->text().trimmed() : QString();
    if (o.value.isEmpty()) continue;  // 跳过未填 value 的行（value 是必填键）
    out.append(o);
  }
  return out;
}

QVector<JsonTableEnum> JsonTableDesigner::collectEnums() const {
  QVector<JsonTableEnum> out = m_enums;
  // 主表编辑内容 → out；当前行选项表格内容 → out（const 下按值修正副本）
  for (int r = 0; r < m_enumTable->rowCount() && r < out.size(); ++r) {
    out[r].column = cellWidgetText(m_enumTable, r, 0);
    const QTableWidgetItem *cmtIt = m_enumTable->item(r, 1);
    if (cmtIt) out[r].comment = cmtIt->text().trimmed();
  }
  if (m_enumCurRow >= 0 && m_enumCurRow < out.size()) {
    out[m_enumCurRow].options = collectEnumOptions();
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  DDL 生成 / 预览 / 执行
// ════════════════════════════════════════════════════════════

bool JsonTableDesigner::validateForDdl(QString *err) const {
  if (m_nameEdit->text().trimmed().isEmpty()) {
    if (err) *err = QStringLiteral("表名不能为空");
    return false;
  }
  if (m_colsTable->rowCount() == 0) {
    if (err) *err = QStringLiteral("请先添加字段列");
    return false;
  }
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    if (cellText(r, ColColName).isEmpty()) {
      if (err) *err = QStringLiteral("第 %1 行列名不能为空").arg(r + 1);
      return false;
    }
  }
  return true;
}

// ════════════════════════════════════════════════════════════
//  交叉校验 / 列名候选刷新
// ════════════════════════════════════════════════════════════

bool JsonTableDesigner::validateCrossRefs(QString *err) const {
  return TableSchema::validateCrossRefs(collectTable(), m_config, m_config.tables,
                                        m_table.tableName, err);
}

void JsonTableDesigner::refreshColumnCandidates() {
  // 本表列名（枚举列/全局枚举列/联表外键列/编辑列候选）
  QStringList names;
  for (int r = 0; r < m_colsTable->rowCount(); ++r) {
    const QString n = cellText(r, ColColName);
    if (!n.isEmpty()) names.append(n);
  }
  // 读模型 ref 候选 = 本表列名 + 联表输出别名（joinTables 新格式）
  //                  + 旧格式 ManyToOne 联表的「引用名.目标列」
  QStringList refCands = names;
  for (const auto &j : collectJoins()) {
    for (const auto &c : j.cols) {
      if (!c.col.isEmpty()) refCands.append(j.table + QLatin1Char('_') + c.col);
    }
  }
  for (const auto &j : m_table.joinTable) {
    if (!j.isJoinRelations || j.joinType != QStringLiteral("ManyToOne") ||
        j.joinEnClassPro.isEmpty()) {
      continue;
    }
    for (const auto &t : m_config.tables) {
      if (t.modelName != j.joinEnClass) continue;
      for (const auto &c : t.columns) {
        if (!c.name.isEmpty()) refCands.append(j.joinEnClassPro + QLatin1Char('.') + c.name);
      }
      break;
    }
  }
  // 枚举列 / 联表 ON 候选走常驻下拉控件（遍历行设置预设）
  for (int r = 0; r < m_enumTable->rowCount(); ++r) {
    if (auto *c = dynamic_cast<AuiCellEditCombo *>(m_enumTable->cellWidget(r, 0)))
      c->setPresets(names);
  }
  refreshJoinCandidates();
  // 字段呈现「字段引用」列 = 行身份标识（纯文本只读，见 appendRow），无候选；
  // 排序候选也不含 i18n 字段——翻译文本跨表排序第一版不支持
  setSortOptions(refCands);
  if (m_globalEnumCompleter)
    static_cast<CsvCompleter *>(m_globalEnumCompleter)->setCandidates(names);
}

void JsonTableDesigner::refreshI18nFieldCandidates() {
  QStringList names;
  const QString trans = m_i18nTableEdit->text().trimmed();
  if (!trans.isEmpty()) {
    for (const auto &t : m_config.tables) {
      if (t.tableName != trans) continue;
      for (const auto &c : t.columns) names.append(c.name);
      break;
    }
  }
  m_i18nFieldOptions = names;
  // 已有行换候选（保留已填文本，不重建）
  for (int r = 0; r < m_i18nFieldTable->rowCount(); ++r) {
    if (auto *combo = dynamic_cast<AuiCellEditCombo *>(m_i18nFieldTable->cellWidget(r, 0)))
      combo->setPresets(names);
  }
}

QString JsonTableDesigner::toCreateDdl() const {
  return createDdlText(m_nameEdit->text().trimmed(), m_commentEdit->text().trimmed(),
                       collectColumns(), collectIndexes());
}

QString JsonTableDesigner::i18nCreateDdl() const {
  const JsonTableTable main = collectTable();
  if (!main.i18n.isEnabled()) return {};
  // 同目录（含子目录）已有显式翻译表 .jsontable 时不派生（显式配置优先）
  const QString searchDir = m_filePath.isEmpty() ? QStringLiteral(PROJECT_SOURCE_DIR)
                                                 : QFileInfo(m_filePath).absolutePath();
  for (const auto &t : listProjectTables(searchDir)) {
    if (t.tableName == main.i18n.table) return {};
  }
  const JsonTableTable derived = deriveI18nTable(main);
  return createDdlText(derived.tableName, derived.tableComment, derived.columns, derived.indexes);
}

void JsonTableDesigner::previewDdl() {
  QString err;
  if (!validateForDdl(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法生成 DDL"), err);
    return;
  }
  QString sql = toCreateDdl();
  const QString i18nSql = i18nCreateDdl();
  if (!i18nSql.isEmpty()) sql += QStringLiteral("\n\n") + i18nSql;
  m_ddlView->setPlainText(sql);
}

void JsonTableDesigner::importFromDatabase() {
  const QString table = m_nameEdit->text().trimmed();
  if (table.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("无法刷新"), QStringLiteral("请先填写表名"));
    return;
  }

  if (m_dbBusy) return;
  m_dbBusy = true;
  updateDbButtonStates();
  const auto revision = m_revision;
  const JsonTableConfig config = m_config;
  auto *watcher = new QFutureWatcher<DatabaseTableSnapshot>(this);
  connect(
      watcher, &QFutureWatcher<DatabaseTableSnapshot>::finished, this, [this, watcher, revision]() {
        const DatabaseTableSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        m_dbBusy = false;
        updateDbButtonStates();
        if (!snapshot.error.isEmpty()) {
          AuiMessageBox::show(this, QStringLiteral("刷新失败"), snapshot.error);
          return;
        }
        if (revision != m_revision) {
          AuiMessageBox::show(this, QStringLiteral("配置已变化"),
                              QStringLiteral("读取期间配置已被修改，本次结果未应用，请重新刷新。"));
          return;
        }
        applyDatabaseSnapshot(snapshot);
      });
  watcher->setFuture(QtConcurrent::run(
      [config, table]() { return TableDatabaseService::readTable(config, table); }));
}

void JsonTableDesigner::applyDatabaseSnapshot(const DatabaseTableSnapshot &snapshot) {
  const QString table = m_nameEdit->text().trimmed();
  const auto &dbColumns = snapshot.columns;
  const auto &dbIndexes = snapshot.indexes;
  const QString &dbComment = snapshot.comment;
  const bool indexQueryOk = snapshot.indexQueryOk;
  // 差异计算：当前配置 vs 数据库（排除列跳过，但会在预览中标记）
  const QVector<JsonTableColumn> currentColumns = collectColumns();
  const QVector<JsonTableIndex> currentIndexes = collectIndexes();
  const QString currentComment = m_commentEdit->text().trimmed();
  const QStringList initialExcluded = splitCsv(m_excludeEdit->text());
  const QVector<ImportDiffEntry> entries =
      buildImportDiffEntries(currentColumns, dbColumns, initialExcluded);
  const QVector<IndexDiffEntry> indexEntries =
      indexQueryOk ? buildIndexDiffEntries(currentIndexes, dbIndexes) : QVector<IndexDiffEntry>();
  const bool commentDiffers = !dbComment.isEmpty() && dbComment != currentComment;
  if (entries.isEmpty() && indexEntries.isEmpty() && !commentDiffers) {
    AuiMessageBox::show(this, QStringLiteral("刷新结构"),
                        QStringLiteral("结构与数据库一致，无需刷新。"));
    return;
  }

  const ImportDiffResult result = runImportDiffDialog(this, table, entries, indexEntries,
                                                      currentComment, dbComment, initialExcluded);
  if (!result.accepted) return;

  // 应用选择：现有列保序；新增列按数据库顺序追加；删除列仅在勾选时移除。
  // 非数据库属性（显示标题 / 列级取值域）按原列保真，不随刷新改写
  QHash<QString, JsonTableColumn> dbByKey;
  for (const auto &col : dbColumns) {
    dbByKey.insert(col.name.toLower(), col);
  }
  QHash<QString, JsonTableColumn> origByKey;
  for (const auto &col : m_table.columns) {
    origByKey.insert(col.name.toLower(), col);
  }
  QVector<JsonTableColumn> nextColumns;
  for (const auto &cur : currentColumns) {
    const QString key = cur.name.toLower();
    const auto origIt = origByKey.constFind(key);
    if (dbByKey.contains(key) && result.applyByKey.value(key, false)) {
      // 物理属性取数据库值，设计器侧属性按原列保留
      JsonTableColumn merged = dbByKey.value(key);
      if (origIt != origByKey.constEnd()) {
        merged.title = origIt->title;
        merged.domainType = origIt->domainType;
        merged.domainSourceFile = origIt->domainSourceFile;
        merged.domainSourceId = origIt->domainSourceId;
        merged.domainUrl = origIt->domainUrl;
        // 标题由列注释派生时，数据库更新注释会让字段呈现标题跳变：把原显示
        // 标题固化为显式 title，保证刷新后标题不变
        if (merged.title.isEmpty() && !origIt->comment.isEmpty() &&
            merged.comment != origIt->comment) {
          merged.title = origIt->comment;
        }
      }
      nextColumns.append(merged);
    } else if (!dbByKey.contains(key) && result.applyByKey.value(key, false)) {
      // 删除列已勾选：不保留（前端引用由 cleanRemovedColumnRefs 清理）
    } else {
      JsonTableColumn keep = cur;
      if (origIt != origByKey.constEnd()) {
        keep.title = origIt->title;
        keep.domainType = origIt->domainType;
        keep.domainSourceFile = origIt->domainSourceFile;
        keep.domainSourceId = origIt->domainSourceId;
        keep.domainUrl = origIt->domainUrl;
      }
      nextColumns.append(keep);
    }
  }
  QSet<QString> keptKeys;
  for (const auto &col : nextColumns) {
    keptKeys.insert(col.name.toLower());
  }
  for (const auto &db : dbColumns) {
    const QString key = db.name.toLower();
    if (keptKeys.contains(key)) continue;
    if (result.applyByKey.value(key, false)) nextColumns.append(db);
  }

  // 索引应用（索引查询失败时保持原索引，不做任何删除）
  QVector<JsonTableIndex> nextIndexes = currentIndexes;
  if (indexQueryOk) {
    QHash<QString, JsonTableIndex> dbIndexByKey;
    for (const auto &index : dbIndexes) {
      dbIndexByKey.insert(index.name.toLower(), index);
    }
    nextIndexes.clear();
    for (const auto &cur : currentIndexes) {
      const QString key = cur.name.toLower();
      if (dbIndexByKey.contains(key)) {
        nextIndexes.append(result.indexApplyByKey.value(key, false) ? dbIndexByKey.value(key)
                                                                    : cur);
      } else if (!result.indexApplyByKey.value(key, false)) {
        nextIndexes.append(cur);  // 删除索引默认保留
      }
    }
    QSet<QString> keptIndexKeys;
    for (const auto &index : nextIndexes) {
      keptIndexKeys.insert(index.name.toLower());
    }
    for (const auto &index : dbIndexes) {
      const QString key = index.name.toLower();
      if (keptIndexKeys.contains(key)) continue;
      if (result.indexApplyByKey.value(key, false)) nextIndexes.append(index);
    }
  }

  int appliedAdded = 0;
  int appliedModified = 0;
  int appliedRemoved = 0;
  QStringList removedColumns;
  for (const auto &e : entries) {
    if (!result.applyByKey.value(e.name.toLower(), false)) continue;
    switch (e.kind) {
      case ImportDiffKind::Added:
        ++appliedAdded;
        break;
      case ImportDiffKind::Modified:
        ++appliedModified;
        break;
      case ImportDiffKind::Removed:
        ++appliedRemoved;
        removedColumns << e.name;
        break;
      default:
        break;
    }
  }
  int appliedIndexAdded = 0;
  int appliedIndexModified = 0;
  int appliedIndexRemoved = 0;
  for (const auto &e : indexEntries) {
    if (!result.indexApplyByKey.value(e.name.toLower(), false)) continue;
    switch (e.kind) {
      case ImportDiffKind::Added:
        ++appliedIndexAdded;
        break;
      case ImportDiffKind::Modified:
        ++appliedIndexModified;
        break;
      case ImportDiffKind::Removed:
        ++appliedIndexRemoved;
        break;
      default:
        break;
    }
  }

  // 模型回填：以当前编辑内容为基底，只替换数据库来源的信息（列物理属性 /
  // 索引 / 表注释 / 排除列）；字段呈现、联表、按钮、多语言等设计器配置不重建，
  // 避免从数据库刷新时改写非数据库信息
  JsonTableTable updated = collectTable();
  updated.columns = nextColumns;
  if (indexQueryOk) updated.indexes = nextIndexes;
  if (result.applyTableComment && !dbComment.isEmpty()) updated.tableComment = dbComment;
  setExcludedColumns(&updated, result.excludedColumns);
  const RefCleanupStats cleanup = cleanRemovedColumnRefs(&updated, removedColumns);

  load(updated, m_config, m_filePath);
  emit configChanged();
  updateValidateStatus();

  QString msg = QStringLiteral("已应用 %1 项列变更（新增 %2 / 修改 %3 / 删除 %4）。")
                    .arg(appliedAdded + appliedModified + appliedRemoved)
                    .arg(appliedAdded)
                    .arg(appliedModified)
                    .arg(appliedRemoved);
  if (indexQueryOk) {
    msg += QStringLiteral("\n索引：新增 %1 / 修改 %2 / 删除 %3。")
               .arg(appliedIndexAdded)
               .arg(appliedIndexModified)
               .arg(appliedIndexRemoved);
  } else {
    msg += QStringLiteral("\n索引结构读取失败，本次未同步索引。");
  }
  if (cleanup.total() > 0) {
    msg += QStringLiteral("\n已清理前端引用 %1 处（%2）。")
               .arg(cleanup.total())
               .arg(cleanup.summary());
  }
  if (!result.excludedColumns.isEmpty()) {
    msg += QStringLiteral("\n排除列：%1").arg(result.excludedColumns.join(QStringLiteral(", ")));
  }
  AuiMessageBox::show(this, QStringLiteral("刷新完成"), msg);
}

void JsonTableDesigner::executeCreateTable() {
  if (m_dbBusy) return;
  QString err;
  if (!validateForDdl(&err)) {
    AuiMessageBox::show(this, QStringLiteral("无法执行建表"), err);
    return;
  }
  // 主表 + 派生翻译表（启用 i18n 且同目录无显式翻译表配置时附带建表）
  QStringList sqls;
  sqls << toCreateDdl();
  const QString i18nSql = i18nCreateDdl();
  if (!i18nSql.isEmpty()) sqls << i18nSql;
  m_ddlView->setPlainText(sqls.join(QStringLiteral("\n\n")));

  if (!AuiMessageBox::confirm(this, QStringLiteral("执行建表"),
                              QStringLiteral("将在数据库「%1」执行 CREATE TABLE（%2 张表）。\n"
                                             "执行前请确认 DDL 预览内容，操作不会自动回滚。")
                                  .arg(m_config.dbDatabase)
                                  .arg(sqls.size())))
    return;

  m_dbBusy = true;
  updateDbButtonStates();
  const JsonTableConfig config = m_config;
  auto *watcher = new QFutureWatcher<DatabaseWriteResult>(this);
  connect(watcher, &QFutureWatcher<DatabaseWriteResult>::finished, this, [this, watcher]() {
    const DatabaseWriteResult result = watcher->result();
    watcher->deleteLater();
    m_dbBusy = false;
    updateDbButtonStates();
    if (!result.error.isEmpty()) {
      AuiMessageBox::show(this, QStringLiteral("建表失败"),
                          QStringLiteral("已完成 %1 条 DDL，后续执行失败：\n%2")
                              .arg(result.completedCount)
                              .arg(result.error));
    } else {
      AuiMessageBox::show(this, QStringLiteral("建表成功"),
                          QStringLiteral("已完成 %1 条 DDL。").arg(result.completedCount));
    }
  });
  watcher->setFuture(QtConcurrent::run(
      [config, sqls]() { return TableDatabaseService::executeDdl(config, sqls); }));
}

// ════════════════════════════════════════════════════════════
//  结果收集
// ════════════════════════════════════════════════════════════

JsonTableTable JsonTableDesigner::collectTable() const {
  JsonTableTable t = m_table;  // extra 未知键保真（joinTable 等未结构化节点原样写回）
  t.tableName = m_nameEdit->text().trimmed();
  t.modelName = m_modelEdit->text().trimmed();
  t.tableComment = m_commentEdit->text().trimmed();
  t.manualUpdateTime = m_manualUpdateCheck->isChecked();
  t.frontend = m_frontendCheck->isChecked();
  t.genAdd = m_genAddCheck->isChecked();
  t.genEdit = m_genEditCheck->isChecked();
  t.genDelete = m_genDeleteCheck->isChecked();
  t.genDetail = m_genDetailCheck->isChecked();
  t.genCache = m_genCacheCheck->isChecked();
  // C 端形态：下拉 → bool + mode（"query" 不落 mode 键，缺省=只读查询）
  const QString appMode = m_appQueryCombo->currentData().toString();
  t.appQuery = (appMode != QStringLiteral("none"));
  t.appQueryMode = (appMode == QStringLiteral("empty")) ? QStringLiteral("empty") : QString();
  t.appAuth = m_appAuthCombo->currentData().toString();
  if (t.appAuth.isEmpty()) t.appAuth = QStringLiteral("user");
  // C 端出参白名单：仅 appQuery 生效；按字段表顺序，过滤已删除/改名的列
  t.appOutCols.clear();
  if (t.appQuery) {
    for (int r = 0; r < m_colsTable->rowCount(); ++r) {
      const QString name = cellText(r, ColColName);
      if (!name.isEmpty() && m_appOutCols.contains(name)) t.appOutCols.append(name);
    }
  }
  t.buttons = m_buttons;
  t.columns = collectColumns();
  t.indexes = collectIndexes();
  // ── 表级进阶键 ──
  t.joinTable = m_table.joinTable;  // 旧格式联表不再编辑，原样保真（避免旧文件保存丢失）
  t.joinTables = collectJoins();
  collectViewTable(&t);  // 统一字段表格 → 三视图（selCols/eq/like/gap/editCols）
  // 字段呈现行序持久化（设计器专属键，存 extra；生成端不消费）：
  // 只有用户真正移动过行（与回填基线不同）才写；未动时保持文件原状，
  // 避免打开后凭空新增键，进而让「单击/失焦」被误判成内容变化
  QStringList currentOrder;
  for (int r = 0; r < m_viewTable->rowCount(); ++r) {
    const QString ref = viewRefText(r);
    if (!ref.isEmpty()) currentOrder << ref;
  }
  if (currentOrder != m_viewBaselineOrder) {
    QJsonArray viewOrderArr;
    for (const QString &ref : currentOrder) viewOrderArr.append(ref);
    if (viewOrderArr.isEmpty())
      t.extra.remove(QString::fromLatin1(JsonTableKey::kViewOrder));
    else
      t.extra.insert(QString::fromLatin1(JsonTableKey::kViewOrder), viewOrderArr);
  }
  t.selColsSort = collectSort();
  t.enums = collectEnums();
  t.globalEnumCols = splitCsv(m_globalEnumEdit->text());
  if (m_i18nCheck->isChecked()) {
    t.i18n.table = m_i18nTableEdit->text().trimmed();
    t.i18n.extKey = m_i18nExtEdit->text().trimmed();
    t.i18n.langKey = m_i18nLangKeyEdit->text().trimmed();
    // 多语言字段：从表格收集（对象形态落盘 name + label，label 空 = 回退链）
    QList<JsonTableI18nField> i18nFields;
    for (int r = 0; r < m_i18nFieldTable->rowCount(); ++r) {
      auto *nameCombo = dynamic_cast<AuiCellEditCombo *>(m_i18nFieldTable->cellWidget(r, 0));
      const QString name = nameCombo ? nameCombo->text().trimmed() : QString();
      if (name.isEmpty()) continue;
      JsonTableI18nField f;
      f.name = name;
      // 标题列读多语言字段表自身（cellText 固定读字段表格 m_colsTable，不能用）
      f.label = cellOf(m_i18nFieldTable, r, 1);
      i18nFields.append(f);
    }
    t.i18n.fields = i18nFields;
    t.i18n.defaultLang = m_i18nDefaultLangEdit->text().trimmed();
    if (t.i18n.defaultLang.isEmpty()) t.i18n.defaultLang = QStringLiteral("zh");
    t.i18n.langFrom = m_i18nLangFromCombo->currentData().toString();
    if (t.i18n.langFrom.isEmpty()) t.i18n.langFrom = QStringLiteral("body");
  } else {
    t.i18n = JsonTableI18n();  // 未启用：清空（table 空 → toJson 不落键）
  }
  // 排除列：设计器专属键，写入 extra 保真（生成期不消费）
  setExcludedColumns(&t, splitCsv(m_excludeEdit->text()));
  return t;
}
