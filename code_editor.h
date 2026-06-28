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
 */
class CodeEditor : public QPlainTextEdit {
  Q_OBJECT

public:
  /// 验证模式
  enum ValidationMode {
    NoValidation,      ///< 不做验证
    JsonValidation,    ///< JSON 语法验证
    TemplateValidation ///< 模板标签 + 括号验证
  };

  explicit CodeEditor(QWidget *parent = nullptr);

  /// 设置验证模式
  void setValidationMode(ValidationMode mode);

  void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);
  int lineNumberAreaWidth() const;

signals:
  /**
   * @brief 验证结果信号
   * @param message 错误信息，无错误时为空
   */
  void validationMessage(const QString &message);

protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void updateLineNumberAreaWidth(int newBlockCount);
  void highlightCurrentLine();
  void updateLineNumberArea(const QRect &rect, int dy);

  /// 延迟触发验证（防抖）
  void scheduleValidation();

  /// 执行验证
  void performValidation();

private:
  /// 刷新 ExtraSelection 列表（行高亮 + 括号匹配 + 错误标记）
  void refreshExtraSelections();

  /// JSON 验证，返回错误信息列表
  QStringList validateJson();

  /// 模板验证，返回错误信息列表
  QStringList validateTemplate();

  /// 将错误区间应用到 ExtraSelection 并标记波浪下划线
  void applyErrorUnderline(int from, int length, const QString &tooltip,
                           QList<QTextEdit::ExtraSelection> &selections);

  ValidationMode m_validationMode = NoValidation;
  QTimer m_validationTimer;                           ///< 防抖定时器
  QList<QTextEdit::ExtraSelection> m_errorSelections; ///< 持久化的错误标记
  QSet<int> m_errorLines;                             ///< 有错误的行号集合
  QWidget *m_lineNumberArea;
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