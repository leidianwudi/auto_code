/**
 * @file debug_panel.cpp
 * @brief 调试面板实现
 */

#include "debug_panel.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/component/aui_tree.h"

/// @brief 调试面板 UI 状态存储文件（记录各列表列宽）
static QString debugUiSettingsPath() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty()) dir = QDir::homePath() + QStringLiteral("/.auto_code");
  QDir().mkpath(dir);
  return dir + QStringLiteral("/debug_ui.ini");
}

DebugPanel::DebugPanel(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(4);

  // ── 控制栏 ──
  auto *controlBar = new QHBoxLayout;
  controlBar->setContentsMargins(4, 4, 4, 0);
  controlBar->setSpacing(4);

  m_continueBtn = AuiButton::createDebugStepButton(0);
  m_stepOverBtn = AuiButton::createDebugStepButton(1);
  m_stepIntoBtn = AuiButton::createDebugStepButton(2);
  m_stepOutBtn = AuiButton::createDebugStepButton(3);
  for (QPushButton *btn : {m_continueBtn, m_stepOverBtn, m_stepIntoBtn, m_stepOutBtn}) {
    // 允许在窄侧边栏下收缩，避免撑大整个面板
    btn->setMinimumWidth(0);
    controlBar->addWidget(btn);
  }
  m_continueBtn->setToolTip(QStringLiteral("继续执行 (F5)"));
  m_stepOverBtn->setToolTip(QStringLiteral("单步执行 (F10)"));
  m_stepIntoBtn->setToolTip(QStringLiteral("单步进入 (F11)"));
  m_stepOutBtn->setToolTip(QStringLiteral("单步跳出 (Shift+F11)"));

  m_statusLabel = new QLabel(QStringLiteral("未调试"));
  controlBar->addWidget(m_statusLabel, 1);

  root->addLayout(controlBar);

  // ── 调用栈 / 变量 双 tab ──
  m_tabs = new QTabWidget;
  QTabWidget *tabs = m_tabs;
  tabs->setDocumentMode(true);
  // 指定 tab 头四边空白（左/右 6px，上/下 3px），并保留文字颜色（未选中灰 / 选中深）
  AuiStyle::applyTabBarPadding(tabs->tabBar(), 10, 5, 10, 5);

  m_stackTree = AuiTree::createListTree();
  m_stackTree->setColumnCount(2);
  m_stackTree->setHeaderLabels({QStringLiteral("函数"), QStringLiteral("位置")});
  m_stackTree->setRootIsDecorated(false);
  m_stackTree->setColumnWidth(0, 150);
  m_stackTree->setColumnWidth(1, 90);
  tabs->addTab(m_stackTree, QStringLiteral("调用栈"));

  // 双击调用栈条目：打开对应文件并定位到函数所在行
  connect(m_stackTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
    if (!item) return;
    const QString filePath = item->data(0, Qt::UserRole).toString();
    bool ok = false;
    const int line = item->data(1, Qt::UserRole).toInt(&ok);
    if (!filePath.isEmpty() && ok && line > 0) {
      emit stackFrameActivated(filePath, line);
    }
  });

  m_varTree = AuiTree::createListTree();
  m_varTree->setColumnCount(4);
  m_varTree->setHeaderLabels({QStringLiteral("作用域"), QStringLiteral("名称"),
                              QStringLiteral("值"), QStringLiteral("位置")});
  m_varTree->setRootIsDecorated(true);  // 在最左侧显示展开/折叠箭头，点击箭头展开
  m_varTree->setColumnWidth(0, 50);
  m_varTree->setColumnWidth(1, 100);
  m_varTree->setColumnWidth(2, 150);
  m_varTree->setColumnWidth(3, 120);
  // 双击只定位，不展开/折叠（展开只能通过最左侧箭头）
  m_varTree->setExpandsOnDoubleClick(false);
  tabs->addTab(m_varTree, QStringLiteral("变量"));

  // 双击变量条目：打开对应文件并定位到变量声明行
  connect(m_varTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
    if (!item) return;
    const QString filePath = item->data(0, Qt::UserRole).toString();
    bool ok = false;
    const int line = item->data(1, Qt::UserRole).toInt(&ok);
    if (!filePath.isEmpty() && ok && line > 0) {
      emit varActivated(filePath, line);
    }
  });

  // ── 断点列表（类似 VSCode 的 BREAKPOINTS 视图）──
  //    生效列(复选框)  |  文件  |  行
  m_removeBreakBtn = new QPushButton(QStringLiteral("移除全部断点"));
  m_removeBreakBtn->setToolTip(QStringLiteral("移除所有已设置断点"));
  connect(m_removeBreakBtn, &QPushButton::clicked, this,
          [this]() { emit breakpointRemoveAllRequested(); });

  auto *breakTab = new QWidget;
  auto *breakLayout = new QVBoxLayout(breakTab);
  breakLayout->setContentsMargins(0, 0, 0, 0);
  breakLayout->setSpacing(2);
  breakLayout->addWidget(m_removeBreakBtn);

  m_breakTree = AuiTree::createListTree();
  m_breakTree->setColumnCount(3);
  m_breakTree->setHeaderLabels(
      {QStringLiteral("生效"), QStringLiteral("文件"), QStringLiteral("行")});
  m_breakTree->setRootIsDecorated(false);
  m_breakTree->setColumnWidth(0, 34);
  m_breakTree->setColumnWidth(1, 130);
  m_breakTree->setColumnWidth(2, 40);
  breakLayout->addWidget(m_breakTree, 1);
  tabs->addTab(breakTab, QStringLiteral("断点"));

  // 点击「生效」列图标：切换断点生效状态（延迟触发，避免在信号处理中重建清树导致崩溃）
  connect(m_breakTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
    if (!item || column != 0) return;
    const QString filePath = item->data(1, Qt::UserRole).toString();
    bool ok = false;
    const int line = item->data(2, Qt::UserRole).toInt(&ok);
    if (filePath.isEmpty() || !ok || line <= 0) return;
    const bool enabled = item->data(3, Qt::UserRole).toBool();
    QTimer::singleShot(0, this, [this, filePath, line, enabled]() {
      emit breakpointToggleEnabledRequested(filePath, line, !enabled);
    });
  });

  // 双击断点条目：打开对应文件并定位到断点行
  connect(m_breakTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
    if (!item) return;
    const QString filePath = item->data(1, Qt::UserRole).toString();
    bool ok = false;
    const int line = item->data(2, Qt::UserRole).toInt(&ok);
    if (!filePath.isEmpty() && ok && line > 0) {
      emit breakpointActivated(filePath, line);
    }
  });

  // 右键菜单：删除单个断点 / 启用或禁用断点
  m_breakTree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_breakTree, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
    auto *item = m_breakTree->itemAt(pos);
    if (!item) return;
    const QString filePath = item->data(1, Qt::UserRole).toString();
    bool ok = false;
    const int line = item->data(2, Qt::UserRole).toInt(&ok);
    if (filePath.isEmpty() || !ok || line <= 0) return;
    const bool enabled = item->data(3, Qt::UserRole).toBool();

    QMenu menu(m_breakTree);
    QAction *toggleAction =
        menu.addAction(enabled ? QStringLiteral("禁用断点") : QStringLiteral("启用断点"));
    connect(toggleAction, &QAction::triggered, this, [this, filePath, line, enabled]() {
      emit breakpointToggleEnabledRequested(filePath, line, !enabled);
    });
    QAction *delAction = menu.addAction(QStringLiteral("移除断点"));
    connect(delAction, &QAction::triggered, this,
            [this, filePath, line]() { emit breakpointDeleteRequested(filePath, line); });
    menu.exec(m_breakTree->viewport()->mapToGlobal(pos));
  });

  root->addWidget(tabs, 1);

  // 列宽被拖动后保存，下次启动还原
  for (QTreeWidget *tree : {m_stackTree, m_varTree, m_breakTree}) {
    connect(tree->header(), &QHeaderView::sectionResized, this,
            [this](int, int, int) { saveHeaderStates(); });
  }
  restoreHeaderStates();

  // 默认未激活
  setActive(false);

  // 应用当前主题样式（页签栏背景、列表、状态文字等）
  refreshStyle();
}

void DebugPanel::saveHeaderStates() {
  QSettings s(debugUiSettingsPath(), QSettings::IniFormat);
  s.setValue(QStringLiteral("stack/header"), m_stackTree->header()->saveState());
  s.setValue(QStringLiteral("var/header"), m_varTree->header()->saveState());
  s.setValue(QStringLiteral("break/header"), m_breakTree->header()->saveState());
}

void DebugPanel::restoreHeaderStates() {
  QSettings s(debugUiSettingsPath(), QSettings::IniFormat);
  m_stackTree->header()->restoreState(s.value(QStringLiteral("stack/header")).toByteArray());
  m_varTree->header()->restoreState(s.value(QStringLiteral("var/header")).toByteArray());
  m_breakTree->header()->restoreState(s.value(QStringLiteral("break/header")).toByteArray());
  // restoreState 会把旧的 stretchLastSection 标志一并还原，这里统一强制回 true，
  // 保证最后一列始终自动拉满剩余宽度（列宽总和小于父控件宽度时右边界线贴合右缘）
  for (QTreeWidget *tree : {m_stackTree, m_varTree, m_breakTree}) {
    tree->header()->setStretchLastSection(true);
  }
}

void DebugPanel::setSnapshot(const QVector<AcDebugFrame> &stack, const QList<AcDebugVar> &vars) {
  // 调用栈（自底向上显示，顶层在最后）
  m_stackTree->clear();
  for (const auto &frame : stack) {
    auto *item = new QTreeWidgetItem;
    item->setText(0, frame.funcName.isEmpty() ? QStringLiteral("(顶层)") : frame.funcName);
    item->setText(1, frame.line > 0 ? QStringLiteral("行 %1").arg(frame.line) : QString());
    // 存储完整路径与行号，供双击定位到函数
    item->setData(0, Qt::UserRole, frame.filePath);
    item->setData(1, Qt::UserRole, frame.line);
    m_stackTree->addTopLevelItem(item);
  }

  // 变量（数组/对象等复杂变量递归展开为可展开子节点，默认折叠，点击最左侧箭头展开）
  m_varTree->clear();
  for (const auto &v : vars) {
    auto *item = new QTreeWidgetItem;
    item->setText(0, v.scope);
    item->setText(1, v.name);
    item->setText(2, formatValue(v.value));
    item->setText(3, formatLocation(v.filePath, v.line, v.funcName));
    // 存储完整路径与行号，供双击定位到变量声明行
    item->setData(0, Qt::UserRole, v.filePath);
    item->setData(1, Qt::UserRole, v.line);
    m_varTree->addTopLevelItem(item);
    if (v.value.isArray()) {
      const QJsonArray &a = v.value.toArray();
      for (int i = 0; i < a.size(); ++i) appendVarValue(item, QString::number(i), a[i]);
    } else if (v.value.isObject()) {
      const QJsonObject &o = v.value.toObject();
      for (auto it = o.begin(); it != o.end(); ++it) appendVarValue(item, it.key(), it.value());
    }
    item->setExpanded(false);  // 默认折叠，点击最左侧箭头展开
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

// ── 生成断点生效列图标：生效=实心红圆，失效=空心红圆 ──
static QIcon breakpointIcon(bool enabled) {
  QPixmap pm(16, 16);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  const QColor red(0xf4, 0x47, 0x47);
  if (enabled) {
    p.setPen(Qt::NoPen);
    p.setBrush(red);
    p.drawEllipse(4, 4, 8, 8);
  } else {
    p.setPen(QPen(red, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(4, 4, 8, 8);
  }
  p.end();
  return QIcon(pm);
}

void DebugPanel::setBreakpoints(const QList<QPair<QString, AcBreakpoint>> &breakpoints) {
  m_breakTree->blockSignals(true);  // 重建时避免触发 itemClicked
  m_breakTree->clear();
  for (const auto &bp : breakpoints) {
    auto *item = new QTreeWidgetItem;
    item->setIcon(0, breakpointIcon(bp.second.enabled));
    item->setText(1, QFileInfo(bp.first).fileName());
    item->setToolTip(0, bp.second.enabled ? QStringLiteral("生效（点击禁用）")
                                          : QStringLiteral("失效（点击启用）"));
    item->setToolTip(1, bp.first);
    item->setText(2, QString::number(bp.second.line));
    // 存储完整路径、行号与当前生效状态，供点击/定位使用
    item->setData(1, Qt::UserRole, bp.first);
    item->setData(2, Qt::UserRole, bp.second.line);
    item->setData(3, Qt::UserRole, bp.second.enabled);
    m_breakTree->addTopLevelItem(item);
  }
  m_breakTree->blockSignals(false);
  m_removeBreakBtn->setEnabled(!breakpoints.isEmpty());
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

void DebugPanel::refreshStyle() {
  // ── 页签栏（调用栈 / 变量 / 断点）──
  // VSCode 风格：简洁无边框，选中 tab 顶部蓝色指示条 + 文字高亮，
  // 未选中 tab 灰色文字，hover 时文字微亮。颜色随主题即时重建。
  if (m_tabs) {
    m_tabs->setStyleSheet(
        QStringLiteral(
            // 面板区域：无额外边框，背景用面板色
            "QTabWidget::pane { border: none; background: %1; }"
            // tab 栏背景
            "QTabBar { background: %1; }"
            // 未选中 tab：灰色文字，上下留白，顶部 2px 透明边框（占位，避免选中时高度跳动）
            "QTabBar::tab {"
            "  color: %2; padding: 6px 14px;"
            "  border-top: 2px solid transparent;"
            "  border-bottom: 1px solid %4;"
            "  background: transparent;"
            "}"
            // hover：文字微亮（用次要文字色）
            "QTabBar::tab:hover { color: %5; }"
            // 选中 tab：正文色 + 顶部蓝色指示条
            "QTabBar::tab:selected {"
            "  color: %3; border-top: 2px solid %6;"
            "}")
            .arg(AuiStyle::panelBackground().name(),     // %1 面板背景
                 AuiStyle::inactiveTabColor().name(),    // %2 未选中灰
                 AuiStyle::textColor().name(),           // %3 选中正文色
                 AuiStyle::borderColor().name(),         // %4 底部分隔线
                 AuiStyle::secondaryTextColor().name(),  // %5 hover 文字
                 QStringLiteral("#0e7afe")));            // %6 蓝色指示条
  }

  // ── 三个列表（调用栈 / 变量 / 断点）── 重新生成带当前主题色的列表样式
  for (QTreeWidget *tree : {m_stackTree, m_varTree, m_breakTree}) {
    if (tree) tree->setStyleSheet(AuiTree::listStyleSheet());
  }

  // ── 状态文字 ──
  if (m_statusLabel) {
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                     .arg(AuiStyle::inactiveTabColor().name()));
  }

  // ── 移除全部断点按钮 ──
  if (m_removeBreakBtn) {
    m_removeBreakBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton { border: none; background: transparent; color: %1; text-align: left; "
            "padding: 2px 4px; } "
            "QPushButton:hover { background: rgba(128,128,128,0.15); }")
            .arg(AuiStyle::inactiveTabColor().name()));
  }

  // ── 4 个调试单步按钮（继续/单步执行/进入/跳出）── 重建样式与图标颜色
  AuiButton::refreshThemedButtons(this);
}

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

QString DebugPanel::formatLocation(const QString &filePath, int line, const QString &funcName) {
  if (filePath.isEmpty() || line <= 0) return QString();
  QString loc = QStringLiteral("%1:%2").arg(QFileInfo(filePath).fileName()).arg(line);
  if (!funcName.isEmpty()) {
    loc = QStringLiteral("%1  %2").arg(funcName, loc);
  }
  return loc;
}