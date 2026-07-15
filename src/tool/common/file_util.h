#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <QString>

/**
 * @brief 文件系统通用工具
 */
class FileUtil {
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
};

#endif  // FILE_UTIL_H