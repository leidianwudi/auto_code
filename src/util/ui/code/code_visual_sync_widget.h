/**
 * @file code_visual_sync_widget.h
 * @brief 代码/可视化双视图编辑器的同步骨架基类
 *
 * 统一 JsonVueWidget / JsonSourceWidget / SchemaJsonWidget 三种包装器的重复逻辑：
 *  - 页结构约定：index 0 = 代码编辑器（CodeEditor + LightJson + JSON 校验，基类构造时
 *    自动创建），index 1 = 可视化视图（子类构造时 addWidget 添加）；
 *  - 模式切换与同步时机（switchToCode/switchToVisual/toggleMode）；
 *  - 焦点管理（focusActiveView：可视化模式必须聚焦可视化视图本身，聚焦隐藏的代码页无效）；
 *  - m_syncing 防循环标志、setPlainTextIfChanged 防抖写回、
 *    syncVisualToCode 的「代码模式为权威来源」守卫。
 *
 * 子类职责：
 *  - 构造时创建可视化编辑器并 addWidget（成为 index 1）；
 *  - 实现 visualView() 返回可视化视图控件（供聚焦）；
 *  - 实现 syncCodeToVisualImpl()/syncVisualToCodeImpl() 完成实际数据搬运
 *    （m_syncing 防循环标志由基类统一管理）；
 *  - 把可视化编辑器的「内容变化」信号连接到 onVisualContentChanged()。
 */

#pragma once

#include <QStackedWidget>
#include <QString>

class CodeEditor;

/**
 * @class CodeVisualSyncWidget
 * @brief 代码/可视化双视图编辑器的公共基类
 */
class CodeVisualSyncWidget : public QStackedWidget {
  Q_OBJECT

public:
  explicit CodeVisualSyncWidget(QWidget *parent = nullptr);

  /// 内部代码编辑器（供 MainDevMgr 集成：修改标记、保存等）
  CodeEditor *codeEditor() const { return m_editor; }

  /// 当前是否为可视化模式
  bool isVisualMode() const { return currentIndex() == 1; }

  /// 聚焦当前显示的视图（可视化模式聚焦可视化视图，否则聚焦代码编辑器）
  void focusActiveView();

  /// 切换到代码模式（先把可视化数据写回代码）
  void switchToCode();

  /// 切换到可视化模式（先把代码内容加载到可视化，避免可视化视图未填充）
  void switchToVisual();

  /// 切换模式
  void toggleMode();

  /// 从代码编辑器内容加载到可视化编辑器（m_syncing 防循环标志由基类管理）
  void syncCodeToVisual();

  /// 从可视化编辑器内容写回代码编辑器。
  /// 仅可视化模式下生效：代码模式下用户可能直接改过代码，此时代码是权威
  /// 来源，覆盖会导致修改丢失（保存前 flush 时误调也无副作用）。
  void syncVisualToCode();

  /// 可视化编辑器内容变化的统一入口（子类把可视化编辑器的变化信号连接到这里）：
  /// 写回代码编辑器并广播 contentChanged；m_syncing 期间忽略，避免信号回环
  void onVisualContentChanged();

signals:
  /// 模式切换时发射
  void modeChanged(bool visualMode);

  /// 内容发生变化时发射（用于触发修改标记）
  void contentChanged();

protected:
  /// 可视化视图控件（focusActiveView 聚焦用）
  virtual QWidget *visualView() const = 0;

  /// 实际的「代码 → 可视化」数据搬运（调用时 m_syncing 已由基类置位）
  virtual void syncCodeToVisualImpl() = 0;

  /// 实际的「可视化 → 代码」数据搬运（调用时 m_syncing 已由基类置位）
  virtual void syncVisualToCodeImpl() = 0;

  /// 内容未变化时不重设文本：避免无意义的 document 变更（误标修改、触发重排）
  void setPlainTextIfChanged(const QString &text);

  CodeEditor *m_editor = nullptr;  ///< 代码编辑器（index 0，基类构造时创建）

private:
  bool m_syncing = false;  ///< 同步中标志，避免信号回环
};
