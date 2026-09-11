/**
 * @file json_vue_widget.h
 * @brief .jsonvue 编辑器包装器
 *
 * 继承 CodeVisualSyncWidget（index 0 = 代码编辑器，index 1 = 可视化编辑器），
 * 模式切换/焦点/防循环同步由基类统一处理，这里只实现 .jsonvue 的数据搬运：
 * 代码 ↔ JsonVueConfig 双向转换，内容指纹跳过重建（反复切换不卡顿）。
 *
 * 对外保留 codeEditor()（继承所得）用于与 MainDevMgr 集成（修改标记、保存等）。
 */

#pragma once

#include <QByteArray>
#include <QString>

#include "src/util/ui/code/code_visual_sync_widget.h"

class JsonVueEditor;

/**
 * @class JsonVueWidget
 * @brief .jsonvue 编辑器包装器
 */
class JsonVueWidget : public CodeVisualSyncWidget {
  Q_OBJECT

public:
  explicit JsonVueWidget(QWidget *parent = nullptr);

  /// 获取可视化编辑器
  JsonVueEditor *visualEditor() const { return m_visual; }

  /// 缓存磁盘原始 .jsonvue 内容，供可视化写回时保真合并（避免数据被清空/精简）
  void setPreservedSource(const QString &src);

  /// 设置 baseUrl（透传给可视化编辑器）
  void setBaseUrl(const QString &baseUrl);

  /// 记录当前 .jsonvue 文件路径（透传给可视化编辑器，用于推导 jsonsource 搜索根）
  void setSourceFilePath(const QString &path);

  /// 从 AC 脚本文件加载 HTTP 配置（透传给可视化编辑器）
  void loadHttpConfigFromAcFile(const QString &acFilePath);

  /// 从 jsonvue 文件向上查找最近的 api_auth_data.ac（透传给可视化编辑器）
  static QString findNearestApiAuthDataAc(const QString &jsonvueFilePath);

protected:
  QWidget *visualView() const override;
  void syncCodeToVisualImpl() override;
  void syncVisualToCodeImpl() override;

private:
  JsonVueEditor *m_visual = nullptr;  ///< 可视化编辑器
  QByteArray m_lastVisualHash;  ///< 可视化页当前内容的指纹（未变化则跳过重载，避免反复切换卡顿）
};
