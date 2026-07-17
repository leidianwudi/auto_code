/**
 * @file symbol_navigator.cpp
 * @brief 符号导航器实现（专职模块）
 *
 * 负责符号表的存储、查询和导航功能，
 * 包括悬停提示、转到定义、查找引用等操作。
 */

#include "symbol_navigator.h"

#include <QRegularExpression>

#include "src/engine/script/ac_symbol_table.h"

SymbolNavigator::SymbolNavigator(QObject *parent) : QObject(parent) {}

SymbolNavigator::~SymbolNavigator() {}

void SymbolNavigator::setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols) {
  m_symbolTable = symbols;
}

QString SymbolNavigator::identifierAtCursor(int pos, const QString &text, int *startPos,
                                            int *endPos) {
  if (pos < 0 || pos >= text.size()) {
    if (startPos) *startPos = pos;
    if (endPos) *endPos = pos;
    return QString();
  }

  // 向前扫描标识符字符（支持 :: 静态访问）
  int start = pos;
  while (start > 0) {
    QChar ch = text[start - 1];
    if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('$') &&
        !text.mid(start - 1, 2).startsWith(QStringLiteral("::"))) {
      break;
    }
    --start;
  }

  // 向后扫描标识符字符（支持 :: 静态访问）
  int end = pos;
  while (end < text.size()) {
    QChar ch = text[end];
    if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('$') &&
        !text.mid(end, 2).startsWith(QStringLiteral("::"))) {
      break;
    }
    ++end;
  }

  if (start == end) {
    if (startPos) *startPos = pos;
    if (endPos) *endPos = pos;
    return QString();
  }

  if (startPos) *startPos = start;
  if (endPos) *endPos = end;
  return text.mid(start, end - start);
}

const AcSymbolEntry *SymbolNavigator::findDefinition(const QString &name) const {
  if (name.isEmpty()) return nullptr;

  // 精确匹配
  auto it = m_symbolTable.find(name);
  if (it != m_symbolTable.end()) return &it.value();

  // 静态访问匹配：ClassName::member -> 尝试 ClassName.member
  if (name.contains(QStringLiteral("::"))) {
    QString key = name;
    key.replace(QStringLiteral("::"), QStringLiteral("."));
    auto it3 = m_symbolTable.find(key);
    if (it3 != m_symbolTable.end()) return &it3.value();

    // 也尝试只匹配类名
    QString className = name.section(QStringLiteral("::"), 0, 0);
    auto it4 = m_symbolTable.find(className);
    if (it4 != m_symbolTable.end()) return &it4.value();
  }

  // 类方法/属性匹配：同名方法/属性/参数
  for (auto it2 = m_symbolTable.begin(); it2 != m_symbolTable.end(); ++it2) {
    if (it2.value().name == name &&
        (it2.value().kind == AcSymbolKind::kMethod || it2.value().kind == AcSymbolKind::kProperty ||
         it2.value().kind == AcSymbolKind::kParameter)) {
      return &it2.value();
    }
  }

  return nullptr;
}

QVector<QPair<int, QString>> SymbolNavigator::findReferences(const QString &name,
                                                             const QString &text) const {
  QVector<QPair<int, QString>> refs;
  if (name.isEmpty()) return refs;

  // 扫描全文查找标识符出现位置
  QStringList lines = text.split(QLatin1Char('\n'));
  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));

  for (int i = 0; i < lines.size(); ++i) {
    auto matchIter = re.globalMatch(lines[i]);
    while (matchIter.hasNext()) {
      auto match = matchIter.next();
      refs.append(qMakePair(i + 1, lines[i]));  // 行号从 1 开始
    }
  }

  return refs;
}

QString SymbolNavigator::generateHoverText(const QString &name) const {
  const AcSymbolEntry *entry = findDefinition(name);
  if (!entry) return QString();

  QString tooltip;

  // 显示签名（函数/方法）
  if (!entry->signature.isEmpty()) {
    tooltip += entry->signature + QStringLiteral("\n\n");
  }

  // 显示类型信息
  tooltip += QStringLiteral("Type: ") + QString::number(static_cast<int>(entry->kind));

  // 显示参数列表（如果有）
  if (!entry->params.isEmpty()) {
    QStringList paramNames;
    for (const auto &param : entry->params) {
      paramNames.append(param.name);
    }
    tooltip += QStringLiteral("\nParams: ") + paramNames.join(QStringLiteral(", "));
  }

  // 显示引用数量
  int refCount = findReferences(name, QString()).size();
  if (refCount > 0) {
    tooltip += QStringLiteral("\n\nReferences: ") + QString::number(refCount);
  }

  return tooltip;
}