/**
 * @file tree_state_store.h
 * @brief 目录树状态持久化数据层（与 UI 解耦）
 *
 * 负责目录树状态的序列化/反序列化，不依赖 TreeDir 或任何 UI 组件。
 * 状态包括：勾选文件、启动项、选中启动项、可视化开关、展开节点。
 */

#pragma once

#include <QString>
#include <QStringList>

/**
 * @class TreeStateStore
 * @brief 目录树状态的数据存储
 *
 * 持有相对路径形式的状态数据，并提供 load/save 能力。
 * 路径统一使用相对根目录的形式，由调用方（TreeDir）负责绝对/相对转换。
 */
class TreeStateStore {
public:
  QStringList checkedRelPaths;   ///< 已勾选 json 文件的相对路径
  QStringList startupRelPaths;   ///< 启动项 .ac 文件的相对路径
  QString selectedStartupRel;    ///< 当前选中启动项的相对路径
  QStringList expandedRelPaths;  ///< 展开目录节点的相对路径（层级用 "/" 分隔）
  bool visualToggle = false;     ///< 可视化编辑按钮状态

  /// 从配置文件加载；成功返回 true（文件不存在或解析失败返回 false）
  bool load(const QString &configPath);
  /// 持久化到配置文件
  void save(const QString &configPath) const;
};