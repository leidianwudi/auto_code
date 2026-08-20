/**
 * @file main_dev_mgr_file.cpp
 * @brief 文件操作实现（MainDevMgr 的编辑器创建、文件打开、重命名、删除）
 */

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollBar>
#include <QTextStream>

#include "debug_controller.h"
#include "editor_lookup.h"
#include "main_dev_mgr.h"
#include "main_dev_model.h"
#include "main_dev_ui.h"
#include "src/engine/ac_language.h"
#include "src/ui/json_vue/json_vue_widget.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/code/code_editor.h"
#include "src/util/ui/component/aui_message_box.h"
#include "src/util/ui/highlighter/light_ac.h"
#include "src/util/ui/highlighter/light_json.h"
#include "src/util/ui/highlighter/light_tpl.h"

// ──────────────────────────────────────────────────────────────
//  创建 / 打开编辑器
// ──────────────────────────────────────────────────────────────

CodeEditor *MainDevMgr::createEditorForFile(const QString &filePath) {
  auto *editor = new CodeEditor;

  if (filePath.endsWith(AcFileSuffix::kJson, Qt::CaseInsensitive)) {
    editor->setSyntaxHighlighter(new LightJson(editor->document()));
    editor->setValidationMode(CodeEditor::JsonValidation);
  } else if (filePath.endsWith(AcFileSuffix::kAc, Qt::CaseInsensitive)) {
    editor->setSyntaxHighlighter(new LightAc(editor->document()));
    editor->setValidationMode(CodeEditor::AcValidation);
  } else {
    // .tpl 及其他未知类型统一按模板处理
    editor->setSyntaxHighlighter(new LightTpl(editor->document()));
    editor->setValidationMode(CodeEditor::TemplateValidation);
  }

  return editor;
}

CodeEditor *MainDevMgr::openFileInEditor(const QString &filePath, QTabWidget *target) {
  // ── 查重：遍历所有面板组的所有标签页 ──
  // 支持 CodeEditor 和 JsonVueWidget（包装器）两种类型
  for (int i = 0; i < m_ui->editorPanelCount(); ++i) {
    auto *tabs = m_ui->editorPanelAt(i);
    if (!tabs) continue;
    for (int j = 0; j < tabs->count(); ++j) {
      CodeEditor *editor = editorFromWidget(tabs->widget(j));
      if (editor && editor->objectName() == filePath) {
        tabs->setCurrentIndex(j);
        // 会话恢复阶段不主动抢焦点，让用户首次点击时触发 focusChanged → 定位目录树
        if (!m_restoringSession) {
          tabs->setFocus();
          editor->setFocus();
        }
        return editor;
      }
    }
  }

  // ── 读取文件 ──
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    AuiMessageBox::show(m_ui, QStringLiteral("打开失败"),
                        QStringLiteral("无法打开文件: %1").arg(filePath));
    return nullptr;
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  // ── 创建编辑器 ──
  // .jsonvue 文件使用 JsonVueWidget 包装器（CodeEditor + 可视化编辑器）
  bool isJsonVue = filePath.endsWith(AcFileSuffix::kJsonvue, Qt::CaseInsensitive);
  CodeEditor *editor = nullptr;
  QWidget *tabWidget = nullptr;
  JsonVueWidget *jvw = nullptr;

  if (isJsonVue) {
    jvw = new JsonVueWidget;
    editor = jvw->codeEditor();
    editor->setPlainText(content);
    tabWidget = jvw;
    // 优先从 .jsonvue 文件向上查找最近的 api_auth_data.ac 加载 HTTP 配置；
    // 找不到时回落到启动项 AC 脚本（向后兼容）
    QString acPath = JsonVueWidget::findNearestApiAuthDataAc(filePath);
    if (acPath.isEmpty()) {
      acPath = m_ui->startupCombo()->currentData().toString();
    }
    if (!acPath.isEmpty()) {
      jvw->loadHttpConfigFromAcFile(acPath);
    }
    // 可视化按钮生效时，自动以可视化方式打开
    if (m_ui->visualToggleBtn() && m_ui->visualToggleBtn()->isChecked()) {
      jvw->switchToVisual();
    }
  } else {
    editor = createEditorForFile(filePath);
    editor->setPlainText(content);
    tabWidget = editor;
  }

  // ── 获取 / 创建面板组 ──
  QTabWidget *tabs = target ? target : currentTabWidget();
  if (!tabs) {
    tabs = m_ui->createEditorPanel();
    m_ui->addEditorPanel(tabs);
    connectEditorPanel(tabs);  // 连接关闭/切换等信号，否则标签关闭按钮无效
  }

  QFileInfo fi(filePath);
  int idx = tabs->addTab(tabWidget, fi.fileName());
  tabs->setTabToolTip(idx, filePath);
  tabs->setCurrentIndex(idx);

  // ── 首次加载文件时刷新主分割器布局 ──
  if (m_model->openFiles.isEmpty()) m_ui->adjustMainSplitter();

  // ── 记录状态 ──
  m_model->registerFile(filePath, content, editor);
  // 必须先设置 objectName（文件路径），再触发验证；
  // 否则验证时 m_filePath 为空，import 解析被跳过，导致误报 "class X has no method" 类型错误
  editor->setObjectName(filePath);
  // 验证结果聚合到「问题」面板：每个编辑器永久连接（connectEditor 只连接当前编辑器，
  // 拆分面板/后台标签页产生的验证结果会丢失，导致问题面板与实际编辑内容不符）
  connect(editor, &CodeEditor::validationIssues, this, &MainDevMgr::onValidationIssues);
  editor->validate();
  // 恢复该文件持久化的断点（关闭重开后保留）：
  // 剔除超出总行数的失效行并同步更新持久存储（DebugController 内处理）
  editor->setBreakpoints(
      debugController()->takeBreakpointsForFile(filePath, editor->document()->blockCount()));
  // 会话恢复阶段不主动抢焦点，让用户首次点击时触发 focusChanged → 定位目录树
  if (!m_restoringSession) editor->setFocus();
  m_ui->setWindowTitle(MainDevUi::fileTitle(fi.fileName()));

  // ── 断点变化时刷新调试面板「断点」列表 ──
  connect(editor, &CodeEditor::breakpointsChanged, this,
          [this]() { debugController()->refreshBreakpointList(); });
  debugController()->refreshBreakpointList();

  // ── 修改标记：内容变化时标签页和树节点绘制红色 "*" ──
  connect(editor->document(), &QTextDocument::modificationChanged, this,
          [this, tabs, editor, filePath, tabWidget](bool changed) {
            for (int i = 0; i < tabs->count(); ++i) {
              if (tabs->widget(i) == tabWidget) {
                auto *bar = qobject_cast<DraggableTabBar *>(tabs->tabBar());
                if (bar) bar->setTabModified(i, changed);
                break;
              }
            }
            // 更新树形目录对应文件的修改状态
            m_ui->fileTree()->setFileModified(filePath, changed);
            // 更新保存按钮可用状态
            updateSaveButtonState();
          });

  // ── JsonVueWidget：可视化编辑器内容变化时也触发修改标记 ──
  if (jvw) {
    connect(jvw, &JsonVueWidget::contentChanged, this,
            [jvw, editor]() { editor->document()->setModified(true); });
  }

  // ── 打开文件后保存当前打开列表，供下次启动还原 ──
  saveOpenFilesToSettings();

  return editor;
}

// ──────────────────────────────────────────────────────────────
//  保存 + 同步（拆分副本场景）
// ──────────────────────────────────────────────────────────────

bool MainDevMgr::saveAndSync(CodeEditor *editor) {
  if (!editor) return false;
  if (!m_model->saveEditor(editor)) return false;
  // 保存成功后，同步其他打开同一文件的编辑器实例内容
  syncEditorsForFile(editor->objectName(), editor->toPlainText(), editor);
  return true;
}

void MainDevMgr::syncEditorsForFile(const QString &filePath, const QString &content,
                                    CodeEditor *sourceEditor) {
  if (filePath.isEmpty()) return;
  // 遍历所有面板的所有标签页，找到同路径的其他编辑器实例
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      CodeEditor *editor = editorFromWidget(tabs->widget(ti));
      if (!editor || editor == sourceEditor) continue;
      if (editor->objectName() != filePath) continue;

      // 保存光标位置和滚动位置
      QTextCursor cursor = editor->textCursor();
      int pos = qMin(cursor.position(), content.length());
      int anchor = qMin(cursor.anchor(), content.length());
      int scrollY = editor->verticalScrollBar()->value();

      // 更新内容（setPlainText 会重置 undo 栈）
      editor->setPlainText(content);
      editor->document()->setModified(false);

      // 恢复光标和滚动位置
      cursor.setPosition(anchor);
      cursor.setPosition(pos, QTextCursor::KeepAnchor);
      editor->setTextCursor(cursor);
      editor->verticalScrollBar()->setValue(scrollY);
    }
  }
}

void MainDevMgr::onRenameFile(const QString &oldPath, const QString &newName) {
  // 校验新名称
  if (newName.isEmpty()) return;

  static const QString kInvalidChars = QStringLiteral("\\/:*?\"<>|");
  for (const QChar &c : newName) {
    if (kInvalidChars.contains(c)) {
      AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                          QStringLiteral("文件名不能包含以下字符: %1").arg(kInvalidChars));
      return;
    }
  }

  QFileInfo oldInfo(oldPath);
  QString parentDir = oldInfo.absolutePath();
  QString newPath = QDir::cleanPath(parentDir + QStringLiteral("/") + newName);

  // 同名检查
  if (QFileInfo::exists(newPath)) {
    AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                        QStringLiteral("已存在同名文件或文件夹: %1").arg(newName));
    return;
  }

  bool isDir = oldInfo.isDir();

  if (isDir) {
    // 文件夹重命名
    QDir dir(parentDir);
    if (!dir.rename(oldInfo.fileName(), newName)) {
      AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                          QStringLiteral("无法重命名文件夹: %1").arg(oldInfo.fileName()));
      return;
    }

    // 更新所有以旧路径开头的已打开文件
    QString oldDirPath = QDir::cleanPath(oldPath);
    for (const QString &filePath : m_model->openFilePaths()) {
      if (filePath.startsWith(oldDirPath + QStringLiteral("/"))) {
        QString newFilePath = newPath + filePath.mid(oldDirPath.length());
        CodeEditor *editor = m_model->openFiles.value(filePath);
        if (editor) {
          editor->setObjectName(newFilePath);
          // 更新 tab 标题和 tooltip
          for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
            auto *tabs = m_ui->editorPanelAt(pi);
            if (!tabs) continue;
            for (int ti = 0; ti < tabs->count(); ++ti) {
              if (tabs->widget(ti) == editor) {
                tabs->setTabText(ti, QFileInfo(newFilePath).fileName());
                tabs->setTabToolTip(ti, newFilePath);
                break;
              }
            }
          }
        }
        // 更新 model 中的注册信息
        QString content = m_model->fileContents.value(filePath);
        m_model->openFiles.remove(filePath);
        m_model->fileContents.remove(filePath);
        m_model->openFiles.insert(newFilePath, editor);
        m_model->fileContents.insert(newFilePath, content);
      }
    }
  } else {
    // 文件重命名
    QFile file(oldPath);
    if (!file.rename(newPath)) {
      AuiMessageBox::show(m_ui, QStringLiteral("重命名失败"),
                          QStringLiteral("无法重命名文件: %1").arg(oldInfo.fileName()));
      return;
    }

    // 更新已打开的编辑器
    CodeEditor *editor = m_model->openFiles.value(oldPath);
    if (editor) {
      editor->setObjectName(newPath);
      for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
        auto *tabs = m_ui->editorPanelAt(pi);
        if (!tabs) continue;
        for (int ti = 0; ti < tabs->count(); ++ti) {
          if (tabs->widget(ti) == editor) {
            tabs->setTabText(ti, newName);
            tabs->setTabToolTip(ti, newPath);
            break;
          }
        }
      }
      // 更新 model 中的注册信息
      QString content = m_model->fileContents.value(oldPath);
      m_model->openFiles.remove(oldPath);
      m_model->fileContents.remove(oldPath);
      m_model->openFiles.insert(newPath, editor);
      m_model->fileContents.insert(newPath, content);
    }
  }

  // 更新启动项数据（文件重命名或文件夹重命名中的 .ac 文件路径）
  if (isDir) {
    QString oldDirPath = QDir::cleanPath(oldPath);
    QList<QPair<QString, QString>> renames;
    for (const QString &sp : m_ui->fileTree()->startupFiles()) {
      if (sp.startsWith(oldDirPath + QStringLiteral("/"))) {
        QString newSp = newPath + sp.mid(oldDirPath.length());
        renames.append({sp, newSp});
      }
    }
    m_ui->fileTree()->renameStartupPaths(renames);
  } else {
    m_ui->fileTree()->renameStartupPath(oldPath, newPath);
  }

  // 刷新树
  m_ui->fileTree()->refreshTree();
}

// ──────────────────────────────────────────────────────────────
//  删除
// ──────────────────────────────────────────────────────────────

void MainDevMgr::onDeleteFile(const QString &path) {
  QFileInfo info(path);
  if (!info.exists()) return;

  bool isDir = info.isDir();
  QString name = info.fileName();

  // 确认对话框
  if (!AuiMessageBox::confirm(
          m_ui, QString::fromUtf8(CodeConstants::UiText::kConfirmDelete),
          isDir ? QStringLiteral("确定要删除文件夹 \"%1\" 及其所有内容吗？").arg(name)
                : QStringLiteral("确定要删除文件 \"%1\" 吗？").arg(name))) {
    return;
  }

  if (isDir) {
    // 删除文件夹前，关闭所有以该路径开头的已打开文件
    QString dirPath = QDir::cleanPath(path);
    QStringList toClose;
    for (const QString &filePath : m_model->openFilePaths()) {
      if (filePath.startsWith(dirPath + QStringLiteral("/"))) {
        toClose.append(filePath);
      }
    }
    for (const QString &filePath : toClose) {
      CodeEditor *editor = m_model->openFiles.value(filePath);
      if (editor) {
        for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
          auto *tabs = m_ui->editorPanelAt(pi);
          if (!tabs) continue;
          for (int ti = 0; ti < tabs->count(); ++ti) {
            if (tabs->widget(ti) == editor) {
              closeTab(tabs, ti);
              break;
            }
          }
        }
        m_model->openFiles.remove(filePath);
        m_model->fileContents.remove(filePath);
      }
    }

    QDir dir(path);
    if (!dir.removeRecursively()) {
      AuiMessageBox::show(m_ui, QStringLiteral("删除失败"),
                          QStringLiteral("无法删除文件夹: %1").arg(name));
      return;
    }
  } else {
    // 删除文件前，关闭已打开的编辑器
    CodeEditor *editor = m_model->openFiles.value(path);
    if (editor) {
      for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
        auto *tabs = m_ui->editorPanelAt(pi);
        if (!tabs) continue;
        for (int ti = 0; ti < tabs->count(); ++ti) {
          if (tabs->widget(ti) == editor) {
            closeTab(tabs, ti);
            break;
          }
        }
      }
      m_model->openFiles.remove(path);
      m_model->fileContents.remove(path);
    }

    QFile file(path);
    if (!file.remove()) {
      AuiMessageBox::show(m_ui, QStringLiteral("删除失败"),
                          QStringLiteral("无法删除文件: %1").arg(name));
      return;
    }
  }

  // 刷新树
  m_ui->fileTree()->refreshTree();
}
