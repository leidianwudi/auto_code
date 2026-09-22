/**
 * @file json_global_enum_widget.h
 * @brief .jsonglobalenum 编辑器包装器
 *
 * 继承 CodeVisualSyncWidget（index 0 = 代码编辑器，index 1 = 可视化编辑器），
 * 模式切换/焦点/防循环同步由基类统一处理，这里只实现 .jsonglobalenum 的
 * 数据搬运：代码 ↔ JsonGlobalEnumConfig 双向转换，写回时以磁盘原文为底保真合并。
 */

#pragma once

#include <QString>

#include "src/util/ui/code/code_visual_sync_widget.h"

class JsonGlobalEnumEditor;

/**
 * @class JsonGlobalEnumWidget
 * @brief .jsonglobalenum 编辑器包装器
 */
class JsonGlobalEnumWidget : public CodeVisualSyncWidget {
  Q_OBJECT

public:
  explicit JsonGlobalEnumWidget(QWidget *parent = nullptr);

  /// 获取可视化编辑器
  JsonGlobalEnumEditor *visualEditor() const { return m_visual; }

  /// 缓存磁盘原始 .jsonglobalenum 内容，供可视化写回时保真合并
  void setPreservedSource(const QString &src);

protected:
  QWidget *visualView() const override;
  void syncCodeToVisualImpl() override;
  void syncVisualToCodeImpl() override;

private:
  JsonGlobalEnumEditor *m_visual = nullptr;  ///< 可视化编辑器
};
