/**
 * @file json_source_widget.h
 * @brief .jsonsource 编辑器包装器
 *
 * 继承 CodeVisualSyncWidget（index 0 = 代码编辑器，index 1 = 可视化编辑器），
 * 模式切换/焦点/防循环同步由基类统一处理，这里只实现 .jsonsource 的数据搬运：
 * 代码 ↔ JsonSourceConfig 双向转换，写回时以磁盘原文为底保真合并。
 */

#pragma once

#include <QString>

#include "src/util/ui/code/code_visual_sync_widget.h"

class JsonSourceEditor;

/**
 * @class JsonSourceWidget
 * @brief .jsonsource 编辑器包装器
 */
class JsonSourceWidget : public CodeVisualSyncWidget {
  Q_OBJECT

public:
  explicit JsonSourceWidget(QWidget *parent = nullptr);

  /// 获取可视化编辑器
  JsonSourceEditor *visualEditor() const { return m_visual; }

  /// 缓存磁盘原始 .jsonsource 内容，供可视化写回时保真合并
  void setPreservedSource(const QString &src);

  /// 设置 HTTP 请求参数（动态数据源"测试"按钮使用）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

protected:
  QWidget *visualView() const override;
  void syncCodeToVisualImpl() override;
  void syncVisualToCodeImpl() override;

private:
  JsonSourceEditor *m_visual = nullptr;  ///< 可视化编辑器
};
