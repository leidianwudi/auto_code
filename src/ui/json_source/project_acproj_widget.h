/**
 * @file project_acproj_widget.h
 * @brief .acproj 编辑器包装器
 *
 * 继承 CodeVisualSyncWidget（index 0 = 代码编辑器，index 1 = 可视化编辑器），
 * 模式切换/焦点/防循环同步由基类统一处理，这里只实现 .acproj 的数据搬运：
 * 代码 ↔ { name, meta: {...} } 根 JSON 对象双向转换，
 * 未知键（根 / meta / db 三层）由 ProjectConfigEditor 保真合并。
 */

#pragma once

#include <QByteArray>

#include "src/util/ui/code/code_visual_sync_widget.h"

class ProjectConfigEditor;

/**
 * @class ProjectAcprojWidget
 * @brief .acproj 编辑器包装器
 */
class ProjectAcprojWidget : public CodeVisualSyncWidget {
  Q_OBJECT

public:
  explicit ProjectAcprojWidget(QWidget *parent = nullptr);

  /// 获取可视化编辑页
  ProjectConfigEditor *visualEditor() const { return m_visual; }

protected:
  QWidget *visualView() const override;
  void syncCodeToVisualImpl() override;
  void syncVisualToCodeImpl() override;

private:
  ProjectConfigEditor *m_visual = nullptr;
  QByteArray m_lastVisualHash;  ///< 可视化页当前内容指纹（未变化则跳过整页重建）
};
