/**
 * @file view_field_config_dialog.h
 * @brief jsontable 字段呈现 ⚙ 字段设置面板
 *
 * 四分区统一界面（对齐 jsonvue ColumnStyleDialog 心谋，独立新类不动 json_vue 行为）：
 *   - 取值域（仅物理列 ref 显示；联表 ref 只读隐藏）：类型（未声明/数据源/远程接口）
 *     + 数据源树 + 选项预览 + 远程源配置按钮（复用 ComboboxConfigDialog）
 *   - 列表页展示：displayType（自动/图片）+ 列宽 px + 列表缩略图宽高(px)
 *   - 编辑表单：控件类型 + 编辑标题覆盖 + 上传预设 + 编辑缩略图宽高(px)
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
  /// 构建界面
  void setupUI();
  /// 构建取值域数据源候选树（buildDomainSourceCandidates 共享实现）
  void buildDomainCandidates();
  /// 按取值域类型显隐子控件并恢复选中
  void rebuildDomainControls();
  /// 刷新取值域选项预览
  void updateDomainPreview();
  /// 当前取值域引用（"文件#id"，未选时为空）
  QString currentDomainRef() const;
  /// 上传预设引用（"文件#id"，未选时为空）
  QString currentUploadRef() const;
  /// 编辑表单控件启用联动（占位/长度/行数/数值范围/精度/日期格式随控件类型）
  /// 行级显隐联动（QFormLayout::setRowVisible 整行折叠，对齐 jsonvue 字段设置）：
  /// 列表缩略图行随 displayType=image；编辑参数行随编辑控件类型按需出现
  void updateListRows();
  void updateEditFormRows();
  /// 编辑表单区（── 编辑表单 ── 分隔行起至末尾）整体显隐；联表行整体折叠
  void setEditRowsVisible(bool on);
  /// 按 field widget 反查表单行号；-1 = 不在表单中
  int formRowOf(QWidget *field) const;
  void setFormRowVisible(QWidget *field, bool on);
  /// 取值域 → 编辑控件自动推导（仅控件为"自动"时生效：恰2项→boolean、其它→select）
  void deriveWidgetFromDomain();

  QString m_ref;
  bool m_isJoinRow = false;
  QString m_searchRoot;

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

  // ── 列表页展示 ──
  QComboBox *m_displayTypeCombo = nullptr;
  QComboBox *m_widthCombo = nullptr;
  QSpinBox *m_listThumbW = nullptr;
  QSpinBox *m_listThumbH = nullptr;

  // ── 编辑表单 ──
  QCheckBox *m_editEditableCheck = nullptr;  ///< 编辑时可编辑（false=表单内只读展示）
  QComboBox *m_widgetCombo = nullptr;
  QLineEdit *m_placeholderEdit = nullptr;   ///< 占位提示（input/textarea/image）
  QSpinBox *m_maxlengthSpin = nullptr;      ///< 最大长度（input，0=不限）
  QSpinBox *m_textareaRowsSpin = nullptr;   ///< 行数（textarea，0=缺省 4）
  QDoubleSpinBox *m_minSpin = nullptr;      ///< 最小值（int/float，0=不限）
  QDoubleSpinBox *m_maxSpin = nullptr;      ///< 最大值（int/float，0=不限）
  QSpinBox *m_precisionSpin = nullptr;      ///< 小数位（float，0=缺省 2）
  QComboBox *m_dateFormatCombo = nullptr;   ///< 日期格式覆盖（空=按类型推导）
  QSpinBox *m_formSpanSpin = nullptr;       ///< 编辑表单栅格 span（0=缺省 12）
  QLineEdit *m_formDefaultEdit = nullptr;   ///< 表单新建预填值
  QComboBox *m_uploadSourceCombo = nullptr;
  QSpinBox *m_editThumbW = nullptr;
  QSpinBox *m_editThumbH = nullptr;

  QFormLayout *m_form = nullptr;  ///< 面板表单（行级显隐用）
  QLabel *m_editSep = nullptr;    ///< ── 编辑表单 ── 分隔行（联表行整区折叠）
};
