/**
 * @file setting_mgr.cpp
 * @brief 设置界面 — 控制器层实现
 */

#include "setting_mgr.h"

#include "setting_model.h"
#include "setting_ui.h"

// ──────────────────────────────────────────────────────────────
//  onCreateWindow — 创建 SettingUi 窗口（open() 时调用）
// ──────────────────────────────────────────────────────────────

QWidget *SettingMgr::onCreateWindow() {
  // ── 创建 MVC 组件 ──
  m_ui = new SettingUi;
  m_model = new SettingModel;

  // ── 注入模型并构建界面 ──
  m_ui->setModel(m_model);
  m_ui->setupUI();
  m_ui->reloadAll();

  return m_ui;
}
