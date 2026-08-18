/**
 * @file setting_store.cpp
 * @brief 运行时设置存储实现
 */

#include "setting_store.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>

#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_style.h"

// ──────────────────────────────────────────────────────────────
//  颜色 key 常量（与 AuiStyle / LightColor 读取保持一致）
// ──────────────────────────────────────────────────────────────

namespace {

// 界面颜色
inline const char *kUI_Bg = "ui.background";
inline const char *kUI_TitleBar = "ui.titleBarBackground";
inline const char *kUI_Text = "ui.textColor";
inline const char *kUI_Hover = "ui.hoverBackground";
inline const char *kUI_InactiveTab = "ui.inactiveTabColor";
inline const char *kUI_Border = "ui.borderColor";
inline const char *kUI_BorderDark = "ui.borderDarkColor";
inline const char *kUI_Panel = "ui.panelBackground";
inline const char *kUI_ListAlt = "ui.listAlternateBackground";
inline const char *kUI_ListHover = "ui.listHoverBackground";
inline const char *kUI_ListSel = "ui.listSelectionBackground";
inline const char *kUI_TabUnselBg = "ui.tabUnselectedBackground";
inline const char *kUI_TabHoverBg = "ui.tabHoverBackground";
inline const char *kUI_ErrorText = "ui.errorTextColor";
inline const char *kUI_ErrorTipBg = "ui.errorToolTipBackground";
inline const char *kUI_Modified = "ui.modifiedColor";
inline const char *kUI_Compile = "ui.compileButtonColor";
inline const char *kUI_SecondaryText = "ui.secondaryTextColor";
inline const char *kUI_MutedText = "ui.mutedTextColor";
inline const char *kUI_SuccessText = "ui.successTextColor";

// 编辑器颜色
inline const char *kEd_LineNumBg = "editor.lineNumberBackground";
inline const char *kEd_LineNumText = "editor.lineNumberText";
inline const char *kEd_CurrentLine = "editor.currentLineBackground";
inline const char *kEd_Bg = "editor.background";
inline const char *kEd_Text = "editor.text";
inline const char *kEd_BracketMatch = "editor.bracketMatch";
inline const char *kEd_BracketParen = "editor.bracketParen";
inline const char *kEd_BracketSquare = "editor.bracketSquare";
inline const char *kEd_BracketBrace = "editor.bracketBrace";
inline const char *kEd_BracketMismatch = "editor.bracketMismatch";
inline const char *kEd_ErrorUnderline = "editor.errorUnderline";
inline const char *kEd_ErrorLine = "editor.errorLineBackground";
inline const char *kEd_Warning = "editor.warning";
inline const char *kEd_IndentGuide = "editor.indentGuide";
inline const char *kEd_IndentActive = "editor.indentGuideActive";
inline const char *kEd_FindMatch = "editor.findMatchBackground";
inline const char *kEd_FindCurrent = "editor.findCurrentMatchBackground";

// 代码高亮颜色
inline const char *kHL_Keyword = "hl.keyword";
inline const char *kHL_Comment = "hl.comment";
inline const char *kHL_CommentBlock = "hl.commentBlock";
inline const char *kHL_String = "hl.string";
inline const char *kHL_StringTemplate = "hl.stringTemplate";
inline const char *kHL_Number = "hl.number";
inline const char *kHL_Boolean = "hl.boolean";
inline const char *kHL_Null = "hl.null";
inline const char *kHL_Special = "hl.special";
inline const char *kHL_Builtin = "hl.builtin";
inline const char *kHL_Call = "hl.call";
inline const char *kHL_Variable = "hl.variable";
inline const char *kHL_Constant = "hl.constant";
inline const char *kHL_Operator = "hl.operator";
inline const char *kHL_Punctuation = "hl.punctuation";
inline const char *kHL_Type = "hl.type";
inline const char *kHL_Decorator = "hl.decorator";
inline const char *kHL_ClassName = "hl.classname";
inline const char *kHL_FuncDecl = "hl.funcdecl";
inline const char *kHL_Import = "hl.import";

// ──────────────────────────────────────────────────────────────
//  快捷键 key 常量
// ──────────────────────────────────────────────────────────────

inline const char *kSC_Open = "sc.openFile";
inline const char *kSC_OpenFolder = "sc.openFolder";
inline const char *kSC_Split = "sc.splitEditor";
inline const char *kSC_Close = "sc.closeTab";
inline const char *kSC_Save = "sc.save";
inline const char *kSC_SaveAll = "sc.saveAll";
inline const char *kSC_Find = "sc.find";
inline const char *kSC_DebugStart = "sc.debugStart";
inline const char *kSC_DebugStepOver = "sc.debugStepOver";
inline const char *kSC_DebugStepInto = "sc.debugStepInto";
inline const char *kSC_DebugStepOut = "sc.debugStepOut";
inline const char *kSC_Settings = "sc.settings";

// ──────────────────────────────────────────────────────────────
//  字体 key 常量
// ──────────────────────────────────────────────────────────────

inline const char *kFontUI = "font.ui";      ///< 窗口字体
inline const char *kFontTree = "font.tree";  ///< 目录树字体
inline const char *kFontCode = "font.code";  ///< 代码字体

/// 字体大小允许范围（磅值）
constexpr int kFontSizeMin = 6;
constexpr int kFontSizeMax = 40;

}  // namespace

SettingStore::SettingStore() : QObject(nullptr) {
  // ── 界面颜色 ──
  registerColor(QString::fromLatin1(kUI_Bg), QStringLiteral("窗口背景"), QStringLiteral("界面"),
                QColor(0xf3, 0xf3, 0xf3), QColor(0x1e, 0x1e, 0x1e));
  registerColor(QString::fromLatin1(kUI_TitleBar), QStringLiteral("标题栏背景"),
                QStringLiteral("界面"), QColor(0xdd, 0xdd, 0xdd), QColor(0x3c, 0x3c, 0x3c));
  registerColor(QString::fromLatin1(kUI_Text), QStringLiteral("文字"), QStringLiteral("界面"),
                QColor(0x33, 0x33, 0x33), QColor(0xd4, 0xd4, 0xd4));
  registerColor(QString::fromLatin1(kUI_Hover), QStringLiteral("悬停背景"), QStringLiteral("界面"),
                QColor(0xe4, 0xe4, 0xe4), QColor(0x2a, 0x2d, 0x2e));
  registerColor(QString::fromLatin1(kUI_InactiveTab), QStringLiteral("未选中标签文字"),
                QStringLiteral("界面"), QColor(0x88, 0x88, 0x88), QColor(0x88, 0x88, 0x88));
  registerColor(QString::fromLatin1(kUI_Border), QStringLiteral("边框"), QStringLiteral("界面"),
                QColor(0xd4, 0xd4, 0xd4), QColor(0x4a, 0x4a, 0x4a));
  registerColor(QString::fromLatin1(kUI_BorderDark), QStringLiteral("深边框"),
                QStringLiteral("界面"), QColor(0xb0, 0xb0, 0xb0), QColor(0x5a, 0x5a, 0x5a));
  registerColor(QString::fromLatin1(kUI_Panel), QStringLiteral("面板背景"), QStringLiteral("界面"),
                QColor(Qt::white), QColor(0x25, 0x25, 0x26));
  registerColor(QString::fromLatin1(kUI_ListAlt), QStringLiteral("列表交替行背景"),
                QStringLiteral("界面"), QColor(0xf5, 0xf5, 0xf5), QColor(0x25, 0x25, 0x26));
  registerColor(QString::fromLatin1(kUI_ListHover), QStringLiteral("列表悬停背景"),
                QStringLiteral("界面"), QColor(0xe8, 0xf0, 0xff), QColor(0x2a, 0x2d, 0x2e));
  registerColor(QString::fromLatin1(kUI_ListSel), QStringLiteral("列表选中背景"),
                QStringLiteral("界面"), QColor(0xc7, 0xe0, 0xff), QColor(0x26, 0x4f, 0x78));
  registerColor(QString::fromLatin1(kUI_TabUnselBg), QStringLiteral("标签未选中背景"),
                QStringLiteral("界面"), QColor(0xe8, 0xe8, 0xe8), QColor(0x2d, 0x2d, 0x2d));
  registerColor(QString::fromLatin1(kUI_TabHoverBg), QStringLiteral("标签悬停背景"),
                QStringLiteral("界面"), QColor(0xdc, 0xdc, 0xdc), QColor(0x3c, 0x3c, 0x3c));
  registerColor(QString::fromLatin1(kUI_ErrorText), QStringLiteral("错误文字"),
                QStringLiteral("界面"), QColor(0xf4, 0x47, 0x47), QColor(0xff, 0x6b, 0x6b));
  registerColor(QString::fromLatin1(kUI_ErrorTipBg), QStringLiteral("错误提示背景"),
                QStringLiteral("界面"), QColor(0xff, 0xff, 0xcc), QColor(0x4a, 0x3a, 0x1e));
  registerColor(QString::fromLatin1(kUI_SecondaryText), QStringLiteral("次要文字"),
                QStringLiteral("界面"), QColor(0x55, 0x55, 0x55), QColor(0xb0, 0xb0, 0xb0));
  registerColor(QString::fromLatin1(kUI_MutedText), QStringLiteral("弱化文字"),
                QStringLiteral("界面"), QColor(0x99, 0x99, 0x99), QColor(0x88, 0x88, 0x88));
  registerColor(QString::fromLatin1(kUI_SuccessText), QStringLiteral("成功文字"),
                QStringLiteral("界面"), QColor(0x2e, 0x7d, 0x32), QColor(0x4c, 0xaf, 0x50));
  registerColor(QString::fromLatin1(kUI_Modified), QStringLiteral("文件修改标记"),
                QStringLiteral("界面"), QColor(0xcc, 0x33, 0x33), QColor(0xff, 0x6b, 0x6b));
  registerColor(QString::fromLatin1(kUI_Compile), QStringLiteral("编译按钮"),
                QStringLiteral("界面"), QColor(0x44, 0x99, 0x00), QColor(0x66, 0xcc, 0x00));

  // ── 编辑器颜色 ──
  registerColor(QString::fromLatin1(kEd_Bg), QStringLiteral("编辑器背景"), QStringLiteral("编辑器"),
                QColor(Qt::white), QColor(0x1e, 0x1e, 0x1e));
  registerColor(QString::fromLatin1(kEd_Text), QStringLiteral("编辑器文字"),
                QStringLiteral("编辑器"), QColor(Qt::black), QColor(0xd4, 0xd4, 0xd4));
  registerColor(QString::fromLatin1(kEd_LineNumBg), QStringLiteral("行号背景"),
                QStringLiteral("编辑器"), QColor(Qt::lightGray).lighter(110),
                QColor(0x1e, 0x1e, 0x1e));
  registerColor(QString::fromLatin1(kEd_LineNumText), QStringLiteral("行号文字"),
                QStringLiteral("编辑器"), QColor(0x99, 0x99, 0x99), QColor(0x85, 0x85, 0x85));
  registerColor(QString::fromLatin1(kEd_CurrentLine), QStringLiteral("当前行高亮"),
                QStringLiteral("编辑器"), QColor(0xf2, 0xf2, 0xf2), QColor(0x2a, 0x2a, 0x2a));
  registerColor(QString::fromLatin1(kEd_BracketMatch), QStringLiteral("括号匹配"),
                QStringLiteral("编辑器"), QColor(0xe8, 0xf0, 0xfe), QColor(0x3c, 0x3c, 0x3c));
  registerColor(QString::fromLatin1(kEd_BracketParen), QStringLiteral("圆括号匹配"),
                QStringLiteral("编辑器"), QColor(255, 127, 80), QColor(0xff, 0x8c, 0x50));
  registerColor(QString::fromLatin1(kEd_BracketSquare), QStringLiteral("方括号匹配"),
                QStringLiteral("编辑器"), QColor(60, 179, 113), QColor(0x3c, 0xb3, 0x71));
  registerColor(QString::fromLatin1(kEd_BracketBrace), QStringLiteral("花括号匹配"),
                QStringLiteral("编辑器"), QColor(65, 105, 225), QColor(0x41, 0x69, 0xe1));
  registerColor(QString::fromLatin1(kEd_BracketMismatch), QStringLiteral("括号不匹配"),
                QStringLiteral("编辑器"), QColor(255, 0, 0), QColor(0xff, 0x50, 0x50));
  registerColor(QString::fromLatin1(kEd_ErrorUnderline), QStringLiteral("错误波浪线"),
                QStringLiteral("编辑器"), QColor(0xd3, 0x2f, 0x2f), QColor(0xf4, 0x87, 0x71));
  registerColor(QString::fromLatin1(kEd_ErrorLine), QStringLiteral("错误行背景"),
                QStringLiteral("编辑器"), QColor(0xf2, 0xde, 0xde), QColor(0x44, 0x2b, 0x2b));
  registerColor(QString::fromLatin1(kEd_Warning), QStringLiteral("代码警告"),
                QStringLiteral("编辑器"), QColor(0xf5, 0x7c, 0x00), QColor(0xcc, 0xa7, 0x00));
  registerColor(QString::fromLatin1(kEd_IndentGuide), QStringLiteral("缩进参考线"),
                QStringLiteral("编辑器"), QColor(0xdd, 0xdd, 0xdd), QColor(0x33, 0x33, 0x33));
  registerColor(QString::fromLatin1(kEd_IndentActive), QStringLiteral("缩进参考线(当前)"),
                QStringLiteral("编辑器"), QColor(0xbb, 0xbb, 0xbb), QColor(0x55, 0x55, 0x55));
  registerColor(QString::fromLatin1(kEd_FindMatch), QStringLiteral("查找匹配"),
                QStringLiteral("编辑器"), QColor(0xff, 0xc6, 0x6d), QColor(0x9a, 0x6d, 0x2a));
  registerColor(QString::fromLatin1(kEd_FindCurrent), QStringLiteral("查找当前匹配"),
                QStringLiteral("编辑器"), QColor(0xff, 0x99, 0x33), QColor(0xb0, 0x6a, 0x1e));

  // ── 代码高亮颜色（浅色 / 深色，对齐 Trae ICube 主题） ──
  // 关键字（流程控制）
  registerColor(QString::fromLatin1(kHL_Keyword), QStringLiteral("关键字"),
                QStringLiteral("代码高亮"), QColor(0x5F, 0x36, 0xB2), QColor(0xB3, 0x8C, 0xFF));
  // 单行注释（灰色斜体）
  registerColor(QString::fromLatin1(kHL_Comment), QStringLiteral("单行注释"),
                QStringLiteral("代码高亮"), QColor(0x83, 0x93, 0xA3), QColor(0x73, 0x77, 0x80));
  // 块注释（灰色斜体，略深以作区分）
  registerColor(QString::fromLatin1(kHL_CommentBlock), QStringLiteral("块注释"),
                QStringLiteral("代码高亮"), QColor(0x7C, 0x8C, 0x9C), QColor(0x6F, 0x73, 0x7A));
  // 普通字符串
  registerColor(QString::fromLatin1(kHL_String), QStringLiteral("字符串"),
                QStringLiteral("代码高亮"), QColor(0x4D, 0xA6, 0x21), QColor(0x82, 0xD9, 0x9F));
  // 模板字符串
  registerColor(QString::fromLatin1(kHL_StringTemplate), QStringLiteral("模板字符串"),
                QStringLiteral("代码高亮"), QColor(0x4D, 0xA6, 0x21), QColor(0x82, 0xD9, 0x9F));
  // 数字
  registerColor(QString::fromLatin1(kHL_Number), QStringLiteral("数字"), QStringLiteral("代码高亮"),
                QColor(0xE5, 0x45, 0x95), QColor(0xF4, 0x8C, 0xCA));
  // 布尔值 true/false（constant.language）
  registerColor(QString::fromLatin1(kHL_Boolean), QStringLiteral("布尔值"),
                QStringLiteral("代码高亮"), QColor(0x17, 0x5C, 0xE6), QColor(0x80, 0xBB, 0xFF));
  // 空值 null/undefined（constant.language）
  registerColor(QString::fromLatin1(kHL_Null), QStringLiteral("空值"), QStringLiteral("代码高亮"),
                QColor(0x17, 0x5C, 0xE6), QColor(0x80, 0xBB, 0xFF));
  // 内置变量 this/self/super
  registerColor(QString::fromLatin1(kHL_Special), QStringLiteral("内置变量(this/self)"),
                QStringLiteral("代码高亮"), QColor(0xC9, 0x91, 0x00), QColor(0xDE, 0xD4, 0x7E));
  // 内置函数（function 色）
  registerColor(QString::fromLatin1(kHL_Builtin), QStringLiteral("内置函数"),
                QStringLiteral("代码高亮"), QColor(0x40, 0x78, 0xF2), QColor(0xF2, 0x9D, 0x79));
  // 函数调用 / 函数声明名（function 色）
  registerColor(QString::fromLatin1(kHL_Call), QStringLiteral("函数调用"),
                QStringLiteral("代码高亮"), QColor(0x40, 0x78, 0xF2), QColor(0xF2, 0x9D, 0x79));
  registerColor(QString::fromLatin1(kHL_FuncDecl), QStringLiteral("函数声明名"),
                QStringLiteral("代码高亮"), QColor(0x40, 0x78, 0xF2), QColor(0xF2, 0x9D, 0x79));
  // 变量（形参 / 对象属性）
  registerColor(QString::fromLatin1(kHL_Variable), QStringLiteral("变量"),
                QStringLiteral("代码高亮"), QColor(0xC9, 0x91, 0x00), QColor(0xDE, 0xD4, 0x7E));
  // 常量
  registerColor(QString::fromLatin1(kHL_Constant), QStringLiteral("常量"),
                QStringLiteral("代码高亮"), QColor(0x98, 0x68, 0x01), QColor(0x80, 0xBB, 0xFF));
  // 运算符
  registerColor(QString::fromLatin1(kHL_Operator), QStringLiteral("运算符"),
                QStringLiteral("代码高亮"), QColor(0x00, 0x00, 0x00), QColor(0xD5, 0xD8, 0xE0));
  // 标点符号
  registerColor(QString::fromLatin1(kHL_Punctuation), QStringLiteral("标点符号"),
                QStringLiteral("代码高亮"), QColor(0x17, 0x18, 0x1A), QColor(0xD5, 0xD8, 0xE0));
  // 类型标注 / 类名
  registerColor(QString::fromLatin1(kHL_Type), QStringLiteral("类型标注"),
                QStringLiteral("代码高亮"), QColor(0xB1, 0x5E, 0xF2), QColor(0x81, 0xCF, 0xE0));
  registerColor(QString::fromLatin1(kHL_ClassName), QStringLiteral("类名"),
                QStringLiteral("代码高亮"), QColor(0xB1, 0x5E, 0xF2), QColor(0x81, 0xCF, 0xE0));
  // 装饰器
  registerColor(QString::fromLatin1(kHL_Decorator), QStringLiteral("装饰器"),
                QStringLiteral("代码高亮"), QColor(0x5F, 0x36, 0xB2), QColor(0xB3, 0x8C, 0xFF));
  // 导入模块名
  registerColor(QString::fromLatin1(kHL_Import), QStringLiteral("导入模块名"),
                QStringLiteral("代码高亮"), QColor(0x5F, 0x36, 0xB2), QColor(0xB3, 0x8C, 0xFF));

  // ── 快捷键 ──
  registerShortcut(QString::fromLatin1(kSC_Open), QStringLiteral("打开文件"),
                   QString::fromUtf8(CodeConstants::UiText::kFile), QStringLiteral("Ctrl+O"));
  registerShortcut(QString::fromLatin1(kSC_OpenFolder), QStringLiteral("打开文件夹"),
                   QString::fromUtf8(CodeConstants::UiText::kFile), QStringLiteral(""));
  registerShortcut(QString::fromLatin1(kSC_Save), QStringLiteral("保存"),
                   QString::fromUtf8(CodeConstants::UiText::kFile), QStringLiteral("Ctrl+S"));
  registerShortcut(QString::fromLatin1(kSC_SaveAll), QStringLiteral("保存全部"),
                   QString::fromUtf8(CodeConstants::UiText::kFile), QStringLiteral("Ctrl+Shift+S"));
  registerShortcut(QString::fromLatin1(kSC_Split), QStringLiteral("向右拆分编辑器"),
                   QStringLiteral("视图"), QStringLiteral("Ctrl+\\"));
  registerShortcut(QString::fromLatin1(kSC_Close), QStringLiteral("关闭标签页"),
                   QStringLiteral("视图"), QStringLiteral("Ctrl+W"));
  registerShortcut(QString::fromLatin1(kSC_Find), QStringLiteral("查找/替换"),
                   QStringLiteral("编辑"), QStringLiteral("Ctrl+F"));
  registerShortcut(QString::fromLatin1(kSC_DebugStart), QStringLiteral("启动/继续调试"),
                   QStringLiteral("调试"), QStringLiteral("F5"));
  registerShortcut(QString::fromLatin1(kSC_DebugStepOver), QStringLiteral("单步跳过"),
                   QStringLiteral("调试"), QStringLiteral("F10"));
  registerShortcut(QString::fromLatin1(kSC_DebugStepInto), QStringLiteral("单步进入"),
                   QStringLiteral("调试"), QStringLiteral("F11"));
  registerShortcut(QString::fromLatin1(kSC_DebugStepOut), QStringLiteral("单步跳出"),
                   QStringLiteral("调试"), QStringLiteral("Shift+F11"));
  registerShortcut(QString::fromLatin1(kSC_Settings), QStringLiteral("打开设置"),
                   QStringLiteral("视图"), QStringLiteral(""));

  // ── 字体大小 / 字体风格 ──
  registerFont(QString::fromLatin1(kFontUI), QStringLiteral("窗口字体"), 10);
  registerFont(QString::fromLatin1(kFontTree), QStringLiteral("目录树字体"), 10);
  // 默认字体族常量统一在 CodeConstants 定义（AuiStyle 运行时回退也用它），
  // 避免 SettingStore 反向依赖 AuiStyle 造成循环依赖
  registerFont(QString::fromLatin1(kFontCode), QStringLiteral("代码字体"),
               CodeConstants::Editor::kDefaultFontSize,
               QString::fromLatin1(CodeConstants::Editor::kDefaultEditorFontFamily));

  // 字体修改防抖：拖动字号 SpinBox 会连续触发，合并为一次应用，避免全窗口刷新卡顿
  m_fontTimer = new QTimer(this);
  m_fontTimer->setSingleShot(true);
  m_fontTimer->setInterval(80);
  connect(m_fontTimer, &QTimer::timeout, this, &SettingStore::onFontsDebounced);
}

// init — 加载配置文件
void SettingStore::init() {
  if (m_initialized) return;
  m_initialized = true;
  loadFromFile();
}

void SettingStore::registerColor(const QString &key, const QString &label, const QString &category,
                                 const QColor &light, const QColor &dark) {
  m_themeLight[key] = light;
  m_themeDark[key] = dark;
  m_labels[key] = label;
  m_categories[key] = category;
  if (!m_colorOrder.contains(key)) m_colorOrder.append(key);
}

void SettingStore::registerShortcut(const QString &key, const QString &label,
                                    const QString &category, const QString &defaultSeq) {
  m_shortcuts[key] = QKeySequence(defaultSeq);
  m_shortcutLabels[key] = label;
  m_shortcutCategories[key] = category;
  if (!m_shortcutOrder.contains(key)) m_shortcutOrder.append(key);
}

void SettingStore::registerFont(const QString &key, const QString &label, int defaultSize,
                                const QString &defaultFamily) {
  m_fontDefaults[key] = defaultSize;
  m_fontLabels[key] = label;
  m_fontFamilyDefaults[key] = defaultFamily;
  if (!m_fontOrder.contains(key)) m_fontOrder.append(key);
}

QString SettingStore::storePath() const {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty())
    dir = QDir::homePath() + QString::fromUtf8(CodeConstants::Paths::kAppDataDirName);
  QDir().mkpath(dir);
  return dir + QStringLiteral("/settings.json");
}

void SettingStore::loadFromFile() {
  QFile f(storePath());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QJsonParseError perr;
  QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) return;
  QJsonObject root = doc.object();

  // 主题
  QString theme = root.value(QStringLiteral("theme")).toString();
  if (theme == QStringLiteral("dark"))
    m_theme = ThemeDark;
  else if (theme == QStringLiteral("custom"))
    m_theme = ThemeCustom;
  else
    m_theme = ThemeLight;

  // 自定义颜色
  QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
  for (auto it = colors.begin(); it != colors.end(); ++it) {
    const QString name = it.value().toString();
    QColor c(name);
    if (c.isValid()) m_custom[it.key()] = c;
  }

  // 自定义快捷键
  QJsonObject scs = root.value(QStringLiteral("shortcuts")).toObject();
  for (auto it = scs.begin(); it != scs.end(); ++it) {
    QKeySequence seq(it.value().toString());
    if (!seq.isEmpty() && m_shortcuts.contains(it.key())) m_shortcuts[it.key()] = seq;
  }

  // 自定义字体大小
  QJsonObject fonts = root.value(QStringLiteral("fonts")).toObject();
  for (auto it = fonts.begin(); it != fonts.end(); ++it) {
    if (m_fontDefaults.contains(it.key())) m_fontCustom[it.key()] = it.value().toInt();
  }

  // 自定义字体风格（字体族）
  QJsonObject fontFamilies = root.value(QStringLiteral("fontFamilies")).toObject();
  for (auto it = fontFamilies.begin(); it != fontFamilies.end(); ++it) {
    if (m_fontOrder.contains(it.key())) m_fontFamilyCustom[it.key()] = it.value().toString();
  }
}

QColor SettingStore::color(const QString &key) const {
  if (m_custom.contains(key)) return m_custom.value(key);
  const QHash<QString, QColor> &base = (m_theme == ThemeDark) ? m_themeDark : m_themeLight;
  auto it = base.find(key);
  return (it != base.end()) ? it.value() : QColor();
}

void SettingStore::setColor(const QString &key, const QColor &c) {
  if (!m_themeLight.contains(key)) return;
  if (!c.isValid()) {
    m_custom.remove(key);
  } else {
    m_custom[key] = c;
  }
  if (m_theme != ThemeCustom) m_theme = ThemeCustom;
  emit colorsChanged();
}

bool SettingStore::hasCustomColor(const QString &key) const { return m_custom.contains(key); }

QStringList SettingStore::colorKeys() const { return m_colorOrder; }

QString SettingStore::colorLabel(const QString &key) const { return m_labels.value(key, key); }

QString SettingStore::colorCategory(const QString &key) const {
  return m_categories.value(key, QStringLiteral("其他"));
}

void SettingStore::resetColor(const QString &key) {
  if (!m_custom.contains(key)) return;
  m_custom.remove(key);
  emit colorsChanged();
}

void SettingStore::resetAllColors() {
  if (m_custom.isEmpty()) return;
  m_custom.clear();
  emit colorsChanged();
}

void SettingStore::setTheme(Theme t) {
  if (m_theme == t) return;
  m_theme = t;
  if (t != ThemeCustom) m_custom.clear();  // 切回内置主题时丢弃自定义覆盖
  emit themeChanged();
}

QString SettingStore::themeName(Theme t) const {
  switch (t) {
    case ThemeLight:
      return QString::fromUtf8(CodeConstants::UiText::kLight);
    case ThemeDark:
      return QStringLiteral("深色");
    case ThemeCustom:
      return QStringLiteral("自定义");
  }
  return QString::fromUtf8(CodeConstants::UiText::kLight);
}

// ── 快捷键 ──

QKeySequence SettingStore::shortcut(const QString &key) const { return m_shortcuts.value(key); }

void SettingStore::setShortcut(const QString &key, const QKeySequence &seq) {
  if (!m_shortcutOrder.contains(key)) return;
  m_shortcuts[key] = seq;
  emit shortcutsChanged();
}

QStringList SettingStore::shortcutKeys() const { return m_shortcutOrder; }

QString SettingStore::shortcutLabel(const QString &key) const {
  return m_shortcutLabels.value(key, key);
}

QString SettingStore::shortcutCategory(const QString &key) const {
  return m_shortcutCategories.value(key, QStringLiteral("其他"));
}

// ── 字体大小 ──

int SettingStore::fontSize(const QString &key) const {
  const int v = m_fontCustom.value(key, m_fontDefaults.value(key, 10));
  return qBound(kFontSizeMin, v, kFontSizeMax);
}

void SettingStore::setFontSize(const QString &key, int size) {
  if (!m_fontOrder.contains(key)) return;
  size = qBound(kFontSizeMin, size, kFontSizeMax);
  const int def = m_fontDefaults.value(key, 10);
  if (size == def) {
    m_fontCustom.remove(key);  // 与默认一致时视为恢复默认
  } else {
    m_fontCustom[key] = size;
  }
  if (key == QString::fromLatin1(kFontUI)) {
    // 窗口字体：全窗口级刷新代价大，防抖合并后统一应用（见 onFontsDebounced）
    m_fontTimer->start();
  } else {
    // 目录树/代码等组件字体：立即广播，由对应组件即时重设，无需全窗口刷新
    emit fontsChanged();
  }
}

QStringList SettingStore::fontKeys() const { return m_fontOrder; }

QString SettingStore::fontLabel(const QString &key) const { return m_fontLabels.value(key, key); }

bool SettingStore::hasCustomFont(const QString &key) const { return m_fontCustom.contains(key); }

void SettingStore::resetFontSize(const QString &key) {
  if (!m_fontCustom.remove(key) || !m_fontOrder.contains(key)) return;
  if (key == QString::fromLatin1(kFontUI)) {
    m_fontTimer->start();
  } else {
    emit fontsChanged();
  }
}

// ── 字体风格（字体族） ──

QString SettingStore::fontFamily(const QString &key) const {
  // 自定义优先；未自定义返回默认（可能为空串=跟随系统/默认）
  return m_fontFamilyCustom.value(key, m_fontFamilyDefaults.value(key, QString()));
}

bool SettingStore::hasCustomFontFamily(const QString &key) const {
  return m_fontFamilyCustom.contains(key);
}

void SettingStore::setFontFamily(const QString &key, const QString &family) {
  if (!m_fontOrder.contains(key)) return;
  // 语义约定：空串 = 恢复默认（跟随内置默认族）；非空 = 用户显式自定义。
  // 注意：即使 family 恰好等于默认族（如 Consolas），也按「用户显式选择」保存，
  // 以便界面能区分「默认」与「显式选了默认族」这两种状态。
  if (family.isEmpty()) {
    m_fontFamilyCustom.remove(key);
  } else {
    m_fontFamilyCustom[key] = family;
  }
  if (key == QString::fromLatin1(kFontUI)) {
    // 窗口字体：全窗口级刷新代价大，防抖合并后统一应用
    m_fontTimer->start();
  } else {
    emit fontsChanged();
  }
}

void SettingStore::resetFontFamily(const QString &key) {
  if (!m_fontFamilyCustom.remove(key) || !m_fontOrder.contains(key)) return;
  if (key == QString::fromLatin1(kFontUI)) {
    m_fontTimer->start();
  } else {
    emit fontsChanged();
  }
}

void SettingStore::resetAllFonts() {
  if (m_fontCustom.isEmpty() && m_fontFamilyCustom.isEmpty()) return;
  m_fontCustom.clear();
  m_fontFamilyCustom.clear();
  // 可能包含窗口字体，统一走防抖（其内也会广播 fontsChanged 供组件重置）
  m_fontTimer->start();
}

void SettingStore::onFontsDebounced() {
  // 一次性应用窗口字体到 qApp 与所有已创建窗口
  applyWindowFont();
  // 窗口级消费者（主窗口 / 设置界面）刷新
  emit windowFontChanged();
  // 组件级消费者（目录树 / 代码编辑器）按各自设置重置
  emit fontsChanged();
}

void SettingStore::applyWindowFont() {
  // 统一走框架入口：设置 qApp 字体并即时刷新所有已创建窗口，
  // 避免遗漏工具栏等已存在控件的字体更新（见 AuiStyle::applyAppFont）
  AuiStyle::applyAppFont();
}

// ── 持久化 ──

void SettingStore::save() {
  QJsonObject root;
  switch (m_theme) {
    case ThemeLight:
      root[QStringLiteral("theme")] = QStringLiteral("light");
      break;
    case ThemeDark:
      root[QStringLiteral("theme")] = QStringLiteral("dark");
      break;
    case ThemeCustom:
      root[QStringLiteral("theme")] = QStringLiteral("custom");
      break;
  }

  QJsonObject colors;
  for (auto it = m_custom.begin(); it != m_custom.end(); ++it) {
    colors[it.key()] = it.value().name();
  }
  root[QStringLiteral("colors")] = colors;

  QJsonObject scs;
  for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
    scs[it.key()] = it.value().toString();
  }
  root[QStringLiteral("shortcuts")] = scs;

  QJsonObject fonts;
  for (auto it = m_fontCustom.begin(); it != m_fontCustom.end(); ++it) {
    fonts[it.key()] = it.value();
  }
  root[QStringLiteral("fonts")] = fonts;

  QJsonObject fontFamilies;
  for (auto it = m_fontFamilyCustom.begin(); it != m_fontFamilyCustom.end(); ++it) {
    fontFamilies[it.key()] = it.value();
  }
  root[QStringLiteral("fontFamilies")] = fontFamilies;

  QFile f(storePath());
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void SettingStore::apply() {
  save();
  emit themeChanged();
  emit colorsChanged();
  emit shortcutsChanged();
  emit fontsChanged();
}

// ── 全局风格 ──

QPalette SettingStore::buildPalette() const {
  QPalette p;
  const QColor bg = color(QStringLiteral("ui.background"));
  const QColor text = color(QStringLiteral("ui.textColor"));
  const QColor panel = color(QStringLiteral("ui.panelBackground"));
  const QColor border = color(QStringLiteral("ui.borderColor"));
  const QColor sel = color(QStringLiteral("ui.listSelectionBackground"));
  const QColor alt = color(QStringLiteral("ui.listAlternateBackground"));
  const QColor tipBg = color(QStringLiteral("ui.errorToolTipBackground"));
  const QColor editorBg = color(QStringLiteral("editor.background"));
  const QColor editorText = color(QStringLiteral("editor.text"));

  // 基础色（正常态）
  p.setColor(QPalette::Window, bg);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, editorBg);
  p.setColor(QPalette::AlternateBase, alt);
  p.setColor(QPalette::Text, editorText);
  p.setColor(QPalette::PlaceholderText, text);
  p.setColor(QPalette::Button, panel);
  p.setColor(QPalette::ButtonText, text);
  // Fusion 用 Light/Midlight 提亮选中 tab / 凸起控件的棱边。深色主题下若不显式设置，
  // 会回落到系统计算的偏亮颜色，导致选中 tab 背景被提亮成浅色、与文字对比度不足。
  // 这里把 Light/Midlight 设为面板的轻微提亮，保证选中 tab 背景始终贴近面板深色。
  p.setColor(QPalette::Light, panel.lighter(108));
  p.setColor(QPalette::Midlight, panel.lighter(104));
  p.setColor(QPalette::Dark, border.darker(120));
  p.setColor(QPalette::Mid, border);
  p.setColor(QPalette::BrightText, QColor(255, 0, 0));
  p.setColor(QPalette::Highlight, sel);
  p.setColor(QPalette::HighlightedText, text);
  p.setColor(QPalette::ToolTipBase, tipBg);
  p.setColor(QPalette::ToolTipText, text);
  // 深色主题下 QToolTip 改为白底 + 深色文字，避免与深色提示背景颜色过近、影响可读性
  if (theme() == ThemeDark) {
    p.setColor(QPalette::ToolTipBase, QColor(Qt::white));
    p.setColor(QPalette::ToolTipText, QColor(0x33, 0x33, 0x33));
  }
  p.setColor(QPalette::Link, sel);

  // 禁用态统一降为边框色，避免原生控件回落到系统主题色
  p.setColor(QPalette::Disabled, QPalette::WindowText, border);
  p.setColor(QPalette::Disabled, QPalette::Text, border);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, border);
  p.setColor(QPalette::Disabled, QPalette::Highlight, border);
  p.setColor(QPalette::Disabled, QPalette::HighlightedText, bg);

  return p;
}

void SettingStore::applyGlobalStyle() {
  if (!qApp) return;
  // 全局风格只应用一次：Fusion 基础风格 + 程序化复选框/单选框指示器代理。
  // Fusion 使用 QPalette 渲染，彻底脱离 Windows 系统主题色；
  // 指示器代理用 QPainter 自绘打勾/圆点，绕开 QSS image 对图片支持不稳定的问题。
  if (!m_globalStyleApplied) {
    m_globalStyleApplied = true;
    if (QStyle *s = AuiStyle::createAppStyle()) {
      s->setParent(qApp);
      qApp->setStyle(s);
    }
  }
  qApp->setPalette(buildPalette());

  // 全局样式表应用到 QApplication 级，保证弹出菜单/右键菜单/下拉弹出等
  // 顶层弹出控件能可靠继承主题色（仅窗口级样式表无法覆盖这些弹窗）
  qApp->setStyleSheet(AuiStyle::mainStyleSheet());
}
