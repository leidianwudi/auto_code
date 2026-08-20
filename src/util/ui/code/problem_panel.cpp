/**
 * @file problem_panel.cpp
 * @brief 问题列表控件实现
 */

#include "problem_panel.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOption>
#include <algorithm>

#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

ProblemPanel::ProblemPanel(QWidget *parent) : QTreeWidget(parent) {
  // 只读列表，支持鼠标框选复制
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setRootIsDecorated(false);
  setItemsExpandable(false);
  setUniformRowHeights(true);
  setTextElideMode(Qt::ElideRight);
  setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

  setColumnCount(2);
  // 隐藏列头：问题面板只展示内容行（问题描述 + 位置），不显示「问题/位置」标题行
  setHeaderHidden(true);
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

  // 主题化样式 + 字体（背景/文字/选中色跟随主题，字体跟随「代码字体」设置）
  reloadStyle();

  // 字体大小 / 字体族变化时即时刷新
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this, &ProblemPanel::reloadStyle);
}

// ════════════════════════════════════════════════════════════
//  数据填充
// ════════════════════════════════════════════════════════════

void ProblemPanel::setIssues(const QVector<IssueItem> &issues) {
  clear();
  m_lastIssues = issues;
  if (issues.isEmpty()) return;

  // 按文件分组，同文件内按行号升序排序（保证稳定的展示顺序）
  QMap<QString, QVector<IssueItem>> grouped;
  for (const IssueItem &it : issues) grouped[it.filePath].append(it);
  for (auto g = grouped.begin(); g != grouped.end(); ++g) {
    std::sort(g.value().begin(), g.value().end(),
              [](const IssueItem &a, const IssueItem &b) { return a.line < b.line; });
  }

  for (auto g = grouped.begin(); g != grouped.end(); ++g) {
    for (const IssueItem &it : g.value()) {
      const bool hasLine = it.line > 0;

      QTreeWidgetItem *item = new QTreeWidgetItem(this);
      // 第一列：问题描述（错误/警告按严重级别着色）
      const QColor color = (it.severity == ValidationResult::kError) ? AuiStyle::errorTextColor()
                                                                     : AuiStyle::warningColor();
      item->setText(0, it.message);
      item->setForeground(0, color);
      item->setIcon(0, style()->standardIcon(it.severity == ValidationResult::kError
                                                 ? QStyle::SP_MessageBoxCritical
                                                 : QStyle::SP_MessageBoxWarning));
      // 第二列：位置（文件名:行）
      item->setText(1,
                    hasLine ? QStringLiteral("%1:%2").arg(it.fileName).arg(it.line) : it.fileName);
      item->setToolTip(0, it.message);

      // 存储定位信息（供双击跨文件跳转）
      item->setData(0, Qt::UserRole, it.line);
      item->setData(1, Qt::UserRole, it.line);
      item->setData(0, Qt::UserRole + 1, it.filePath);
      item->setData(1, Qt::UserRole + 1, it.filePath);
    }
  }
}

void ProblemPanel::clearIssues() {
  clear();
  m_lastIssues.clear();
}

// ════════════════════════════════════════════════════════════
//  主题 / 字体刷新
// ════════════════════════════════════════════════════════════

void ProblemPanel::reloadStyle() {
  // 字体（大小/字体族）随「代码字体」设置更新
  QFont f = font();
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.code"));
  if (!fam.isEmpty()) f.setFamily(fam);
  f.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.code")));
  setFont(f);

  // 行高随字体自适应，避免字号变大后文字被裁剪
  const int itemH = qMax(20, fontMetrics().height() + 6);

  // 背景 / 文字 / 选中色随当前主题重建
  setStyleSheet(QStringLiteral("QTreeWidget { background: %1; color: %2; border: none; }"
                               "QTreeWidget::item { height: %3px; }"
                               "QTreeWidget::item:selected { background: %4; color: %2; }")
                    .arg(AuiStyle::panelBackground().name(), AuiStyle::textColor().name())
                    .arg(itemH)
                    .arg(AuiStyle::listSelectionBackground().name()));

  // 用当前主题色重新上色已有条目（错误红 / 警告橙）
  if (!m_lastIssues.isEmpty()) setIssues(m_lastIssues);
  viewport()->update();
}

// ════════════════════════════════════════════════════════════
//  双击跳转
// ════════════════════════════════════════════════════════════

void ProblemPanel::mouseDoubleClickEvent(QMouseEvent *event) {
  QTreeWidgetItem *item = itemAt(event->position().toPoint());
  if (item) {
    const QString filePath = item->data(0, Qt::UserRole + 1).toString();
    const int line = item->data(0, Qt::UserRole).toInt();
    emit issueActivated(filePath, line);
    return;
  }
  QTreeWidget::mouseDoubleClickEvent(event);
}

// ════════════════════════════════════════════════════════════
//  复制支持（QTreeWidget 原生不支持 Ctrl+C，这里补齐）
// ════════════════════════════════════════════════════════════

void ProblemPanel::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  // 复制选中问题行（支持多选）
  QAction *copySelAct = menu.addAction(QStringLiteral("复制选中问题"));
  copySelAct->setShortcut(QKeySequence::Copy);
  copySelAct->setEnabled(!selectedItems().isEmpty());

  // 复制全部问题行
  QAction *copyAllAct = menu.addAction(QStringLiteral("复制全部问题"));
  copyAllAct->setEnabled(topLevelItemCount() > 0);

  QAction *chosen = menu.exec(event->globalPos());
  if (!chosen) return;

  if (chosen == copySelAct) {
    QGuiApplication::clipboard()->setText(formatIssuesForCopy(selectedItems()));
  } else if (chosen == copyAllAct) {
    QList<QTreeWidgetItem *> all;
    for (int i = 0; i < topLevelItemCount(); ++i) all.append(topLevelItem(i));
    QGuiApplication::clipboard()->setText(formatIssuesForCopy(all));
  }
}

void ProblemPanel::keyPressEvent(QKeyEvent *event) {
  // Ctrl+C：复制选中的问题行
  if (event->matches(QKeySequence::Copy) && !selectedItems().isEmpty()) {
    QGuiApplication::clipboard()->setText(formatIssuesForCopy(selectedItems()));
    return;
  }
  QTreeWidget::keyPressEvent(event);
}

QString ProblemPanel::formatIssuesForCopy(const QList<QTreeWidgetItem *> &items) const {
  QStringList lines;
  for (const QTreeWidgetItem *item : items) {
    // 格式：消息 (文件名:行)
    lines << QStringLiteral("%1 (%2)").arg(item->text(0), item->text(1));
  }
  return lines.join(QLatin1Char('\n'));
}
