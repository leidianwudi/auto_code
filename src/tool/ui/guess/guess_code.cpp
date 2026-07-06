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

// ──────────────────────────────────────────────────────────────
//  补全词库
// ──────────────────────────────────────────────────────────────

QStringList GuessCode::getAllCompletions(FileType type) {
  switch (type) {
    case AcFile: {
      // 关键字和内置函数由 AcKeyword::kAll / AcBuiltin::kAll 统一定义
      // 原生类（如 DB）仅在 new 后出现，由 showCompleter() 动态添加
      return AcKeyword::kAll + AcBuiltin::kAll;
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
          QString::fromLatin1(AcTemplate::kExprOpen) + QString::fromLatin1(AcBuiltin::kPrint) +
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

QStringList GuessCode::extractLetVariables(const QString &text) {
  QStringList vars;
  // 匹配 "let 变量名" 或 "let 变量名 = ..."
  static const QRegularExpression re(QStringLiteral("\\b") + QString::fromLatin1(AcKeyword::kLet) +
                                     QStringLiteral("\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\b"));
  QRegularExpressionMatchIterator it = re.globalMatch(text);
  while (it.hasNext()) {
    QRegularExpressionMatch m = it.next();
    vars << m.captured(1);
  }
  vars.removeDuplicates();
  return vars;
}