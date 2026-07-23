/**
 * @file help_key_model.cpp
 * @brief 快捷键查看器 — 数据模型层实现
 */

#include "help_key_model.h"

// ════════════════════════════════════════════════════════════
//  构造 — 自动加载默认快捷键
// ════════════════════════════════════════════════════════════

HelpKeyModel::HelpKeyModel() { loadDefaults(); }

// ════════════════════════════════════════════════════════════
//  loadDefaults — 加载默认快捷键数据（按分类分组排序）
// ════════════════════════════════════════════════════════════

void HelpKeyModel::loadDefaults() {
  // ── 文件操作 ──
  m_entries.append({QStringLiteral("文件操作"), QStringLiteral("打开文件"), QStringLiteral("Ctrl+O")});
  m_entries.append({QStringLiteral("文件操作"), QStringLiteral("保存当前文件"), QStringLiteral("Ctrl+S")});
  m_entries.append({QStringLiteral("文件操作"), QStringLiteral("关闭标签页"), QStringLiteral("Ctrl+W")});

  // ── 编辑操作 ──
  m_entries.append({QStringLiteral("编辑操作"), QStringLiteral("查找"), QStringLiteral("Ctrl+F")});
  m_entries.append({QStringLiteral("编辑操作"), QStringLiteral("替换"), QStringLiteral("Ctrl+H")});
  m_entries.append({QStringLiteral("编辑操作"), QStringLiteral("跳转到匹配括号"), QStringLiteral("Ctrl+M")});
  m_entries.append({QStringLiteral("编辑操作"), QStringLiteral("选中括号内所有内容"), QStringLiteral("Ctrl+Shift+M")});

  // ── 导航 ──
  m_entries.append({QStringLiteral("导航"), QStringLiteral("转到定义"), QStringLiteral("F12")});
  m_entries.append({QStringLiteral("导航"), QStringLiteral("后退"), QStringLiteral("Alt+Left")});
  m_entries.append({QStringLiteral("导航"), QStringLiteral("前进"), QStringLiteral("Alt+Right")});
  m_entries.append({QStringLiteral("导航"), QStringLiteral("搜索工作区符号"), QStringLiteral("Ctrl+T")});

  // ── 视图操作 ──
  m_entries.append({QStringLiteral("视图操作"), QStringLiteral("向右拆分编辑器"), QStringLiteral("Ctrl+\\")});

  // ── 执行 ──
  m_entries.append({QStringLiteral("执行"), QStringLiteral("执行脚本"), QStringLiteral("F5")});
}
