/**
 * @file main_dev_model.cpp
 * @brief MainDev 数据模型层实现
 */

#include "main_dev_model.h"

#include <QFile>
#include <QTextStream>

#include "src/tool/ui/code/code_editor.h"

// ──────────────────────────────────────────────────────────────
//  查询方法
// ──────────────────────────────────────────────────────────────

bool MainDevModel::isFileOpen(const QString &filePath) const {
  return openFiles.contains(filePath);
}

QString MainDevModel::fileContent(const QString &filePath) const {
  return fileContents.value(filePath);
}

bool MainDevModel::isRegisteredElsewhere(const QString &filePath, CodeEditor *excludeEditor) const {
  auto it = openFiles.find(filePath);
  if (it == openFiles.end()) return false;
  return it.value() != excludeEditor;
}

bool MainDevModel::hasAnyModified() const {
  for (auto it = openFiles.cbegin(); it != openFiles.cend(); ++it) {
    if (it.value() && it.value()->document()->isModified()) return true;
  }
  return false;
}

// ──────────────────────────────────────────────────────────────
//  文件操作
// ──────────────────────────────────────────────────────────────

bool MainDevModel::saveEditor(CodeEditor *editor) {
  if (!editor) return false;

  QString filePath = editor->objectName();
  if (filePath.isEmpty()) return false;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning("保存失败: %s", qPrintable(filePath));
    return false;
  }

  file.write(editor->toPlainText().toUtf8());
  file.close();
  editor->document()->setModified(false);

  // 同步更新缓存内容
  fileContents[filePath] = editor->toPlainText();
  return true;
}

// ──────────────────────────────────────────────────────────────
//  状态管理
// ──────────────────────────────────────────────────────────────

void MainDevModel::registerFile(const QString &filePath, const QString &content,
                                CodeEditor *editor) {
  fileContents[filePath] = content;
  openFiles[filePath] = editor;
}

void MainDevModel::unregisterFile(const QString &filePath) {
  fileContents.remove(filePath);
  openFiles.remove(filePath);
}