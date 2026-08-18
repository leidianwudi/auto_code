/**
 * @file editor_lookup.h
 * @brief 编辑器查找辅助（MainDevMgr / DebugController 共用）
 *
 * 主窗口的编辑面板中，tab 页 widget 可能是 CodeEditor（普通文件）
 * 或 JsonVueWidget（.jsonvue 可视化包装，内部含 CodeEditor）。
 * 此前各处用重复的 qobject_cast + 回退循环解析，统一收敛到这里。
 */

#pragma once

#include <functional>

#include "src/ui/json_vue/json_vue_widget.h"
#include "src/ui/main_dev/main_dev_ui.h"
#include "src/util/ui/code/code_editor.h"

/// 从 tab 页 widget 解析出 CodeEditor（兼容 JsonVueWidget 包装），失败返回 nullptr
inline CodeEditor *editorFromWidget(QWidget *w) {
  if (!w) return nullptr;
  if (auto *editor = qobject_cast<CodeEditor *>(w)) return editor;
  if (auto *jvw = qobject_cast<JsonVueWidget *>(w)) return jvw->codeEditor();
  return nullptr;
}

/// 遍历主窗口所有编辑面板的所有编辑器；回调返回 false 时提前结束
inline void forEachEditor(MainDevUi *ui, const std::function<bool(CodeEditor *)> &fn) {
  if (!ui || !fn) return;
  for (int pi = 0; pi < ui->editorPanelCount(); ++pi) {
    auto *tabs = ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      CodeEditor *editor = editorFromWidget(tabs->widget(ti));
      if (editor && !fn(editor)) return;
    }
  }
}

/// 在所有编辑面板中查找已打开指定文件的编辑器（未打开则返回 nullptr）
inline CodeEditor *editorForFile(MainDevUi *ui, const QString &filePath) {
  CodeEditor *found = nullptr;
  forEachEditor(ui, [&found, &filePath](CodeEditor *editor) {
    if (editor->objectName() == filePath) {
      found = editor;
      return false;
    }
    return true;
  });
  return found;
}
