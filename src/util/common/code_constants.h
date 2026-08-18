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
constexpr int kTabWidthSpaces = 2;    ///< Tab 键对应的空格数
constexpr int kIndentSpaces = 2;      ///< 自动缩进的空格数
///< 统一行高的额外间距（像素），叠加在自然行高上，避免行过于拥挤
constexpr int kLineHeightExtraSpacing = 2;
constexpr int kEditorTabHeight = 30;  ///< 代码编辑框 tab 头高度（px），可自行调整
}  // namespace Editor

// ──────────────────────────────────────────────────────────────
//  性能优化参数
// ──────────────────────────────────────────────────────────────

namespace Performance {
constexpr int kValidationDebounceMs =
    0;  ///< 验证防抖时间（毫秒）；0 = 输入后立即验证（验证耗时 <1ms，即时反馈）
constexpr int kHoverDebounceMs = 500;  ///< 悬停提示防抖时间（毫秒）
constexpr int kMaxLogLines = 5000;     ///< 日志最大行数（防止内存溢出）
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
constexpr const char *kAppDataDirName = "/.auto_code";              ///< 无 AppData 时的回退数据目录
constexpr const char *kFileDirName = "/file";                       ///< 项目源码下的 file 资源目录
constexpr const char *kTreeConfigFile = "/tree.config";             ///< 目录树勾选状态配置文件
constexpr const char *kBreakpointsStoreFile = "/breakpoints.json";  ///< 断点持久化文件
constexpr const char *kBreakpointsJsonKey = "breakpoints";          ///< 断点存储 JSON 键名
}  // namespace Paths

// ──────────────────────────────────────────────────────────────
//  MIME 类型（拖拽等内部协议）
// ──────────────────────────────────────────────────────────────

namespace Mime {
constexpr const char *kAutoCodeTab = "application/x-auto-code-tab";  ///< 编辑器标签拖拽协议
}  // namespace Mime

// ──────────────────────────────────────────────────────────────
//  通用 UI 文案（跨文件重复、语义一致）
// ──────────────────────────────────────────────────────────────

namespace UiText {
constexpr const char *kConfirm = "确定";                    ///< 确认按钮
constexpr const char *kCancel = "取消";                     ///< 取消按钮
constexpr const char *kDelete = "删除";                     ///< 删除按钮
constexpr const char *kConfig = "配置";                     ///< 配置列/按钮
constexpr const char *kRequired = "必填";                   ///< 必填标记
constexpr const char *kConfirmDelete = "确认删除";          ///< 删除确认对话框标题
constexpr const char *kNameCannotBeEmpty = "名称不能为空";  ///< 名称校验提示
constexpr const char *kFile = "文件";                       ///< 文件列标题
constexpr const char *kNew = "新建";                        ///< 新建按钮
constexpr const char *kColor = "颜色";                      ///< 颜色标签
constexpr const char *kLight = "浅色";                      ///< 浅色主题名
constexpr const char *kDatetimeFull = "年月日时分秒";       ///< 日期格式：年月日时分秒
constexpr const char *kYearMonth = "年月";                  ///< 日期格式：年月
constexpr const char *kDateRange = "日期范围";              ///< 查询类型：日期范围
constexpr const char *kEnableBreakpoint = "启用断点";       ///< 断点右键菜单
constexpr const char *kDisableBreakpoint = "禁用断点";      ///< 断点右键菜单
constexpr const char *kRemoveBreakpoint = "移除断点";       ///< 断点右键菜单
}  // namespace UiText

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