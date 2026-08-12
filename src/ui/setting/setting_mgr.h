/**
 * @file setting_mgr.h
 * @brief 设置界面 — 控制器层
 *
 * 继承 AuiMgr<SettingMgr> 实现单例控制器，负责：
 * - 创建 SettingUi（视图）和 SettingModel（数据）
 * - 注入模型并初始化界面与数据
 * - 管理对话框的打开/激活（可重复调用 open()）
 *
 * 使用方式：SettingMgr::ins().open();
 */

#pragma once

#include <QObject>

#include "src/util/ui/aui_mgr.h"

class SettingUi;
class SettingModel;

/**
 * @class SettingMgr
 * @brief 设置界面控制器（单例）
 *
 * MVC 中的控制器层，遵循 HelpKeyMgr / DemoMgr 的设计模式：
 * - 继承 AuiMgr<SettingMgr>，通过 ins() 获取实例
 * - onCreateWindow() 创建 SettingUi + SettingModel 并绑定
 * - open() 可重复调用，首次创建窗口，后续仅激活
 */
class SettingMgr : public AuiMgr<SettingMgr> {
  Q_OBJECT

  friend class AuiMgr<SettingMgr>;

public:
  SettingMgr() = default;
  ~SettingMgr() override = default;

protected:
  QWidget *onCreateWindow() override;

private:
  SettingUi *m_ui = nullptr;        ///< 视图层
  SettingModel *m_model = nullptr;  ///< 数据模型
};
