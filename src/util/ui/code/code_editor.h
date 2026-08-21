/**
 * @file code_editor.h
 * @brief 代码编辑器控件（重构后）
 *
 * 基于 QPlainTextEdit 的增强编辑器，
 * 通过组合模式集成以下专职模块：
 * - BracketMatcher: 括号匹配算法
 * - CodeValidator: 语法验证逻辑
 * - IndentGuide: 缩进参考线绘制
 * - SymbolNavigator: 符号导航功能
 *
 * 核心职责：
 * - UI 显示（行号、高亮、布局管理）
 * - 用户交互（键盘、鼠标事件处理）
 * - 组件协调（调用各模块完成具体功能）
 */

#pragma once

#include <QCompleter>
#include <QHash>
#include <QMap>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTimer>
#include <QVector>

#include "bracket_matcher.h"
#include "indent_guide.h"
#include "src/engine/schema_validator.h"
#include "src/engine/script/ac_debugger.h"
#include "src/engine/validation_result.h"
#include "symbol_navigator.h"

class CodeFindBar;
class FixedLineHeightLayout;  ///< 统一行高布局（修复中文 fallback 导致的行高变化）
class QPaintEvent;
class QResizeEvent;
class QWidget;
class AuiErrorToolTip;  ///< 自定义可选中/复制的错误提示弹窗

/**
 * @class CodeEditor
 * @brief 增强的代码编辑器控件（重构后）
 *
 * 职责分离后的轻量级编辑器类：
 * ✅ UI 显示和布局（行号区域、高亮）
 * ✅ 事件分发和处理
 * ✅ 文本缓存和性能优化
 * ❌ 括号匹配 → 委托给 BracketMatcher
 * ❌ 语法验证 → 委托给 CodeValidator
 * ❌ 缩进参考线 → 委托给 IndentGuide
 * ❌ 符号导航 → 委托给 SymbolNavigator
 */
class CodeEditor : public QPlainTextEdit {
  Q_OBJECT
  friend class CodeFindBar;  ///< 查找栏需要访问 m_findSelections

public:
  /// 验证模式枚举（兼容旧接口）
  enum ValidationMode {
    NoValidation,        ///< 不做验证
    JsonValidation,      ///< JSON 语法验证
    TemplateValidation,  ///< 模板标签 + 括号匹配验证
    AcValidation         ///< AC 脚本语法验证
  };

  explicit CodeEditor(QWidget *parent = nullptr);
  ~CodeEditor();

  // ── 接口：验证相关 ──

  void setValidationMode(ValidationMode mode);
  void validate();
  void formatCode();

  // ── 接口：查找/替换 ──

  /// 显示查找/替换栏（Ctrl+F）
  void showFindBar();
  /// 隐藏查找/替换栏
  void hideFindBar();
  /// 查找/替换栏是否可见
  bool isFindBarVisible() const;
  /// 获取查找/替换栏控件（供外部管理显示/隐藏）
  CodeFindBar *findBar() const;
  /// 更新查找栏布局和视口边距
  void updateFindBarLayout();

  // ── 接口：行号显示 ──

  void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);
  int lineNumberAreaWidth() const;

  // ── 接口：代码折叠（类似 VSCode，标记位于代码行右缘）──

  /// 指定 block 是否为可折叠区间的起始行
  bool isFoldStart(int block) const;
  /// 切换指定起始 block 的折叠状态
  void toggleFold(int block);
  /// 折叠标记宽度（px）：绘制在行号区右侧（紧贴代码）
  static constexpr int kFoldMarkerWidth = 16;
  /// 断点圆点 gutter 宽度（px）：绘制在行号区左侧，预留空间避免与行号数字重叠
  static constexpr int kBreakpointGutterWidth = 18;
  /// 折叠标记是否常显（折叠态）或需悬停（未折叠）- 由调用方据 m_collapsedStarts 判断
  bool isCollapsed(int block) const { return m_collapsedStarts.contains(block); }
  /// 鼠标是否位于行号区（灰色区）：true 时显示全部可折叠图标
  bool foldShowAll() const { return m_foldShowAll; }
  /// 设置鼠标是否在行号区（进入/离开灰色区时更新）
  void setFoldAreaActive(bool active);
  /// 当前悬停的可折叠起始行（-1=无），供绘制的当前行高亮
  int foldHoverBlock() const { return m_foldHoverBlock; }
  /// 设置折叠标记悬停行（行号区鼠标移动时调用，-1 清除）
  void setFoldHoverBlock(int block);
  /// 返回折叠标记矩形（行号区坐标，x 紧贴代码左侧），供绘制与命中检测
  QRect foldMarkerRect(int block) const;
  /// 重建折叠区间并重算 visible（文本/折叠状态变化时调用）
  void rebuildFold();
  /// 获取当前处于折叠态的起始 block 号集合（供会话持久化，重启后还原）
  QSet<int> collapsedFoldBlocks() const { return m_collapsedStarts; }
  /// 恢复折叠态（供会话还原）：覆盖折叠块集合并按当前文本重算折叠区间后应用
  void restoreFoldState(const QSet<int> &blocks);

  // ── 性能优化：文本缓存 ──

  /**
   * @brief 获取缓存的文本内容（避免重复 toPlainText() 调用）
   * @return 文本内容的常量引用
   */
  const QString &cachedText() const;

  // ── 接口：符号引用查找（供外部控制器跨文件调用）──

  /**
   * @brief 查找当前文件中指定符号的所有引用位置
   * @param name 符号名称
   * @return 引用列表（行号 + 上下文文本）
   */
  QVector<QPair<int, QString>> findSymbolReferences(const QString &name) const;

  // ── 接口：断点调试 ──

  /// 切换指定行号（0-based blockNumber）的断点状态
  void toggleBreakpoint(int blockNumber);
  /// 根据行号区 y 坐标返回所在行（1-based），无命中返回 0
  int lineAtY(int y) const;
  /// 根据行号区 y 坐标切换断点（供 LineNumberArea 调用）
  void toggleBreakpointAtY(int y);
  /// 指定行号（1-based）是否有断点
  bool hasBreakpoint(int line) const;
  /// 指定行号（1-based）的断点是否生效
  bool isBreakpointEnabled(int line) const;
  /// 设置指定行号（1-based）断点是否生效
  void setBreakpointEnabled(int line, bool enabled);
  /// 获取当前断点集合（行号 → 是否生效，行号 1-based）
  QMap<int, bool> breakpoints() const;
  /// 覆盖设置断点集合（行号 → 是否生效，行号 1-based）
  void setBreakpoints(const QMap<int, bool> &lines);
  /// 清空所有断点
  void clearBreakpoints();
  /// 设置当前调试暂停行（1-based），<=0 表示清除高亮
  void setDebugLine(int line);
  /// 清除当前调试行高亮
  void clearDebugLine();
  /// 设置当前调试变量的快照（鼠标悬停时显示类型/值）
  void setDebugVariables(const QList<AcDebugVar> &vars);
  /// 清除调试变量快照
  void clearDebugVariables();
  /// 注册当前文件对应的语法高亮器（主题切换时刷新）
  void setSyntaxHighlighter(QSyntaxHighlighter *h);
  /// 当前文件对应的语法高亮器
  QSyntaxHighlighter *syntaxHighlighter() const { return m_highlighter; }
  /// 从 SettingStore 重新读取颜色并刷新高亮（主题切换时调用）
  void reloadColors();

  /// 从设置读取代码字体大小并应用（构造与字体设置变化时调用）
  void applyFontFromSetting();

  /// 根据字体测量并设置统一行高（消除中英文混排时的行高抖动）
  void updateFixedLineHeight(const QFont &font);

  /// 把所有块重写为统一行高（整篇重载 setPlainText 后恢复文本布局层行高用）
  void applyFixedBlockLineHeight();

  /// 把点击点纵向修正到文本行中心，规避统一行高造成的「点击行尾吸附」
  QPointF snapClickToText(const QPointF &pos) const;

signals:
  void validationMessage(const QString &message, int errorCount = 0);
  /// 结构化验证结果（文件路径 + 问题列表），供底部“问题”面板展示与双击跳转
  void validationIssues(const QString &filePath, const QVector<ValidationResult> &issues);
  void requestGoToLine(const QString &filePath, int line);
  void aboutToNavigate(const QString &targetFilePath, int targetLine);
  void requestFindReferences(const QString &filePath, int line, const QString &context);
  void requestFindReferencesAll(const QString &symbolName);  ///< 跨文件查找引用
  void requestWorkspaceSymbols();                            ///< 工作区符号搜索 (Ctrl+T)
  void breakpointsChanged();                                 ///< 断点集合发生变化
  void requestDebugStart();                                  ///< F5：启动调试/继续
  void requestDebugStepOver();                               ///< F10：单步执行
  void requestDebugStepInto();                               ///< F11：单步进入
  void requestDebugStepOut();                                ///< Shift+F11：单步跳出

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  bool viewportEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
  void updateLineNumberAreaWidth(int newBlockCount);
  void highlightCurrentLine();
  void updateLineNumberArea(const QRect &rect, int dy);
  void scheduleValidation();
  void performValidation();
  void insertCompletion(const QString &completion);

private:
  // ── UI 相关 ──
  void refreshExtraSelections();
  void showErrorTooltip(const QPoint &pos, const QString &text);
  void hideErrorTooltip();
  int calculateNewLineIndent(const QString &linePrefix) const;

public:
  /// 当前文件是否为可调试的 .ac 脚本（objectName 即文件路径）
  bool isDebuggableFile() const;

private:
  // ── 补全相关 ──
  void initCompleter(ValidationMode mode);
  void showCompleter();

  // ── 验证相关（委托给 CodeValidator）──
  void validateWithValidator(IValidator *validator);
  void applyValidationResults(const QVector<ValidationResult> &results);
  void runSchemaValidation(const QJsonObject &doc, QVector<ValidationResult> &results);

  // ── 导航快捷方法（委托给各模块）──
  void goToDefinition(const QString &name);
  void goToTypeDefinition(const QString &name);  ///< 转到类型定义（new ClassName() → class）
  void showSignatureHelp();                      ///< 函数签名提示（输入 ( 时触发）
  QString identifierAtCursor(int pos, int *startPos = nullptr, int *endPos = nullptr) const;
  const AcSymbolEntry *findSymbolDefinition(const QString &name) const;
  const AcSymbolEntry *findPropertyDefinition(const QString &propName) const;
  int findSymbolLineByName(const QString &name) const;
  void setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols);

  // ── 括号导航（使用 BracketMatcher 模块）──
  void jumpToMatchingBracket();
  void selectBetweenBrackets();

  // ── 代码折叠内部辅助 ──
  /// 根据 m_collapsedStarts 设置各块的 visible，并触发布局重算
  void applyFoldVisibility();
  /// 由文本/打开文件时调用：按缩进计算可折叠区间并应用折叠态
  void computeFoldRanges();

  // ── 符号高亮（用于查找引用）──
  void highlightSymbolReferences(const QString &name);

  // ── 悬停提示（使用 SymbolNavigator）──
  void showSymbolHover(int pos, const QPoint &gpos);

  // ── JSON 属性悬停提示与 Ctrl+点击跳转（基于 $schema）──
  void showJsonPropertyHover(const QString &jsonPath, const QPoint &gpos);
  int findSchemaPropertyLine(const QString &className, const QString &propName) const;

  // ── 统一的导航目标判定（悬停手形光标与 Ctrl+点击跳转共用同一判定）──
  //   保证各模式下"手形 ⇔ 可跳转"，避免两套逻辑表现不一致
  bool navigationTargetAt(int pos) const;

  // ── 成员变量（精简后）──

  ValidationMode m_validationMode = NoValidation;

  // 专职模块（组合模式）
  BracketMatcher m_bracketMatcher;    ///< 括号匹配器
  IndentGuide m_indentGuide;          ///< 缩进参考线
  SymbolNavigator m_symbolNavigator;  ///< 符号导航器

  // 符号表（用于补全和导航）
  QHash<QString, AcSymbolEntry> m_symbolTable;  ///< 当前文件的符号表

  // JSON Schema 校验与提示（由 $schema 字段指定）
  SchemaValidator m_schema;     ///< 已加载的 schema 校验器
  QString m_schemaPath;         ///< 当前生效的 schema 文件路径（用于缓存）
  bool m_schemaLoaded = false;  ///< schema 是否已成功加载

  // UI 组件
  QWidget *m_lineNumberArea = nullptr;       ///< 行号显示区域
  QCompleter *m_completer = nullptr;         ///< 代码补全器
  QPointer<AuiErrorToolTip> m_errorTooltip;  ///< 错误提示弹窗

  // 性能优化：文本缓存
  mutable QString m_cachedText;     ///< 缓存的文本内容
  mutable int m_cacheVersion = -1;  ///< 文档版本号（用于失效检测）

  // 错误标记（错误波浪线统一由 paintEvent 依据 m_errorRanges 绘制，不再使用 ExtraSelection）
  QList<QTextEdit::ExtraSelection> m_referenceSelections;  ///< 引用高亮标记
  QSet<int> m_errorLines;

  // 悬停提示相关
  QTimer *m_hoverTimer = nullptr;       ///< 悬停防抖定时器
  QTimer *m_validationTimer = nullptr;  ///< 验证防抖定时器
  QString m_currentHoverSymbol;         ///< 当前悬停的符号名

  // 查找/替换栏
  CodeFindBar *m_findBar = nullptr;                   ///< 查找/替换栏控件
  QList<QTextEdit::ExtraSelection> m_findSelections;  ///< 查找匹配高亮（由 CodeFindBar 写入）

  // 断点调试
  QMap<int, bool> m_breakpoints;  ///< 断点集合（行号 → 是否生效，行号 1-based）
  int m_debugLine = -1;           ///< 当前调试暂停行（1-based），-1 表示无
  QList<AcDebugVar> m_debugVars;  ///< 当前调试变量快照（悬停显示用）

  // 语法高亮器（主题切换时刷新）
  QSyntaxHighlighter *m_highlighter = nullptr;

  // 统一行高布局（消除中英文混排时行高随内容抖动）
  FixedLineHeightLayout *m_fixedLineHeightLayout = nullptr;
  qreal m_fixedBlockLineHeight = 0;  ///< 文本布局层统一行高（px），整篇重载后重应用依据
  QTimer *m_lineHeightTimer = nullptr;  ///< 整篇重载后延后重应用块统一行高

  // 代码折叠（VSCode 风格）
  QHash<int, int> m_foldRanges;   ///< 可折叠起始 block → 结束 block（含），折叠区域内的块被折叠
  QSet<int> m_collapsedStarts;    ///< 当前处于折叠态的起始 block 号集合
  int m_foldHoverBlock = -1;      ///< 行号区悬停的折叠起始 block（-1=无），仅供高亮
  bool m_foldShowAll = false;     ///< 鼠标位于行号区（灰色区）：显示全部可折叠图标
  bool m_foldValid = false;       ///< m_foldRanges 是否已随文档重建
  QTimer *m_foldRebuildTimer = nullptr;  ///< 文档变化后延后重建折叠区间
  bool m_applyingFold = false;           ///< 折叠 visible 应用中的防重入标志

  struct ErrorRange {
    int start;
    int length;
    QString tooltip;
    ErrorRange(int s, int l, const QString &t) : start(s), length(l), tooltip(t) {}
  };
  QVector<ErrorRange> m_errorRanges;
};

/**
 * @class LineNumberArea
 * @brief 行号显示区域（内部辅助类）
 */
class LineNumberArea : public QWidget {
public:
  explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), m_codeEditor(editor) {}

  QSize sizeHint() const override { return QSize(m_codeEditor->lineNumberAreaWidth(), 0); }

protected:
  void paintEvent(QPaintEvent *event) override {
    QRect area = rect();
    m_codeEditor->lineNumberAreaPaintEvent(event, area);
  }

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  CodeEditor *m_codeEditor;
};