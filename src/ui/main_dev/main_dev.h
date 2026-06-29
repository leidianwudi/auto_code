/**
 * @file main_dev.h
 * @brief 代码编辑主窗口（MVC 壳）
 *
 * 仅充当 QMainWindow + 组合 MVC 三层组件：
 * - MainDevUi（视图层）：创建并持有所有界面控件
 * - MainDevModel（数据层）：存储打开文件状态
 * - MainDevMgr（控制器层）：处理所有业务逻辑和信号连接
 */

#pragma once

#include <QMainWindow>

class MainDevUi;
class MainDevModel;
class MainDevMgr;

/**
 * @class MainDev
 * @brief 代码开发主窗口（MVC 壳）
 *
 * 职责仅：
 * 1. 创建 MVC 三层组件
 * 2. 调用 MainDevUi::setupUI 构建界面
 * 3. 调用 MainDevMgr::init 连接信号
 * 4. 调用 MainDevMgr::loadFiles 加载文件树
 *
 * 自身不包含任何 UI 控件或业务逻辑。
 */
class MainDev : public QMainWindow {
  Q_OBJECT

public:
  explicit MainDev(QWidget *parent = nullptr);
  ~MainDev() override = default;

private:
  MainDevUi *m_ui = nullptr;
  MainDevModel *m_model = nullptr;
  MainDevMgr *m_mgr = nullptr;
};