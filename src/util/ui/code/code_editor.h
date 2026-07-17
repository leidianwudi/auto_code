/**
 * @file code_editor.h
 * @brief 代码编辑器控件（重构后）
 *
 * 基于 QPlainTextEdit 的增强编辑器，
 * 通过组合模式集成以下专职模块：
 * - BracketMatcher: 括号匹配算法
 * - CodeValidator: 语法验证逻辑
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
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QTextBlock>
#include <QTimer>
#include <QVector>

#include "bracket_matcher.h"
#include "code_validator.h"
#include "symbol_navigator.h"

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
 * ❌ 符号导航 → 委托给 SymbolNavigator
 */
class CodeEditor : public QPlainTextEdit {
  Q_OBJECT

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

  // ── 接口：行号显示 ──

  void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);
  int lineNumberAreaWidth() const;

  // ── 性能优化：文本缓存 ──

  /**
   * @brief 获取缓存的文本内容（避免重复 toPlainText() 调用）
   * @return 文本内容的常量引用
   */
  const QString &cachedText() const;

signals:
  void validationMessage(const QString &message, int errorCount = 0);
  void requestGoToLine(const QString &filePath, int line);
  void aboutToNavigate(const QString &targetFilePath, int targetLine);
  void requestFindReferences(const QString &filePath, int line, const QString &context);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  bool viewportEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
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

  // ── 补全相关 ──
  void initCompleter(ValidationMode mode);
  void showCompleter();

  // ── 验证相关（委托给 CodeValidator）──
  void validateWithValidator(IValidator *validator);

  // ── 导航快捷方法（委托给各模块）──
  void goToDefinition(const QString &name);
  QString identifierAtCursor(int pos, int *startPos = nullptr, int *endPos = nullptr) const;
  const AcSymbolEntry *findSymbolDefinition(const QString &name) const;
  QVector<QPair<int, QString>> findSymbolReferences(const QString &name) const;
  int findSymbolLineByName(const QString &name) const;
  void setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols);

  // ── 括号导航（使用 BracketMatcher 模块）──
  void jumpToMatchingBracket();
  void selectBetweenBrackets();

  // ── 符号高亮（用于查找引用）──
  void highlightSymbolReferences(const QString &name);

  // ── 错误处理（委托给 CodeValidator）──
  void applyErrorUnderline(int start, int length, const QString &tooltip,
                           QList<QTextEdit::ExtraSelection> &selections);

  // ── 悬停提示（使用 SymbolNavigator）──
  void showSymbolHover(int pos, const QPoint &gpos);

  // ── 成员变量（精简后）──

  ValidationMode m_validationMode = NoValidation;

  // 专职模块（组合模式）
  BracketMatcher m_bracketMatcher;    ///< 括号匹配器
  CodeValidator m_codeValidator;      ///< 代码验证器
  SymbolNavigator m_symbolNavigator;  ///< 符号导航器

  // 符号表（用于补全和导航）
  QHash<QString, AcSymbolEntry> m_symbolTable;  ///< 当前文件的符号表

  // UI 组件
  QWidget *m_lineNumberArea = nullptr;       ///< 行号显示区域
  QCompleter *m_completer = nullptr;         ///< 代码补全器
  QPointer<AuiErrorToolTip> m_errorTooltip;  ///< 错误提示弹窗

  // 性能优化：文本缓存
  mutable QString m_cachedText;     ///< 缓存的文本内容
  mutable int m_cacheVersion = -1;  ///< 文档版本号（用于失效检测）

  // 错误标记（从 CodeValidator 同步）
  QList<QTextEdit::ExtraSelection> m_errorSelections;
  QList<QTextEdit::ExtraSelection> m_referenceSelections;  ///< 引用高亮标记
  QSet<int> m_errorLines;

  // 悬停提示相关
  QTimer *m_hoverTimer = nullptr;       ///< 悬停防抖定时器
  QTimer *m_validationTimer = nullptr;  ///< 验证防抖定时器
  QString m_currentHoverSymbol;         ///< 当前悬停的符号名

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

private:
  CodeEditor *m_codeEditor;
};