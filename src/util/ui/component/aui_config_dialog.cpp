/**
 * @file aui_config_dialog.cpp
 * @brief json_vue 配置对话框公共工具实现
 */

#include "aui_config_dialog.h"

#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPushButton>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include "src/util/common/code_constants.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_combo_box.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/component/aui_tree_combo.h"

// ════════════════════════════════════════════════════════════
//  无边框对话框框架
// ════════════════════════════════════════════════════════════

ConfigDialogFrame beginConfigDialog(QDialog *dialog, const QString &title, const QMargins &margins,
                                    int spacing) {
  dialog->setWindowTitle(title);
  AuiWindow::setupFramelessDialog(dialog);

  TitleBarOptions opts;
  opts.title = title;
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  TitleBarResult tb = AuiWindow::createTitleBar(dialog, opts);

  ConfigDialogFrame frame;
  frame.titleBar = tb.titleBar;
  frame.contentWidget = new QWidget;
  frame.contentLayout = new QVBoxLayout(frame.contentWidget);
  frame.contentLayout->setContentsMargins(margins);
  frame.contentLayout->setSpacing(spacing);
  return frame;
}

void finishConfigDialog(QDialog *dialog, const ConfigDialogFrame &frame) {
  // 底部按钮在业务内容之后添加，保证显示在对话框底部
  auto btns = AuiButton::createDialogButtons(dialog);
  // 确定 → accept，取消 → reject（此前遗漏连接，导致三个配置对话框按钮无效）
  QObject::connect(btns.okBtn, &QPushButton::clicked, dialog, &QDialog::accept);
  if (btns.cancelBtn)
    QObject::connect(btns.cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
  frame.contentLayout->addLayout(btns.layout);
  AuiWindow::applyWindowFrame(dialog, frame.titleBar, frame.contentWidget);
}

// ════════════════════════════════════════════════════════════
//  下拉框工具
// ════════════════════════════════════════════════════════════

void comboSelectData(QComboBox *combo, const QVariant &data, int fallback) {
  if (!combo) return;
  int idx = combo->findData(data);
  if (idx < 0) idx = fallback;
  if (idx >= 0) combo->setCurrentIndex(idx);
}

QComboBox *createNumericCombo(QWidget *parent, const QList<double> &presetValues, double current) {
  auto *combo = AuiComboBox::create(parent);
  combo->setEditable(true);
  for (double v : presetValues) combo->addItem(QString::number(v), v);
  int idx = combo->findData(current);
  if (idx >= 0) {
    combo->setCurrentIndex(idx);
  } else {
    combo->setEditText(QString::number(current));
  }
  return combo;
}

double numericComboValue(const QComboBox *combo, double fallback) {
  if (!combo) return fallback;
  QVariant d = combo->currentData();
  return d.isValid() ? d.toDouble() : combo->currentText().toDouble();
}

// ════════════════════════════════════════════════════════════
//  表格工具
// ════════════════════════════════════════════════════════════

QPushButton *makeCompactButton(const QString &text, QWidget *parent) {
  auto *btn = new QPushButton(text, parent);
  btn->setStyleSheet(
      QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
                     "  padding: 2px 6px; font-size: 12px;"
                     "}"
                     "QPushButton:hover { background: %3; }"
                     "QPushButton:disabled { color: %4; background: %1; }")
          .arg(AuiStyle::background().name(), AuiStyle::borderColor().name(),
               AuiStyle::hoverBackground().name(), AuiStyle::mutedTextColor().name()));
  return btn;
}

QPushButton *makeTableDeleteButton(QTableWidget *table, int column, QWidget *parent) {
  auto *btn = makeCompactButton(QString::fromUtf8(CodeConstants::UiText::kDelete), parent);
  QObject::connect(btn, &QPushButton::clicked, table, [table, column, btn]() {
    for (int r = 0; r < table->rowCount(); ++r) {
      if (table->cellWidget(r, column) == btn) {
        table->removeRow(r);
        break;
      }
    }
  });
  return btn;
}

// ════════════════════════════════════════════════════════════
//  配置表格统一委托：纯复选框单元格的勾选框居中
// ════════════════════════════════════════════════════════════

namespace {

/// QTableWidget 的勾选框默认贴单元格左缘（setTextAlignment 只作用于文本，
/// 管不到勾选框）。配置表的复选框列均为"无文本纯勾选"，本委托把这类单元格
/// 的勾选框绘制与点击命中区一起水平居中；普通文本 / 控件单元格不受影响。
class CenterCheckDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  /// 无文本 + 有勾选态 = 纯复选框单元格
  static bool isBareCheckCell(const QModelIndex &index) {
    return index.data(Qt::CheckStateRole).isValid() &&
           index.data(Qt::DisplayRole).toString().isEmpty();
  }

  /// 以单元格水平中心为中点的窄条矩形（样式会在此矩形内垂直居中勾选框；
  /// 悬停/选中高亮也随矩形居中，视觉上即"勾选框居中"）
  static QStyleOptionViewItem shrunkOption(const QStyleOptionViewItem &option) {
    QStyleOptionViewItem opt = option;
    const int side = qMin(opt.rect.height(), 24);
    opt.rect = QRect(opt.rect.center().x() - side / 2, opt.rect.top(), side, opt.rect.height());
    return opt;
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyledItemDelegate::paint(painter, isBareCheckCell(index) ? shrunkOption(option) : option,
                               index);
  }

  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override {
    // 命中区与绘制区用同一矩形，保证点在居中勾选框上能切换、点空白处不误触
    return QStyledItemDelegate::editorEvent(
        event, model, isBareCheckCell(index) ? shrunkOption(option) : option, index);
  }
};

}  // namespace

QTableWidget *makeConfigTable(const std::initializer_list<ConfigTableColumn> &columns,
                              QWidget *parent, int minHeight, int maxHeight,
                              QAbstractItemView::SelectionBehavior selection) {
  auto *table = new QTableWidget(0, static_cast<int>(columns.size()), parent);
  QStringList headers;
  for (const auto &c : columns) headers << c.header;
  table->setHorizontalHeaderLabels(headers);
  table->verticalHeader()->setVisible(false);
  // 表头不响应点击：避免点击列标题整列选中（配置表无此操作需求），
  // 拖拽调宽不受影响（resize 走独立的光标/手柄逻辑）
  table->horizontalHeader()->setSectionsClickable(false);
  // 纯复选框单元格统一居中（绘制 + 点击命中），全项目配置表生效
  table->setItemDelegate(new CenterCheckDelegate(table));
  table->setSelectionBehavior(selection);
  table->setMinimumHeight(minHeight);
  if (maxHeight > 0) table->setMaximumHeight(maxHeight);
  int col = 0;
  for (const auto &c : columns) {
    table->horizontalHeader()->setSectionResizeMode(col, c.mode);
    if (c.width > 0) table->setColumnWidth(col, c.width);
    ++col;
  }
  return table;
}

QTableWidgetItem *makeConfigCheckItem(bool on) {
  auto *it = new QTableWidgetItem;
  it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
  it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
  it->setTextAlignment(Qt::AlignCenter);
  return it;
}
