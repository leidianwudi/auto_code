/**
 * @file setting_model.h
 * @brief 设置界面 — 数据模型层
 *
 * 封装对 SettingStore 的读写，为视图层提供结构化数据访问：
 * - 主题选择（浅色 / 深色 / 自定义）
 * - 颜色列表（分类分组）
 * - 快捷键列表（分类分组）
 *
 * 遵循项目 MVC 架构，由 SettingMgr 创建并注入视图。
 */

#pragma once

#include <QList>
#include <QString>

class SettingStore;

/**
 * @struct ColorEntry
 * @brief 单个可配置颜色条目
 */
struct ColorEntry {
  QString key;        ///< 颜色 key（对应 SettingStore 中的存储键）
  QString label;      ///< 显示名
  QString category;   ///< 分类（界面 / 编辑器 / 代码高亮）
  QString hex;        ///< 当前颜色值（#RRGGBB）
  bool custom;        ///< 是否为用户自定义
};

/**
 * @struct ShortcutSettingEntry
 * @brief 单个可配置快捷键条目
 */
struct ShortcutSettingEntry {
  QString key;      ///< 快捷键 key
  QString label;    ///< 显示名
  QString category; ///< 分类（文件 / 视图 / 编辑 / 调试）
  QString sequence; ///< 快捷键序列文本
};

/**
 * @class SettingModel
 * @brief 设置数据模型
 *
 * 从 SettingStore 读取主题、颜色、快捷键数据，供视图展示与编辑。
 */
class SettingModel {
public:
  SettingModel();

  /// 当前主题索引（0=浅色 1=深色 2=自定义）
  int themeIndex() const;
  /// 设置主题（索引）
  void setThemeIndex(int index);
  /// 主题名称列表
  QStringList themeNames() const;

  /// 所有颜色条目
  QList<ColorEntry> colors() const;
  /// 设置单个颜色（hex #RRGGBB）
  void setColor(const QString &key, const QString &hex);
  /// 重置单个颜色为主题默认
  void resetColor(const QString &key);
  /// 重置所有颜色
  void resetAllColors();

  /// 所有快捷键条目
  QList<ShortcutSettingEntry> shortcuts() const;
  /// 设置单个快捷键
  void setShortcut(const QString &key, const QString &sequence);

  /// 保存到磁盘
  void save();

private:
  SettingStore &m_store;
};
