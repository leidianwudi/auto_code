/**
 * @file navigation_history.h
 * @brief 导航历史 —「后退 / 前进」双栈状态机（从 MainDevMgr 拆出的 collaborator）
 *
 * 纯逻辑、无 UI 依赖：只管栈与位置去重，不执行跳转（跳转由 MainDevMgr 完成）。
 * 语义保持拆出前的既有行为：
 * - push 新位置时清空前进栈（浏览器语义）
 * - 与栈顶重复的位置不重复记录
 * - 后退栈上限 100 条（超出删最旧）
 * - 前进后其余前进项清空（保持既有 push 的清栈副作用，非浏览器完整语义）
 */

#pragma once

#include <QStack>
#include <QString>

/// @brief 导航历史记录项
struct NavigationEntry {
  QString filePath;  ///< 文件路径
  int line = 0;      ///< 行号（1-based）
  int column = 0;    ///< 列号（1-based）
};

/// @brief 导航历史双栈（后退 / 前进）
class NavigationHistory {
public:
  /// 后退栈是否可弹
  bool canBack() const { return !m_back.isEmpty(); }
  /// 前进栈是否可弹
  bool canForward() const { return !m_forward.isEmpty(); }
  int backSize() const { return m_back.size(); }
  int forwardSize() const { return m_forward.size(); }

  /// 是否正在执行导航跳转（跳转期间 push 静默忽略，防循环记录）
  bool isNavigating() const { return m_navigating; }
  void setNavigating(bool on) { m_navigating = on; }

  /// 记录新位置：与栈顶重复跳过；成功记录则清空前进栈并裁剪超限历史
  /// @return 是否实际入栈（去重 / 导航中时返回 false）
  bool push(const NavigationEntry &entry) {
    if (m_navigating) return false;
    if (!m_back.isEmpty()) {
      const auto &last = m_back.top();
      if (last.filePath == entry.filePath && last.line == entry.line &&
          last.column == entry.column)
        return false;
    }
    m_back.push(entry);
    m_forward.clear();  // 新操作时清空前进栈
    while (m_back.size() > kMaxEntries) m_back.remove(0);
    return true;
  }

  /// 弹出后退栈目标位置（调用方先 canBack() 检查；不触碰前进栈）
  NavigationEntry popBack() { return m_back.pop(); }

  /// 压入前进栈（后退时记录当前位置用；不清后退栈）
  void pushForward(const NavigationEntry &entry) { m_forward.push(entry); }

  /// 弹出前进栈目标位置（调用方先 canForward() 检查）
  NavigationEntry popForward() { return m_forward.pop(); }

private:
  static constexpr int kMaxEntries = 100;   ///< 后退栈上限
  QStack<NavigationEntry> m_back;           ///< 后退栈
  QStack<NavigationEntry> m_forward;        ///< 前进栈
  bool m_navigating = false;                ///< 是否正在执行导航跳转
};
