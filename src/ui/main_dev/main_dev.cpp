/**
 * @file main_dev.cpp
 * @brief 代码编辑主窗口（MVC 壳）实现
 */

#include "main_dev.h"
#include "main_dev_mgr.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"

// ──────────────────────────────────────────────────────────────
//  构造
// ──────────────────────────────────────────────────────────────

MainDev::MainDev(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Auto Code - 开发模式"));
  resize(1400, 850);

  // ── 创建 MVC 三层 ──
  m_ui = new MainDevUi(this);
  m_model = new MainDevModel;

  // ── 构建界面 ──
  m_ui->setupUI(this);

  // ── 初始化控制器（内部创建单例），连接信号 ──
  MainDevMgr::init(m_ui, m_model, this);

  // ── 获取控制器实例 ──
  m_mgr = MainDevMgr::instance();

  // ── 加载文件树 ──
  m_mgr->loadFiles();
}