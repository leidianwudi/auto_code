/**
 * @file create_model.h
 * @brief 新建文件对话框 — 数据模型层
 */

#pragma once

#include <QString>
#include <QVector>

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
    Folder,     ///< 文件夹
    Ac,         ///< .ac 文件
    Tpl,        ///< .tpl 文件
    Json,       ///< .json 文件
    Jsonvue,    ///< .jsonvue 文件（Vue3 后台管理界面配置）
    Jsonsource, ///< .jsonsource 文件（下拉框数据源配置）
    Jsonupload, ///< .jsonupload 文件（图片上传预设配置）
    FileTypeCount
  };

  /// 文件类型选项（新建对话框与查找面板文件类型过滤共用；不含文件夹）
  struct FileTypeOption {
    QString label;  ///< 显示标签，如 ".ac 文件"
    QString suffix; ///< 文件后缀，如 ".ac"
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

  /// 所有"文件"类型选项（标签 → 后缀，按枚举顺序）。
  /// 供新建对话框 / 查找面板文件类型过滤共用；以后扩展文件类型只需改枚举 + label/suffix。
  static QVector<FileTypeOption> fileTypeOptions();

  /// 获取完整路径（父目录 + 文件名 + 后缀）
  QString fullPath() const;

  /// 验证输入是否合法，不合法时返回 false 并设置 error 描述
  bool validate(QString &error) const;

private:
  QString m_parentDir;
  FileType m_fileType = Folder;
  QString m_fileName;
};