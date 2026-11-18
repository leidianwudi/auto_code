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
 *
 * 误报「已修改」防护（重要）：
 *  - 可视化侧的交互信号不等于内容变化：单击/聚焦/双击单元格、下拉弹层同步、
 *    程序性 setCurrentIndex/setEditText 都可能触发编辑器的变化信号。
 *    基类比较回填完成后的内容指纹，内容未变则忽略通知，也不重新序列化原文。
 *  - 打开文件后的指纹基线必须用「回填完成后的完整序列化」（与
 *    syncVisualToCodeImpl 写回时同一口径）。若用解析值做基线，规范化产物
 *    （键补齐/默认值回写等）会与之不等，用户第一次单击/失焦就会被误标已修改。
 *  - 结构性/顺序类键（如行序）只在用户真正改变时才落盘，未变化时保持文件原状，
 *    否则打开后第一次广播就会因新增键而判成内容变化。
 */

#pragma once

#include <QByteArray>
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
  bool syncCodeToVisual();

  /// 从可视化编辑器内容写回代码编辑器。
  /// 仅可视化模式下生效：代码模式下用户可能直接改过代码，此时代码是权威
  /// 来源，覆盖会导致修改丢失（保存前 flush 时误调也无副作用）。
  /// 返回是否实际改动代码文本；内容未编辑时保留原文、注释和修改状态。
  bool syncVisualToCode();

  /// 可视化编辑器内容变化的统一入口（子类把可视化编辑器的变化信号连接到这里）：
  /// 写回代码编辑器并广播 contentChanged；m_syncing 期间忽略，避免信号回环
  void onVisualContentChanged();

  /// 可视化加载失败原因；失败时停留代码页，不允许空配置回写。
  QString syncError() const { return m_syncError; }

  /// 快照撤销：是否有可撤销/可恢复的步（撤销栈/恢复栈非空）。
  /// 覆盖两类变化：可视化修改写回、代码模式打字（停顿聚合为一步）
  bool canUndo() const { return !m_undoStack.isEmpty(); }
  bool canRedo() const { return !m_redoStack.isEmpty(); }

  /// 撤销一步：当前文本入恢复栈，弹出撤销栈恢复；可视化模式下自动回填可视化页。
  /// 撤销到栈底（打开时原文）时文档 modified 被清 → 保存按钮自动置灰
  Q_INVOKABLE bool undoStep();

  /// 恢复一步：与撤销对称；新编辑（写回/打字）会使恢复栈失效
  Q_INVOKABLE bool redoStep();

  /// 刷新可视化页的主题样式，代码页由宿主统一刷新。
  virtual void reloadStyle() {}

signals:
  /// 解析或格式检查失败，供宿主输出定位信息。
  void synchronizationFailed(const QString &error);

  /// 模式切换时发射
  void modeChanged(bool visualMode);

  /// 内容发生变化时发射（用于触发修改标记）
  void contentChanged();

  /// 撤销/恢复栈变化（宿主刷新撤销/恢复按钮可用状态）
  void undoStackChanged();

protected:
  /// 进入可视化前检查根对象；特殊格式可扩展检查，失败时不得改动可视化状态。
  virtual bool validateVisualSource(QString *error) const;

  /// 当前可视化内容的指纹，回填与编辑须使用相同收集口径。
  virtual QByteArray visualFingerprint() const = 0;

  /// 可视化视图控件（focusActiveView 聚焦用）
  virtual QWidget *visualView() const = 0;

  /// 实际的「代码 → 可视化」数据搬运（调用时 m_syncing 已由基类置位）
  virtual void syncCodeToVisualImpl() = 0;

  /// 实际的「可视化 → 代码」数据搬运（调用时 m_syncing 已由基类置位）
  virtual void syncVisualToCodeImpl() = 0;

  /// 内容未变化时不重设文本：避免无意义的 document 变更（误标修改、触发重排）
  void setPlainTextIfChanged(const QString &text);

  CodeEditor *m_editor = nullptr;  ///< 代码编辑器（index 0，基类构造时创建）

private slots:
  /// 代码打字停顿聚合：停顿后把该步打字前的状态压入撤销栈
  void snapshotTyping();

private:
  /// 恢复快照到代码视图（undo/redo 共用）：可视化模式回填可视化页、
  /// modified 按栈深度重算、m_lastSnapshotText 对齐；全程 m_restoring 防回环
  void restoreSnapshot(const QString &snap);

  QByteArray m_visualBaseline;  ///< 最近成功回填/回写后的内容，不包含原文排版差异
  bool m_visualReady = false;  ///< 只有成功加载的可视化数据允许写回
  QString m_syncError;
  bool m_syncing = false;  ///< 同步中标志，避免信号回环
  QStringList m_undoStack;   ///< 撤销快照（index 0 = 最早；上限 kUndoStackLimit）
  QStringList m_redoStack;   ///< 恢复快照（新编辑时清空；undo/redo 互相对称搬运）
  /// 下一个打字步开始时应入栈的文本：各状态定格点（聚合超时/写回完成/撤销恢复/
  /// 代码回填）刷新为当前文本，打字步压栈时取它而非当前文本（此时已含新键入）
  QString m_lastSnapshotText;
  bool m_restoring = false;  ///< undo/redo 恢复中（contentsChange/清栈防回环）
  /// 可视化 → 代码写回的防抖定时器：可视化侧连续输入（每字符触发）时，
  /// 全量序列化 + setPlainText 全文重建 + 全文重高亮的开销是每键 O(文档)，
  /// 大文件明显卡顿；停顿 300ms 后合并为一次写回（切模式/保存路径直接调
  /// syncVisualToCode() 即时 flush，不受防抖影响）
  QTimer *m_syncDebounceTimer = nullptr;
  QTimer *m_typeSnapshotTimer = nullptr;  ///< 打字停顿聚合定时器（单次，~800ms）
};
