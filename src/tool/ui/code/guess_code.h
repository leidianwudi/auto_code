/**
 * @file guess_code.h
 * @brief 代码自动补全 — 提供 AC / TPL / JSON 的关键字和函数名建议
 */

#pragma once

#include <QHash>
#include <QStringList>

class QCompleter;
class QObject;

class GuessCode {
public:
  /// @brief 文件类型
  enum FileType {
    AcFile,    ///< AC 脚本
    TplFile,   ///< TPL 模板
    JsonFile,  ///< JSON 文件
  };

  /// @brief 获取指定文件类型的所有补全建议列表
  static QStringList getAllCompletions(FileType type);

  /// @brief 从文件类型创建 QCompleter 实例（调用者接管所有权）
  static QCompleter *createCompleter(FileType type, QObject *parent = nullptr);

  /// @brief 从编辑器验证模式（int）创建补全器，自动映射文件类型
  /// @param validationMode 传入 CodeEditor::ValidationMode 枚举值即可
  static QCompleter *createCompleter(int validationMode, QObject *parent = nullptr);

  /// @brief 将 CodeEditor::ValidationMode 枚举值映射为 FileType
  static FileType validationModeToFileType(int validationMode);

  /// @brief 从 AC 脚本文本中提取所有 let 声明的变量名
  /// @param text 编辑器全文
  /// @return 去重后的变量名列表
  static QStringList extractLetVariables(const QString &text, int cursorLine = -1);

  /// @brief 从 AC 脚本文本中提取用户定义的函数名
  /// @param text 编辑器全文
  /// @param cursorLine 光标所在行号（-1 表示提取全部，>=0 表示只提取该行之前定义的）
  /// @return 去重后的函数名列表
  static QStringList extractUserFunctions(const QString &text, int cursorLine = -1);

  /// @brief 从 AC 脚本文本中提取用户定义的类名
  /// @param text 编辑器全文
  /// @param cursorLine 光标所在行号（-1 表示提取全部，>=0 表示只提取该行之前定义的）
  /// @return 去重后的类名列表
  static QStringList extractUserClasses(const QString &text, int cursorLine = -1);

  /// @brief 从 AC 脚本文本中提取类方法映射
  /// @param text 编辑器全文
  /// @return 类名 → 该类的函数名列表
  static QHash<QString, QStringList> extractClassMethods(const QString &text);

  /// @brief 从 AC 脚本文本中提取变量类型映射
  /// @param text 编辑器全文
  /// @return 变量名 → 类型名（通过 let x = new Type() 解析）
  static QHash<QString, QString> extractVariableTypes(const QString &text);
};