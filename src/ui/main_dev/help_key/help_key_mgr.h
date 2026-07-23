/**
 * @file help_key_mgr.h
 * @brief 快捷键查看器 — 控制器层
 *
 * 继承 AuiMgr<HelpKeyMgr> 实现单例控制器，负责：
 * - 创建 HelpKeyUi（视图）和 HelpKeyModel（数据）
 * - 将模型数据填充到视图表格
 * - 管理对话框的打开/激活（可重复调用 open()）
 *
 * 使用方式：HelpKeyMgr::ins().open();
 */

#pragma once

#include <QObject>

#include "src/util/ui/aui_mgr.h"

class HelpKeyUi;
class HelpKeyModel;

/**
 * @class HelpKeyMgr
 * @brief 快捷键查看器控制器（单例）
 *
 * MVC 中的控制器层，遵循 DemoMgr 的设计模式：
 * - 继承 AuiMgr<HelpKeyMgr>，通过 ins() 获取实例
 * - onCreateWindow() 创建 HelpKeyUi + HelpKeyModel 并填充表格
 * - open() 可重复调用，首次创建窗口，后续仅激活
 */
class HelpKeyMgr : public AuiMgr<HelpKeyMgr> {
  Q_OBJECT

  friend class AuiMgr<HelpKeyMgr>;

public:
  HelpKeyMgr() = default;
  ~HelpKeyMgr() override = default;

protected:
  QWidget *onCreateWindow() override;

private:
  HelpKeyUi *m_ui = nullptr;      ///< 视图层
  HelpKeyModel *m_model = nullptr;  ///< 数据模型
};
