/**
 * @file guess_code.h
 * @brief 代码自动补全 — 提供 AC / TPL / JSON 的关键字和函数名建议
 */

#pragma once

#include <QStringList>

class QCompleter;
class QObject;

class GuessCode {
public:
  /// @brief 文件类型
  enum FileType {
    AcFile,   ///< AC 脚本
    TplFile,  ///< TPL 模板
    JsonFile, ///< JSON 文件
  };

  /// @brief 获取指定文件类型的所有补全建议列表
  static QStringList getAllCompletions(FileType type);

  /// @brief 从文件类型创建 QCompleter 实例（调用者接管所有权）
  static QCompleter *createCompleter(FileType type, QObject *parent = nullptr);

  /// @brief 从编辑器验证模式（int）创建补全器，自动映射文件类型
  /// @param validationMode 传入 CodeEditor::ValidationMode 枚举值即可
  static QCompleter *createCompleter(int validationMode,
                                     QObject *parent = nullptr);
};