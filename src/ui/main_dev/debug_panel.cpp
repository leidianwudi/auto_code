/**
 * @file debug_panel.cpp
 * @brief 调试面板实现
 */

#include "debug_panel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QTabWidget>
#include <QVBoxLayout>

#include "src/util/ui/component/aui_style.h"

DebugPanel::DebugPanel(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(4);

  // ── 控制栏 ──
  auto *controlBar = new QHBoxLayout;
  controlBar->setContentsMargins(4, 4, 4, 0);
  controlBar->setSpacing(4);

  m_continueBtn = new QPushButton(QStringLiteral("继续"));
  m_stepOverBtn = new QPushButton(QStringLiteral("跳过"));
  m_stepIntoBtn = new QPushButton(QStringLiteral("进入"));
  m_stepOutBtn = new QPushButton(QStringLiteral("跳出"));
  for (QPushButton *btn : {m_continueBtn, m_stepOverBtn, m_stepIntoBtn, m_stepOutBtn}) {
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    // 允许在窄侧边栏下收缩，避免撑大整个面板
    btn->setMinimumWidth(0);
    controlBar->addWidget(btn);
  }
  m_continueBtn->setToolTip(QStringLiteral("继续执行 (F5)"));
  m_stepOverBtn->setToolTip(QStringLiteral("单步跳过 (F10)"));
  m_stepIntoBtn->setToolTip(QStringLiteral("单步进入 (F11)"));
  m_stepOutBtn->setToolTip(QStringLiteral("单步跳出 (Shift+F11)"));

  m_statusLabel = new QLabel(QStringLiteral("未调试"));
  m_statusLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(AuiStyle::inactiveTabColor().name()));
  controlBar->addWidget(m_statusLabel, 1);

  root->addLayout(controlBar);

  // ── 调用栈 / 变量 双 tab ──
  auto *tabs = new QTabWidget;
  tabs->setDocumentMode(true);

  m_stackTree = new QTreeWidget;
  m_stackTree->setColumnCount(2);
  m_stackTree->setHeaderLabels({QStringLiteral("函数"), QStringLiteral("位置")});
  m_stackTree->setRootIsDecorated(false);
  m_stackTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_stackTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_stackTree->setAlternatingRowColors(true);
  tabs->addTab(m_stackTree, QStringLiteral("调用栈"));

  m_varTree = new QTreeWidget;
  m_varTree->setColumnCount(3);
  m_varTree->setHeaderLabels(
      {QStringLiteral("作用域"), QStringLiteral("名称"), QStringLiteral("值")});
  m_varTree->setRootIsDecorated(false);
  m_varTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_varTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_varTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_varTree->setAlternatingRowColors(true);
  tabs->addTab(m_varTree, QStringLiteral("变量"));

  // ── 断点列表（类似 VSCode 的 BREAKPOINTS 视图）──
  m_breakTree = new QTreeWidget;
  m_breakTree->setColumnCount(2);
  m_breakTree->setHeaderLabels({QStringLiteral("文件"), QStringLiteral("行")});
  m_breakTree->setRootIsDecorated(false);
  m_breakTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_breakTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_breakTree->setAlternatingRowColors(true);
  tabs->addTab(m_breakTree, QStringLiteral("断点"));

  // 双击断点条目：打开对应文件并定位到断点行
  connect(m_breakTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
    if (!item) return;
    const QString filePath = item->data(0, Qt::UserRole).toString();
    bool ok = false;
    const int line = item->data(1, Qt::UserRole).toInt(&ok);
    if (!filePath.isEmpty() && ok && line > 0) {
      emit breakpointActivated(filePath, line);
    }
  });

  root->addWidget(tabs, 1);

  // 默认未激活
  setActive(false);
}

void DebugPanel::setSnapshot(const QVector<AcDebugFrame> &stack, const QList<AcDebugVar> &vars) {
  // 调用栈（自底向上显示，顶层在最后）
  m_stackTree->clear();
  for (const auto &frame : stack) {
    auto *item = new QTreeWidgetItem;
    item->setText(0, frame.funcName.isEmpty() ? QStringLiteral("(顶层)") : frame.funcName);
    item->setText(1, frame.line > 0 ? QStringLiteral("行 %1").arg(frame.line) : QString());
    m_stackTree->addTopLevelItem(item);
  }

  // 变量（数组/对象等复杂变量递归展开为可展开子节点）
  m_varTree->clear();
  for (const auto &v : vars) {
    auto *item = new QTreeWidgetItem;
    item->setText(0, v.scope);
    item->setText(1, v.name);
    item->setText(2, formatValue(v.value));
    m_varTree->addTopLevelItem(item);
    if (v.value.isArray()) {
      const QJsonArray &a = v.value.toArray();
      for (int i = 0; i < a.size(); ++i) appendVarValue(item, QString::number(i), a[i]);
    } else if (v.value.isObject()) {
      const QJsonObject &o = v.value.toObject();
      for (auto it = o.begin(); it != o.end(); ++it) appendVarValue(item, it.key(), it.value());
    }
  }
}

/// @brief 递归展开复杂变量（数组/对象）为子节点
void DebugPanel::appendVarValue(QTreeWidgetItem *parent, const QString &key,
                                const QJsonValue &val) {
  auto *item = new QTreeWidgetItem(parent);
  item->setText(0, QString());
  item->setText(1, key);
  item->setText(2, formatValue(val));
  if (val.isArray()) {
    const QJsonArray &a = val.toArray();
    for (int i = 0; i < a.size(); ++i) appendVarValue(item, QString::number(i), a[i]);
  } else if (val.isObject()) {
    const QJsonObject &o = val.toObject();
    for (auto it = o.begin(); it != o.end(); ++it) appendVarValue(item, it.key(), it.value());
  }
}

void DebugPanel::setBreakpoints(const QList<QPair<QString, int>> &breakpoints) {
  m_breakTree->clear();
  for (const auto &bp : breakpoints) {
    auto *item = new QTreeWidgetItem;
    item->setText(0, QFileInfo(bp.first).fileName());
    item->setToolTip(0, bp.first);
    item->setText(1, QString::number(bp.second));
    // 存储完整路径与行号，供双击定位使用
    item->setData(0, Qt::UserRole, bp.first);
    item->setData(1, Qt::UserRole, bp.second);
    m_breakTree->addTopLevelItem(item);
  }
}

void DebugPanel::clear() {
  m_stackTree->clear();
  m_varTree->clear();
  // 断点列表是持久视图（类似 VSCode 的 BREAKPOINTS），调试结束后保留显示
  m_statusLabel->setText(QStringLiteral("未调试"));
}

void DebugPanel::setPaused(bool paused) {
  m_continueBtn->setEnabled(paused);
  m_stepOverBtn->setEnabled(paused);
  m_stepIntoBtn->setEnabled(paused);
  m_stepOutBtn->setEnabled(paused);
}

void DebugPanel::setActive(bool active) {
  m_continueBtn->setEnabled(active);
  m_stepOverBtn->setEnabled(active);
  m_stepIntoBtn->setEnabled(active);
  m_stepOutBtn->setEnabled(active);
  if (!active) {
    m_statusLabel->setText(QStringLiteral("未调试"));
  }
}

void DebugPanel::setStatus(const QString &text) { m_statusLabel->setText(text); }

QString DebugPanel::formatValue(const QJsonValue &v) {
  if (v.isString()) return v.toString();
  if (v.isDouble()) return QString::number(v.toDouble());
  if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  if (v.isNull()) return QStringLiteral("null");
  if (v.isUndefined()) return QStringLiteral("undefined");
  if (v.isArray()) return QStringLiteral("Array(%1)").arg(v.toArray().size());
  if (v.isObject()) return QStringLiteral("Object");
  return QString();
}