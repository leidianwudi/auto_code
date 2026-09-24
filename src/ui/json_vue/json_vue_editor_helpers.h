/**
 * @file json_vue_editor_helpers.h
 * @brief json_vue 编辑器共享辅助函数（header-only）
 *
 * 原 json_vue_editor.cpp 匿名 namespace 中的辅助逻辑，被拆分后的多个实现文件
 * （json_vue_editor.cpp / _http.cpp / _columns.cpp / _config.cpp）共用：
 * - 自绘无边框下拉框 NoBorderCombo 与工厂 newTableCombo
 * - 列配置 / 查询字段 / 操作按钮配置的 QPushButton 动态属性存取与摘要文本
 * - 接口鉴权配置文件常量 kApiAuthDataAcFile
 */

#pragma once

#include <QComboBox>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QPainter>
#include <QStyleOption>
#include <QTableWidget>

#include "json_vue_model.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_style.h"

/// 接口鉴权/连接配置文件（baseUrl、authHeader、postData），从 .jsonvue 所在目录向上查找
inline const QString kApiAuthDataAcFile = QStringLiteral("api_auth_data.ac");

/// 自绘 QComboBox：无边框，只有文字 + 箭头，文字区域不受原生样式限制。
/// 弹层高度保证复用通用机制（AuiComboBox::ensurePopupFit，全局过滤器兜底），
/// 这里仅在弹出前显式调用一次，让固定高度赶在 Qt 原生几何计算之前生效。
class NoBorderCombo : public QComboBox {
public:
  explicit NoBorderCombo(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
  void showPopup() override {
    AuiComboBox::ensurePopupFit(this);
    QComboBox::showPopup();
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 所在行选中联动：表格的行选中背景会被不透明的控件盖住（只有文本项列能看到
    // 高亮）。这里检测所在行的选中态，用表格的选中色绘制背景与文字，保证
    // "点任意单元格 = 整行选中"的视觉反馈完整。
    QWidget *vp = parentWidget();
    auto *table = vp ? qobject_cast<QTableWidget *>(vp->parentWidget()) : nullptr;
    bool rowSelected = false;
    if (table && vp) {
      const int row = table->rowAt(mapTo(vp, QPoint(0, 0)).y());
      rowSelected = row >= 0 && table->selectionModel()->isRowSelected(row);
    }

    QColor bg;
    QColor fg;
    if (rowSelected) {
      bg = table->palette().color(QPalette::Highlight);
      fg = table->palette().color(QPalette::HighlightedText);
    } else if (hasFocus()) {
      bg = AuiStyle::listSelectionBackground();
      fg = AuiStyle::textColor();
    } else {
      bg = AuiStyle::panelBackground();
      fg = AuiStyle::textColor();
    }

    // 背景（直接用主题色，深色主题下也能看清）
    p.fillRect(rect(), bg);

    // 文字
    QRect textRect = rect().adjusted(2, 0, -18, 0);
    p.setPen(fg);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, currentText());

    // 向下箭头
    QRect arrowRect(rect().right() - 18, 0, 18, rect().height());
    QStyleOption arrowOpt;
    arrowOpt.rect = arrowRect.adjusted(3, 0, -3, 0);
    arrowOpt.palette = palette();
    arrowOpt.state = QStyle::State_Enabled;
    p.setPen(fg);
    style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrowOpt, &p, this);
  }
};

/// 创建用于表格单元格的 QComboBox（自绘，无边框，文字区域最大）
inline QComboBox *newTableCombo() { return new NoBorderCombo; }

/// 将 ColumnConfig 的所有配置存储到 QPushButton 的动态属性中
inline void storeColumnConfig(QPushButton *btn, const ColumnConfig &col) {
  btn->setProperty(JsonVueKey::kSelectUrl, col.selectUrl);
  btn->setProperty(JsonVueKey::kSelectSourceFile, col.selectSourceFile);
  btn->setProperty(JsonVueKey::kSelectSourceId, col.selectSourceId);
  btn->setProperty(JsonVueKey::kSelectValueField, col.selectValueField);
  btn->setProperty(JsonVueKey::kSelectLabelField, col.selectLabelField);
  btn->setProperty(JsonVueKey::kSelectPaged, col.selectPaged);
  btn->setProperty(JsonVueKey::kSelectPageKey, col.selectPageKey);
  btn->setProperty(JsonVueKey::kSelectPageSizeKey, col.selectPageSizeKey);
  btn->setProperty(JsonVueKey::kSelectPageSize, col.selectPageSize);
  btn->setProperty(JsonVueKey::kSelectSearchTitle, col.selectSearchTitle);
  btn->setProperty(JsonVueKey::kSelectSearchField, col.selectSearchField);
  btn->setProperty(JsonVueKey::kSelectMethod, col.selectMethod);
  btn->setProperty(JsonVueKey::kPlaceholder, col.placeholder);
  btn->setProperty(JsonVueKey::kMaxlength, col.maxlength);
  btn->setProperty(JsonVueKey::kMinValue, col.minValue);
  btn->setProperty(JsonVueKey::kMaxValue, col.maxValue);
  btn->setProperty(JsonVueKey::kPrecision, col.precision);
  btn->setProperty(JsonVueKey::kDateFormat, col.dateFormat);
  btn->setProperty(JsonVueKey::kTextareaRows, col.textareaRows);
  btn->setProperty(JsonVueKey::kRequired, col.required);
  btn->setProperty(JsonVueKey::kColumnWidth, col.columnWidth);
  btn->setProperty(JsonVueKey::kColumnFixed, col.columnFixed);
  btn->setProperty(JsonVueKey::kFormatter, col.formatter);
  btn->setProperty(JsonVueKey::kFormSpan, col.formSpan);
  btn->setProperty(JsonVueKey::kDisplayType, col.displayType);
  // tagItems 存为 QVariantList（每个元素为 QVariantMap）
  QVariantList tagItemsVar;
  for (const auto &t : col.tagItems) {
    QVariantMap m;
    m[JsonVueKey::kTagValue] = t.value;
    m[JsonVueKey::kTagText] = t.text;
    m[JsonVueKey::kTagColor] = t.color;
    tagItemsVar.append(m);
  }
  btn->setProperty(JsonVueKey::kTagItems, tagItemsVar);
  btn->setProperty(JsonVueKey::kBoolTrueText, col.boolTrueText);
  btn->setProperty(JsonVueKey::kBoolFalseText, col.boolFalseText);
  btn->setProperty(JsonVueKey::kBoolSourceFile, col.boolSourceFile);
  btn->setProperty(JsonVueKey::kBoolSourceId, col.boolSourceId);
  // 字段取值域（方案 B：列渲染/编辑/查询三处共享的字段级数据源声明）
  btn->setProperty(JsonVueKey::kDomainType, col.domainType);
  btn->setProperty(JsonVueKey::kDomainSourceFile, col.domainSourceFile);
  btn->setProperty(JsonVueKey::kDomainSourceId, col.domainSourceId);
  btn->setProperty(JsonVueKey::kDomainUrl, col.domainUrl);
  btn->setProperty(JsonVueKey::kDefaultValue, col.defaultValue);
  btn->setProperty(JsonVueKey::kDefaultSort, col.defaultSort);
  // 编辑样式/编辑可编辑/开关可编辑（从表格列移入对话框后，存到 property 供读取）
  btn->setProperty(JsonVueKey::kEditStyle, editStyleToString(col.editStyle));
  btn->setProperty(JsonVueKey::kEditEditable, col.editEditable);
  btn->setProperty(JsonVueKey::kSwitchEditable, col.switchEditable);
}

/// 从 QPushButton 的动态属性读取 ColumnConfig 的所有配置
inline void readColumnConfig(QPushButton *btn, ColumnConfig &col) {
  col.selectUrl = btn->property(JsonVueKey::kSelectUrl).toString();
  col.selectSourceFile = btn->property(JsonVueKey::kSelectSourceFile).toString();
  col.selectSourceId = btn->property(JsonVueKey::kSelectSourceId).toString();
  col.selectValueField = btn->property(JsonVueKey::kSelectValueField).toString();
  col.selectLabelField = btn->property(JsonVueKey::kSelectLabelField).toString();
  col.selectPaged = btn->property(JsonVueKey::kSelectPaged).toBool();
  col.selectPageKey = btn->property(JsonVueKey::kSelectPageKey).toString();
  col.selectPageSizeKey = btn->property(JsonVueKey::kSelectPageSizeKey).toString();
  col.selectPageSize = btn->property(JsonVueKey::kSelectPageSize).toInt();
  col.selectSearchTitle = btn->property(JsonVueKey::kSelectSearchTitle).toString();
  col.selectSearchField = btn->property(JsonVueKey::kSelectSearchField).toString();
  col.selectMethod = btn->property(JsonVueKey::kSelectMethod).toString();
  col.placeholder = btn->property(JsonVueKey::kPlaceholder).toString();
  col.maxlength = btn->property(JsonVueKey::kMaxlength).toInt();
  col.minValue = btn->property(JsonVueKey::kMinValue).toDouble();
  col.maxValue = btn->property(JsonVueKey::kMaxValue).toDouble();
  col.precision = btn->property(JsonVueKey::kPrecision).isValid()
                      ? btn->property(JsonVueKey::kPrecision).toInt()
                      : 2;
  col.dateFormat = btn->property(JsonVueKey::kDateFormat).toString();
  col.textareaRows = btn->property(JsonVueKey::kTextareaRows).isValid()
                         ? btn->property(JsonVueKey::kTextareaRows).toInt()
                         : 3;
  col.required = btn->property(JsonVueKey::kRequired).toBool();
  col.columnWidth = btn->property(JsonVueKey::kColumnWidth).toInt();
  col.columnFixed = btn->property(JsonVueKey::kColumnFixed).toString();
  col.formatter = btn->property(JsonVueKey::kFormatter).toString();
  col.formSpan = btn->property(JsonVueKey::kFormSpan).isValid()
                     ? btn->property(JsonVueKey::kFormSpan).toInt()
                     : 12;
  col.displayType = btn->property(JsonVueKey::kDisplayType).toString();
  // tagItems 从 QVariantList 读取
  col.tagItems.clear();
  const QVariantList tagItemsVar = btn->property(JsonVueKey::kTagItems).toList();
  for (const auto &v : tagItemsVar) {
    QVariantMap m = v.toMap();
    TagItem t;
    t.value = m.value(JsonVueKey::kTagValue).toString();
    t.text = m.value(JsonVueKey::kTagText).toString();
    t.color = m.value(JsonVueKey::kTagColor).toString();
    col.tagItems.append(t);
  }
  col.boolTrueText = btn->property(JsonVueKey::kBoolTrueText).toString();
  col.boolFalseText = btn->property(JsonVueKey::kBoolFalseText).toString();
  col.boolSourceFile = btn->property(JsonVueKey::kBoolSourceFile).toString();
  col.boolSourceId = btn->property(JsonVueKey::kBoolSourceId).toString();
  // 字段取值域（方案 B）
  col.domainType = btn->property(JsonVueKey::kDomainType).toString();
  col.domainSourceFile = btn->property(JsonVueKey::kDomainSourceFile).toString();
  col.domainSourceId = btn->property(JsonVueKey::kDomainSourceId).toString();
  col.domainUrl = btn->property(JsonVueKey::kDomainUrl).toString();
  col.defaultValue = btn->property(JsonVueKey::kDefaultValue).toString();
  col.defaultSort = btn->property(JsonVueKey::kDefaultSort).toString();
  // 编辑样式/编辑可编辑/开关可编辑
  col.editStyle = stringToEditStyle(btn->property(JsonVueKey::kEditStyle).toString());
  col.editEditable = btn->property(JsonVueKey::kEditEditable).isValid()
                         ? btn->property(JsonVueKey::kEditEditable).toBool()
                         : true;
  col.switchEditable = btn->property(JsonVueKey::kSwitchEditable).toBool();
}

/// 编辑样式摘要片段（编辑/显示相关的行内提示，供 columnConfigSummary 复用）
inline void appendEditStyleSummary(QStringList &parts, const ColumnConfig &col) {
  switch (col.editStyle) {
    case EditStyle::Text:
      if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
      if (col.maxlength > 0) parts << QStringLiteral("max:%1").arg(col.maxlength);
      if (!col.placeholder.isEmpty()) parts << col.placeholder;
      break;
    case EditStyle::Int:
      if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
      if (col.minValue != 0 || col.maxValue != 0) {
        parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
      }
      break;
    case EditStyle::Float:
    case EditStyle::Money:
      if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
      parts << QStringLiteral("%1位小数").arg(col.precision);
      if (col.minValue != 0 || col.maxValue != 0) {
        parts << QStringLiteral("%1~%2").arg(col.minValue).arg(col.maxValue);
      }
      break;
    case EditStyle::Date: {
      if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
      QString fmt = col.dateFormat;
      if (fmt == JsonVueStyle::kDatetime)
        parts << QString::fromUtf8(CodeConstants::UiText::kDatetimeFull);
      else if (fmt == JsonVueStyle::kDate)
        parts << QStringLiteral("年月日");
      else if (fmt == JsonVueStyle::kMonth)
        parts << QString::fromUtf8(CodeConstants::UiText::kYearMonth);
      else if (fmt == JsonVueStyle::kYear)
        parts << QStringLiteral("年");
      else if (fmt == JsonVueStyle::kDaterange)
        parts << QString::fromUtf8(CodeConstants::UiText::kDateRange);
      break;
    }
    case EditStyle::Select:
      if (!col.selectSourceFile.isEmpty()) {
        // 引用 jsonsource 数据源：显示文件名 + 数据源字段映射
        QString fileBase = QFileInfo(col.selectSourceFile).completeBaseName();
        if (!col.selectValueField.isEmpty() && !col.selectLabelField.isEmpty()) {
          parts << QStringLiteral("%1:%2→%3").arg(fileBase, col.selectValueField,
                                                   col.selectLabelField);
        } else {
          parts << QStringLiteral("%1").arg(fileBase);
        }
      } else if (!col.selectUrl.isEmpty()) {
        if (!col.selectValueField.isEmpty() && !col.selectLabelField.isEmpty()) {
          parts << QStringLiteral("%1→%2").arg(col.selectValueField, col.selectLabelField);
        } else {
          parts << QStringLiteral("已配置");
        }
      }
      break;
    case EditStyle::TextArea:
      if (col.required) parts << QString::fromUtf8(CodeConstants::UiText::kRequired);
      parts << QStringLiteral("%1行").arg(col.textareaRows);
      if (!col.placeholder.isEmpty()) parts << col.placeholder;
      break;
    default:
      break;
  }
}

/// 取值域摘要文本（方案 B：字段级数据源声明，供列配置表「取值域」列显示）
inline QString domainSummary(const ColumnConfig &col) {
  if (col.domainType == QString::fromLatin1(JsonVueDomain::kEnum)) {
    return QStringLiteral("枚举·") + QFileInfo(col.domainSourceFile).completeBaseName();
  }
  if (col.domainType == QString::fromLatin1(JsonVueDomain::kStatic)) {
    return QStringLiteral("静态·") + QFileInfo(col.domainSourceFile).completeBaseName();
  }
  if (col.domainType == QString::fromLatin1(JsonVueDomain::kRemote)) {
    return QStringLiteral("远程·") + col.domainUrl;
  }
  return QStringLiteral("—");
}

/// 生成列配置摘要文本
inline QString columnConfigSummary(const ColumnConfig &col) {
  QStringList parts;
  // 取值域（方案 B：字段级数据源声明）置于摘要最前——字段级属性优先于展示/编辑样式；
  // 未声明（"—"）时不显示，保持未配置列的摘要干净
  const QString dom = domainSummary(col);
  if (dom != QStringLiteral("—")) {
    parts << QStringLiteral("取值域:%1").arg(dom);
  }
  // 关键配置：显示样式、编辑样式始终显示，便于一眼看出列的渲染/编辑方式
  // displayType 为空字符串时表示"纯文本(text)"
  parts << QStringLiteral("显示:%1").arg(
      col.displayType.isEmpty() ? QString::fromLatin1(JsonVueStyle::kText) : col.displayType);
  parts << QStringLiteral("编辑:%1").arg(editStyleToString(col.editStyle));
  // 判断是否为开关样式：(displayType == boolean || tag) && switchEditable
  bool isSwitch =
      (col.displayType == JsonVueStyle::kBoolean || col.displayType == JsonVueStyle::kTag) &&
      col.switchEditable;
  if (isSwitch) {
    parts << QStringLiteral("开关");
  }
  appendEditStyleSummary(parts, col);

  // 通用配置
  if (col.columnWidth > 0) parts << QStringLiteral("宽%1").arg(col.columnWidth);
  if (!col.columnFixed.isEmpty()) parts << QStringLiteral("固定%1").arg(col.columnFixed);
  if (!col.formatter.isEmpty()) parts << QStringLiteral("格式:%1").arg(col.formatter);
  if (col.formSpan != 12) parts << QStringLiteral("span:%1").arg(col.formSpan);
  if (!col.defaultValue.isEmpty()) parts << QStringLiteral("默认:%1").arg(col.defaultValue);
  if (!col.defaultSort.isEmpty()) parts << QStringLiteral("排序:%1").arg(col.defaultSort);

  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}

/// 将 QueryFieldConfig 的所有配置存储到 QPushButton 的动态属性中
inline void storeQueryConfig(QPushButton *btn, const QueryFieldConfig &q) {
  btn->setProperty(JsonVueKey::kSelectUrl, q.selectUrl);
  btn->setProperty(JsonVueKey::kSelectSourceFile, q.selectSourceFile);
  btn->setProperty(JsonVueKey::kSelectSourceId, q.selectSourceId);
  btn->setProperty(JsonVueKey::kSelectValueField, q.selectValueField);
  btn->setProperty(JsonVueKey::kSelectLabelField, q.selectLabelField);
  btn->setProperty(JsonVueKey::kSelectPaged, q.selectPaged);
  btn->setProperty(JsonVueKey::kSelectPageKey, q.selectPageKey);
  btn->setProperty(JsonVueKey::kSelectPageSizeKey, q.selectPageSizeKey);
  btn->setProperty(JsonVueKey::kSelectPageSize, q.selectPageSize);
  btn->setProperty(JsonVueKey::kSelectSearchTitle, q.selectSearchTitle);
  btn->setProperty(JsonVueKey::kSelectSearchField, q.selectSearchField);
  btn->setProperty(JsonVueKey::kSelectMethod, q.selectMethod);
  btn->setProperty(JsonVueKey::kDomainInherit, q.domainInherit);
  btn->setProperty(JsonVueKey::kPlaceholder, q.placeholder);
  btn->setProperty(JsonVueKey::kDateFormat, q.dateFormat);
}

/// 从 QPushButton 的动态属性读取 QueryFieldConfig 的所有配置
inline void readQueryConfig(QPushButton *btn, QueryFieldConfig &q) {
  q.selectUrl = btn->property(JsonVueKey::kSelectUrl).toString();
  q.selectSourceFile = btn->property(JsonVueKey::kSelectSourceFile).toString();
  q.selectSourceId = btn->property(JsonVueKey::kSelectSourceId).toString();
  q.selectValueField = btn->property(JsonVueKey::kSelectValueField).toString();
  q.selectLabelField = btn->property(JsonVueKey::kSelectLabelField).toString();
  q.selectPaged = btn->property(JsonVueKey::kSelectPaged).toBool();
  q.selectPageKey = btn->property(JsonVueKey::kSelectPageKey).toString();
  q.selectPageSizeKey = btn->property(JsonVueKey::kSelectPageSizeKey).toString();
  q.selectPageSize = btn->property(JsonVueKey::kSelectPageSize).toInt();
  q.selectSearchTitle = btn->property(JsonVueKey::kSelectSearchTitle).toString();
  q.selectSearchField = btn->property(JsonVueKey::kSelectSearchField).toString();
  q.selectMethod = btn->property(JsonVueKey::kSelectMethod).toString();
  // 查询筛选是否沿用同名列取值域（方案 B：无此属性时默认 true，与旧数据行为一致）
  q.domainInherit = btn->property(JsonVueKey::kDomainInherit).isValid()
                        ? btn->property(JsonVueKey::kDomainInherit).toBool()
                        : true;
  q.placeholder = btn->property(JsonVueKey::kPlaceholder).toString();
  q.dateFormat = btn->property(JsonVueKey::kDateFormat).toString();
}

/// 生成查询字段配置摘要文本
inline QString queryConfigSummary(const QueryFieldConfig &q) {
  QStringList parts;
  switch (q.inputStyle) {
    case QueryInputStyle::Text:
      if (!q.placeholder.isEmpty()) parts << q.placeholder;
      break;
    case QueryInputStyle::Date: {
      QString fmt = q.dateFormat;
      if (fmt == JsonVueStyle::kDatetime)
        parts << QString::fromUtf8(CodeConstants::UiText::kDatetimeFull);
      else if (fmt == JsonVueStyle::kDate)
        parts << QStringLiteral("年月日");
      else if (fmt == JsonVueStyle::kMonth)
        parts << QString::fromUtf8(CodeConstants::UiText::kYearMonth);
      else if (fmt == JsonVueStyle::kYear)
        parts << QStringLiteral("年");
      else if (fmt == JsonVueStyle::kDaterange)
        parts << QString::fromUtf8(CodeConstants::UiText::kDateRange);
      break;
    }
    case QueryInputStyle::Select:
      // 查询筛选默认沿用同名列的取值域（方案 B，含布尔枚举列自动带「全部」空选项）；
      // 取消继承时使用字段自身的独立数据源
      if (q.domainInherit) {
        parts << QStringLiteral("沿用列取值域");
      } else if (!q.selectUrl.isEmpty() || !q.selectSourceFile.isEmpty()) {
        parts << QStringLiteral("独立数据源");
      } else {
        parts << QStringLiteral("未配置");
      }
      break;
  }
  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}

/// 生成操作按钮配置摘要文本（按行为类型显示关键配置）
inline QString buttonConfigSummary(const ButtonConfig &btn) {
  QStringList parts;
  switch (btn.actionType) {
    case ButtonActionType::Ajax:
      if (!btn.apiName.isEmpty()) parts << btn.apiName;
      break;
    case ButtonActionType::Confirm:
      if (!btn.apiName.isEmpty()) parts << btn.apiName;
      if (!btn.confirmText.isEmpty()) parts << QStringLiteral("确认:%1").arg(btn.confirmText);
      break;
    case ButtonActionType::Dialog:
      if (!btn.dialogTitle.isEmpty()) parts << btn.dialogTitle;
      parts << QStringLiteral("字段数:%1").arg(btn.dialogFields.size());
      break;
    case ButtonActionType::Link:
      if (!btn.linkPath.isEmpty()) parts << btn.linkPath;
      break;
  }
  // 按钮样式（primary/success/danger/warning 等）
  if (!btn.buttonType.isEmpty()) parts << QStringLiteral("样式:%1").arg(btn.buttonType);
  if (parts.isEmpty()) return QStringLiteral("⚙");
  return QStringLiteral("⚙ ") + parts.join(QStringLiteral(", "));
}
