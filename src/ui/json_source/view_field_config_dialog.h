/**
 * @file view_field_config_dialog.h
 * @brief jsontable 字段呈现 ⚙ 字段设置面板
 *
 * 三分区 Tab 界面（对齐 jsonvue ColumnStyleDialog 心谋，独立新类不动 json_vue 行为）：
 *   - 列表展示：呈现方式（自动/文本/开关/金额/标签/图片）+ 列宽 + 缩略图
 *   - 编辑表单：控件类型 + 自动推导提示 + 控件参数 + 高级（栅格/默认值）
 *   - 取值域：类型 + 数据源树 + 选项预览 + 远程源配置
 *
 * 「自动」的推导结果以只读提示实时展示，底部汇总最终生成效果；不兼容的
 * 编辑控件按数据库类型过滤，避免出现「varchar 选数字却仍生成文本框」。
 *
 * 结果经 getter 取回，由 JsonTableDesigner 写回行 UserRole 槽位与列定义。
 */

#pragma once

#include <QDialog>
#include <QHash>

#include "json_table_model.h"

class AuiTreeCombo;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QToolButton;
class QWidget;

/**
 * @class ViewFieldConfigDialog
 * @brief 字段呈现 ⚙ 字段设置面板（取值域/列表页展示/编辑表单）
 */
class ViewFieldConfigDialog : public QDialog {
  Q_OBJECT

public:
  /// ref=字段引用（物理列名或联表 pro.col）；isJoinRow=true 时取值域/编辑表单区隐藏
  ViewFieldConfigDialog(const QString &ref, bool isJoinRow, QWidget *parent = nullptr);

  /// .jsonsource/.jsonupload 搜索根目录（当前 .jsontable 文件所在目录）
  void setSearchRoot(const QString &dir);

  /// 数据库类型（mysqlType）+ 是否表内枚举：用于自动推导提示与编辑控件过滤。
  /// 必须在 setCurrent 之前调用。
  void setMysqlType(const QString &mysqlType, bool hasEnum = false);

  // ── 当前行已有配置（列表侧来自 selCol，编辑侧来自 editCol；联表行仅列表生效）──
  void setCurrent(const JsonTableSelCol &sc, const JsonTableEditCol &ec);

  /// 列级取值域（物理列定义；联表行忽略）
  void setDomain(const JsonTableColumn &col);

  // ── 结果读取 ──
  /// 列表侧结果（ref/displayType/缩略图；ref 沿用打开时的值）
  void fillListResult(JsonTableSelCol *sc) const;
  /// 编辑侧结果（widget/标题/上传/缩略图/精细参数；联表行仅 width 有效）
  void fillEditResult(JsonTableEditCol *ec) const;
  int width() const;
  /// 取值域类型（""/enum/static/remote；enum/static 由选中源选项数推导）
  QString domainType() const;
  QString domainSourceFile() const;
  QString domainSourceId() const;
  QString domainUrl() const;

private:
  /// 构建界面（Tabs：列表展示/编辑表单/取值域）
  void setupUI();
  /// 构建列表展示页（基础 + 高级折叠）
  void buildListTab();
  /// 构建编辑表单页（基础 + 控件参数 + 高级折叠）
  void buildEditTab();
  /// 构建取值域页
  void buildDomainTab();
  /// 按 mysqlType 重建编辑控件候选（不兼容项不提供；已有配置保留并提示）
  void rebuildWidgetOptions(const QString &preferred);
  /// 取值域数据源候选树（buildDomainSourceCandidates 共享实现）
  void buildDomainCandidates();
  /// 按取值域类型显隐子控件并恢复选中
  void rebuildDomainControls();
  /// 刷新取值域选项预览
  void updateDomainPreview();
  /// 当前取值域引用（"文件#id"，未选时为空）
  QString currentDomainRef() const;
  /// 上传预设引用（"文件#id"，未选时为空）
  QString currentUploadRef() const;
  /// 最终生效的编辑控件（显式选择 > 取值域推导 > mysqlType 推导）
  QString effectiveWidget() const;
  /// 仅按 mysqlType 推导的控件
  QString mysqlWidget() const;
  /// 列表呈现方式的最终结果（文本/开关/金额/标签/图片，不含“自动”前缀）
  QString listModeResult() const;
  /// 自动推导的补充原因（表内枚举/tinyint(1)；无补充时为空）
  QString listModeReason() const;
  /// 刷新自动推导提示与底部生成结果摘要
  void refreshHints();
  /// 按列类型/控件刷新 Tab 可见性：
  /// 非物理列（多语言）只保留列表；取值域仅在已配置域或下拉控件时显示。
  void updateTabVisibility();
  /// 列表缩略图行随呈现方式；编辑参数行随有效控件与数据库类型按需出现
  void updateListRows();
  void updateEditFormRows();
  void setFormRowVisible(QWidget *field, bool on);
  /// 取值域 → 编辑控件自动推导（仅控件为"自动"时生效：恰2项→boolean、其它→select）
  void deriveWidgetFromDomain();

  QString m_ref;
  bool m_isJoinRow = false;
  QString m_searchRoot;
  QString m_mysqlType;      ///< 物理列 MySQL 类型（联表/多语言行可为空）
  bool m_hasEnum = false;   ///< 表内枚举列：自动推导为下拉/标签

  // ── 取值域（方案 B）──
  QComboBox *m_domainTypeCombo = nullptr;
  QWidget *m_domainSourceRow = nullptr;
  AuiTreeCombo *m_domainSourceCombo = nullptr;
  QPushButton *m_domainUrlBtn = nullptr;
  QLabel *m_domainPreviewLabel = nullptr;
  QString m_domainSourceFile;  ///< enum/static 引用的数据源文件（缓存）
  QString m_domainSourceId;    ///< enum/static 引用的数据源 id（缓存）
  // 远程源配置缓存（复用 select 语义，domainUrl = selectUrl）
  QString m_selectUrl;
  QString m_selectValueField;
  QString m_selectLabelField;
  bool m_selectPaged = false;
  QString m_selectPageKey;
  QString m_selectPageSizeKey;
  int m_selectPageSize = 20;
  QString m_selectSearchTitle;
  QString m_selectSearchField;
  QString m_selectMethod;
  QHash<QString, QPair<QString, QString>> m_domainBoolTexts;   ///< ref → {真文字,假文字}
  QHash<QString, QString> m_domainPreviews;                    ///< ref → 选项预览
  QHash<QString, int> m_domainOptionCounts;                    ///< ref → 选项数（动态源 -1）
  QHash<QString, bool> m_domainDynamicFlags;                   ///< ref → 是否动态源

  // ── 页面骨架 ──
  QLabel *m_fieldNameLabel = nullptr;  ///< 字段名 + 数据库类型摘要
  QLabel *m_typeLabel = nullptr;
  QTabWidget *m_tabs = nullptr;
  QWidget *m_editTab = nullptr;        ///< 编辑表单页（多语言字段隐藏）
  QWidget *m_domainTab = nullptr;      ///< 取值域页（按需显示）
  QLabel *m_summaryLabel = nullptr;    ///< 底部「生成结果」摘要
  QList<QFormLayout *> m_forms;        ///< 所有表单（行级显隐按控件反查）
  QWidget *m_listAdvBox = nullptr;     ///< 列表高级区容器
  QWidget *m_editAdvBox = nullptr;     ///< 编辑高级区容器
  QToolButton *m_listAdvBtn = nullptr;
  QToolButton *m_editAdvBtn = nullptr;

  // ── 列表展示 ──
  QComboBox *m_listModeCombo = nullptr;  ///< 自动/文本/开关/金额/标签/图片
  QLabel *m_listHint = nullptr;          ///< 自动推导结果提示
  QComboBox *m_widthCombo = nullptr;
  QWidget *m_listThumbRow = nullptr;  ///< 列表缩略图行容器（整行显隐随呈现方式=image）
  QSpinBox *m_listThumbW = nullptr;
  QSpinBox *m_listThumbH = nullptr;

  // ── 编辑表单 ──
  QCheckBox *m_editEditableCheck = nullptr;  ///< 表单内可编辑（false=只读展示）
  QCheckBox *m_requiredCheck = nullptr;      ///< 必填（提交时校验；对齐 jsonvue 字段设置）
  QComboBox *m_widgetCombo = nullptr;
  QLabel *m_widgetHint = nullptr;            ///< 自动推导结果/不兼容提示
  QLineEdit *m_placeholderEdit = nullptr;    ///< 占位提示（input/textarea/image）
  QSpinBox *m_maxlengthSpin = nullptr;       ///< 最大长度（input，0=不限）
  QSpinBox *m_textareaRowsSpin = nullptr;    ///< 行数（textarea，0=缺省 4）
  QWidget *m_rangeRow = nullptr;             ///< 数值范围行容器（整行显隐）
  QDoubleSpinBox *m_minSpin = nullptr;       ///< 最小值（数值控件，0=不限）
  QDoubleSpinBox *m_maxSpin = nullptr;       ///< 最大值（数值控件，0=不限）
  QSpinBox *m_precisionSpin = nullptr;       ///< 小数位（decimal/float，0=缺省 2）
  QComboBox *m_dateFormatCombo = nullptr;    ///< 日期格式覆盖（空=按类型推导）
  QComboBox *m_formSpanCombo = nullptr;      ///< 编辑表单栅格（自动/半行12/整行24）
  QLineEdit *m_formDefaultEdit = nullptr;    ///< 表单新建预填值
  QComboBox *m_uploadSourceCombo = nullptr;
  QWidget *m_editThumbRow = nullptr;  ///< 编辑缩略图行容器（整行显隐随 image/upload 控件）
  QSpinBox *m_editThumbW = nullptr;
  QSpinBox *m_editThumbH = nullptr;
};
