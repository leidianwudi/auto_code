/**
 * @file path_resolver.h
 * @brief 文件路径解析工具
 *
 * 统一管理 AC 脚本引擎中 builtin.d.ac、import 文件等资源的搜索路径构造，
 * 消除 ac_validator / ac_executor / main_dev_mgr 中的重复路径拼接代码。
 */

#pragma once

#include <QString>
#include <QStringList>

/// @brief 文件路径解析工具（静态工具类）
class PathResolver {
public:
  /// @brief 构造 AC 脚本资源搜索路径列表
  ///
  /// 搜索顺序：
  /// 1. 脚本所在目录
  /// 2. 脚本所在目录的上一级（../）
  /// 3. 应用可执行文件目录/file
  /// 4. 项目源码目录/file（PROJECT_SOURCE_DIR/file）
  ///
  /// @param scriptPath 当前脚本路径（空串时跳过脚本相关路径）
  /// @return 搜索路径列表（已去重）
  static QStringList fileSearchPaths(const QString &scriptPath = QString());

  /// @brief 在搜索路径中查找指定文件
  /// @param scriptPath 当前脚本路径
  /// @param fileName 目标文件名（如 "builtin.d.ac"）
  /// @return 找到的文件绝对路径，未找到返回空串
  static QString findFile(const QString &scriptPath, const QString &fileName);

  /// @brief 解析 import 相对路径为绝对路径
  /// @param importPath import 语句中的路径
  /// @param scriptPath 当前脚本路径（作为相对路径基准）
  /// @return 规范化后的绝对路径
  static QString resolveImportPath(const QString &importPath, const QString &scriptPath);

  /// @brief 解析 json 文件中 $schema 引用为 schema 文件绝对路径
  ///
  /// 统一解析规则（CodeEditor 校验 / 工作区诊断 / 可视化编辑共用，避免各自实现漂移）：
  ///   - 以 / 开头 → 项目源码目录 file/ 下（公共 schema，供所有 json 共享）
  ///   - 相对路径   → 基于 json 文件所在目录
  ///   - 其他（绝对路径）→ 原样使用
  ///
  /// @param jsonFilePath 引用方 json 文件路径（相对路径解析基准）
  /// @param schemaRef $schema 引用值
  /// @return 规范化后的 schema 文件绝对路径
  static QString resolveSchemaPath(const QString &jsonFilePath, const QString &schemaRef);
};
