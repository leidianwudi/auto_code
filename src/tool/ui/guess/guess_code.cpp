/**
 * @file guess_code.cpp
 * @brief 代码自动补全实现
 */

#include "guess_code.h"

#include <QCompleter>
#include <QListView>
#include <QRegularExpression>
#include <QStringListModel>
#include <QStyleFactory>

#include "../aui_style.h"
#include "src/engine/ac_language.h"
#include "src/engine/script/ac_lexer.h"

// ──────────────────────────────────────────────────────────────
//  补全词库
// ──────────────────────────────────────────────────────────────

QStringList GuessCode::getAllCompletions(FileType type) {
  switch (type) {
    case AcFile: {
      // 关键字和内置函数由 AcKeyword::kAll / AcBuiltin::kAll 统一定义
      // 原生类（如 DB）仅在 new 后出现，由 showCompleter() 动态添加
      QStringList words = AcKeyword::kAll;
      for (const auto &fn : AcBuiltin::kAll) words << fn + QStringLiteral("()");
      return words;
    }
    case TplFile:
      return {
          QString::fromLatin1(AcTemplate::kExprOpen) +
              QString::fromLatin1(AcTemplate::kIfPrefix).trimmed(),
          QString::fromLatin1(AcTemplate::kElse),
          QString::fromLatin1(AcTemplate::kIfClose),
          QString::fromLatin1(AcTemplate::kExprOpen) +
              QString::fromLatin1(AcTemplate::kEachPrefix).trimmed(),
          QString::fromLatin1(AcTemplate::kEachClose),
          QString::fromLatin1(AcTemplate::kExprOpen) + QString::fromLatin1(AcBuiltin::kPrintLog) +
              QLatin1Char('('),
      };
    case JsonFile:
      return {
          QString::fromLatin1(AcKeyword::kTrue),
          QString::fromLatin1(AcKeyword::kFalse),
          QStringLiteral("null"),
      };
  }
  return {};
}

// ──────────────────────────────────────────────────────────────
//  QCompleter 工厂
// ──────────────────────────────────────────────────────────────

QCompleter *GuessCode::createCompleter(FileType type, QObject *parent) {
  QStringList words = getAllCompletions(type);
  if (words.isEmpty()) return nullptr;

  auto *completer = new QCompleter(words, parent);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  completer->setFilterMode(Qt::MatchContains);  // 包含匹配，更灵活

  // 弹出框样式：无圆角、紧凑间距
  // 方案：创建自定义 QListView，设置 Fusion 风格 + FramelessWindowHint，
  //       从根本上避免 Windows DWM 绘制圆角
  auto *popupView = new QListView();
  popupView->setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
  popupView->setSelectionBehavior(QAbstractItemView::SelectRows);
  popupView->setSelectionMode(QAbstractItemView::SingleSelection);
  popupView->setFocusPolicy(Qt::NoFocus);
  popupView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  // 设置垂直滚动条但只在需要时显示
  popupView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  QStyle *fusionStyle = QStyleFactory::create(QStringLiteral("Fusion"));
  if (fusionStyle) popupView->setStyle(fusionStyle);
  popupView->setStyleSheet(AuiStyle::popupListStyleSheet());

  completer->setPopup(popupView);
  return completer;
}

// ──────────────────────────────────────────────────────────────
//  从验证模式创建补全器
// ──────────────────────────────────────────────────────────────

QCompleter *GuessCode::createCompleter(int validationMode, QObject *parent) {
  // validationMode 对应 CodeEditor::ValidationMode 枚举值
  FileType ft = AcFile;
  switch (validationMode) {
    case 1:  // CodeEditor::JsonValidation
      ft = JsonFile;
      break;
    case 2:  // CodeEditor::TemplateValidation
      ft = TplFile;
      break;
    default:
      ft = AcFile;
      break;
  }
  return createCompleter(ft, parent);
}

// ──────────────────────────────────────────────────────────────
//  验证模式 → 文件类型
// ──────────────────────────────────────────────────────────────

GuessCode::FileType GuessCode::validationModeToFileType(int validationMode) {
  switch (validationMode) {
    case 1:  // CodeEditor::JsonValidation
      return JsonFile;
    case 2:  // CodeEditor::TemplateValidation
      return TplFile;
    default:
      return AcFile;
  }
}

// ──────────────────────────────────────────────────────────────
//  提取 let 变量
// ──────────────────────────────────────────────────────────────

QStringList GuessCode::extractLetVariables(const QString &text, int cursorLine) {
  QStringList vars;
  // 匹配 "let 变量名" 或 "let 变量名 = ..."
  static const QRegularExpression re(QStringLiteral("\\b") + QString::fromLatin1(AcKeyword::kLet) +
                                     QStringLiteral("\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\b"));
  QRegularExpressionMatchIterator it = re.globalMatch(text);
  while (it.hasNext()) {
    QRegularExpressionMatch m = it.next();
    if (cursorLine >= 0) {
      int defLine = text.left(m.capturedStart()).count('\n') + 1;
      if (defLine >= cursorLine) continue;
    }
    vars << m.captured(1);
  }
  vars.removeDuplicates();
  return vars;
}

// ──────────────────────────────────────────────────────────────
//  提取用户自定义函数名
// ──────────────────────────────────────────────────────────────

QStringList GuessCode::extractUserFunctions(const QString &text, int cursorLine) {
  QStringList funcs;
  AcLexer lexer;
  QString error;
  QVector<Token> tokens = lexer.tokenize(text, error);
  if (!error.isEmpty()) return funcs;

  for (int i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == TOK_CLASS) {
      // 跳过整个类定义体（含嵌套大括号），避免收集类方法
      while (i < tokens.size() && tokens[i].type != TOK_LBRACE) ++i;
      int braceDepth = 0;
      while (i < tokens.size()) {
        if (tokens[i].type == TOK_LBRACE)
          ++braceDepth;
        else if (tokens[i].type == TOK_RBRACE) {
          --braceDepth;
          if (braceDepth == 0) break;
        }
        ++i;
      }
      continue;
    }

    if (tokens[i].type == TOK_FUNCTION && i + 1 < tokens.size() &&
        tokens[i + 1].type == TOK_IDENT) {
      int defLine = tokens[i].line;
      if (cursorLine < 0 || defLine < cursorLine) {
        funcs << tokens[i + 1].text + QStringLiteral("()");
      }
    }
  }
  funcs.removeDuplicates();
  return funcs;
}

// ──────────────────────────────────────────────────────────────
//  提取用户自定义类名
// ──────────────────────────────────────────────────────────────

QStringList GuessCode::extractUserClasses(const QString &text, int cursorLine) {
  QStringList classes;
  AcLexer lexer;
  QString error;
  QVector<Token> tokens = lexer.tokenize(text, error);
  if (!error.isEmpty()) return classes;

  for (int i = 0; i < tokens.size() - 1; ++i) {
    if (tokens[i].type == TOK_CLASS && tokens[i + 1].type == TOK_IDENT) {
      int defLine = tokens[i].line;
      if (cursorLine < 0 || defLine < cursorLine) {
        classes << tokens[i + 1].text;
      }
    }
  }
  classes.removeDuplicates();
  return classes;
}

// ──────────────────────────────────────────────────────────────
//  提取类方法映射
// ──────────────────────────────────────────────────────────────

QHash<QString, QStringList> GuessCode::extractClassMethods(const QString &text) {
  QHash<QString, QStringList> result;
  AcLexer lexer;
  QString error;
  QVector<Token> tokens = lexer.tokenize(text, error);
  if (!error.isEmpty()) return result;

  for (int i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == TOK_CLASS && i + 1 < tokens.size() && tokens[i + 1].type == TOK_IDENT) {
      QString className = tokens[i + 1].text;
      QStringList methods;

      // 跳过 class 关键字和类名，定位到类体 TOK_LBRACE
      i += 2;
      while (i < tokens.size() && tokens[i].type != TOK_LBRACE) ++i;
      if (i >= tokens.size()) break;

      // 扫描类体，收集 TOK_FUNCTION → TOK_IDENT
      int braceDepth = 1;
      ++i;
      while (i < tokens.size() && braceDepth > 0) {
        if (tokens[i].type == TOK_LBRACE) {
          ++braceDepth;
        } else if (tokens[i].type == TOK_RBRACE) {
          --braceDepth;
          if (braceDepth == 0) break;
        } else if (tokens[i].type == TOK_FUNCTION && i + 1 < tokens.size() &&
                   tokens[i + 1].type == TOK_IDENT) {
          methods << tokens[i + 1].text + QStringLiteral("()");
        }
        ++i;
      }
      methods.removeDuplicates();
      if (!methods.isEmpty()) result[className] = methods;
    }
  }
  return result;
}

// ──────────────────────────────────────────────────────────────
//  提取变量类型映射
// ──────────────────────────────────────────────────────────────

QHash<QString, QString> GuessCode::extractVariableTypes(const QString &text) {
  QHash<QString, QString> result;
  AcLexer lexer;
  QString error;
  QVector<Token> tokens = lexer.tokenize(text, error);
  if (!error.isEmpty()) return result;

  // 匹配模式: let varName = new ClassName ( ... )
  for (int i = 0; i < tokens.size(); ++i) {
    if (tokens[i].type == TOK_LET && i + 1 < tokens.size() && tokens[i + 1].type == TOK_IDENT) {
      QString varName = tokens[i + 1].text;

      // 查找 = new ClassName
      int j = i + 2;
      while (j < tokens.size() && tokens[j].type != TOK_EQUALS && tokens[j].type != TOK_SEMI &&
             tokens[j].type != TOK_LBRACE && tokens[j].type != TOK_EOF) {
        ++j;
      }
      if (j >= tokens.size() || tokens[j].type != TOK_EQUALS) continue;

      // 跳过 = 和空格/new
      ++j;
      while (j < tokens.size() && tokens[j].type != TOK_NEW && tokens[j].type != TOK_SEMI &&
             tokens[j].type != TOK_LBRACE && tokens[j].type != TOK_EOF) {
        ++j;
      }
      if (j >= tokens.size() || tokens[j].type != TOK_NEW) continue;

      // new 后面的 TOK_IDENT 就是类型名
      if (j + 1 < tokens.size() && tokens[j + 1].type == TOK_IDENT) {
        result[varName] = tokens[j + 1].text;
      }
    }
  }
  return result;
}