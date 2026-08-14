/**
 * @file help_doc_mgr.cpp
 * @brief 帮助文档界面 — 控制器层实现
 */

#include "help_doc_mgr.h"

#include "help_doc_model.h"
#include "help_doc_ui.h"

// ──────────────────────────────────────────────────────────────
//  onCreateWindow — 创建 HelpDocUi 窗口（open() 时调用）
// ──────────────────────────────────────────────────────────────

QWidget *HelpDocMgr::onCreateWindow() {
  // ── 创建 MVC 组件 ──
  m_ui = new HelpDocUi;
  m_model = new HelpDocModel;

  // ── 注入模型并构建界面 ──
  m_ui->setModel(m_model);
  m_ui->setupUI();

  return m_ui;
}
