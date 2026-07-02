#pragma once

#include <QString>

class MainEngine {
public:
  static MainEngine &ins();

  /**
   * @brief 设置项目根目录（tree.config 所在目录）
   */
  void setRootDir(const QString &dir) { m_rootDir = dir; }

  /**
   * @brief 执行 main.ac 脚本
   * @param mainAcPath main.ac 文件的绝对路径
   * @return 错误信息，空字符串表示成功
   */
  QString execute(const QString &mainAcPath);

private:
  MainEngine() = default;
  QString readFileUtf8(const QString &path);
  QString m_rootDir;
};