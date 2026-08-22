/**
 * @file main_dev_mgr_session.cpp
 * @brief MainDevMgr 会话管理实现（从 main_dev_mgr.cpp 拆分）
 *
 * 包含以下功能：
 * - 会话状态存储路径（sessionSettingsPath）
 * - 保存打开文件列表与编辑器现场（saveOpenFilesToSettings）
 * - 还原上次打开的文件、面板数量与编辑器现场（restoreOpenFilesFromSettings）
 *
 * 实现 VSCode 式"关闭程序后重启还原现场"：文件列表按面板分组保存，
 * 每个文件记录光标位置 / 滚动位置 / 折叠状态。
 */

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTextCursor>

#include "editor_lookup.h"
#include "main_dev_mgr.h"
#include "main_dev_ui.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/code/code_editor.h"

/// @brief 会话状态存储文件（记录上次打开的文件列表）
static QString sessionSettingsPath() {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty())
    dir = QDir::homePath() + QString::fromUtf8(CodeConstants::Paths::kAppDataDirName);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/session.ini");
}

void MainDevMgr::saveOpenFilesToSettings() {
  // 按编辑器面板分组保存（还原拆分数量与每个面板的文件）
  QList<QVariant> groups;
  QList<QVariant> activeIdxes;
  for (int pi = 0; pi < m_ui->editorPanelCount(); ++pi) {
    auto *tabs = m_ui->editorPanelAt(pi);
    if (!tabs) continue;
    QStringList files;
    for (int ti = 0; ti < tabs->count(); ++ti) {
      CodeEditor *editor = editorFromWidget(tabs->widget(ti));
      if (!editor) continue;
      const QString fp = editor->objectName();
      if (!fp.isEmpty()) files.append(fp);
    }
    if (files.isEmpty()) continue;  // 跳过空面板
    groups << QVariant(files);
    // 保存当前激活标签页，保证重启后仍停留在关闭前正在编辑的文件
    activeIdxes << tabs->currentIndex();
  }
  QSettings s(sessionSettingsPath(), QSettings::IniFormat);
  s.setValue(QStringLiteral("session/editorPanels"), QVariant(groups));
  s.setValue(QStringLiteral("session/activeIndexes"), QVariant(activeIdxes));

  // ── 采集每个打开文件的光标/滚动/折叠状态，供重启后还原现场（类似 VSCode） ──
  QJsonObject states;
  forEachEditor(m_ui, [&states](CodeEditor *editor) {
    const QString fp = editor->objectName();
    if (fp.isEmpty()) return true;
    QJsonObject st;
    st.insert(QStringLiteral("scrollY"), editor->verticalScrollBar()->value());
    st.insert(QStringLiteral("scrollX"), editor->horizontalScrollBar()->value());
    st.insert(QStringLiteral("cursorPos"), editor->textCursor().position());
    QJsonArray foldArr;
    for (int b : editor->collapsedFoldBlocks()) foldArr.append(b);
    st.insert(QStringLiteral("foldBlocks"), foldArr);
    states.insert(fp, st);
    return true;
  });
  // 用 JSON 字符串而非 QVariantMap 存储，避免文件路径（含冒号/斜杠）作为 Ini 键被转义
  s.setValue(QStringLiteral("session/editorStates"),
             QString::fromUtf8(QJsonDocument(states).toJson(QJsonDocument::Compact)));
}

void MainDevMgr::restoreOpenFilesFromSettings() {
  // 还原期间抑制目录树定位：打开文件会触发 setCurrentIndex/焦点变化，
  // 进而 locateFile 自动展开并滚动目录树，破坏保存的展开状态
  m_restoringSession = true;

  QSettings s(sessionSettingsPath(), QSettings::IniFormat);
  const QList<QVariant> groups = s.value(QStringLiteral("session/editorPanels")).toList();
  const QList<QVariant> activeIdxes = s.value(QStringLiteral("session/activeIndexes")).toList();

  // ── 读取已保存的编辑器现场（光标/滚动/折叠），打开文件后按路径还原 ──
  QJsonObject states;
  {
    const QString raw = s.value(QStringLiteral("session/editorStates")).toString();
    if (!raw.isEmpty()) {
      const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
      if (doc.isObject()) states = doc.object();
    }
  }

  // 还原单个编辑器的折叠 → 滚动 → 光标（顺序固定：折叠影响滚动范围，故先折叠）
  auto restoreOne = [](CodeEditor *editor, const QJsonObject &st) {
    if (!editor || st.isEmpty()) return;
    QSet<int> collapsed;
    const QJsonArray foldArr = st.value(QStringLiteral("foldBlocks")).toArray();
    for (const auto &v : foldArr) collapsed.insert(v.toInt());
    editor->restoreFoldState(collapsed);
    auto *vs = editor->verticalScrollBar();
    auto *hs = editor->horizontalScrollBar();
    vs->setValue(qBound(vs->minimum(), st.value(QStringLiteral("scrollY")).toInt(0), vs->maximum()));
    hs->setValue(qBound(hs->minimum(), st.value(QStringLiteral("scrollX")).toInt(0), hs->maximum()));
    const int cursorPos =
        qBound(0, st.value(QStringLiteral("cursorPos")).toInt(0), editor->document()->characterCount());
    QTextCursor c = editor->textCursor();
    c.setPosition(cursorPos);
    editor->setTextCursor(c);
  };

  QTabWidget *target = nullptr;  // nullptr → 打开到默认（第一个）面板
  for (int gi = 0; gi < groups.size(); ++gi) {
    const QStringList files = groups[gi].toStringList();
    for (const QString &fp : files) {
      if (QFileInfo::exists(fp)) {
        CodeEditor *editor = openFileInEditor(fp, target);
        restoreOne(editor, states.value(fp).toObject());
      }
    }
    // 恢复该面板的当前（激活）标签，停留在关闭前正在编辑的文件
    int active = (gi < activeIdxes.size()) ? activeIdxes[gi].toInt() : files.size() - 1;
    QTabWidget *panel = target ? target : currentTabWidget();
    if (panel && active >= 0 && active < panel->count()) panel->setCurrentIndex(active);
    // 下一组文件应放到新建的编辑器面板中，还原拆分数量
    if (gi < groups.size() - 1) {
      auto *panel2 = m_ui->createEditorPanel();
      m_ui->addEditorPanel(panel2);
      connectEditorPanel(panel2);  // 连接关闭/切换等信号，否则标签关闭按钮无效
      target = panel2;
    }
  }

  // 清理还原过程中产生的空面板（其文件已不存在），避免在编辑器区出现空白条
  for (int pi = m_ui->editorPanelCount() - 1; pi >= 0; --pi) {
    if (m_ui->editorPanelCount() <= 1) break;  // 至少保留一个编辑面板
    auto *panel = m_ui->editorPanelAt(pi);
    if (panel && panel->count() == 0) m_ui->removeEditorPanelAt(pi);
  }

  m_restoringSession = false;
}
