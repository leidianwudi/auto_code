/**
 * @file main_dev_mgr_theme.cpp
 * @brief MainDevMgr 主题/字体刷新实现（从 main_dev_mgr.cpp 拆分）
 *
 * 包含以下功能：
 * - refreshTheme       — 应用设置后刷新全局样式、编辑器高亮、对话框与标签文字色
 * - refreshWindowFont  — 窗口字体变化后的轻量刷新（只重建标题栏样式）
 *
 * 与信号连接相关的防抖定时器逻辑仍保留在 main_dev_mgr.cpp 的 initUi 中。
 */

#include <QApplication>
#include <QDialog>
#include <QLabel>

#include "main_dev_mgr.h"
#include "main_dev_ui.h"
#include "src/ui/json_vue/json_vue_editor.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/setting_store.h"

/// 应用设置后刷新全局样式与编辑器高亮
void MainDevMgr::refreshTheme() {
  if (!m_ui) return;

  // 全局 Fusion 风格 + 调色板（原生控件菜单/下拉/表格/滚动条等随主题变化）
  SettingStore::ins().applyGlobalStyle();

  // 重新应用主窗口全局样式表（背景、边框等随主题变化）
  m_ui->setStyleSheet(AuiStyle::mainStyleSheet());

  // 刷新标题栏及菜单按钮颜色（标题栏背景、文件/视图按钮文字等）
  m_ui->refreshTitleBarStyle();

  // 刷新主窗口 log 输出面板（背景/文字色随主题更新）
  if (m_ui->outputPanel()) m_ui->outputPanel()->reloadStyle();

  // 刷新问题面板（背景/文字/条目颜色随主题重建）
  if (m_ui->problemPanel()) m_ui->problemPanel()->reloadStyle();

  // 刷新调试面板（调用栈/变量/断点页签栏与列表的颜色随主题重建）
  if (m_ui->debugPanel()) m_ui->debugPanel()->refreshStyle();

  // 刷新所有已打开编辑器的高亮颜色（语法高亮 / 行号 / 当前行等），
  // 以及 .jsonvue 的代码编辑器与可视化编辑器样式
  for (int p = 0; p < m_ui->editorPanelCount(); ++p) {
    QTabWidget *tabs = m_ui->editorPanelAt(p);
    if (!tabs) continue;
    for (int i = 0; i < tabs->count(); ++i) {
      QWidget *w = tabs->widget(i);
      if (auto *ed = qobject_cast<CodeEditor *>(w)) {
        ed->reloadColors();
      } else if (auto *jvw = qobject_cast<JsonVueWidget *>(w)) {
        if (jvw->codeEditor()) jvw->codeEditor()->reloadColors();
        if (jvw->visualEditor()) jvw->visualEditor()->reloadStyle();
      }
    }
  }

  // ── 刷新所有打开的顶层窗口（主窗口 + 对话框），保证文字随主题变色 ──
  // 背景色由全局调色板自动变化，但文字控件（QLabel/QPushButton）若持有创建时
  // 固化的样式表，深色下仍是浅色主题的深色文字而看不清，这里统一用当前文字色重建。
  const QWidgetList toplevels = qApp->topLevelWidgets();
  for (QWidget *w : toplevels) {
    // 对话框重建窗口级样式表与标题栏
    if (auto *dlg = qobject_cast<QDialog *>(w)) {
      dlg->setStyleSheet(AuiStyle::mainStyleSheet() + AuiStyle::dialogStyleSheet());
      const auto bars = dlg->findChildren<QWidget *>(QStringLiteral("AuiTitleBar"));
      for (QWidget *tb : bars) {
        AuiStyle::applyTitleBarStyle(tb);
        tb->update();
        for (QWidget *child : tb->findChildren<QWidget *>()) child->update();
      }
      // 刷新标题文字（如「设置」）颜色
      if (QLabel *tl = dlg->findChild<QLabel *>(QStringLiteral("AuiTitleLabel")))
        AuiStyle::applyTitleLabelStyle(tl);
      // 重建对话框标准按钮样式 + 标题栏控制按钮图标
      AuiButton::refreshThemedButtons(dlg);
    }
    // 重建无专门样式表的普通 QLabel 文字色（标题文字与有自定义样式的标签除外），
    // 用 auiAutoLabel 属性标记，使每次切换主题都用当前文字色重建、不固化旧色
    for (QLabel *l : w->findChildren<QLabel *>()) {
      if (l->objectName() == QStringLiteral("AuiTitleLabel")) continue;
      if (l->styleSheet().isEmpty() || l->property("auiAutoLabel").toBool()) {
        l->setProperty("auiAutoLabel", true);
        l->setStyleSheet(QStringLiteral("color: %1;").arg(AuiStyle::textColor().name()));
      }
    }
    // 强制 repolish，确保已存在子控件（含 QToolButton/QPushButton/QLabel/QMenu 等）重新解析
    // 新的样式表颜色。只 repolish 顶层窗口不够，子控件的 QSS 颜色需各自 unpolish/polish 才会重算。
    auto repolish = [](QWidget *root) {
      QList<QWidget *> all;
      all.reserve(64);
      all << root;
      all << root->findChildren<QWidget *>();
      for (QWidget *c : all) {
        c->style()->unpolish(c);
        c->style()->polish(c);
        c->update();
      }
    };
    repolish(w);
  }
}

/// 窗口字体变化后的轻量刷新（区别于 refreshTheme 的重活）
void MainDevMgr::refreshWindowFont() {
  if (!m_ui) return;
  // 窗口字体已由 SettingStore::applyWindowFont 应用到 qApp 与所有窗口，
  // 并已触发全部子控件重排 + 重绘（见 AuiStyle::applyAppFont）。
  // 这里只需重建标题栏文字样式：标题字号随窗口字号缩放，
  // 且标题字号是固化在样式表里的，必须重建才能生效。
  m_ui->refreshTitleBarStyle();
}
