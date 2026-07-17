/**
 * @file code_validator.h
 * @brief 代码验证器（从 CodeEditor 中拆分）
 *
 * 负责语法验证、错误标记和验证结果展示，
 * 支持 JSON / 模板标签 / AC 脚本三种模式。
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QTextEdit>

class IValidator;
class QTimer;

/**
 * @class CodeValidator
 * @brief 代码验证器
 *
 * 独立的验证逻辑实现，支持：
 * - 多种验证模式（JSON/模板/AC脚本）
 * - 防抖机制（避免频繁验证）
 * - 错误位置追踪和高亮
 * - 统一的错误提示接口
 */
class CodeValidator : public QObject {
  Q_OBJECT
public:
  /// 验证模式枚举
  enum class Mode {
    None,      ///< 不做验证
    Json,      ///< JSON 语法验证
    Template,  ///< 模板标签 + 括号匹配验证
    AcScript   ///< AC 脚本语法验证
  };

  explicit CodeValidator(QObject *parent = nullptr);

  /// 设置验证模式
  void setMode(Mode mode);

  /// 获取当前模式
  Mode mode() const { return m_mode; }

  /// 手动触发一次验证
  void validate(const QString &text);

  /// 延迟触发验证（防抖）
  void scheduleValidation(const QString &text);

  /// 获取待验证的文本
  QString pendingText() const;

  /// 获取错误选择列表（用于 ExtraSelection）
  QList<QTextEdit::ExtraSelection> errorSelections() const { return m_errorSelections; }

  /// 获取有错误的行号集合
  QSet<int> errorLines() const { return m_errorLines; }

  /// 获取错误数量
  int errorCount() const { return m_errorCount; }

  /// 获取错误消息
  QString errorMessage() const { return m_errorMessage; }

  /// 清除所有错误标记
  void clearErrors();

  /// 是否有错误
  bool hasErrors() const;

private:
  // 具体验证逻辑（按模式分派）
  void validateJson(const QString &text);
  void validateTemplate(const QString &text);
  void validateAcScript(const QString &text);

signals:
  /**
   * @brief 验证完成信号
   * @param message 错误信息
   * @param count 错误数量
   */
  void validationFinished(const QString &message, int count);

  /**
   * @brief 请求验证信号（防抖定时器触发后发射）
   */
  void validationRequested();

private:
  /// 执行实际验证逻辑
  void performValidation(const QString &text);

  /// 使用指定的验证器执行验证
  void validateWithValidator(IValidator *validator, const QString &text);

  /// 将错误区间应用到 ExtraSelection
  void applyErrorUnderline(int from, int length, const QString &tooltip,
                           QList<QTextEdit::ExtraSelection> &selections);

  Mode m_mode = Mode::None;
  QTimer *m_debounceTimer;
  QString m_pendingText;

  QList<QTextEdit::ExtraSelection> m_errorSelections;
  QSet<int> m_errorLines;
  int m_errorCount = 0;
  QString m_errorMessage;

  struct ErrorRange {
    int start;
    int length;
    QString tooltip;
    ErrorRange(int s, int l, const QString &t) : start(s), length(l), tooltip(t) {}
  };
  QVector<ErrorRange> m_errorRanges;
};