/**
 * @file ac_diagnostic.cpp
 * @brief 结构化诊断收集器实现
 */

#include "ac_diagnostic.h"

void AcDiagCollector::error(const QString &code, const QString &msg, const QString &filePath,
                            const AcLoc &loc, int length) {
  AcDiagnostic d;
  d.code = code;
  d.severity = AcDiagSeverity::kError;
  d.filePath = filePath;
  d.loc = loc;
  d.length = length;
  d.message = msg;
  m_items.append(d);
}

void AcDiagCollector::warning(const QString &code, const QString &msg, const QString &filePath,
                              const AcLoc &loc, int length) {
  AcDiagnostic d;
  d.code = code;
  d.severity = AcDiagSeverity::kWarning;
  d.filePath = filePath;
  d.loc = loc;
  d.length = length;
  d.message = msg;
  m_items.append(d);
}

bool AcDiagCollector::hasErrors() const {
  for (const auto &d : m_items) {
    if (d.isError()) return true;
  }
  return false;
}

QVector<AcDiagnostic> AcDiagCollector::errors() const {
  QVector<AcDiagnostic> out;
  for (const auto &d : m_items) {
    if (d.isError()) out.append(d);
  }
  return out;
}

QString AcDiagCollector::firstErrorMessage() const {
  for (const auto &d : m_items) {
    if (d.isError()) return d.message;
  }
  return QString();
}

ValidationResult toValidationResult(const AcDiagnostic &d) {
  ValidationResult::Severity sev =
      d.isWarning() ? ValidationResult::kWarning : ValidationResult::kError;
  return ValidationResult(d.loc.line, d.loc.col, d.length, d.message, sev);
}
