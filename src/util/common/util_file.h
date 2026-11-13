#ifndef UTIL_FILE_H
#define UTIL_FILE_H

#include <QString>

/**
 * @brief 文件系统通用工具
 */
class UtilFile {
public:
  /**
   * @brief 在文件资源管理器中显示文件或文件夹
   *
   * - 文件: 打开所在文件夹并选中该文件
   * - 文件夹: 直接打开该文件夹
   * - 路径为空或不存在: 忽略
   *
   * @param path 文件或文件夹的绝对路径
   */
  static void showInExplorer(const QString &path);

  /**
   * @brief 读取文件全部内容（UTF-8 编码）
   * @param path 文件路径
   * @return 文件内容字符串；打开失败返回空串
   */
  static QString readUtf8(const QString &path);

  /// 原子替换 UTF-8 文件；写入或提交失败时保留原文件并返回错误。
  /// createParents 用于代码生成，编辑器保存不应隐式创建已被删除的目录。
  static bool writeUtf8(const QString &path, const QString &content, QString *error = nullptr,
                        bool createParents = false);
};

#endif  // UTIL_FILE_H
