/**
 * @file symbol_rename.h
 * @brief 语义级符号引用收集（AC / TPL）—「查找所有引用」与「重命名」共用
 *
 * 重命名与"查找所有引用"都需要精确的引用位置：改名/高亮错一处就误导。
 * 本模块基于语法树做语义解析：
 * - 作用域推断：局部变量/参数/for 变量精确到声明作用域，消除同名遮蔽误报
 * - 类型推断（AC 成员）：obj.method / obj.prop 通过接收者类型（类型注解 /
 *   new Class / 静态类名 / this）解析到具体类的成员，区分不同类的同名成员
 * - 跨文件：全局符号按 import { name as alias } from "file" 追踪引用文件
 *
 * 纯函数，可在后台线程调用（QtConcurrent::run），不访问任何 UI。
 */

#pragma once

#include <QHash>
#include <QString>
#include <QVector>

/// @brief 语义引用位置（查找所有引用 / 重命名共用）
struct RenameRef {
  QString filePath;      ///< 完整文件路径
  int line = 0;          ///< 1-based 行号
  int column = 0;        ///< 0-based 起始列
  int length = 0;        ///< 匹配长度
  bool isDeclaration = false;  ///< 是否为符号声明位置
};

/// @brief 收集目标符号的所有语义引用位置
/// @param rootDir          工作区根目录
/// @param triggerFilePath  触发文件（.ac / .tpl）
/// @param triggerLine      触发行（1-based）
/// @param triggerColumn    触发列（0-based）
/// @param name             目标符号名（光标下的标识符）
/// @return 引用位置列表（含声明位置），纯函数、线程安全
QVector<RenameRef> collectSymbolReferences(const QString &rootDir,
                                           const QString &triggerFilePath, int triggerLine,
                                           int triggerColumn, const QString &name);

/// @brief 带实时缓冲内容的引用收集（与 collectSymbolReferences 相同，但优先读缓冲）
/// @param liveContents 文件路径 → 实时内存内容（已打开编辑器 + 未打开但有缓冲修改的文件）。
///                     收集时对这些文件用缓冲内容而非磁盘快照，保证重命名未保存期间
///                     对新名的引用收集一致。调用方在主线程构建后按值传入（后台线程安全）。
QVector<RenameRef> collectSymbolReferencesLive(
    const QString &rootDir, const QString &triggerFilePath, int triggerLine, int triggerColumn,
    const QString &name, const QHash<QString, QString> &liveContents);
