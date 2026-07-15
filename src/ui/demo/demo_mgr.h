/**
 * @file demo_mgr.h
 * @brief Demo 示例管理器（控制器层）
 *
 * 继承 AuiMgr<DemoMgr> 实现单例控制器，负责：
 * - 创建 DemoUi（视图）和 DemoModel（数据）
 * - 处理所有业务逻辑（模板渲染、文件 I/O）
 * - 连接 UI 信号到业务逻辑
 */

#pragma once

#include <QObject>

#include "src/util/ui/aui_mgr.h"


class DemoUi;
class DemoModel;
class TplEngine;

/**
 * @class DemoMgr
 * @brief Demo 示例控制器（单例）
 *
 * MVC 中的控制器层，遵循 MainDevMgr 的设计模式：
 * - 继承 AuiMgr<DemoMgr>，通过 ins() 获取实例
 * - onCreateWindow() 创建 DemoUi + DemoModel
 * - 处理模板生成、文件加载/保存等业务逻辑
 */
class DemoMgr : public AuiMgr<DemoMgr> {
  Q_OBJECT

  friend class AuiMgr<DemoMgr>;

public:
  DemoMgr() = default;
  ~DemoMgr() override = default;

protected:
  QWidget *onCreateWindow() override;

private slots:
  void onGenerate();
  void onLoadTemplate();
  void onLoadData();
  void onSaveOutput();

private:
  void initUi();

  DemoUi *m_ui = nullptr;
  DemoModel *m_model = nullptr;
  TplEngine *m_engine = nullptr;
};