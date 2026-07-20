/**
 * @file tpl_engine.cpp
 * @brief 模板引擎实现
 *
 * TplEngine 的职责（精简后）：
 * 1. render()       — 对外入口：清空错误，调用 renderBlock
 * 2. renderBlock()  — 递归扫描 ${...}，交给 TplFactory 创建处理器
 * 3. resolvePath()  — 嵌套属性路径解析（共享工具，供 handler 调用）
 *
 * 算术表达式求值已移至 BlockExpression。
 */

#include "tpl_engine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>

#include "../ac_language.h"
#include "../function/fun_mgr.h"
#include "../schema_validator.h"
#include "tpl_lexer.h"
#include "tpl_parser.h"
#include "tpl_renderer.h"

// 静态成员定义

const SchemaValidator *TplEngine::sm_validator = nullptr;
QString TplEngine::sm_schemaClass;

TplEngine::TplEngine() : m_handlerFactory(*this) {}

// setSchema — 设置 Schema 校验
void TplEngine::setSchema(const QString &className, const SchemaValidator *validator) {
  sm_schemaClass = className;
  sm_validator = validator;
}

// clearSchema — 清除 Schema 校验
void TplEngine::clearSchema() {
  sm_schemaClass.clear();
  sm_validator = nullptr;
}

// render — 对外渲染入口
//
// 新实现流程（AST 方案）：
//   1. Lexer   — 把模板字符串切成 Token 流
//   2. Parser  — 把 Token 流组装成 AST 树
//   3. Renderer — 遍历 AST 树，输出最终字符串
//
// 空行控制规则（明确且简单）：
//   - 块标签（${if}/${each}/${else} 等）独占一行时，剔除该行的缩进和换行符
//   - 块标签行内出现时，保留所有空白字符
//   - 不做任何"智能空行压缩"，模板里几个 \n 就输出几个 \n
QString TplEngine::render(const QString &tmpl, const QJsonObject &data) const {
  m_lastError.clear();

  if (sm_validator && !sm_schemaClass.isEmpty()) {
    QString err = sm_validator->validate(sm_schemaClass, data);
    if (!err.isEmpty()) {
      m_lastError = err;
      return {};
    }
  }

  // 去除 \r（统一行尾格式）
  QString cleanTmpl = tmpl;
  cleanTmpl.remove(QLatin1Char('\r'));

  // 阶段 1：词法分析
  QString lexError;
  QList<TplLexer::Token> tokens = TplLexer::tokenize(cleanTmpl, lexError);
  if (!lexError.isEmpty()) {
    m_lastError = lexError;
    return {};
  }

  // 阶段 2：语法分析
  QString parseError;
  QList<QSharedPointer<TplAst::AstNode>> ast = TplParser::parse(tokens, parseError);
  if (!parseError.isEmpty()) {
    m_lastError = parseError;
    return {};
  }

  // 阶段 3：渲染
  QString result = TplRenderer::render(ast, data, *this);

  return result;
}

// ─────────────────────────────────────────────────────────────────────
// 辅助：判断表达式是否为"块标签"（注释/条件/循环及其闭合标签）
// 块标签与普通变量标签的区别：块标签本身不产生输出，也不在文本行内出现
static bool isBlockTagExpr(const QString &expr) {
  if (expr.startsWith(QChar('#'))) return true;                                    // ${# 注释内容}
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kIfPrefix))) return true;    // ${if ...}
  if (expr.startsWith(QString::fromLatin1(AcTemplate::kEachPrefix))) return true;  // ${each ...}
  // 注意：expr 是 ${...} 内部内容，不含 ${ 和 }，需用 AcKeyword::kElse ("else") 而非
  // AcTemplate::kElse ("${else}")
  if (expr.startsWith(QString::fromLatin1(AcKeyword::kElse) + QLatin1Char(' ') +
                      QString::fromLatin1(AcTemplate::kIfPrefix)))
    return true;                                                   // ${else if ...}
  if (expr == QString::fromLatin1(AcKeyword::kElse)) return true;  // ${else}
  if (expr == QStringLiteral("/if")) return true;                  // ${/if}
  if (expr == QStringLiteral("/each")) return true;                // ${/each}
  return false;
}

// 辅助：判断从 from 到 to 之间的字符是否只包含空白和块标签
// 用于判断一行是否"只有块标签"（整行可以跳过，不输出）
static bool isOnlyBlockTagsAndWhitespace(const QString &block, int from, int to) {
  int cur = from;
  while (cur < to) {
    if (block[cur].isSpace()) {
      cur++;
      continue;
    }
    // 遇到 ${ 开头，检查是否是块标签
    if (cur + 1 < to && block[cur] == QChar('$') && block[cur + 1] == QChar('{')) {
      int exprStart = cur + 2;
      // 找到匹配的 }
      int depth = 1, scan = exprStart;
      while (scan < to && depth > 0) {
        if (scan + 1 < to && block[scan] == QChar('$') && block[scan + 1] == QChar('{')) {
          depth++;
          scan += 2;
        } else if (block[scan] == QChar('}')) {
          depth--;
          scan++;
        } else {
          scan++;
        }
      }
      if (depth != 0) return false;  // 未闭合的 ${，不是块标签行
      // 提取表达式并判断是否是块标签
      QString expr = block.mid(exprStart, scan - exprStart - 1).trimmed();
      if (!isBlockTagExpr(expr)) return false;  // 非块标签（如 ${field.name}），不能跳过
      cur = scan;
    } else {
      return false;  // 非空白且非 ${ 开头，不能跳过
    }
  }
  return true;
}

// 辅助：判断标签是否"独占一行"（标签所在行只有空白和块标签）
// 增强版：不仅支持单个标签独占一行，还支持一行多个块标签（如 ${else}${if ...}）
// 参数：block 模板文本，tagStart ${ 的位置，tagEndAfter } 的下一个位置
// 返回：true = 标签所在行只有块标签和空白，整行可以跳过
static bool isTagAloneOnLine(const QString &block, int tagStart, int tagEndAfter) {
  // 找到标签所在行的范围
  int lineBegin = tagStart;
  while (lineBegin > 0 && block[lineBegin - 1] != QChar('\n')) {
    lineBegin--;
  }
  int lineEnd = tagEndAfter;
  while (lineEnd < block.length() && block[lineEnd] != QChar('\n')) {
    lineEnd++;
  }

  // 检查整行是否只包含空白和块标签
  return isOnlyBlockTagsAndWhitespace(block, lineBegin, lineEnd);
}

// 辅助：从 pos 开始跳过当前行剩余的空白及行尾换行符
// 如果 skipBlankLines=true，还会继续跳过后续的完整空行（只包含空白的行）
// 返回：跳过空白后新的位置
static int skipRestOfLine(const QString &block, int pos, bool skipBlank = false) {
  // 先跳过当前行的剩余空白和换行符
  while (pos < block.length() && block[pos] != QChar('\n')) {
    if (!block[pos].isSpace()) break;
    pos++;
  }
  if (pos < block.length() && block[pos] == QChar('\n')) pos++;

  // 如果需要，继续跳过后续的完整空行
  if (skipBlank) {
    while (pos < block.length()) {
      int lineStart = pos;
      bool isBlankLine = true;

      while (pos < block.length() && block[pos] != QChar('\n')) {
        if (!block[pos].isSpace()) {
          isBlankLine = false;
          break;
        }
        pos++;
      }

      if (!isBlankLine) {
        pos = lineStart;
        break;
      }  // 不是空行，回退

      if (pos < block.length() && block[pos] == QChar('\n')) pos++;  // 吃掉空行的 \n
    }
  }

  return pos;
}

// ── 辅助：检测 [from, to) 区间是否只包含空白字符 ──
//   用于判断：tag 之前的行首内容是否只是缩进（而非真实代码）
static bool isAllWhitespace(const QString &block, int from, int to) {
  for (int i = from; i < to; i++) {
    if (!block[i].isSpace()) return false;
  }
  return true;
}

// renderBlock — 递归扫描 ${...} 并交给处理器
//
// 基本流程：
//   1. 找到下一个 ${
//   2. 输出标签之前的文本（智能处理空行）
//   3. 判断标签类型并处理
//
// 空白行处理策略：
//   - 当遇到"独占一行的块标签"时，标签之前的末尾空行会被裁剪掉
//   - 这样可以避免模板中为结构清晰添加的空行泄露到输出中

QString TplEngine::renderBlock(const QString &block, const QJsonObject &context) const {
  QString result;
  int pos = 0;

  while (pos < block.length()) {
    int start = block.indexOf(QString::fromLatin1(AcTemplate::kExprOpen), pos);
    if (start == -1) {
      result += block.mid(pos);
      break;
    }

    int lineStart = (start == 0) ? 0 : (block.lastIndexOf(QChar('\n'), start - 1) + 1);

    // ── Step 1: 预判标签类型 ──
    bool isComment = false;
    bool isBlockTag = false;
    bool aloneOnLine = false;
    int closeEnd = -1;
    QString expr;

    int commentPos = start + 2;
    if (commentPos < block.length() && block[commentPos] == QChar('#')) {
      isComment = true;
      isBlockTag = true;
      // 注释使用贪婪匹配：找到行内最后一个 } 作为注释结束符
      // 这样注释内容中的 }（如代码示例 import { A } from 'x'）不会被误判为注释结束
      int lineEnd = block.indexOf(QChar('\n'), commentPos);
      if (lineEnd == -1) lineEnd = block.length();
      int lastClose = block.lastIndexOf(QChar('}'), lineEnd - 1);
      if (lastClose > commentPos) {
        closeEnd = lastClose + 1;
        aloneOnLine = isTagAloneOnLine(block, start, closeEnd);
      }
    } else {
      int end = block.indexOf(QLatin1Char('}'), start + 2);
      if (end != -1) {
        closeEnd = end + 1;
        expr = block.mid(start + 2, end - start - 2).trimmed();
        isBlockTag = isBlockTagExpr(expr);
        aloneOnLine = isTagAloneOnLine(block, start, closeEnd);
      }
    }

    if (closeEnd == -1) {
      m_lastError = QStringLiteral("Unclosed ${ at position %1").arg(start);
      return {};
    }

    // ── Step 2: 输出标签之前的文本 [pos, start)，智能处理空行 ──
    // 设计原则（与 Jinja2/Mustache 一致）：
    //   独占一行的标签只删除自己所在行（缩进+标签+换行），
    //   不影响周围的空行。模板作者通过添加/删除空行来控制输出间距。
    if (start > pos) {
      int outputEnd = start;

      if ((isComment || isBlockTag) && aloneOnLine) {
        // 当前是独占一行的块标签 → 只裁剪标签所在行的缩进空白
        // 整行（包括缩进）都不应该出现在输出中，但标签前的空行保留
        int trimPos = start - 1;

        // 从 start 往回扫描，跳过标签所在行的缩进空白
        while (trimPos >= pos && block[trimPos] != QChar('\n')) {
          trimPos--;
        }

        // 不再往回扫描空行——空行是模板作者有意添加的，应保留
        outputEnd = trimPos + 1;
      }

      if (outputEnd > pos) {
        result += block.mid(pos, outputEnd - pos);
      }
    }

    // ── Step 3: 处理标签本身 ──
    if (isComment) {
      if (aloneOnLine) {
        pos = skipRestOfLine(block, closeEnd, false);  // 注释标签：只跳过本行，不跳后续空行
      } else {
        pos = closeEnd;
      }
      continue;
    }

    if (isBlockTag && aloneOnLine) {
      // 独占一行的块标签：跳过开标签行的剩余内容（含换行符），
      // 这样 handle 提取的 body 不会包含开标签行的换行符，
      // 避免循环体每次迭代都多输出一个空行
      pos = skipRestOfLine(block, closeEnd, false);
      auto h = m_handlerFactory.createHandler(expr);
      if (!h->handle(block, pos, expr, context, result)) return {};
      // 跳过闭标签行的剩余内容（含换行符）
      pos = skipRestOfLine(block, pos, false);
    } else {
      pos = closeEnd;
      auto h = m_handlerFactory.createHandler(expr);
      if (!h->handle(block, pos, expr, context, result)) return {};
    }
  }

  return result;
}

// resolvePath — 路径解析入口（函数调用 / 变量路径分发）

QJsonValue TplEngine::resolvePath(const QString &path, const QJsonObject &context) const {
  int parenPos = path.indexOf(QLatin1Char('('));
  if (parenPos != -1 && path.endsWith(QLatin1Char(')'))) {
    return resolveFuncCall(path, context);
  }
  return resolveVarPath(path, context);
}

// resolveFuncCall — 函数调用解析（类名.函数名(参数)）

QJsonValue TplEngine::resolveFuncCall(const QString &path, const QJsonObject &context) const {
  int parenPos = path.indexOf(QLatin1Char('('));
  QString fullFunc = path.left(parenPos);
  QString argsStr = path.mid(parenPos + 1);
  argsStr.chop(1);  // 去掉尾部的 )

  // 解析参数：逗号分隔，去除首尾空白
  QStringList rawArgs;
  if (!argsStr.isEmpty()) {
    int start = 0;
    for (int i = 0; i < argsStr.length(); ++i) {
      if (argsStr[i] == QLatin1Char(',')) {
        rawArgs.append(argsStr.mid(start, i - start).trimmed());
        start = i + 1;
      }
    }
    rawArgs.append(argsStr.mid(start).trimmed());
  }

  // 解析参数：递归调用 resolvePath 处理每个参数
  QJsonArray evalArgs;
  for (const QString &raw : rawArgs) {
    bool ok = false;
    double num = raw.toDouble(&ok);
    if (ok) {
      evalArgs.append(num);
    } else if ((raw.startsWith(QStringLiteral("\"")) && raw.endsWith(QStringLiteral("\""))) ||
               (raw.startsWith(QStringLiteral("'")) && raw.endsWith(QStringLiteral("'")))) {
      evalArgs.append(raw.mid(1, raw.length() - 2));
    } else {
      evalArgs.append(resolvePath(raw, context));
    }
  }

  // 解析函数名：类名.函数名
  int dotPos = fullFunc.indexOf(QLatin1Char('.'));
  if (dotPos != -1) {
    QString clsName = fullFunc.left(dotPos);
    QString funcName = fullFunc.mid(dotPos + 1);
    return FunMgr::ins().call(clsName, funcName, evalArgs);
  }

  // 无类名前缀：尝试作为一级内置函数（注册在 "builtin" 伪类下）
  if (FunMgr::ins().contains(QString::fromLatin1(AcRuntime::kBuiltinClass), fullFunc)) {
    return FunMgr::ins().call(QString::fromLatin1(AcRuntime::kBuiltinClass), fullFunc, evalArgs);
  }

  m_lastError = QStringLiteral("invalid function call format: %1").arg(path);
  return QJsonValue();
}

// resolveVarPath — 嵌套属性路径解析（按点号逐层查找）

QJsonValue TplEngine::resolveVarPath(const QString &path, const QJsonObject &context) const {
  QStringList parts = path.split(QStringLiteral("."));
  QJsonValue value = QJsonValue(context);

  for (const QString &part : parts) {
    if (value.isObject()) {
      value = value.toObject().value(part);
    } else if (value.isArray()) {
      bool ok = false;
      int idx = part.toInt(&ok);
      if (ok) {
        QJsonArray arr = value.toArray();
        if (idx >= 0 && idx < arr.size())
          value = arr[idx];
        else
          return QJsonValue();
      } else {
        return QJsonValue();
      }
    } else {
      return QJsonValue();
    }
  }

  return value;
}