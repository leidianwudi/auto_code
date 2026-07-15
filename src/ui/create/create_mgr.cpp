/**
 * @file create_mgr.cpp
 * @brief 新建文件对话框 — 控制器层实现
 */

#include "create_mgr.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "create_model.h"
#include "create_ui.h"
#include "src/util/ui/component/aui_message_box.h"

// ════════════════════════════════════════════════════════════
//  onCreateWindow — AuiMgr 接口
// ════════════════════════════════════════════════════════════

QWidget *CreateMgr::onCreateWindow() {
  // CreateUi 本身是 QDialog，每次打开都是模态对话框，
  // 不需要由 AuiMgr 长期持有窗口，这里仅返回 nullptr 即可。
  // 实际创建在 createNew() 中动态打开。
  return nullptr;
}

// ════════════════════════════════════════════════════════════
//  createNew — 打开对话框并执行创建
// ════════════════════════════════════════════════════════════

bool CreateMgr::createNew(const QString &parentDir, QWidget *parentWidget) {
  // ── 1. 打开对话框 ──
  CreateUi dlg(parentWidget);
  if (dlg.exec() != QDialog::Accepted) return false;

  // ── 2. 读取用户输入 ──
  CreateModel model;
  model.setParentDir(parentDir);
  model.setFileType(static_cast<CreateModel::FileType>(dlg.fileTypeIndex()));
  model.setFileName(dlg.fileName());

  // ── 3. 验证 ──
  QString error;
  if (!model.validate(error)) {
    AuiMessageBox::show(parentWidget, QStringLiteral("创建失败"), error);
    return false;
  }

  // ── 4. 执行创建 ──
  QString fullPath = model.fullPath();
  bool ok = false;

  if (model.fileType() == CreateModel::Folder) {
    // 创建文件夹
    ok = QDir().mkpath(fullPath);
  } else {
    // 创建文件（确保父目录存在，写入空内容）
    QFileInfo fi(fullPath);
    QDir().mkpath(fi.absolutePath());
    QFile file(fullPath);
    ok = file.open(QIODevice::WriteOnly | QIODevice::Text);
    if (ok) file.close();
  }

  if (!ok) {
    AuiMessageBox::show(parentWidget, QStringLiteral("创建失败"),
                        QStringLiteral("无法创建: %1").arg(fullPath));
    return false;
  }

  return true;
}