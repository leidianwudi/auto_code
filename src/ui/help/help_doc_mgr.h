/**
 * @file help_doc_mgr.h
 * @brief 帮助文档界面 — 控制器层（MVC）
 *
 * 继承 AuiMgr<HelpDocMgr> 实现单例控制器，负责：
 * - 创建 HelpDocUi（视图）和 HelpDocModel（数据）
 * - 注入模型并初始化界面与数据
 * - 管理对话框的打开/激活（可重复调用 open()）
 *
 * 使用方式：HelpDocMgr::ins().open();
 */

#pragma once

#include <QObject>

#include "src/util/ui/aui_mgr.h"

class HelpDocUi;
class HelpDocModel;

/**
 * @class HelpDocMgr
 * @brief 帮助文档界面控制器（单例）
 *
 * MVC 中的控制器层，遵循 SettingMgr / DemoMgr 的设计模式：
 * - 继承 AuiMgr<HelpDocMgr>，通过 ins() 获取实例
 * - onCreateWindow() 创建 HelpDocUi + HelpDocModel 并绑定
 * - open() 可重复调用，首次创建窗口，后续仅激活
 */
class HelpDocMgr : public AuiMgr<HelpDocMgr> {
  Q_OBJECT

  friend class AuiMgr<HelpDocMgr>;

public:
  HelpDocMgr() = default;
  ~HelpDocMgr() override = default;

protected:
  QWidget *onCreateWindow() override;

private:
  HelpDocUi *m_ui = nullptr;        ///< 视图层
  HelpDocModel *m_model = nullptr;  ///< 数据模型
};
