#pragma once

#include <QString>

class MainEngine {
public:
  static MainEngine &ins();

  /**
   * @brief 执行 main.ac 脚本
   * @param mainAcPath main.ac 文件的绝对路径
   * @return 错误信息，空字符串表示成功
   */
  QString execute(const QString &mainAcPath);

private:
  MainEngine() = default;
  QString readFileUtf8(const QString &path);
};