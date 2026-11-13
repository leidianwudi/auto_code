/**
 * @file main_dev_model.cpp
 * @brief MainDev 数据模型层实现
 */

#include "main_dev_model.h"

#include "src/util/common/util_file.h"
#include "src/util/ui/code/code_editor.h"

// ──────────────────────────────────────────────────────────────
//  查询方法
// ──────────────────────────────────────────────────────────────

bool MainDevModel::isRegisteredElsewhere(const QString &filePath, CodeEditor *excludeEditor) const {
  auto it = openFiles.find(filePath);
  if (it == openFiles.end()) return false;
  return it.value() != excludeEditor;
}

// ──────────────────────────────────────────────────────────────
//  文件操作
// ──────────────────────────────────────────────────────────────

bool MainDevModel::saveEditor(CodeEditor *editor, QString *error) {
  if (error) error->clear();
  if (!editor) return false;

  QString filePath = editor->objectName();
  if (filePath.isEmpty()) {
    if (error) *error = QStringLiteral("文件路径不能为空");
    return false;
  }

  if (!UtilFile::writeUtf8(filePath, editor->toPlainText(), error)) {
    return false;
  }
  editor->document()->setModified(false);

  return true;
}

// ──────────────────────────────────────────────────────────────
//  状态管理
// ──────────────────────────────────────────────────────────────

void MainDevModel::registerFile(const QString &filePath, CodeEditor *editor) {
  openFiles[filePath] = editor;
}

void MainDevModel::unregisterFile(const QString &filePath) { openFiles.remove(filePath); }
