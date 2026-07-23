/**
 * @file help_key_model.h
 * @brief 快捷键查看器 — 数据模型层
 *
 * 存储应用中所有快捷键条目（分类、功能描述、快捷键序列）。
 * 遵循项目 MVC 架构风格，由 HelpKeyMgr 创建并填充，
 * HelpKeyUi 通过 getter 只读访问用于表格展示。
 */

#pragma once

#include <QList>
#include <QString>

/**
 * @struct ShortcutEntry
 * @brief 单个快捷键条目
 */
struct ShortcutEntry {
  QString category;     ///< 分类（如 "文件操作"）
  QString description;  ///< 功能描述（如 "打开文件"）
  QString keySequence;  ///< 快捷键序列文本（如 "Ctrl+O"）
};

/**
 * @class HelpKeyModel
 * @brief 快捷键数据模型
 *
 * 纯数据类，管理所有快捷键条目。
 * 构造时自动加载默认快捷键列表，按分类排序存储。
 */
class HelpKeyModel {
public:
  HelpKeyModel();

  /// 获取所有快捷键条目（已按分类排序）
  QList<ShortcutEntry> entries() const { return m_entries; }

  /// 获取条目数量
  int count() const { return m_entries.size(); }

private:
  /// 加载默认快捷键数据（按分类分组排序）
  void loadDefaults();

  QList<ShortcutEntry> m_entries;  ///< 快捷键条目列表
};
