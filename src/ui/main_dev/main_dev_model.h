/**
 * @file main_dev_model.h
 * @brief MainDev 数据模型层
 *
 * 存储编辑器的运行时状态数据，包括：
 * - 已打开文件的路径和内容
 * - 编辑器实例映射
 * - 最后活跃的编辑器面板组
 * - 当前光标追踪的编辑器
 */

#pragma once

#include <QHash>
#include <QString>
#include <QTabWidget>

class CodeEditor;

/**
 * @class MainDevModel
 * @brief 编辑器数据模型
 *
 * 纯数据类，以文件路径为 key 管理所有已打开文件的状态，
 * 包括文件内容和编辑器实例。由 MainDev 创建，MainDevMgr 读写。
 */
class MainDevModel {
public:
  MainDevModel() = default;

  /// 用户数据角色：在 QTreeWidgetItem 中存储文件完整路径
  static constexpr int FilePathRole = Qt::UserRole + 1;

  /**
   * @brief 判断文件是否已打开
   * @param filePath 文件绝对路径
   * @return true 如果该文件已存在标签页中
   */
  bool isFileOpen(const QString &filePath) const {
    return openFiles.contains(filePath);
  }

  /**
   * @brief 记录文件打开状态
   * @param filePath 文件绝对路径
   * @param content 文件内容
   * @param editor 对应的编辑器实例
   */
  void registerFile(const QString &filePath, const QString &content,
                    CodeEditor *editor) {
    fileContents[filePath] = content;
    openFiles[filePath] = editor;
  }

  /**
   * @brief 移除文件记录（标签页关闭时调用）
   * @param filePath 文件绝对路径
   */
  void unregisterFile(const QString &filePath) {
    fileContents.remove(filePath);
    openFiles.remove(filePath);
  }

  /**
   * @brief 获取已打开文件的内容
   * @param filePath 文件绝对路径
   * @return 文件内容，若未打开则返回空字符串
   */
  QString fileContent(const QString &filePath) const {
    return fileContents.value(filePath);
  }

  /// filePath → 文件内容映射（标签页关闭时同步清理）
  QHash<QString, QString> fileContents;

  /// filePath → editor 映射（跨所有面板组，用于快速查找已打开文件）
  QHash<QString, CodeEditor *> openFiles;

  /// 最后获得焦点的编辑器面板组（点击目录树时用于定位目标面板）
  QTabWidget *lastActivePanel = nullptr;

  /// 当前已连接光标位置信号的编辑器
  CodeEditor *connectedEditor = nullptr;
};