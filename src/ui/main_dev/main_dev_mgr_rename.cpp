/**
 * @file main_dev_mgr_rename.cpp
 * @brief 语义级重命名实现（MainDevMgr 的 F2 重命名）
 *
 * 流程：F2 弹窗输入新名 → 后台线程语义收集引用（作用域 + 跨文件 import）
 * → 应用替换（已打开编辑器走 document 支持撤销；未打开文件直接改写）。
 */

#include <QFile>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextStream>

#include <algorithm>
#include <QtConcurrent/QtConcurrent>

#include "main_dev_mgr.h"
#include "main_dev_ui.h"
#include "src/engine/rename/symbol_rename.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_input_dialog.h"

// ──────────────────────────────────────────────────────────────
//  重命名应用
// ──────────────────────────────────────────────────────────────

void MainDevMgr::applyRenameToFile(const QString &filePath, const QVector<RenameRef> &refs,
                                   const QString &newName) {
  if (refs.isEmpty()) return;
  // 按 (行,列) 降序，避免替换位置漂移
  QVector<RenameRef> sorted = refs;
  std::sort(sorted.begin(), sorted.end(), [](const RenameRef &a, const RenameRef &b) {
    if (a.line != b.line) return a.line > b.line;
    return a.column > b.column;
  });

  // 已打开的编辑器：通过 document 修改（支持撤销）
  if (auto *editor = findOpenEditor(filePath)) {
    QTextDocument *doc = editor->document();
    for (const RenameRef &r : sorted) {
      QTextBlock block = doc->findBlockByNumber(r.line - 1);
      if (!block.isValid()) continue;
      const int pos = block.position() + r.column;
      // 越界保护：不能超出该行内容末尾
      if (pos + r.length > block.position() + block.length() - 1) continue;
      QTextCursor c(doc);
      c.setPosition(pos);
      c.setPosition(pos + r.length, QTextCursor::KeepAnchor);
      c.insertText(newName);
    }
    // 同步其他打开同一文件的编辑器副本
    syncEditorsForFile(filePath, editor->toPlainText(), editor);
    return;
  }

  // 未打开：先存入缓冲、不写盘（树目录标黄，退出时提示保存——VSCode 行为），
  // 打开文件时应用缓冲，保存后才真正落盘
  {
    QFile rf(filePath);
    if (!rf.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&rf);
    QString text = in.readAll();
    rf.close();

    QVector<int> lineStarts;
    lineStarts.append(0);
    for (int i = 0; i < text.size(); ++i) {
      if (text.at(i) == QLatin1Char('\n')) lineStarts.append(i + 1);
    }
    for (const RenameRef &r : sorted) {
      if (r.line < 1 || r.line > lineStarts.size()) continue;
      const int off = lineStarts[r.line - 1] + r.column;
      if (off < 0 || off + r.length > text.size()) continue;
      text.replace(off, r.length, newName);
    }

    m_pendingFileChanges.insert(filePath, text);
    if (m_ui->fileTree()) m_ui->fileTree()->setFileModified(filePath, true);
    updateSaveButtonState();
  }
}

// ──────────────────────────────────────────────────────────────
//  F2 重命名入口
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onRenameSymbol(const QString &filePath, int line, int column,
                                const QString &name) {
  if (name.isEmpty() || !m_ui) return;

  // 弹窗输入新名称
  const QString newName = AuiInputDialog::getText(
      m_ui, QStringLiteral("重命名符号"), QStringLiteral("将「%1」重命名为:").arg(name), name);
  if (newName.isEmpty() || newName == name) return;

  // 新一轮：递增请求序号，作废过期收集结果
  const int requestId = ++m_renameRequestId;
  const QString root = m_ui->fileTree()->rootPath();
  if (root.isEmpty()) return;

  auto *watcher = new QFutureWatcher<QVector<RenameRef>>(this);
  connect(watcher, &QFutureWatcher<QVector<RenameRef>>::finished, this,
          [this, watcher, requestId, newName, name]() {
            watcher->deleteLater();
            if (requestId != m_renameRequestId) return;  // 过期结果丢弃
            const auto refs = watcher->result();
            if (refs.isEmpty()) {
              m_ui->appendOutput(QStringLiteral("未找到符号「%1」的引用，未执行重命名。").arg(name),
                                 false);
              return;
            }
            // 按文件分组应用，并逐个输出重命名成功的文件路径（可能涉及多文件）
            QHash<QString, QVector<RenameRef>> byFile;
            for (const RenameRef &r : refs) byFile[r.filePath].append(r);
            m_ui->appendOutput(
                QStringLiteral("已重命名「%1」→「%2」，共 %3 处：").arg(name, newName).arg(refs.size()),
                false);
            for (auto it = byFile.begin(); it != byFile.end(); ++it) {
              applyRenameToFile(it.key(), it.value(), newName);
              m_ui->appendOutput(
                  QStringLiteral("  %1（%2 处）").arg(it.key()).arg(it.value().size()), false);
            }
            // 清理编辑器上的旧引用/查找高亮（避免残留旧名高亮）
            clearReferenceHighlightFromEditors();
            clearSearchHighlightFromEditors();
            // 重命名可能改动多个文件：触发工作区防抖重扫，刷新其它文件（含未打开文件）的引用错误
            scheduleWorkspaceRescan();
          });
  // 主线程构建实时内容快照（已打开编辑器 + 缓冲文件），后台收集优先读缓冲而非磁盘，
  // 保证重命名未保存期间对新名的引用收集一致
  const QHash<QString, QString> liveContents = collectLiveContents();
  watcher->setFuture(QtConcurrent::run(collectSymbolReferencesLive, root, filePath, line,
                                       column, name, liveContents));
}
