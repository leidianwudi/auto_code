/**
 * @file main_dev_mgr_tab.cpp
 * @brief 标签页操作实现（MainDevMgr 的标签管理、拆分、关闭）
 */

#include <QFileInfo>
#include <QTabWidget>

#include "main_dev_mgr.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "main_dev_ui_ext.h"
#include "src/engine/ac_language.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/code/code_find_bar.h"

// ──────────────────────────────────────────────────────────────
//  当前编辑器 / 面板组查找
// ──────────────────────────────────────────────────────────────

CodeEditor *MainDevMgr::currentEditor() const {
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs && tabs->isVisible()) {
      auto *w = tabs->currentWidget();
      // 直接 CodeEditor
      auto *editor = qobject_cast<CodeEditor *>(w);
      if (!editor) {
        // JsonVueWidget 包装器
        auto *jvw = qobject_cast<JsonVueWidget *>(w);
        if (jvw) editor = jvw->codeEditor();
      }
      if (editor) return editor;
    }
  }
  return nullptr;
}

QTabWidget *MainDevMgr::currentTabWidget() const {
  // 1. 优先从焦点控件向上查找 QTabWidget 祖先
  QWidget *focus = QApplication::focusWidget();
  while (focus) {
    if (auto *tabs = qobject_cast<QTabWidget *>(focus)) return tabs;
    focus = focus->parentWidget();
  }

  // 2. 再 fallback 到最后活跃面板
  if (m_model->lastActivePanel && m_model->lastActivePanel->isVisible() &&
      m_model->lastActivePanel->count() > 0) {
    return m_model->lastActivePanel;
  }

  // 3. 最后 fallback 到第一个可见非空面板
  QTabWidget *emptyPanel = nullptr;
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (tabs && tabs->isVisible()) {
      if (tabs->count() > 0) return tabs;
      if (!emptyPanel) emptyPanel = tabs;
    }
  }
  return emptyPanel;
}

// ──────────────────────────────────────────────────────────────
//  标签页关闭
// ──────────────────────────────────────────────────────────────

void MainDevMgr::closeTab(QTabWidget *tabs, int index) {
  auto *w = tabs->widget(index);
  if (!w) return;

  // 支持 CodeEditor 和 JsonVueWidget 两种类型
  CodeEditor *editor = qobject_cast<CodeEditor *>(w);
  QWidget *container = w;
  if (!editor) {
    auto *jvw = qobject_cast<JsonVueWidget *>(w);
    if (jvw) {
      editor = jvw->codeEditor();
      container = jvw;
    }
  }
  if (!editor) return;

  // ── 清理数据层 ──
  QString filePath = editor->objectName();
  if (!filePath.isEmpty() && !m_model->isRegisteredElsewhere(filePath, editor)) {
    m_model->unregisterFile(filePath);
    // 清除目录树的红点状态（关闭不保存时修改已丢弃）
    m_ui->fileTree()->setFileModified(filePath, false);
  }

  tabs->removeTab(index);
  container->deleteLater();

  // ── 空面板且存在多个面板 → 删除面板组 ──
  if (tabs->count() == 0 && m_ui->editorPanelCount() > 1) {
    int idx = m_ui->editorPanelIndex(tabs);
    if (idx >= 0) m_ui->removeEditorPanelAt(idx);
    m_ui->setEditorPanelsUniformStretch();
    if (m_model->lastActivePanel == tabs) m_model->lastActivePanel = nullptr;
    m_ui->setWindowTitle(MainDevUi::devTitle());
    connectEditor(currentEditor());
    m_ui->applyTabDimming(currentTabWidget());
    return;
  }

  // ── 更新窗口标题 ──
  if (tabs->count() == 0) {
    m_ui->setWindowTitle(MainDevUi::devTitle());
  } else {
    int newIdx = tabs->currentIndex();
    if (newIdx >= 0) {
      QString fullPath = tabs->tabToolTip(newIdx);
      if (!fullPath.isEmpty()) {
        QFileInfo fi(fullPath);
        m_ui->setWindowTitle(MainDevUi::fileTitle(fi.fileName()));
      }
    }
  }

  connectEditor(currentEditor());
}

void MainDevMgr::onTabCloseRequested(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs) return;
  closeTab(tabs, index);
}

void MainDevMgr::onCurrentTabChanged(int index) {
  auto *tabs = qobject_cast<QTabWidget *>(sender());
  if (!tabs) return;

  // ── 同步查找栏显示/隐藏 ──
  // 遍历该面板所有编辑器：暂停非当前标签页的查找栏，恢复当前标签页的查找栏
  for (int i = 0; i < tabs->count(); ++i) {
    auto *w = tabs->widget(i);
    auto *editor = qobject_cast<CodeEditor *>(w);
    if (!editor) {
      auto *jvw = qobject_cast<JsonVueWidget *>(w);
      if (jvw) editor = jvw->codeEditor();
    }
    if (!editor || !editor->findBar()) continue;
    if (i == index) {
      editor->findBar()->resumeVisible();
    } else {
      editor->findBar()->pauseVisible();
    }
  }

  // ── 更新窗口标题 ──
  if (index < 0 || index >= tabs->count()) {
    m_ui->setWindowTitle(MainDevUi::devTitle());
    return;
  }

  QString fullPath = tabs->tabToolTip(index);
  if (!fullPath.isEmpty()) {
    QFileInfo fi(fullPath);
    m_ui->setWindowTitle(MainDevUi::fileTitle(fi.fileName()));
    // 标签页切换时，同步定位树形目录到当前文件
    m_ui->fileTree()->locateFile(fullPath);
  }

  // ── 同步可视化切换按钮状态 ──
  // 按钮状态独立保持，不受非 jsonvue 文件影响
  // 切换到 jsonvue 标签时，让 jsonvue 跟随按钮状态
  auto *curWidget = tabs->widget(index);
  auto *jvw = qobject_cast<JsonVueWidget *>(curWidget);
  if (jvw && m_ui->visualToggleBtn()) {
    bool visualChecked = m_ui->visualToggleBtn()->isChecked();
    if (visualChecked) {
      jvw->switchToVisual();
    } else {
      jvw->switchToCode();
    }
  }

  connectEditor(currentEditor());
  m_model->lastActivePanel = tabs;
  m_ui->applyTabDimming(tabs);
}

void MainDevMgr::onTabBarClicked(int index) {
  // 点击已选中的标签时 currentChanged 不会触发，需要手动激活该面板
  auto *bar = qobject_cast<QTabBar *>(sender());
  if (!bar) return;
  auto *tabs = qobject_cast<QTabWidget *>(bar->parentWidget());
  if (!tabs || index < 0 || index >= tabs->count()) return;

  // 将焦点设置到当前编辑器，触发 onFocusChanged 完成面板切换
  auto *w = tabs->widget(index);
  auto *editor = qobject_cast<CodeEditor *>(w);
  if (!editor) {
    auto *jvw = qobject_cast<JsonVueWidget *>(w);
    if (jvw) editor = jvw->codeEditor();
  }
  if (editor) editor->setFocus();
}

// ──────────────────────────────────────────────────────────────
//  拆分 / 关闭编辑器
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onSplitRight() {
  // 没有打开任何文件时不拆分
  CodeEditor *current = currentEditor();
  if (!current) return;

  QTabWidget *newPanel = m_ui->createEditorPanel();

  connect(newPanel, &QTabWidget::tabCloseRequested, this, &MainDevMgr::onTabCloseRequested);
  connect(newPanel, &QTabWidget::currentChanged, this, &MainDevMgr::onCurrentTabChanged);

  {
    auto *bar = qobject_cast<DraggableTabBar *>(newPanel->tabBar());
    if (bar) {
      connect(bar, &DraggableTabBar::closeOthersRequested, this, &MainDevMgr::onCloseOthers);
      connect(bar, &DraggableTabBar::closeAllRequested, this, &MainDevMgr::onCloseAll);
      connect(bar, &QTabBar::tabBarClicked, this, &MainDevMgr::onTabBarClicked);
    }
  }

  {
    // 复用 createEditorForFile 创建高亮器 + 验证模式一致的编辑器
    QString filePath = current->objectName();
    bool isJsonVue = filePath.endsWith(AcFileSuffix::kJsonvue, Qt::CaseInsensitive);

    if (isJsonVue) {
      // .jsonvue 文件拆分时创建 JsonVueWidget，保持可视化能力
      auto *jvw = new JsonVueWidget;
      auto *editor = jvw->codeEditor();
      editor->setPlainText(current->toPlainText());

      // 从当前启动项 AC 脚本加载 HTTP 配置
      QString acPath = m_ui->startupCombo()->currentData().toString();
      if (!acPath.isEmpty()) {
        jvw->loadHttpConfigFromAcFile(acPath);
      }

      // 可视化按钮生效时，拆分的副本也以可视化方式打开
      if (m_ui->visualToggleBtn() && m_ui->visualToggleBtn()->isChecked()) {
        jvw->switchToVisual();
      }

      QFileInfo fi(filePath);
      QString tabLabel = filePath.isEmpty() ? QStringLiteral("拆分副本") : fi.fileName();
      int idx = newPanel->addTab(jvw, tabLabel);
      newPanel->setTabToolTip(idx, filePath);
      newPanel->setCurrentIndex(idx);

      if (!filePath.isEmpty()) {
        editor->setObjectName(filePath);
        m_model->registerFile(filePath, current->toPlainText(), editor);
      }

      // 连接 contentChanged 信号
      connect(jvw, &JsonVueWidget::contentChanged, this, [jvw, editor, this]() {
        editor->document()->setModified(true);
        updateSaveButtonState();
      });
    } else {
      auto *editor = createEditorForFile(filePath);
      editor->setPlainText(current->toPlainText());

      QFileInfo fi(filePath);
      QString tabLabel = filePath.isEmpty() ? QStringLiteral("拆分副本") : fi.fileName();
      int idx = newPanel->addTab(editor, tabLabel);
      newPanel->setTabToolTip(idx, filePath);
      newPanel->setCurrentIndex(idx);

      if (!filePath.isEmpty()) {
        editor->setObjectName(filePath);
        m_model->registerFile(filePath, current->toPlainText(), editor);
      }
    }
  }

  // ── 计算当前面板在分割器中的大小，用于平分 ──
  QTabWidget *curPanel = currentTabWidget();
  int curIdx = m_ui->editorPanelIndex(curPanel);
  QList<int> oldSizes = curIdx >= 0 ? m_ui->editorSplitter()->sizes() : QList<int>();

  m_ui->addEditorPanel(newPanel);
  m_ui->setEditorPanelsUniformStretch();

  // 仅平分当前面板：原面板和新面板各占一半
  if (curIdx >= 0 && curIdx < oldSizes.size()) {
    int half = oldSizes[curIdx] / 2;
    QList<int> newSizes = m_ui->editorSplitter()->sizes();
    newSizes[curIdx] = half;
    newSizes.insert(curIdx + 1, half);
    m_ui->editorSplitter()->setSizes(newSizes);
  }

  m_model->lastActivePanel = newPanel;
  m_ui->applyTabDimming(newPanel);

  auto *editorInPanel = qobject_cast<CodeEditor *>(newPanel->currentWidget());
  if (editorInPanel)
    editorInPanel->setFocus();
  else
    newPanel->setFocus();
}

void MainDevMgr::onCloseEditor() {
  QTabWidget *tabs = currentTabWidget();
  if (!tabs) return;

  int idx = tabs->currentIndex();
  if (idx >= 0) {
    // 通过信号触发 onTabCloseRequested → closeTab，确保 sender() 兼容
    emit tabs->tabCloseRequested(idx);
  }
}

// ──────────────────────────────────────────────────────────────
//  右键菜单：关闭其它 / 关闭全部
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onCloseOthers(int index) {
  auto *bar = qobject_cast<DraggableTabBar *>(sender());
  if (!bar) return;
  auto *tabs = qobject_cast<QTabWidget *>(bar->parentWidget());
  if (!tabs) return;
  for (int i = tabs->count() - 1; i >= 0; --i) {
    if (i != index) closeTab(tabs, i);
  }
}

void MainDevMgr::onCloseAll() {
  auto *bar = qobject_cast<DraggableTabBar *>(sender());
  if (!bar) return;
  auto *tabs = qobject_cast<QTabWidget *>(bar->parentWidget());
  if (!tabs) return;
  while (tabs->count() > 0) {
    closeTab(tabs, 0);
  }
}
