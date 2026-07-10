/**
 * @file main_dev_model.cpp
 * @brief MainDev 数据模型层实现
 */

#include "main_dev_model.h"

#include "src/tool/ui/code/code_editor.h"


// ──────────────────────────────────────────────────────────────
//  文件状态管理
// ──────────────────────────────────────────────────────────────

bool MainDevModel::isFileOpen(const QString &filePath) const {
  return openFiles.contains(filePath);
}

void MainDevModel::registerFile(const QString &filePath, const QString &content,
                                CodeEditor *editor) {
  fileContents[filePath] = content;
  openFiles[filePath] = editor;
}

void MainDevModel::unregisterFile(const QString &filePath) {
  fileContents.remove(filePath);
  openFiles.remove(filePath);
}

QString MainDevModel::fileContent(const QString &filePath) const {
  return fileContents.value(filePath);
}