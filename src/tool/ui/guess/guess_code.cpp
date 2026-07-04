/**
 * @file guess_code.cpp
 * @brief 代码自动补全实现
 */

#include "guess_code.h"
#include <QCompleter>
#include <QListView>
#include <QStringListModel>
#include <QStyleFactory>

// ──────────────────────────────────────────────────────────────
//  补全词库
// ──────────────────────────────────────────────────────────────

QStringList GuessCode::getAllCompletions(FileType type) {
  switch (type) {
  case AcFile:
    return {
        // 关键字
        QStringLiteral("let"),
        QStringLiteral("main"),
        QStringLiteral("for"),
        QStringLiteral("in"),
        QStringLiteral("if"),
        QStringLiteral("else"),
        QStringLiteral("call"),
        QStringLiteral("return"),
        QStringLiteral("true"),
        QStringLiteral("false"),
        // 内置函数
        QStringLiteral("readJson"),
        QStringLiteral("merge"),
        QStringLiteral("basename"),
        QStringLiteral("render"),
        QStringLiteral("write"),
        QStringLiteral("print"),
        QStringLiteral("getCheckedFiles"),
    };
  case TplFile:
    return {
        QStringLiteral("${if "),    QStringLiteral("${else}"),
        QStringLiteral("${/if}"),   QStringLiteral("${each "),
        QStringLiteral("${/each}"), QStringLiteral("${print("),
    };
  case JsonFile:
    return {
        QStringLiteral("true"),
        QStringLiteral("false"),
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
  if (words.isEmpty())
    return nullptr;

  auto *completer = new QCompleter(words, parent);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setCompletionMode(QCompleter::PopupCompletion);
  completer->setFilterMode(Qt::MatchContains); // 包含匹配，更灵活

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
  if (fusionStyle)
    popupView->setStyle(fusionStyle);
  popupView->setStyleSheet(QStringLiteral("QListView {"
                                          "  border: 1px solid #999;"
                                          "  border-radius: 0px;"
                                          "  padding: 0px;"
                                          "  margin: 0px;"
                                          "  background: white;"
                                          "}"
                                          "QListView::item {"
                                          "  padding: 1px 4px;"
                                          "  min-height: 18px;"
                                          "}"));

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
  case 1: // CodeEditor::JsonValidation
    ft = JsonFile;
    break;
  case 2: // CodeEditor::TemplateValidation
    ft = TplFile;
    break;
  default:
    ft = AcFile;
    break;
  }
  return createCompleter(ft, parent);
}