/** @file editor_factory.h
 * @brief 所有文件类型的编辑标签页创建入口（打开、拆分、会话恢复共用）。
 */
#pragma once
#include <QString>
class QWidget;
class CodeEditor;

/// 创建控件与文件上下文；标签注册、模式切换与会话状态由宿主管理。
class EditorFactory {
public:
  /// 创建包含代码页和可选可视化页的标签；editorOut 返回实际代码编辑器。
  static QWidget *create(const QString &filePath, const QString &content,
                         const QString &httpConfigPath, CodeEditor **editorOut);

private:
  static CodeEditor *createCodeEditor(const QString &filePath);
};
