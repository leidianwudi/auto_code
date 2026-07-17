/**
 * @file code_constants.h
 * @brief 全局常量定义（消除魔法数字）
 *
 * 统一管理编辑器、验证器、UI 等模块的配置参数，
 * 避免硬编码，便于维护和调整。
 */

#pragma once

namespace CodeConstants {

// ──────────────────────────────────────────────────────────────
//  编辑器配置
// ──────────────────────────────────────────────────────────────

namespace Editor {
constexpr int kDefaultFontSize = 10;  ///< 默认字体大小（pt）
constexpr int kTabWidthSpaces = 4;    ///< Tab 键对应的空格数
constexpr int kIndentSpaces = 2;      ///< 自动缩进的空格数
}  // namespace Editor

// ──────────────────────────────────────────────────────────────
//  性能优化参数
// ──────────────────────────────────────────────────────────────

namespace Performance {
constexpr int kValidationDebounceMs = 500;  ///< 验证防抖时间（毫秒）
constexpr int kHoverDebounceMs = 500;       ///< 悬停提示防抖时间（毫秒）
constexpr int kMaxLogLines = 5000;          ///< 日志最大行数（防止内存溢出）
}  // namespace Performance

// ──────────────────────────────────────────────────────────────
//  UI 显示参数
// ──────────────────────────────────────────────────────────────

namespace UI {
constexpr int kStatusDisplayDuration = 3000;  ///< 状态栏消息显示时长（毫秒）
constexpr int kTooltipDelayMs = 300;          ///< 工具提示延迟时间（毫秒）
constexpr int kAnimationDurationMs = 200;     ///< 动画过渡时长（毫秒）
}  // namespace UI

// ──────────────────────────────────────────────────────────────
//  文件路径常量
// ──────────────────────────────────────────────────────────────

namespace Paths {
constexpr const char *kProjectSourceDir = PROJECT_SOURCE_DIR;
}  // namespace Paths

// ──────────────────────────────────────────────────────────────
//  快捷键定义
// ──────────────────────────────────────────────────────────────

namespace Shortcuts {
constexpr const char *kGoToDefinition = "F12";
constexpr const char *kJumpToBracket = "Ctrl+M";
constexpr const char *kSelectBetweenBrackets = "Ctrl+Shift+M";
constexpr const char *kFormatCode = "Ctrl+I";
}  // namespace Shortcuts

}  // namespace CodeConstants