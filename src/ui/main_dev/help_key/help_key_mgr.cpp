/**
 * @file help_key_mgr.cpp
 * @brief 快捷键查看器 — 控制器层实现
 */

#include "help_key_mgr.h"

#include "help_key_model.h"
#include "help_key_ui.h"

// ──────────────────────────────────────────────────────────────
//  onCreateWindow — 创建 HelpKeyUi 窗口（open() 时调用）
// ──────────────────────────────────────────────────────────────

QWidget *HelpKeyMgr::onCreateWindow() {
  // ── 创建 MVC 组件 ──
  m_ui = new HelpKeyUi;
  m_model = new HelpKeyModel;

  // ── 构建界面 ──
  m_ui->setupUI();

  // ── 填充表格数据（模型数据已按分类排序） ──
  m_ui->populateTable(m_model->entries());

  return m_ui;
}
