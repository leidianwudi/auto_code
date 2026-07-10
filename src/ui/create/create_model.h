/**
 * @file create_model.h
 * @brief 新建文件对话框 — 数据模型层
 */

#pragma once

#include <QString>

#include "src/engine/ac_language.h"

/**
 * @class CreateModel
 * @brief 新建文件/文件夹的数据模型
 *
 * 纯数据类，存储用户输入的文件类型、名称和目标目录。
 * 提供验证逻辑，由 CreateMgr 驱动，CreateUi 通过 getter 读取。
 */
class CreateModel {
public:
  /// 文件类型枚举
  enum FileType {
    Folder,   ///< 文件夹
    Ac,       ///< .ac 文件
    Tpl,      ///< .tpl 文件
    Json,     ///< .json 文件
    Tpljson,  ///< .tpljson 文件
    FileTypeCount
  };

  CreateModel() = default;

  // ── Setters ──

  void setParentDir(const QString &dir) { m_parentDir = dir; }
  void setFileType(FileType type) { m_fileType = type; }
  void setFileName(const QString &name) { m_fileName = name.trimmed(); }

  // ── Getters ──

  QString parentDir() const { return m_parentDir; }
  FileType fileType() const { return m_fileType; }
  QString fileName() const { return m_fileName; }

  /// 获取文件类型的显示标签（用于下拉框）
  static QString fileTypeLabel(FileType type);

  /// 获取文件类型的后缀名（文件夹返回空字符串）
  static QString suffix(FileType type);

  /// 获取完整路径（父目录 + 文件名 + 后缀）
  QString fullPath() const;

  /// 验证输入是否合法，不合法时返回 false 并设置 error 描述
  bool validate(QString &error) const;

private:
  QString m_parentDir;
  FileType m_fileType = Folder;
  QString m_fileName;
};