/**
 * @file code_editor.h
 * @brief 代码编辑器控件
 *
 * 基于 QPlainTextEdit 的增强编辑器，提供：
 * - 行号显示
 * - 当前行高亮
 * - 括号匹配高亮
 * - 语法验证（JSON / 模板标签）
 */

#pragma once

#include <QPlainTextEdit>
#include <QSet>
#include <QString>
#include <QTimer>

class QPaintEvent;
class QResizeEvent;
class QWidget;

/**
 * @class CodeEditor
 * @brief 增强的代码编辑器控件
 *
 * 基于 QPlainTextEdit，提供以下增强功能：
 * - 行号显示（左侧行号区域，错误行号显示红色）
 * - 当前行高亮（淡黄色背景）
 * - 括号匹配高亮（青色背景，支持 () [] {}）
 * - 实时语法验证（JSON 或模板标签，带 500ms 防抖）
 * - 错误波浪下划线（红色波浪线 + Tooltip）
 */
class CodeEditor : public QPlainTextEdit {
  Q_OBJECT

public:
  /// 验证模式枚举
  enum ValidationMode {
    NoValidation,      ///< 不做验证
    JsonValidation,    ///< JSON 语法验证（使用 QJsonDocument）
    TemplateValidation ///< 模板标签 + 括号匹配验证
  };

  explicit CodeEditor(QWidget *parent = nullptr);

  /// 设置验证模式，切换后立即执行一次验证
  void setValidationMode(ValidationMode mode);

  /// 绘制行号区域（由 LineNumberArea 委托调用）
  void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);
  /// 计算行号区域宽度（根据行数动态计算）
  int lineNumberAreaWidth() const;

signals:
  /**
   * @brief 验证结果信号
   * @param message 错误信息，无错误时为空字符串
   */
  void validationMessage(const QString &message);

protected:
  void resizeEvent(QResizeEvent *event) override;

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

private:
  /// 刷新 ExtraSelection 列表（行高亮 + 括号匹配 + 错误标记）
  void refreshExtraSelections();
  /// JSON 验证，返回错误信息列表
  QStringList validateJson();
  /// 模板验证（4 步：括号匹配、${} 闭合、标签配对、方法检查），返回错误信息列表
  QStringList validateTemplate();
  /// 将错误区间应用到 ExtraSelection 并标记红色波浪下划线
  void applyErrorUnderline(int from, int length, const QString &tooltip,
                           QList<QTextEdit::ExtraSelection> &selections);

  ValidationMode m_validationMode = NoValidation;
  QTimer m_validationTimer;                           ///< 防抖定时器（500ms）
  QList<QTextEdit::ExtraSelection> m_errorSelections; ///< 持久化的错误标记
  QSet<int> m_errorLines;                             ///< 有错误的行号集合
  QWidget *m_lineNumberArea;                          ///< 行号显示区域
};

/**
 * @class LineNumberArea
 * @brief 行号显示区域（内部辅助类）
 */
class LineNumberArea : public QWidget {
public:
  explicit LineNumberArea(CodeEditor *editor)
      : QWidget(editor), m_codeEditor(editor) {}

  QSize sizeHint() const override {
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    QRect area = rect();
    m_codeEditor->lineNumberAreaPaintEvent(event, area);
  }

private:
  CodeEditor *m_codeEditor;
};