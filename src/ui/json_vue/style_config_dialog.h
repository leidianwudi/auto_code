/**
 * @file style_config_dialog.h
 * @brief 字段样式配置对话框
 *
 * 提供两个独立的样式配置对话框：
 *   - ColumnStyleDialog：配置表格列显示样式、编辑表单样式和通用配置
 *   - QueryStyleDialog：配置查询字段输入样式（占位提示/日期格式）
 *
 * 显示类型（含开关）和编辑样式在对话框内选择，子控件根据选择动态展示。
 */

#pragma once

#include <QDialog>
#include <QHash>
#include <QPair>

#include "json_vue_model.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;
class AuiTreeCombo;
class QWidget;

// ════════════════════════════════════════════════════════════
//  ColumnStyleDialog：列配置样式对话框
// ════════════════════════════════════════════════════════════

/**
 * @class ColumnStyleDialog
 * @brief 字段设置面板（原"列样式配置"，方案 B 后升级为字段级设置）
 *
 * 以字段为中心的属性面板（对齐成熟低代码平台的字段面板形态）：
 * 取值域（字段级数据源声明，三处共享）→ 列表页展示 → 编辑表单 → 通用配置。
 * 入口：列配置表 ⚙ 按钮或「取值域」列双击。
 *
 * 注：类名/文件名保持 ColumnStyleDialog 以控制改动面，概念上即"字段设置"。
 */
class ColumnStyleDialog : public QDialog {
  Q_OBJECT

public:
  explicit ColumnStyleDialog(EditStyle style, QWidget *parent = nullptr);
  ~ColumnStyleDialog() override;

protected:
  /// 确认时验证数据（检查 tagItems 重复/空值等）
  void accept() override;

public:
  /// 显示当前编辑的字段名（面板标题区，只读提示）
  void setFieldName(const QString &v);
  // ── 编辑样式 ──
  void setEditStyle(EditStyle style);
  EditStyle editStyle() const;

  // ── 编辑可编辑 ──
  void setEditEditable(bool v);
  bool editEditable() const;

  // ── 开关可编辑（displayType 为 boolean/tag 且 switchEditable 时生效）──
  void setSwitchEditable(bool v);
  bool switchEditable() const;

  // ── 下拉框数据源（editStyle == Select 时使用，由对话框内部按钮配置）──
  void setSelectUrl(const QString &v);
  QString selectUrl() const;
  void setSelectValueField(const QString &v);
  QString selectValueField() const;
  void setSelectLabelField(const QString &v);
  QString selectLabelField() const;
  /// 引用的 .jsonsource 文件路径
  void setSelectSourceFile(const QString &v);
  QString selectSourceFile() const;
  /// 引用的数据源 id
  void setSelectSourceId(const QString &v);
  QString selectSourceId() const;
  /// 下拉框是否查询分页加载
  void setSelectPaged(bool v);
  bool selectPaged() const;
  /// 页码参数名（查询分页时使用）
  void setSelectPageKey(const QString &v);
  QString selectPageKey() const;
  /// 页大小参数名（查询分页时使用）
  void setSelectPageSizeKey(const QString &v);
  QString selectPageSizeKey() const;
  /// 默认页大小（查询分页时使用）
  void setSelectPageSize(int v);
  int selectPageSize() const;
  /// 查询标题（查询分页搜索框提示）
  void setSelectSearchTitle(const QString &v);
  QString selectSearchTitle() const;
  /// 字段名（查询分页搜索参数 key）
  void setSelectSearchField(const QString &v);
  QString selectSearchField() const;
  /// 查询请求方式（GET/POST）
  void setSelectMethod(const QString &v);
  QString selectMethod() const;
  /// 设置 HTTP 配置（供下拉框数据源测试按钮使用）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

  /// 设置 .jsonsource 文件搜索根目录（当前编辑 jsonvue 文件所在目录，可为空）
  void setSearchRoot(const QString &dir);

  // ── 编辑样式子配置 ──
  void setPlaceholder(const QString &v);
  QString placeholder() const;

  void setMaxlength(int v);
  int maxlength() const;

  void setMinValue(double v);
  double minValue() const;

  void setMaxValue(double v);
  double maxValue() const;

  void setPrecision(int v);
  int precision() const;

  void setDateFormat(const QString &v);
  QString dateFormat() const;

  void setTextareaRows(int v);
  int textareaRows() const;

  // ── 通用配置 ──
  void setRequired(bool v);
  bool required() const;

  void setColumnWidth(int v);
  int columnWidth() const;

  void setColumnFixed(const QString &v);
  QString columnFixed() const;

  void setFormatter(const QString &v);
  QString formatter() const;

  void setFormSpan(int v);
  int formSpan() const;

  // ── 表格列显示样式 ──
  void setDisplayType(const QString &v);
  QString displayType() const;

  /// 设置标签映射数组（displayType == "tag" 时使用）
  void setTagItems(const QList<TagItem> &items);
  /// 获取标签映射数组
  QList<TagItem> tagItems() const;

  void setBoolTrueText(const QString &v);
  QString boolTrueText() const;

  void setBoolFalseText(const QString &v);
  QString boolFalseText() const;

  /// boolean 引用的静态数据源（恰好 2 项选项的静态源）；引用时真假文字锁定为数据源值。
  /// 注：方案 B 后布尔源统一由取值域承担，此接口仅作旧保存路径兼容
  void setBoolSourceRef(const QString &file, const QString &id);
  QString boolSourceFile() const;
  QString boolSourceId() const;

  // ── 字段取值域（方案 B：声明一次，列渲染/编辑控件/查询筛选三处共用）──
  /// 取值域类型（""/enum/static/remote，见 JsonVueDomain）；空 = 未声明（生成/编辑侧按旧键回退）
  void setDomainType(const QString &v);
  QString domainType() const;
  /// enum/static 引用的数据源（文件基名 + id，全局枚举固定基名 global_enum.jsonsource）
  void setDomainSourceRef(const QString &file, const QString &id);
  QString domainSourceFile() const;
  QString domainSourceId() const;
  /// remote 手动数据源 URL（复用 selectUrl 缓存，与下拉参数同组）
  void setDomainUrl(const QString &v);
  QString domainUrl() const;

  /// image 编辑样式引用的上传预设（.jsonupload 文件 + 预设 id）；空 = URL 手工输入
  void setUploadSourceRef(const QString &file, const QString &id);
  QString uploadSourceFile() const;
  QString uploadSourceId() const;

  // ── 通用配置（默认值/排序）──
  void setDefaultValue(const QString &v);
  QString defaultValue() const;

  void setDefaultSort(const QString &v);
  QString defaultSort() const;

private:
  /// 构建界面
  void setupUI();
  /// 添加一行标签+控件
  void addRow(const QString &labelText, QWidget *widget);
  /// 重建显示样式子控件
  void rebuildDisplayTypeControls();
  /// 重建编辑样式子控件
  void rebuildEditStyleControls();
  /// 从 tagItems 表格收集数据
  QList<TagItem> collectTagItems() const;
  /// 用 tagItems 填充表格（末尾附"+"添加行）
  void populateTagItems(const QList<TagItem> &items);
  /// 在指定行处插入一个标签映射行（值/文字/颜色下拉/删除按钮）
  void insertTagRow(int row, const TagItem &item);
  /// 验证 tagItems 数据（检查空值和重复 value）
  bool validateTagItems(QString *error) const;
  /// 动态重建后自适应对话框大小
  void adjustToContents();

  // ── 字段取值域（方案 B）──
  /// 构建取值域数据源候选树（类型简化后列出全部静态源；enumOnly 参数保留兼容）
  void buildDomainCandidates(bool enumOnly = false);
  /// 按取值域类型显隐子控件、重建候选并恢复选中
  void rebuildDomainControls();
  /// 取值域变化时自动推导显示/编辑样式（用户已手动改过样式则不联动）
  void deriveStylesFromDomain();
  /// 刷新取值域选项预览
  void updateDomainPreview();
  /// 用取值域候选的真/假文字填充并锁定文字框（ref 为空时解锁）
  void applyDomainBoolTexts(const QString &ref);
  /// 显隐取值域行（field 与其标签同步）
  void setDomainRowVisible(QWidget *field, bool visible);
  /// 当前取值域引用（"文件#id"，未选时为空）
  QString currentDomainRef() const;

  EditStyle m_editStyle = EditStyle::Text;
  bool m_syncing = false;  ///< 显示样式/编辑样式联动同步中（防递归互触）

  QFormLayout *m_formLayout = nullptr;         ///< 主表单布局
  QVBoxLayout *m_displayTypeLayout = nullptr;  ///< 显示样式子控件容器布局
  QVBoxLayout *m_editStyleLayout = nullptr;    ///< 编辑样式子控件容器布局
  QWidget *m_displayTypeWidget = nullptr;      ///< 显示样式子控件容器
  QWidget *m_editStyleWidget = nullptr;        ///< 编辑样式子控件容器

  // ── 显示样式 ──
  QComboBox *m_displayTypeCombo = nullptr;

  // ── 编辑样式 ──
  QComboBox *m_editStyleCombo = nullptr;
  QCheckBox *m_editEditableCheck = nullptr;
  QCheckBox *m_switchEditableCheck = nullptr;

  // ── 编辑样式子控件（按需创建）──
  QLineEdit *m_placeholderEdit = nullptr;
  QComboBox *m_maxlengthCombo = nullptr;
  QComboBox *m_minValueCombo = nullptr;
  QComboBox *m_maxValueCombo = nullptr;
  QComboBox *m_precisionCombo = nullptr;
  QComboBox *m_dateFormatCombo = nullptr;
  QComboBox *m_textareaRowsCombo = nullptr;

  // ── 字段取值域（方案 B：顶部声明区，列渲染/编辑/查询三处共享）──
  QLabel *m_fieldNameLabel = nullptr;           ///< 字段名显示（面板顶部，只读）
  QComboBox *m_domainTypeCombo = nullptr;       ///< 取值域类型（无/枚举/静态源/远程源）
  AuiTreeCombo *m_domainSourceCombo = nullptr;  ///< enum/static 数据源树形下拉
  QWidget *m_domainSourceRow = nullptr;         ///< 数据源行容器（整行显隐切换）
  QLabel *m_domainPreviewLabel = nullptr;       ///< 选项预览（如 开启=1 / 关闭=0）
  QPushButton *m_domainUrlBtn = nullptr;        ///< remote 配置入口（开 ComboboxConfigDialog）
  QString m_cachedDomainType;                   ///< 取值域类型缓存
  QString m_cachedDomainSourceFile;             ///< 取值域选中源文件（恢复选中）
  QString m_cachedDomainSourceId;               ///< 取值域选中源 id
  bool m_stylesTouched = false;  ///< 用户已手动改过显示/编辑样式（取值域推导让位）
  /// 取值域候选的选项预览缓存（ref → "开启=1 / 关闭=0"，超过 4 项截断）
  QHash<QString, QString> m_domainOptionPreviews;
  /// 取值域候选的选项数缓存（ref → 选项数；恰 2 项 = 枚举能力，布尔样式可选）
  QHash<QString, int> m_domainOptionCounts;

  // ── 显示样式子控件（按需创建）──
  QTableWidget *m_tagItemsTable = nullptr;  ///< tag 标签映射表（动态增删行）
  QLineEdit *m_boolTrueTextEdit = nullptr;
  QLineEdit *m_boolFalseTextEdit = nullptr;
  QComboBox *m_uploadSourceCombo = nullptr;  ///< image 编辑样式的上传预设下拉（.jsonupload）

  // ── 通用配置控件 ──
  QCheckBox *m_requiredCheck = nullptr;
  QComboBox *m_columnWidthCombo = nullptr;
  QComboBox *m_columnFixedCombo = nullptr;
  QComboBox *m_formatterCombo = nullptr;
  QComboBox *m_formSpanCombo = nullptr;

  // ── 通用配置（默认值/排序）──
  QLineEdit *m_defaultValueEdit = nullptr;
  QComboBox *m_defaultSortCombo = nullptr;

  /// 缓存当前值（重建控件时用于恢复）
  QString m_cachedPlaceholder;
  int m_cachedMaxlength = 0;
  double m_cachedMinValue = 0;
  double m_cachedMaxValue = 0;
  int m_cachedPrecision = 2;
  QString m_cachedDateFormat;
  int m_cachedTextareaRows = 3;
  QList<TagItem> m_cachedTagItems;  ///< tag 标签映射缓存
  QString m_cachedBoolTrueText;
  QString m_cachedBoolFalseText;
  QString m_cachedBoolSourceFile;    ///< boolean 选中的静态数据源文件（恢复选中并锁定文字）
  QString m_cachedBoolSourceId;      ///< boolean 选中的静态数据源 id
  /// 候选数据源的真/假文字映射（id → {真值文字, 假值文字}）。
  /// 构建候选下拉时一次性从 .jsonsource / .jsonglobalenum 解析缓存，
  /// 选中项切换时直接查表锁定文字（不重复读文件，同步产物缺失也可用）
  QHash<QString, QPair<QString, QString>> m_boolSourceTexts;
  QString m_cachedUploadSourceFile;  ///< image 选中的上传预设文件（恢复选中）
  QString m_cachedUploadSourceId;    ///< image 选中的上传预设 id
  bool m_cachedSwitchEditable = true;
  // 下拉框数据源缓存
  QString m_cachedSelectUrl;
  QString m_cachedSelectSourceFile;
  QString m_cachedSelectSourceId;
  QString m_cachedSelectValueField;
  QString m_cachedSelectLabelField;
  bool m_cachedSelectPaged = false;
  QString m_cachedSelectPageKey = QStringLiteral("page");
  QString m_cachedSelectPageSizeKey = QStringLiteral("pageSize");
  int m_cachedSelectPageSize = 20;
  QString m_cachedSelectSearchTitle;
  QString m_cachedSelectSearchField;
  QString m_cachedSelectMethod = QString::fromLatin1(JsonVueHttp::kPost);
  // HTTP 配置（供下拉框数据源测试按钮使用）
  QString m_baseUrl;
  QString m_authHeader;
  QString m_postData;
  // .jsonsource 文件搜索根目录（当前编辑 jsonvue 文件所在目录）
  QString m_searchRoot;
};

// ════════════════════════════════════════════════════════════
//  QueryStyleDialog：查询字段样式对话框
// ════════════════════════════════════════════════════════════

/**
 * @class QueryStyleDialog
 * @brief 查询字段样式对话框
 *
 * 仅配置查询字段的输入样式特定字段（占位提示、日期格式）。
 * 日期格式不包含"日期范围"——范围查询应通过查询关系（QColRelation）配置。
 */
class QueryStyleDialog : public QDialog {
  Q_OBJECT

public:
  explicit QueryStyleDialog(QueryInputStyle style, QWidget *parent = nullptr);
  ~QueryStyleDialog() override = default;

  // ── 占位提示（text 样式）──
  void setPlaceholder(const QString &v);
  QString placeholder() const;

  // ── 日期格式（date 样式，不含"日期范围"）──
  void setDateFormat(const QString &v);
  QString dateFormat() const;

private:
  /// 构建界面
  void setupUI();

  QueryInputStyle m_queryStyle = QueryInputStyle::Text;

  QLineEdit *m_placeholderEdit = nullptr;
  QComboBox *m_dateFormatCombo = nullptr;

  QString m_cachedPlaceholder;
  QString m_cachedDateFormat;
};
