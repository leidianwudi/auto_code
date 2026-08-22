/**
 * @file code_editor_navigate.cpp
 * @brief 代码编辑器导航功能实现（从 code_editor.cpp 拆分）
 *
 * 包含以下功能：
 * - 标识符提取（identifierAtCursor）
 * - 符号定义查找（findSymbolDefinition / findPropertyDefinition）
 * - 转到定义（goToDefinition / goToTypeDefinition）
 * - 函数签名提示（showSignatureHelp）
 * - 引用查找与高亮（findSymbolReferences / highlightSymbolReferences）
 * - 悬停提示（showSymbolHover）
 * - 括号导航（jumpToMatchingBracket / selectBetweenBrackets）
 */

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QToolTip>

#include "code_editor.h"
#include "src/engine/ac_language.h"
#include "src/engine/script/ac_symbol_table.h"
#include "src/util/ui/component/aui_style.h"

// ──────────────────────────────────────────────────────────────
//  标识符提取与符号查找
// ──────────────────────────────────────────────────────────────

QString CodeEditor::identifierAtCursor(int pos, int *startPos, int *endPos) const {
  const QString &text = cachedText();
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

const AcSymbolEntry *CodeEditor::findSymbolDefinition(const QString &name) const {
  if (name.isEmpty()) return nullptr;

  // 使用 SymbolNavigator 进行查找
  return m_symbolNavigator.findDefinition(name);
}

const AcSymbolEntry *CodeEditor::findPropertyDefinition(const QString &propName) const {
  if (propName.isEmpty()) return nullptr;

  // 从光标位置向前扫描，收集完整属性链：a.b.c → [a, b, c]
  const QString &text = cachedText();
  int cursorPos = textCursor().position();

  // 向前找到当前标识符的起始位置
  int idEnd = cursorPos;
  int idStart = cursorPos;
  while (idEnd < text.size() &&
         (text[idEnd].isLetterOrNumber() || text[idEnd] == QLatin1Char('_') ||
          text[idEnd] == QLatin1Char('$'))) {
    ++idEnd;
  }
  while (idStart > 0 &&
         (text[idStart - 1].isLetterOrNumber() || text[idStart - 1] == QLatin1Char('_') ||
          text[idStart - 1] == QLatin1Char('$'))) {
    --idStart;
  }

  // 检查标识符前面是否是 '.'（属性访问）
  int dotPos = idStart - 1;
  if (dotPos < 0 || text[dotPos] != QLatin1Char('.')) return nullptr;

  // 向前收集完整属性链：a.b.c
  QStringList chain;  // 逆序收集后 prepend
  chain.prepend(propName);

  int scanPos = dotPos;
  while (scanPos > 0 && text[scanPos] == QLatin1Char('.')) {
    int objEnd = scanPos;
    int objStart = scanPos;
    while (objStart > 0 &&
           (text[objStart - 1].isLetterOrNumber() || text[objStart - 1] == QLatin1Char('_') ||
            text[objStart - 1] == QLatin1Char('$'))) {
      --objStart;
    }
    if (objStart == objEnd) break;
    chain.prepend(text.mid(objStart, objEnd - objStart));

    scanPos = objStart - 1;
    while (scanPos > 0 && text[scanPos].isSpace()) --scanPos;
    if (scanPos < 0 || text[scanPos] != QLatin1Char('.')) break;
  }

  if (chain.size() < 2) return nullptr;

  // 从链首开始逐级解析类型
  QString currentType;
  const AcSymbolEntry *result = nullptr;

  const AcSymbolEntry *objEntry = m_symbolNavigator.findDefinition(chain[0]);
  if (objEntry) {
    currentType = objEntry->returnType;
    if (currentType.isEmpty() || currentType == QString::fromLatin1(AcTypeName::kAny)) {
      currentType = chain[0];
      if (!currentType.isEmpty()) currentType[0] = currentType[0].toUpper();
    }
  }

  // 逐级解析属性链：a → TypeA → TypeA.b → TypeB → TypeB.c
  for (int i = 1; i < chain.size() && !currentType.isEmpty(); ++i) {
    QString qualifiedKey = currentType + QStringLiteral(".") + chain[i];
    result = m_symbolNavigator.findDefinition(qualifiedKey);
    if (!result) {
      // 尝试用变量名作为前缀（对象字面量属性）
      QString varKey = chain[0] + QStringLiteral(".") + chain[i];
      result = m_symbolNavigator.findDefinition(varKey);
      if (!result) break;
    }
    if (i < chain.size() - 1) {
      currentType = result->returnType;
      if (currentType.isEmpty() || currentType == QString::fromLatin1(AcTypeName::kAny)) break;
    }
  }

  if (result) return result;

  // 回退：遍历符号表查找名为 propName 的 kProperty 类型符号
  const auto &symbols = m_symbolNavigator.symbolTable();
  for (auto it = symbols.begin(); it != symbols.end(); ++it) {
    if (it.value().name == propName && it.value().kind == AcSymbolKind::kProperty) {
      return &it.value();
    }
  }

  return nullptr;
}

// ──────────────────────────────────────────────────────────────
//  转到定义 / 转到类型定义
// ──────────────────────────────────────────────────────────────

void CodeEditor::goToDefinition(const QString &name) {
  if (name.isEmpty() || !document()) return;

  // 优先检查属性访问上下文：obj.prop → 精确查找 objType.prop
  // 必须在 findSymbolDefinition 之前，避免找到同名的其他类属性（如 Array.length vs String.length）
  const AcSymbolEntry *entry = findPropertyDefinition(name);

  // 非属性访问场景，或属性查找失败时，使用普通符号查找
  if (!entry) {
    entry = findSymbolDefinition(name);
  }

  // 检查是否是 new ClassName() 上下文 → 转到类型定义
  if (!entry) {
    const QString &text = cachedText();
    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    // 向前找 new 关键字
    int idStart = pos;
    while (idStart > 0 &&
           (text[idStart - 1].isLetterOrNumber() || text[idStart - 1] == QLatin1Char('_'))) {
      --idStart;
    }
    // 跳过空格
    int checkPos = idStart - 1;
    while (checkPos > 0 && text[checkPos].isSpace()) --checkPos;
    // 检查是否是 "new" 关键字
    if (checkPos >= 2 && text.mid(checkPos - 2, 3) == QStringLiteral("new") &&
        (checkPos == 2 || !text[checkPos - 3].isLetterOrNumber())) {
      goToTypeDefinition(name);
      return;
    }
  }

  if (!entry) return;

  // 确定目标位置
  QString targetPath = entry->filePath.isEmpty() ? objectName() : entry->filePath;
  int targetLine = entry->line;

  // 行号为 0 时，通过源码搜索定位
  if (targetLine <= 0) {
    targetLine = findSymbolLineByName(name);
    if (targetLine <= 0) targetLine = 1;  // 找不到时默认第 1 行
  }

  // 发射即将导航信号（用于记录历史，无论是否跨文件）
  emit aboutToNavigate(targetPath, targetLine);

  // 如果是当前文件，直接跳转
  if (targetPath == objectName()) {
    QTextCursor cursor(document());
    QTextBlock block = document()->findBlockByNumber(targetLine - 1);
    if (block.isValid()) {
      cursor.setPosition(block.position());
      setTextCursor(cursor);
      ensureCursorVisible();
      setFocus();
    }
  } else {
    // 跨文件跳转，发射信号让外部处理
    emit requestGoToLine(targetPath, targetLine);
  }
}

int CodeEditor::findSymbolLineByName(const QString &name) const {
  const QString &text = cachedText();
  QStringList lines = text.split(QLatin1Char('\n'));

  // 搜索各种定义模式
  QRegularExpression funcRe(QStringLiteral("^\\s*function\\s+") + QRegularExpression::escape(name) +
                            QStringLiteral("\\b"));
  QRegularExpression classRe(QStringLiteral("^\\s*class\\s+") + QRegularExpression::escape(name) +
                             QStringLiteral("\\b"));
  QRegularExpression ifaceRe(QStringLiteral("^\\s*interface\\s+") +
                             QRegularExpression::escape(name) + QStringLiteral("\\b"));
  // 变量声明：let name 或 let name: Type
  QRegularExpression letRe(QStringLiteral("^\\s*let\\s+") + QRegularExpression::escape(name) +
                           QStringLiteral("\\b"));
  // for-in 循环变量：for (name in 或 for (let name in
  QRegularExpression forInRe(QStringLiteral("\\bfor\\s*\\(\\s*(?:let\\s+)?") +
                             QRegularExpression::escape(name) + QStringLiteral("\\b"));

  for (int i = 0; i < lines.size(); ++i) {
    if (funcRe.match(lines[i]).hasMatch() || classRe.match(lines[i]).hasMatch() ||
        ifaceRe.match(lines[i]).hasMatch() || letRe.match(lines[i]).hasMatch() ||
        forInRe.match(lines[i]).hasMatch()) {
      return i + 1;  // 返回 1-based 行号
    }
  }

  // 如果没找到精确匹配，尝试简单搜索标识符
  QRegularExpression identRe(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                             QStringLiteral("\\b"));
  for (int i = 0; i < lines.size(); ++i) {
    if (identRe.match(lines[i]).hasMatch()) {
      return i + 1;
    }
  }

  return 0;
}

void CodeEditor::goToTypeDefinition(const QString &name) {
  if (name.isEmpty() || !document()) return;

  // 在符号表中查找类定义
  const AcSymbolEntry *entry = m_symbolNavigator.findDefinition(name);
  if (!entry || entry->kind != AcSymbolKind::kClass) {
    // 遍历查找类名匹配的条目
    const auto &symbols = m_symbolNavigator.symbolTable();
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
      if (it.value().name == name && it.value().kind == AcSymbolKind::kClass) {
        entry = &it.value();
        break;
      }
    }
  }

  if (!entry) return;

  QString targetPath = entry->filePath.isEmpty() ? objectName() : entry->filePath;
  int targetLine = entry->line;
  if (targetLine <= 0) {
    targetLine = findSymbolLineByName(name);
    if (targetLine <= 0) targetLine = 1;
  }

  emit aboutToNavigate(targetPath, targetLine);

  if (targetPath == objectName()) {
    QTextCursor cursor(document());
    QTextBlock block = document()->findBlockByNumber(targetLine - 1);
    if (block.isValid()) {
      cursor.setPosition(block.position());
      setTextCursor(cursor);
      ensureCursorVisible();
      setFocus();
    }
  } else {
    emit requestGoToLine(targetPath, targetLine);
  }
}

// ──────────────────────────────────────────────────────────────
//  函数签名提示
// ──────────────────────────────────────────────────────────────

void CodeEditor::showSignatureHelp() {
  if (m_validationMode != AcValidation) return;

  const QString &text = cachedText();
  QTextCursor cursor = textCursor();
  int pos = cursor.position();

  // 光标在 ( 之后，向前查找函数名
  // pos 现在在 ( 之后一位
  int parenPos = pos - 1;
  if (parenPos < 0 || text[parenPos] != QLatin1Char('(')) return;

  // 向前跳过空格
  int scanPos = parenPos - 1;
  while (scanPos > 0 && text[scanPos].isSpace()) --scanPos;

  // 提取函数名（支持 obj.method 形式）
  int nameEnd = scanPos + 1;
  int nameStart = scanPos;
  while (nameStart > 0 &&
         (text[nameStart - 1].isLetterOrNumber() || text[nameStart - 1] == QLatin1Char('_') ||
          text[nameStart - 1] == QLatin1Char('.') || text[nameStart - 1] == QLatin1Char('$'))) {
    --nameStart;
  }
  QString funcName = text.mid(nameStart, nameEnd - nameStart);
  if (funcName.isEmpty()) return;

  // 查找函数符号
  const AcSymbolEntry *entry = nullptr;

  // 如果是 obj.method 形式，查找 ClassName.method
  if (funcName.contains(QLatin1Char('.'))) {
    entry = m_symbolNavigator.findDefinition(funcName);
  }

  // 直接按函数名查找
  if (!entry) {
    entry = m_symbolNavigator.findDefinition(funcName);
  }

  // 如果函数名包含 .，尝试只匹配方法名部分
  if (!entry && funcName.contains(QLatin1Char('.'))) {
    QString methodName = funcName.section(QLatin1Char('.'), -1);
    const auto &symbols = m_symbolNavigator.symbolTable();
    for (auto it = symbols.begin(); it != symbols.end(); ++it) {
      if (it.value().name == methodName && it.value().kind == AcSymbolKind::kMethod) {
        entry = &it.value();
        break;
      }
    }
  }

  if (!entry) return;

  // 构建签名提示文本
  QString tip = entry->signature;
  if (!entry->returnType.isEmpty() && entry->returnType != QString::fromLatin1(AcTypeName::kVoid)) {
    tip += QStringLiteral("\n→ ") + entry->returnType;
  }

  // 显示在光标上方
  QRect rect = cursorRect();
  QPoint gpos = viewport()->mapToGlobal(QPoint(rect.x(), rect.y() - 20));
  QToolTip::showText(gpos, tip, this);
}

// ──────────────────────────────────────────────────────────────
//  引用查找与符号高亮
// ──────────────────────────────────────────────────────────────

/// 收集文本中所有注释区域 [起始偏移, 长度)，含 // 单行注释与 /* */ 块注释。
/// 会跳过字符串（"..." 与模板字符串 `...`）内的内容，避免把字符串里的 // 误判为注释。
/// VSCode 规则：被注释的标识符不算引用。
static QVector<QPair<int, int>> collectCommentRanges(const QString &text) {
  QVector<QPair<int, int>> ranges;
  const int n = text.size();
  int i = 0;
  bool inBlock = false;
  int blockStart = 0;
  while (i < n) {
    if (!inBlock) {
      const QChar c = text.at(i);
      if (c == QLatin1Char('"')) {
        int j = i + 1;
        while (j < n) {
          if (text.at(j) == QLatin1Char('\\')) {
            j += 2;
            continue;
          }
          if (text.at(j) == QLatin1Char('"')) {
            ++j;
            break;
          }
          ++j;
        }
        i = j;
      } else if (c == QLatin1Char('`')) {
        int j = i + 1;
        while (j < n) {
          if (text.at(j) == QLatin1Char('\\')) {
            j += 2;
            continue;
          }
          if (text.at(j) == QLatin1Char('`')) {
            ++j;
            break;
          }
          ++j;
        }
        i = j;
      } else if (text.mid(i, 2) == QStringLiteral("/*")) {
        inBlock = true;
        blockStart = i;
        i += 2;
      } else if (text.mid(i, 2) == QStringLiteral("//")) {
        int end = text.indexOf(QLatin1Char('\n'), i);
        if (end < 0) end = n;
        ranges.append(qMakePair(i, end - i));
        i = end;
      } else {
        ++i;
      }
    } else {
      const int close = text.indexOf(QStringLiteral("*/"), i);
      if (close < 0) {
        ranges.append(qMakePair(blockStart, n - blockStart));
        break;
      }
      ranges.append(qMakePair(blockStart, close + 2 - blockStart));
      i = close + 2;
      inBlock = false;
    }
  }
  return ranges;
}

/// 判断字符偏移位置是否位于任一注释区域内
static bool posInComments(const QVector<QPair<int, int>> &ranges, int pos) {
  for (const auto &r : ranges) {
    if (pos >= r.first && pos < r.first + r.second) return true;
  }
  return false;
}

QVector<QPair<int, QString>> CodeEditor::findSymbolReferences(const QString &name) const {
  QVector<QPair<int, QString>> refs;
  if (name.isEmpty()) return refs;

  // 扫描全文查找标识符出现位置（跳过注释中的出现，与 VSCode 规则一致）
  const QString &text = cachedText();
  const QVector<QPair<int, int>> comments = collectCommentRanges(text);
  QStringList lines = text.split(QLatin1Char('\n'));
  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));

  int lineStart = 0;
  for (int i = 0; i < lines.size(); ++i) {
    auto match = re.match(lines[i]);
    if (match.hasMatch()) {
      const int absPos = lineStart + match.capturedStart();
      if (!posInComments(comments, absPos)) {
        refs.append(qMakePair(i + 1, lines[i].trimmed()));
      }
    }
    lineStart += lines[i].size() + 1;  // +1 为行尾换行符
  }

  return refs;
}

/// @brief 格式化调试变量的悬停提示（类型 + 值）
static QString debugVarTooltip(const AcDebugVar &v) {
  const QJsonValue &val = v.value;
  QString type;
  QString value;
  if (val.isString()) {
    type = QString::fromLatin1(AcTypeName::kString);
    value = val.toString();
  } else if (val.isDouble()) {
    type = QString::fromLatin1(AcTypeName::kNumber);
    value = QString::number(val.toDouble());
  } else if (val.isBool()) {
    type = QString::fromLatin1(AcTypeName::kBoolean);
    value = val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  } else if (val.isNull()) {
    type = QString::fromLatin1(AcTypeName::kNull);
    value = QStringLiteral("null");
  } else if (val.isUndefined()) {
    type = QStringLiteral("Undefined");
    value = QStringLiteral("undefined");
  } else if (val.isArray()) {
    type = QString::fromLatin1(AcTypeName::kArray);
    const QJsonArray &a = val.toArray();
    value = QStringLiteral("Array(%1)").arg(a.size());
    QStringList preview;
    int n = qMin(a.size(), 5);
    for (int i = 0; i < n; ++i) {
      const QJsonValue &elem = a[i];
      if (elem.isString())
        preview << elem.toString();
      else if (elem.isDouble())
        preview << QString::number(elem.toDouble());
      else if (elem.isBool())
        preview << (elem.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
      else if (elem.isArray())
        preview << QString::fromLatin1(AcTypeName::kArray);
      else if (elem.isObject())
        preview << QString::fromLatin1(AcTypeName::kObject);
      else if (elem.isNull())
        preview << QStringLiteral("null");
      else
        preview << QStringLiteral("undefined");
    }
    if (!preview.isEmpty())
      value += QStringLiteral(" [") + preview.join(QStringLiteral(", ")) + QStringLiteral("]");
  } else if (val.isObject()) {
    type = QString::fromLatin1(AcTypeName::kObject);
    value = QStringLiteral("Object(%1)").arg(val.toObject().size());
  } else {
    type = QString::fromLatin1(AcTypeName::kAny);
  }

  QString text = QStringLiteral("%1").arg(v.name);
  text += QStringLiteral("\n类型: ") + type;
  if (!value.isEmpty()) text += QStringLiteral("\n值: ") + value;
  return text;
}

void CodeEditor::showSymbolHover(int pos, const QPoint &globalPos) {
  if (m_validationMode != AcValidation) return;

  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
  if (identifier.isEmpty()) {
    QToolTip::hideText();
    return;
  }

  // 调试暂停时：命中调试变量则优先显示类型与值
  if (!m_debugVars.isEmpty()) {
    for (const auto &v : m_debugVars) {
      if (v.name == identifier) {
        m_currentHoverSymbol = identifier;
        QToolTip::showText(globalPos, debugVarTooltip(v), this);
        return;
      }
    }
  }

  // 同一符号不重复显示
  if (identifier == m_currentHoverSymbol) return;
  m_currentHoverSymbol = identifier;

  // 构建提示文本
  QString tipText = identifier;

  // 从符号表获取详细信息
  const AcSymbolEntry *entry = findSymbolDefinition(identifier);

  // 如果直接找不到，尝试属性访问上下文
  if (!entry) {
    entry = findPropertyDefinition(identifier);
  }

  if (entry) {
    // 显示签名
    if (!entry->signature.isEmpty()) {
      tipText = entry->signature;
    }

    // 显示返回类型
    if (!entry->returnType.isEmpty() &&
        entry->returnType != QString::fromLatin1(AcTypeName::kAny) &&
        entry->returnType != QString::fromLatin1(AcTypeName::kVoid)) {
      tipText += QStringLiteral("\n返回: ") + entry->returnType;
    }

    // 显示参数详情（函数/方法）
    if (!entry->params.isEmpty() &&
        (entry->kind == AcSymbolKind::kFunction || entry->kind == AcSymbolKind::kMethod)) {
      tipText += QStringLiteral("\n参数:");
      for (const auto &param : entry->params) {
        tipText += QStringLiteral("\n  ") + param.name + QStringLiteral(": ") +
                   (param.type.className.isEmpty() ? QString::fromLatin1(AcTypeName::kAny)
                                                   : param.type.className);
      }
    }

    // 显示符号类型和所属类
    QString kindStr;
    switch (entry->kind) {
      case AcSymbolKind::kFunction:
        kindStr = QStringLiteral("函数");
        break;
      case AcSymbolKind::kClass:
        kindStr = QStringLiteral("类");
        break;
      case AcSymbolKind::kMethod:
        kindStr = QStringLiteral("方法");
        break;
      case AcSymbolKind::kProperty:
        kindStr = QStringLiteral("属性");
        break;
      case AcSymbolKind::kVariable:
        kindStr = QStringLiteral("变量");
        break;
      case AcSymbolKind::kParameter:
        kindStr = QStringLiteral("参数");
        break;
      default:
        break;
    }
    if (!kindStr.isEmpty()) {
      tipText += QStringLiteral("\n类型: ") + kindStr;
    }
    if (!entry->parentClass.isEmpty()) {
      tipText += QStringLiteral("\n所属: ") + entry->parentClass;
    }

    // 显示文件位置
    if (!entry->filePath.isEmpty() && entry->line > 0) {
      QFileInfo fi(entry->filePath);
      tipText += QStringLiteral("\n位置: ") + fi.fileName() + QStringLiteral(":") +
                 QString::number(entry->line);
    }
  }

  // 查找引用数量
  auto refs = findSymbolReferences(identifier);
  if (!refs.isEmpty()) {
    tipText += QStringLiteral("\n%1 引用").arg(refs.size());
  }

  // 使用标准 QToolTip 显示悬停提示
  QToolTip::showText(globalPos, tipText, this);
}

void CodeEditor::highlightSymbolReferences(const QString &name) {
  m_referenceSelections.clear();

  if (name.isEmpty() || m_validationMode != AcValidation) {
    refreshExtraSelections();
    return;
  }

  auto refs = findSymbolReferences(name);
  QTextCursor cursor(document());

  QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(name) +
                        QStringLiteral("\\b"));
  const QString &text = cachedText();
  // 与 findSymbolReferences 一致：注释中的出现不高亮
  const QVector<QPair<int, int>> comments = collectCommentRanges(text);
  int offset = 0;

  while (offset < text.size()) {
    auto match = re.match(text, offset);
    if (!match.hasMatch()) break;

    int start = match.capturedStart();
    int length = match.capturedLength();
    offset = start + length;

    if (posInComments(comments, start)) continue;

    cursor.setPosition(start);
    cursor.setPosition(start + length, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = cursor;
    sel.format.setBackground(AuiStyle::modifiedColor().lighter(180));
    sel.format.setForeground(AuiStyle::modifiedColor());
    m_referenceSelections.append(sel);
  }

  refreshExtraSelections();
}

// ──────────────────────────────────────────────────────────────
//  括号导航（使用 BracketMatcher 模块）
// ──────────────────────────────────────────────────────────────

void CodeEditor::jumpToMatchingBracket() {
  QTextCursor cursor = textCursor();
  int pos = cursor.position();
  const QString &text = cachedText();

  if (pos <= 0 || pos > text.size()) return;

  QChar ch = text[pos - 1];
  QChar matchCh;
  int matchPos = BracketMatcher::findMatchingBracket(pos, ch, matchCh, text);

  if (matchPos >= 0) {
    cursor.setPosition(matchPos);
    setTextCursor(cursor);
  }
}

void CodeEditor::selectBetweenBrackets() {
  QTextCursor cursor = textCursor();
  int pos = cursor.position();
  const QString &text = cachedText();

  if (pos <= 0 || pos > text.size()) return;

  QChar ch = text[pos - 1];
  QChar matchCh;
  int matchPos = BracketMatcher::findMatchingBracket(pos, ch, matchCh, text);

  if (matchPos >= 0) {
    int start = qMin(pos - 1, matchPos);
    int end = qMax(pos - 1, matchPos);
    cursor.setPosition(start);
    cursor.setPosition(end + 1, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
  }
}

// ──────────────────────────────────────────────────────────────
//  统一的导航目标判定（悬停手形与 Ctrl+点击跳转共用）
// ──────────────────────────────────────────────────────────────

bool CodeEditor::navigationTargetAt(int pos) const {
  switch (m_validationMode) {
    case JsonValidation: {
      // 与鼠标点击跳转完全一致：路径非空 + schema 中可解析到属性
      if (!m_schemaLoaded) return false;
      QString path = m_schema.propertyPathAt(cachedText(), pos);
      QString className, propName;
      return !path.isEmpty() && m_schema.propertyContext(path, &className, &propName) &&
             !propName.isEmpty();
    }
    case AcValidation: {
      // 与 AC 现状一致：任意标识符均可提示/尝试跳转
      int start = 0, end = 0;
      return !identifierAtCursor(pos, &start, &end).isEmpty();
    }
    default:
      return false;
  }
}

// ── JSON 属性悬停提示（基于 $schema）──
void CodeEditor::showJsonPropertyHover(const QString &jsonPath, const QPoint &gpos) {
  if (m_validationMode != JsonValidation || !m_schemaLoaded) return;
  QString desc = m_schema.propertyDescription(jsonPath);
  if (desc.isEmpty()) {
    QToolTip::hideText();
    return;
  }
  QToolTip::showText(gpos, desc, this);
}

// ── 在 schema 文件中定位 className 类下 propName 属性的行号 ──
int CodeEditor::findSchemaPropertyLine(const QString &className, const QString &propName) const {
  QString text;
  {
    QFile f(m_schemaPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 1;
    text = QString::fromUtf8(f.readAll());
  }

  // 先定位类定义块
  QRegularExpression classRe(
      QStringLiteral("[\"']?%1[\"']?\\s*:").arg(QRegularExpression::escape(className)));
  auto cm = classRe.match(text);
  if (!cm.hasMatch()) return 1;
  int searchFrom = cm.capturedStart();

  // 类块内优先定位 properties 块，再在其中找属性
  int propsIdx = text.indexOf(QStringLiteral("properties"), searchFrom);
  if (propsIdx >= 0 && (propsIdx - searchFrom) < 2000) searchFrom = propsIdx;

  QRegularExpression propRe(
      QStringLiteral("[\"']?%1[\"']?\\s*:").arg(QRegularExpression::escape(propName)));
  auto pm = propRe.match(text, searchFrom);
  if (!pm.hasMatch()) return 1;

  int matchPos = pm.capturedStart();
  int line = 1;
  for (int i = 0; i < matchPos && i < text.size(); ++i) {
    if (text[i] == QLatin1Char('\n')) ++line;
  }
  return line;
}

void CodeEditor::setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols) {
  m_symbolTable = symbols;
  m_symbolNavigator.setSymbolTable(symbols);
}
