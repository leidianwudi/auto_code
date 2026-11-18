/**
 * @file aui_config_dialog.cpp
 * @brief json_vue 配置对话框公共工具实现
 */

#include "aui_config_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QLineEdit>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
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

/// 配置表格复制支持事件过滤器：Ctrl+C 把选中单元格按行列组织为 TSV 文本
/// （QTableWidget 原生不支持复制；只读条目也可选中变色后复制，操作与可编辑
/// 单元格一致，只是不进入编辑态、无输入光标）
class TableCopyFilter : public QObject {
public:
  explicit TableCopyFilter(QObject *parent) : QObject(parent) {}

protected:
  bool eventFilter(QObject *watched, QEvent *ev) override {
    if (ev->type() != QEvent::KeyPress) return false;
    auto *ke = static_cast<QKeyEvent *>(ev);
    if (!ke->matches(QKeySequence::Copy)) return false;
    auto *t = qobject_cast<QTableWidget *>(watched);
    if (!t || t->selectedItems().isEmpty()) return false;
    // 选中项按 行号→列号 排列成网格（跳过的列/行留空，粘贴到表格/Excel 对齐）
    QMap<int, QMap<int, const QTableWidgetItem *>> grid;
    for (const auto *it : t->selectedItems()) grid[it->row()][it->column()] = it;
    QStringList lines;
    for (auto rit = grid.constBegin(); rit != grid.constEnd(); ++rit) {
      QStringList cells;
      for (auto cit = rit.value().constBegin(); cit != rit.value().constEnd(); ++cit)
        cells << cit.value()->text();
      lines << cells.join(QLatin1Char('\t'));
    }
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
    return true;
  }
};

/// QTableWidget 的勾选框默认贴单元格左缘（setTextAlignment 只作用于文本，
/// 管不到勾选框）。配置表的复选框列均为"无文本纯勾选"，本委托把这类单元格
/// 的勾选框绘制与点击命中区一起水平居中；普通文本 / 控件单元格不受影响。
/// 另：挂了常驻控件的单元格（AuiCellReadBox / AuiCellEditCombo）由控件负责
/// 显示文字，这里跳过 item 文字绘制——两态文字同源，双击进入编辑态不位移，
/// 也不会出现 item 文字 + 控件文字双层错位（重影）。
class CenterCheckDelegate : public QStyledItemDelegate {
public:
  explicit CenterCheckDelegate(QTableWidget *table)
      : QStyledItemDelegate(table), m_table(table) {}

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
    const bool hasWidget = m_table && m_table->cellWidget(index.row(), index.column());
    QStyleOptionViewItem opt = isBareCheckCell(index) ? shrunkOption(option) : option;
    // 只读条目（ItemIsSelectable 无 ItemIsEditable）的选中高亮在 QSS 环境
    // 下绘制不可靠（可编辑条目靠"进入编辑态的输入框样式"才显得有反馈）：
    // 委托层显式铺高亮背景 + 换高亮文字色，保证选中反馈与可编辑条目一致，
    // 只读单元格同样"选中变色 → Ctrl+C 复制"（编辑态输入框由基类绘制）
    if (opt.state & QStyle::State_Selected) {
      painter->fillRect(opt.rect, opt.palette.highlight());
      opt.palette.setColor(QPalette::Text, opt.palette.highlightedText().color());
    }
    if (hasWidget) {
      // 只画背景/勾选框，文字留给常驻控件（避免与控件文字双层错位）
      QStyleOptionViewItem o = opt;
      initStyleOption(&o, index);
      o.text.clear();
      QStyle *style = o.widget ? o.widget->style() : QApplication::style();
      style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);
      return;
    }
    // 文字统一从单元格左缘 +4px 起绘（与输入框控件实际文字起点同源：QSS
    // padding 0px 2px + QLineEdit 内部 2px 边距）：双击进入编辑态时文字不位移。
    // QSS/原生样式对 item 文字的内边距不一致，靠改 padding 对不齐，故由委托
    // 统一控制绘制位置。图标（setIcon 的类型徽标）同样自绘在文字左侧，避免与
    // 文字重叠
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, index);
    // 选中背景由委托显式填充：全局 QSS 生效后样式层的 item:selected 背景
    // 不绘制（项目多次验证），纯 item 单元格（无 cellWidget）会"选中但无高亮"。
    // 先铺底色再交给样式画勾选框/焦点框，两者互不遮挡
    if (o.state & QStyle::State_Selected) {
      painter->fillRect(o.rect, AuiStyle::listSelectionBackground());
    }
    const QString text = o.text;
    const QIcon icon = o.icon;
    o.text.clear();
    o.icon = QIcon();  // 图标由下面自绘（样式层画 icon 的位置与自绘文字不一致）
    QStyle *style = o.widget ? o.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, painter, o.widget);
    int left = 4;
    if (!icon.isNull()) {
      const int isz = o.decorationSize.isValid() ? o.decorationSize.width() : 16;
      const QRect ir(o.rect.left() + left, o.rect.top() + (o.rect.height() - isz) / 2, isz, isz);
      icon.paint(painter, ir, Qt::AlignCenter,
                 (o.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
      left += isz + 4;  // 图标右侧留 4px 与文字分隔
    }
    if (!text.isEmpty()) {
      const QRect tr = o.rect.adjusted(left, 0, -4, 0);
      const QFontMetrics fm(o.font);
      painter->save();
      painter->setPen(o.palette.color(QPalette::Text));
      painter->setFont(o.font);
      painter->drawText(tr, static_cast<int>(o.displayAlignment),
                        fm.elidedText(text, Qt::ElideRight, tr.width()));
      painter->restore();
    }
  }

  /// 单元格编辑器：无边框、文字起点与 item 自绘文字一致（双击不位移）
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                        const QModelIndex &) const override {
    auto *edit = new QLineEdit(parent);
    edit->setFrame(false);
    edit->setStyleSheet(QStringLiteral("QLineEdit {"
                                       "  border: none;"
                                       "  padding: 0px 2px;"
                                       "}"));
    return edit;
  }

  bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                   const QModelIndex &index) override {
    // 命中区与绘制区用同一矩形，保证点在居中勾选框上能切换、点空白处不误触
    return QStyledItemDelegate::editorEvent(
        event, model, isBareCheckCell(index) ? shrunkOption(option) : option, index);
  }

private:
  QTableWidget *m_table = nullptr;  ///< 所属表格（查单元格是否挂常驻控件）
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
  // Ctrl+C 复制选中单元格（TSV 网格进剪贴板；只读条目同样可选中复制）
  table->installEventFilter(new TableCopyFilter(table));
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
