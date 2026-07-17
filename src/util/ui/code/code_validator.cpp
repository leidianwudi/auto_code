/**
 * @file code_validator.cpp
 * @brief 代码验证器实现（专职模块）
 *
 * 负责不同模式（JSON/模板/AC脚本）的语法验证逻辑，
 * 包括错误检测、标记和结果展示。
 */

#include "code_validator.h"

#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include "src/util/common/code_constants.h"

CodeValidator::CodeValidator(QObject *parent) : QObject(parent) {}

void CodeValidator::setMode(Mode mode) { m_mode = mode; }

void CodeValidator::validate(const QString &text) {
  // 清除旧错误
  m_errorSelections.clear();
  m_errorLines.clear();

  if (text.trimmed().isEmpty()) return;

  // 根据模式执行不同的验证逻辑
  switch (m_mode) {
    case Mode::Json:
      validateJson(text);
      break;
    case Mode::Template:
      validateTemplate(text);
      break;
    case Mode::AcScript:
      validateAcScript(text);
      break;
    case Mode::None:
    default:
      break;
  }
}

void CodeValidator::scheduleValidation(const QString &text) {
  // 防抖机制：避免频繁验证
  if (!m_debounceTimer) {
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() { emit validationRequested(); });
  }

  m_pendingText = text;
  m_debounceTimer->start(CodeConstants::Performance::kValidationDebounceMs);
}

QString CodeValidator::pendingText() const { return m_pendingText; }

bool CodeValidator::hasErrors() const { return !m_errorSelections.isEmpty(); }

void CodeValidator::clearErrors() {
  m_errorSelections.clear();
  m_errorLines.clear();
}

void CodeValidator::validateJson(const QString &text) {
  // JSON 验证逻辑（委托给 JsonValidator）
  // 这里可以调用已有的 JsonValidator 类
  Q_UNUSED(text);
  // TODO: 实现 JSON 验证
}

void CodeValidator::validateTemplate(const QString &text) {
  // 模板验证逻辑（委托给 TplValidator）
  Q_UNUSED(text);
  // TODO: 实现模板验证
}

void CodeValidator::validateAcScript(const QString &text) {
  // AC 脚本验证逻辑（委托给 AcValidator）
  Q_UNUSED(text);
  // TODO: 实现 AC 脚本验证
}