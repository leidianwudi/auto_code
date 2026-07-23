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

  /**
   * @brief 写入字符串到文件（UTF-8 编码，覆盖模式）
   * @param path 文件路径
   * @param content 要写入的内容
   * @return 是否写入成功
   */
  static bool writeUtf8(const QString &path, const QString &content);
};

#endif  // UTIL_FILE_H