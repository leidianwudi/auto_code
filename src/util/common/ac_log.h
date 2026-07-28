/**
 * @file ac_log.h
 * @brief 统一日志宏 — 支持编译期开关控制日志输出
 *
 * 使用方式：
 * @code
 *   AC_LOG_INFO() << "执行完成";
 *   AC_LOG_WARN() << "文件不存在:" << path;
 * @endcode
 *
 * 通过 AC_DEBUG 宏控制：
 *   - 定义 AC_DEBUG → 日志输出到 qDebug
 *   - 未定义 AC_DEBUG → 日志被完全编译消除，零开销
 */

#pragma once

#include <QDebug>

#ifdef AC_DEBUG

/// 信息级日志（调试期输出，发布期消除）
#define AC_LOG_INFO() qDebug() << "[INFO]"
/// 警告级日志（调试期输出，发布期消除）
#define AC_LOG_WARN() qDebug() << "[WARN]"

#else

/// 信息级日志（发布期消除）
#define AC_LOG_INFO() if (false) qDebug()
/// 警告级日志（发布期消除）
#define AC_LOG_WARN() if (false) qDebug()

#endif

/// 错误级日志（始终输出，不受 AC_DEBUG 控制）
#define AC_LOG_ERROR() qWarning() << "[ERROR]"
