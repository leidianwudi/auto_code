/**
 * @file code_log.h
 * @brief 日志输出控件
 *
 * 基于 QPlainTextEdit 的只读日志面板，提供：
 * - 左侧行号显示（独立绘制，不进入文本，复制时不携带）
 * - 错误消息红色显示
 * - 行号自动扩展宽度
 */

#pragma once

#include <QPlainTextEdit>

class QPaintEvent;
class QResizeEvent;
class QWidget;

/**
 * @class CodeLog
 * @brief 带行号的只读日志输出控件
 *
 * 使用方式和 CodeEditor 一致，行号区域独立绘制在左侧
 * 调用 append() 追加日志，clearLog() 清空
 */
class CodeLog : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit CodeLog(QWidget *parent = nullptr);

  /// 追加一行日志，isError=true 时显示红色
  void append(const QString &text, bool isError = false);

  /// 清空所有日志并重置行号计数器
  void clearLog();

  /// 获取当前日志行数
  int logLineCount() const { return m_lineNumber; }

private:
  // ════════════════════════════════════════════════════════════
  //  行号区域
  // ════════════════════════════════════════════════════════════

  /// 行号显示区域（内部辅助类）
  class LineNumberArea : public QWidget {
  public:
    explicit LineNumberArea(CodeLog *log) : QWidget(log), m_codeLog(log) {}
    QSize sizeHint() const override { return QSize(m_codeLog->lineNumberAreaWidth(), 0); }

  protected:
    void paintEvent(QPaintEvent *event) override {
      QRect area = rect();
      m_codeLog->lineNumberAreaPaintEvent(event, area);
    }

  private:
    CodeLog *m_codeLog;
  };

  LineNumberArea *m_lineNumberArea = nullptr;  ///< 行号区域控件
  int m_lineNumber = 0;                        ///< 日志行号计数器

  int lineNumberAreaWidth() const;
  void updateLineNumberAreaWidth(int newBlockCount = 0);
  void lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area);

  // QPlainTextEdit 事件重写
  void resizeEvent(QResizeEvent *event) override;
};