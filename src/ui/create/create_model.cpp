/**
 * @file create_model.cpp
 * @brief 新建文件对话框 — 数据模型层实现
 */

#include "create_model.h"

#include <QDir>
#include <QFileInfo>

// ════════════════════════════════════════════════════════════
//  静态方法：文件类型标签 / 后缀
// ════════════════════════════════════════════════════════════

QString CreateModel::fileTypeLabel(FileType type) {
  switch (type) {
    case Folder:
      return QStringLiteral("文件夹");
    case Ac:
      return QStringLiteral(".ac 文件");
    case Tpl:
      return QStringLiteral(".tpl 文件");
    case Json:
      return QStringLiteral(".json 文件");
    case Tpljson:
      return QStringLiteral(".tpljson 文件");
    default:
      return {};
  }
}

QString CreateModel::suffix(FileType type) {
  switch (type) {
    case Ac:
      return QString::fromLatin1(AcFileSuffix::kAc);
    case Tpl:
      return QString::fromLatin1(AcFileSuffix::kTpl);
    case Json:
      return QString::fromLatin1(AcFileSuffix::kJson);
    case Tpljson:
      return QString::fromLatin1(AcFileSuffix::kTpljson);
    default:
      return {};
  }
}

// ════════════════════════════════════════════════════════════
//  fullPath — 完整路径
// ════════════════════════════════════════════════════════════

QString CreateModel::fullPath() const {
  if (m_fileName.isEmpty()) return {};

  QString name = m_fileName;
  QString ext = suffix(m_fileType);

  // 如果用户输入的名称已包含正确的后缀，不再追加
  if (!ext.isEmpty() && !name.endsWith(ext, Qt::CaseInsensitive)) {
    name += ext;
  }

  return QDir::cleanPath(m_parentDir + QStringLiteral("/") + name);
}

// ════════════════════════════════════════════════════════════
//  validate — 输入验证
// ════════════════════════════════════════════════════════════

bool CreateModel::validate(QString &error) const {
  // 名称不能为空
  if (m_fileName.isEmpty()) {
    error = QStringLiteral("名称不能为空");
    return false;
  }

  // 检查非法字符（Windows 文件系统限制）
  const QString illegal = QStringLiteral("\\/:*?\"<>|");
  for (const QChar &ch : m_fileName) {
    if (illegal.contains(ch)) {
      error = QStringLiteral("文件名不能包含字符: \\ / : * ? \" < > |");
      return false;
    }
  }

  // 检查是否已存在
  QString path = fullPath();
  if (path.isEmpty()) {
    error = QStringLiteral("路径无效");
    return false;
  }
  if (QFileInfo::exists(path)) {
    error = QStringLiteral("文件或文件夹已存在");
    return false;
  }

  return true;
}