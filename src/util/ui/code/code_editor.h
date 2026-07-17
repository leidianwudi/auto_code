/**
 * @file code_editor.h
 * @brief 代码编辑器控件
 *
 * 基于 QPlainTextEdit 的增强编辑器，提供：
 * - 行号显示
 * - 当前行高亮
 * - 括号匹配高亮
 * - 语法验证（JSON / 模板标签 / AC 脚本）
 */

#pragma once

#include <QPlainTextEdit>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>

class QPaintEvent;
class QResizeEvent;
class QWidget;
class QCompleter;
class AuiErrorToolTip;  ///< 自定义可选中/复制的错误提示弹窗
class IValidator;       ///< 统一验证器接口
struct AcSymbolEntry;   ///< AC 符号表条目

/**
 * @class CodeEditor
 * @brief 增强的代码编辑器控件
 *
 * 基于 QPlainTextEdit，提供以下增强功能：
 * - 行号显示（左侧行号区域，错误行号显示红色）
 * - 当前行高亮（淡黄色背景）
 * - 括号匹配高亮（青色背景，支持 () [] {}）
 * - 实时语法验证（JSON / 模板标签 / AC 脚本，带 500ms 防抖）
 * - 错误波浪下划线（红色波浪线 + Tooltip）
 */
class CodeEditor : public QPlainTextEdit {
  Q_OBJECT

public:
  /// 验证模式枚举
  enum ValidationMode {
    NoValidation,        ///< 不做验证
    JsonValidation,      ///< JSON 语法验证（使用 QJsonDocument）
    TemplateValidation,  ///< 模板标签 + 括号匹配验证
    AcValidation         ///< AC 脚本语法验证（使用 AcExecutor）
  };

  explicit CodeEditor(QWidget *parent = nullptr);

  /// 设置验证模式，切换后立即执行一次验证
  void setValidationMode(ValidationMode mode);
  /// 手动触发一次验证（同步执行，立即发出 validationMessage 信号）
  void validate();

  /// @brief 自动排版代码（根据当前验证模式选择格式器）
  void formatCode();

  /// 绘制行号区域（由 LineNumberArea 委托调用）
  void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);
  /// 计算行号区域宽度（根据行数动态计算）
  int lineNumberAreaWidth() const;

signals:
  /**
   * @brief 验证结果信号
   * @param message 错误信息，无错误时为空字符串
   * @param errorCount 错误数量，无错误时为 0
   */
  void validationMessage(const QString &message, int errorCount = 0);

  /**
   * @brief 跳转到文件指定行
   * @param filePath 文件路径
   * @param line 行号（1-based）
   */
  void requestGoToLine(const QString &filePath, int line);

  /**
   * @brief 即将执行导航跳转（用于记录导航历史）
   * @param targetFilePath 目标文件路径
   * @param targetLine 目标行号
   */
  void aboutToNavigate(const QString &targetFilePath, int targetLine);

  /**
   * @brief 查找引用结果
   * @param filePath 文件路径
   * @param line 行号
   * @param context 上下文行文本
   */
  void requestFindReferences(const QString &filePath, int line, const QString &context);

protected:
  void resizeEvent(QResizeEvent *event) override;
  /// 自定义绘制：在标准绘制后，额外绘制超粗红色波浪线（覆盖默认细波浪线）
  void paintEvent(QPaintEvent *event) override;
  /// 处理视口事件（ToolTip 显示错误提示 + 悬停符号提示）
  bool viewportEvent(QEvent *event) override;
  /// 按键事件：Enter 自动缩进 + F12 跳转
  void keyPressEvent(QKeyEvent *event) override;
  /// 右键菜单：增加「格式化代码」「转到定义」「查找引用」等
  void contextMenuEvent(QContextMenuEvent *event) override;
  /// 鼠标移动事件：悬停符号提示
  void mouseMoveEvent(QMouseEvent *event) override;
  /// 鼠标释放事件：Ctrl+点击跳转
  void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
  /// 行数变化时重新计算行号区域宽度
  void updateLineNumberAreaWidth(int newBlockCount);
  /// 刷新当前行高亮
  void highlightCurrentLine();
  /// 更新行号区域（滚动或重绘）
  void updateLineNumberArea(const QRect &rect, int dy);
  /// 延迟触发验证（防抖入口）
  void scheduleValidation();
  /// 执行验证
  void performValidation();
  /// 用户选择补全项后的回调
  void insertCompletion(const QString &completion);

private:
  /// 刷新 ExtraSelection 列表（行高亮 + 括号匹配 + 错误标记 + 符号高亮）
  void refreshExtraSelections();
  /// 使用统一的 IValidator 接口执行验证
  void validateWithValidator(IValidator *validator);
  /// 将错误区间应用到 ExtraSelection 并标记红色波浪下划线
  void applyErrorUnderline(int from, int length, const QString &tooltip,
                           QList<QTextEdit::ExtraSelection> &selections);
  /// 显示自定义错误提示弹窗（可选中/复制）
  void showErrorTooltip(const QPoint &pos, const QString &text);
  /// 隐藏自定义错误提示弹窗
  void hideErrorTooltip();

  /// @brief 计算新行需要的缩进空格数
  /// @param currentLineText 当前行光标前的文本
  /// @return 缩进空格数
  int calculateNewLineIndent(const QString &linePrefix) const;

  /// @brief 根据验证模式初始化（或销毁）代码补全器
  void initCompleter(ValidationMode mode);

  /// @brief 弹出代码补全建议列表
  void showCompleter();

  /// @brief 获取光标下的标识符文本
  QString identifierAtCursor(int pos, int *startPos = nullptr, int *endPos = nullptr) const;

  /// @brief 查找符号定义
  const AcSymbolEntry *findSymbolDefinition(const QString &name) const;

  /// @brief 跳转到符号定义位置
  void goToDefinition(const QString &name);

  /// @brief 通过源码搜索定位符号行号（当 AST 行号不可用时）
  int findSymbolLineByName(const QString &name) const;

  /// @brief 查找符号的所有引用位置
  QVector<QPair<int, QString>> findSymbolReferences(const QString &name) const;

  /// @brief 显示悬停符号提示
  void showSymbolHover(int pos, const QPoint &globalPos);

  /// @brief 高亮当前符号的所有引用
  void highlightSymbolReferences(const QString &name);

  /// @brief 设置符号表（由外部调用，传入符号表数据）
  void setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols);

  /// @brief 获取当前文件的 AC 执行器（用于符号表访问）
  class AcExecutor *acExecutor() const;

  /// @brief 判断位置是否在字符串或注释中（P1: 字符串/注释过滤）
  bool isInStringOrComment(int pos) const;

  /// @brief 查找匹配的括号位置（P1: 性能优化 + P3: 颜色区分）
  int findMatchingBracket(int pos, QChar bracket, QChar &matchBracket) const;

  /// @brief 跳转到匹配括号位置（P2: 快捷键功能）
  void jumpToMatchingBracket();

  /// @brief 选中两个匹配括号之间的所有内容（P2: 快捷键功能）
  void selectBetweenBrackets();

  ValidationMode m_validationMode = NoValidation;
  QTimer m_validationTimer;                                ///< 验证防抖定时器（500ms）
  QTimer m_hoverTimer;                                     ///< 悬停提示防抖定时器（500ms）
  QList<QTextEdit::ExtraSelection> m_errorSelections;      ///< 持久化的错误标记
  QList<QTextEdit::ExtraSelection> m_referenceSelections;  ///< 符号引用高亮
  QSet<int> m_errorLines;                                  ///< 有错误的行号集合
  QWidget *m_lineNumberArea;                               ///< 行号显示区域
  QPointer<AuiErrorToolTip> m_errorTooltip;                ///< 自定义错误提示弹窗
  QPointer<AuiErrorToolTip> m_hoverTooltip;                ///< 悬停符号提示弹窗
  QCompleter *m_completer = nullptr;                       ///< 代码补全器
  QString m_currentHoverSymbol;                            ///< 当前悬停的符号名
  QHash<QString, AcSymbolEntry> m_symbolTable;             ///< 符号表数据

  /// 错误位置列表（用于 paintEvent 自定义绘制，避免 cursor 失效）
  struct ErrorRange {
    int start;        ///< 起始位置
    int length;       ///< 长度
    QString tooltip;  ///< 错误提示文本
    ErrorRange(int s, int l, const QString &t) : start(s), length(l), tooltip(t) {}
  };
  QVector<ErrorRange> m_errorRanges;  ///< 所有错误的位置范围
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