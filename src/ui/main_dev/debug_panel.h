/**
 * @file debug_panel.h
 * @brief 调试面板 — 在目录树区域以 tab 展示调用栈与变量快照
 *
 * 当脚本命中断点或单步暂停时，由 MainDevMgr 将 AcDebugger 携带的
 * 调用栈与变量快照填充到本面板，并提供 继续/单步跳过/单步进入/单步跳出
 * 控制按钮与暂停状态提示。
 */

#pragma once

#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

#include "src/engine/script/ac_debugger.h"

/**
 * @class DebugPanel
 * @brief 调试信息展示面板
 */
class DebugPanel : public QWidget {
  Q_OBJECT

public:
  explicit DebugPanel(QWidget *parent = nullptr);

  /// 侧边栏面板最小尺寸提示设小，避免把左侧目录栏撑宽
  QSize minimumSizeHint() const override { return QSize(0, 0); }

  /// 填充暂停时的调用栈与变量快照
  void setSnapshot(const QVector<AcDebugFrame> &stack, const QList<AcDebugVar> &vars);
  /// 设置全部断点列表（文件路径 + 行号），供「断点」页展示
  void setBreakpoints(const QList<QPair<QString, int>> &breakpoints);
  /// 清空所有内容
  void clear();
  /// 设置暂停状态（暂停时启用单步按钮，运行时禁用）
  void setPaused(bool paused);
  /// 设置调试会话是否激活（未激活时禁用所有操作）
  void setActive(bool active);
  /// 设置状态提示文本（如 "已暂停 @ 行 12" / "运行中"）
  void setStatus(const QString &text);

signals:
  void continueClicked();  ///< 继续执行（F5）
  void stepOverClicked();  ///< 单步跳过（F10）
  void stepIntoClicked();  ///< 单步进入（F11）
  void stepOutClicked();   ///< 单步跳出（Shift+F11）
  /// 双击断点条目：请求打开对应文件并定位到断点位置
  void breakpointActivated(const QString &filePath, int line);

private:
  QTreeWidget *m_stackTree = nullptr;  ///< 调用栈树
  QTreeWidget *m_varTree = nullptr;    ///< 变量树
  QTreeWidget *m_breakTree = nullptr;  ///< 断点列表树
  QPushButton *m_continueBtn = nullptr;
  QPushButton *m_stepOverBtn = nullptr;
  QPushButton *m_stepIntoBtn = nullptr;
  QPushButton *m_stepOutBtn = nullptr;
  QLabel *m_statusLabel = nullptr;

  /// 将 QJsonValue 格式化为可读字符串
  static QString formatValue(const QJsonValue &v);
  /// 递归展开复杂变量（数组/对象）为子节点
  void appendVarValue(QTreeWidgetItem *parent, const QString &key, const QJsonValue &val);
};