/**
 * @file create_mgr.h
 * @brief 新建文件对话框 — 控制器层
 *
 * 负责打开对话框、验证输入、执行文件/文件夹创建。
 * 遵循 MVC 设计模式，与 CreateUi（视图）和 CreateModel（数据）协作。
 * 继承 AuiMgr<CreateMgr> 作为单例 UI 控制器，与项目架构一致。
 */

#pragma once

#include <QObject>

#include "src/util/ui/aui_mgr.h"

class QWidget;

/**
 * @class CreateMgr
 * @brief 新建文件/文件夹的控制器（单例 UI 控制器）
 *
 * MVC 中的控制器层：
 * - 继承 AuiMgr<CreateMgr>，通过 ins() 获取全局唯一实例
 * - 提供静态方法 createNew()，打开模态对话框并执行创建逻辑
 * - 创建成功后返回 true，失败时显示错误提示
 */
class CreateMgr : public AuiMgr<CreateMgr> {
  Q_OBJECT

  friend class AuiMgr<CreateMgr>;

public:
  CreateMgr() = default;
  ~CreateMgr() override = default;

  /**
   * @brief 在 parentDir 目录下新建文件或文件夹
   * @param parentDir 目标父目录路径
   * @param parentWidget 父窗口（用于对话框定位）
   * @return true 创建成功，false 用户取消或创建失败
   */
  static bool createNew(const QString &parentDir, QWidget *parentWidget);

protected:
  /// 创建并初始化 CreateUi 对话框（AuiMgr 接口）
  QWidget *onCreateWindow() override;
};