/**
 * @file json_source_widget.h
 * @brief .jsonsource 编辑器包装器
 *
 * 在一个 QTabWidget 的 tab 内，提供两种视图：
 *   - 代码编辑视图（CodeEditor）
 *   - 可视化编辑视图（JsonSourceEditor）
 *
 * 由工具栏的"可视化/代码"切换按钮控制显示哪个视图。
 * 两个视图共享同一份配置数据，切换时自动同步。
 */

#pragma once

#include <QStackedWidget>
#include <QString>

class CodeEditor;
class JsonSourceEditor;

/**
 * @class JsonSourceWidget
 * @brief .jsonsource 编辑器包装器
 */
class JsonSourceWidget : public QStackedWidget {
  Q_OBJECT

public:
  explicit JsonSourceWidget(QWidget *parent = nullptr);

  /// 获取内部 CodeEditor（供 MainDevMgr 集成）
  CodeEditor *codeEditor() const { return m_editor; }

  /// 获取可视化编辑器
  JsonSourceEditor *visualEditor() const { return m_visual; }

  /// 当前是否为可视化模式
  bool isVisualMode() const { return currentIndex() == 1; }

  /// 聚焦当前显示的视图
  void focusActiveView();

  /// 切换到代码模式
  void switchToCode();

  /// 切换到可视化模式
  void switchToVisual();

  /// 切换模式
  void toggleMode();

  /// 从代码编辑器内容加载到可视化编辑器
  void syncCodeToVisual();

  /// 从可视化编辑器内容写回代码编辑器
  void syncVisualToCode();

  /// 缓存磁盘原始 .jsonsource 内容，供可视化写回时保真合并
  void setPreservedSource(const QString &src);

  /// 设置 HTTP 请求参数（动态数据源"测试"按钮使用）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

signals:
  /// 模式切换时发射
  void modeChanged(bool visualMode);

  /// 内容发生变化时发射（用于触发修改标记）
  void contentChanged();

private:
  CodeEditor *m_editor = nullptr;      ///< 代码编辑器
  JsonSourceEditor *m_visual = nullptr;  ///< 可视化编辑器
  bool m_syncing = false;              ///< 同步中标志，避免循环
};
