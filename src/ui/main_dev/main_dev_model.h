/**
 * @file main_dev_model.h
 * @brief MainDev 数据模型层
 *
 * 存储编辑器的运行时状态数据，提供文件操作和状态管理。
 * 由 MainDevMgr 控制器创建和读写，保存界面会话状态。
 */

#pragma once

#include <QHash>
#include <QPointer>
#include <QString>
#include <QTabWidget>

class CodeEditor;

/**
 * @class MainDevModel
 * @brief 编辑器数据模型
 *
 * MVC 中的数据层，管理：
 * - 已打开文件的路径映射
 * - 编辑器实例映射
 * - 最后活跃的编辑器面板组
 * - 当前光标追踪的编辑器
 * - 文件保存操作
 */
class MainDevModel {
public:
  MainDevModel() = default;

  /// 用户数据角色：在 QTreeWidgetItem 中存储文件完整路径
  static constexpr int FilePathRole = Qt::UserRole + 1;

  // ──────────────────────────────────────────────────────────
  //  查询方法
  // ──────────────────────────────────────────────────────────

  /// 判断文件是否在其它编辑器实例中打开（排除 excludeEditor）
  bool isRegisteredElsewhere(const QString &filePath, CodeEditor *excludeEditor) const;

  /// 获取所有已打开的文件路径列表
  QStringList openFilePaths() const { return openFiles.keys(); }

  // ──────────────────────────────────────────────────────────
  //  文件操作
  // ──────────────────────────────────────────────────────────

  /// 保存编辑器内容到文件，成功返回 true
  bool saveEditor(CodeEditor *editor, QString *error = nullptr);

  // ──────────────────────────────────────────────────────────
  //  状态管理
  // ──────────────────────────────────────────────────────────

  /// 记录文件打开状态
  void registerFile(const QString &filePath, CodeEditor *editor);

  /// 移除文件记录（标签页关闭时调用）
  void unregisterFile(const QString &filePath);

  // ──────────────────────────────────────────────────────────
  //  数据字段
  // ──────────────────────────────────────────────────────────

  /// filePath → editor 映射（跨所有面板组，用于快速查找已打开文件）
  QHash<QString, CodeEditor *> openFiles;

  /// 最后获得焦点的编辑器面板组（点击目录树时用于定位目标面板）
  QTabWidget *lastActivePanel = nullptr;

  /// 当前已连接光标位置信号的编辑器
  /// 使用 QPointer，编辑器被销毁时自动置空，避免悬空指针导致崩溃
  QPointer<CodeEditor> connectedEditor = nullptr;
};
