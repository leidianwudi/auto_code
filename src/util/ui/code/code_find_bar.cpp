/**
 * @file code_find_bar.cpp
 * @brief 代码查找/替换栏控件实现
 */

#include "code_find_bar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include "code_editor.h"
#include "src/util/ui/component/aui_style.h"

// ══════════════════════════════════════════════════════════════════════════════
//  自绘箭头图标（确保 ▶ 和 ▼ 视觉大小一致）
// ══════════════════════════════════════════════════════════════════════════════

static QIcon createArrowIcon(Qt::ArrowType type) {
  int sz = 13;  // 图标像素尺寸
  QPixmap pix(sz, sz);
  pix.fill(Qt::transparent);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);
  p.setBrush(QColor(80, 80, 80));  // 深灰色箭头
  p.setPen(Qt::NoPen);

  if (type == Qt::RightArrow) {
    // ▶ 右三角：顶点在右，左上和左下为底边
    QPolygon tri;
    tri << QPoint(2, 1) << QPoint(sz - 2, sz / 2) << QPoint(2, sz - 1);
    p.drawPolygon(tri);
  } else {
    // ▼ 下三角：顶点在下，左上和右上为底边
    QPolygon tri;
    tri << QPoint(1, 2) << QPoint(sz - 1, 2) << QPoint(sz / 2, sz - 2);
    p.drawPolygon(tri);
  }
  return QIcon(pix);
}

// ══════════════════════════════════════════════════════════════════════════════
//  构造
// ══════════════════════════════════════════════════════════════════════════════

CodeFindBar::CodeFindBar(QPlainTextEdit *editor, QWidget *parent)
    : QWidget(parent), m_editor(editor) {
  setupUI();
  hide();
}

// ══════════════════════════════════════════════════════════════════════════════
//  界面布局
// ══════════════════════════════════════════════════════════════════════════════

void CodeFindBar::setupUI() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(4, 2, 4, 2);
  mainLayout->setSpacing(2);

  // ── 第一行：查找输入 + 导航按钮 + 选项 ──
  auto *findRow = new QHBoxLayout;
  findRow->setContentsMargins(0, 0, 0, 0);
  findRow->setSpacing(4);

  // 查找输入框
  m_findEdit = new QLineEdit;
  m_findEdit->setPlaceholderText(QStringLiteral("查找"));
  m_findEdit->setMinimumWidth(180);
  m_findEdit->setMaximumWidth(300);
  connect(m_findEdit, &QLineEdit::textChanged, this, &CodeFindBar::onFindTextChanged);
  connect(m_findEdit, &QLineEdit::returnPressed, this, &CodeFindBar::findNext);

  // 匹配计数标签
  m_matchLabel = new QLabel;
  m_matchLabel->setMinimumWidth(50);
  m_matchLabel->setAlignment(Qt::AlignCenter);

  // ── 查找导航按钮：上一个(▲) / 下一个(▼) ──
  m_findPrevBtn = new QPushButton(QStringLiteral("\u25B2"));  // ▲ 上一个
  m_findPrevBtn->setObjectName(QStringLiteral("findNavBtn"));
  m_findPrevBtn->setFixedSize(26, 26);
  m_findPrevBtn->setToolTip(QStringLiteral("上一个 (Shift+Enter)"));
  m_findNextBtn = new QPushButton(QStringLiteral("\u25BC"));  // ▼ 下一个
  m_findNextBtn->setObjectName(QStringLiteral("findNavBtn"));
  m_findNextBtn->setFixedSize(26, 26);
  m_findNextBtn->setToolTip(QStringLiteral("下一个 (Enter)"));
  connect(m_findPrevBtn, &QPushButton::clicked, this, &CodeFindBar::findPrev);
  connect(m_findNextBtn, &QPushButton::clicked, this, &CodeFindBar::findNext);

  // 选项复选框
  m_caseCheck = new QCheckBox(QStringLiteral("Aa"));
  m_caseCheck->setToolTip(QStringLiteral("区分大小写"));
  m_caseCheck->setFixedSize(36, 24);
  m_wordCheck = new QCheckBox(QStringLiteral("\\b"));
  m_wordCheck->setToolTip(QStringLiteral("全词匹配"));
  m_wordCheck->setFixedSize(36, 24);
  m_regexCheck = new QCheckBox(QStringLiteral(".*"));
  m_regexCheck->setToolTip(QStringLiteral("正则表达式"));
  m_regexCheck->setFixedSize(36, 24);
  connect(m_caseCheck, &QCheckBox::toggled, this, &CodeFindBar::onFindTextChanged);
  connect(m_wordCheck, &QCheckBox::toggled, this, &CodeFindBar::onFindTextChanged);
  connect(m_regexCheck, &QCheckBox::toggled, this, &CodeFindBar::onFindTextChanged);

  // ── 展开/收起替换区域按钮：▶ 收起状态 / ▼ 展开状态（最左侧按钮）──
  // 使用自绘图标而非 Unicode 字符，确保两种状态下箭头大小一致
  m_toggleReplaceBtn = new QPushButton;
  m_toggleReplaceBtn->setObjectName(QStringLiteral("toggleReplaceBtn"));
  m_toggleReplaceBtn->setFixedSize(26, 26);
  m_toggleReplaceBtn->setToolTip(QStringLiteral("替换"));
  m_toggleReplaceBtn->setIconSize(QSize(14, 14));
  m_toggleReplaceBtn->setIcon(createArrowIcon(Qt::RightArrow));  // ▶ 收起=仅查找
  connect(m_toggleReplaceBtn, &QPushButton::clicked, this, [this]() {
    m_replaceVisible = !m_replaceVisible;
    m_replaceEdit->setVisible(m_replaceVisible);
    m_replaceBtn->setVisible(m_replaceVisible);
    m_replaceAllBtn->setVisible(m_replaceVisible);
    // 切换箭头方向：▶ 收起=仅查找 / ▼ 展开=替换模式
    m_toggleReplaceBtn->setIcon(m_replaceVisible ? createArrowIcon(Qt::DownArrow)     // ▼ 展开
                                                 : createArrowIcon(Qt::RightArrow));  // ▶ 收起

    m_toggleReplaceBtn->setToolTip(m_replaceVisible ? QStringLiteral("收起")
                                                    : QStringLiteral("替换"));
    // 显示替换区域时聚焦替换输入框
    if (m_replaceVisible) m_replaceEdit->setFocus();
    // 通知编辑器更新查找栏高度和视口边距
    emit layoutChanged();
  });

  findRow->addWidget(m_toggleReplaceBtn);
  findRow->addWidget(m_findEdit, 1);
  findRow->addWidget(m_matchLabel);
  findRow->addWidget(m_findPrevBtn);
  findRow->addWidget(m_findNextBtn);
  findRow->addWidget(m_caseCheck);
  findRow->addWidget(m_wordCheck);
  findRow->addWidget(m_regexCheck);

  // ── 关闭按钮（✕）──
  m_closeBtn = new QPushButton(QStringLiteral("\u2715"));  // ✕
  m_closeBtn->setObjectName(QStringLiteral("findCloseBtn"));
  m_closeBtn->setFixedSize(22, 22);
  m_closeBtn->setToolTip(QStringLiteral("关闭 (Esc)"));
  m_closeBtn->setStyleSheet(
      QStringLiteral("QPushButton#findCloseBtn { font-size: 13px; border: none; }"
                     "QPushButton#findCloseBtn:hover { background: #e0e0e0; }"));
  connect(m_closeBtn, &QPushButton::clicked, this, &CodeFindBar::hideFindBar);
  findRow->addWidget(m_closeBtn);

  // ── 第二行：替换输入 + 替换按钮（默认隐藏）──
  auto *replaceRow = new QHBoxLayout;
  replaceRow->setContentsMargins(28, 0, 0, 0);  // 左侧对齐查找输入框
  replaceRow->setSpacing(4);

  m_replaceEdit = new QLineEdit;
  m_replaceEdit->setPlaceholderText(QStringLiteral("替换"));
  m_replaceEdit->setMinimumWidth(180);
  m_replaceEdit->setMaximumWidth(300);
  m_replaceEdit->setVisible(false);

  m_replaceBtn = new QPushButton(QStringLiteral("替换"));
  m_replaceBtn->setFixedHeight(24);
  m_replaceBtn->setVisible(false);
  m_replaceBtn->setToolTip(QStringLiteral("替换当前匹配项"));
  connect(m_replaceBtn, &QPushButton::clicked, this, &CodeFindBar::replaceCurrent);

  m_replaceAllBtn = new QPushButton(QStringLiteral("全部替换"));
  m_replaceAllBtn->setFixedHeight(24);
  m_replaceAllBtn->setVisible(false);
  m_replaceAllBtn->setToolTip(QStringLiteral("替换所有匹配项"));
  connect(m_replaceAllBtn, &QPushButton::clicked, this, &CodeFindBar::replaceAll);

  replaceRow->addWidget(m_replaceEdit, 1);
  replaceRow->addWidget(m_replaceBtn);
  replaceRow->addWidget(m_replaceAllBtn);

  mainLayout->addLayout(findRow);
  mainLayout->addLayout(replaceRow);

  // ── 整体样式 ──
  setStyleSheet(
      QStringLiteral("QCheckBox { spacing: 2px; font-size: 11px; }"
                     "QPushButton { font-size: 12px; }"
                     "QPushButton#findNavBtn { font-size: 14px; }"  // ▲▼ 查找导航按钮
                     "QLineEdit { padding: 2px 4px; }"));
}

// ══════════════════════════════════════════════════════════════════════════════
//  显示 / 隐藏
// ══════════════════════════════════════════════════════════════════════════════

void CodeFindBar::showFindBar() {
  m_intendedVisible = true;
  show();
  // 如果编辑器有选中文本，自动填入查找框
  if (m_editor) {
    QString selected = m_editor->textCursor().selectedText();
    if (!selected.isEmpty() && !selected.contains(QLatin1Char('\n'))) {
      m_findEdit->setText(selected);
    }
  }
  m_findEdit->setFocus();
  m_findEdit->selectAll();
  onFindTextChanged();
}

void CodeFindBar::hideFindBar() {
  m_intendedVisible = false;
  hide();
  // 隐藏时清除查找高亮
  applyFindHighlight({});
  if (m_editor) {
    m_editor->setFocus();
  }
  emit findBarClosed();
}

bool CodeFindBar::isFindBarVisible() const { return m_intendedVisible; }

void CodeFindBar::pauseVisible() {
  // 切换标签页时临时隐藏，不改变 m_intendedVisible
  if (m_intendedVisible) hide();
}

void CodeFindBar::resumeVisible() {
  // 切换回标签页时，如果之前是"应显示"则恢复
  if (m_intendedVisible) {
    show();
    onFindTextChanged();
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  查找逻辑
// ══════════════════════════════════════════════════════════════════════════════

void CodeFindBar::onFindTextChanged() {
  updateMatchCount();
  // 实时高亮所有匹配项（类似 VS Code 的查找高亮）
  if (m_editor && !findText().isEmpty()) {
    QList<QTextEdit::ExtraSelection> selections;
    // 当前光标选中的匹配项用深色高亮
    QTextCursor currentCursor = m_editor->textCursor();
    int selStart = currentCursor.selectionStart();
    int selEnd = currentCursor.selectionEnd();

    QTextDocument *doc = m_editor->document();
    QString pattern = findText();

    if (m_regexCheck->isChecked()) {
      QRegularExpression regex(pattern, m_caseCheck->isChecked()
                                            ? QRegularExpression::NoPatternOption
                                            : QRegularExpression::CaseInsensitiveOption);
      if (!regex.isValid()) {
        m_matchLabel->setText(QStringLiteral("正则错误"));
        applyFindHighlight({});
        return;
      }
      QRegularExpressionMatchIterator it = regex.globalMatch(doc->toPlainText());
      while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QTextCursor cursor(doc);
        cursor.setPosition(match.capturedStart());
        cursor.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection sel;
        sel.cursor = cursor;
        // 当前选中项用深橙色，其他用浅橙色
        bool isCurrent = (match.capturedStart() == selStart && match.capturedEnd() == selEnd);
        sel.format.setBackground(isCurrent ? AuiStyle::findCurrentMatchBackground()
                                           : AuiStyle::findMatchBackground());
        selections.append(sel);
      }
    } else {
      QTextCursor cursor(doc);
      while (!cursor.isNull()) {
        cursor = doc->find(pattern, cursor, findFlags());
        if (!cursor.isNull()) {
          QTextEdit::ExtraSelection sel;
          sel.cursor = cursor;
          // 当前选中项用深橙色，其他用浅橙色
          bool isCurrent = (cursor.selectionStart() == selStart && cursor.selectionEnd() == selEnd);
          sel.format.setBackground(isCurrent ? AuiStyle::findCurrentMatchBackground()
                                             : AuiStyle::findMatchBackground());
          selections.append(sel);
        }
      }
    }

    applyFindHighlight(selections);
  } else {
    applyFindHighlight({});
  }
}

// ══════════════════════════════════════════════════════════════════════════════
//  查找高亮合并（避免被 highlightCurrentLine 覆盖）
// ══════════════════════════════════════════════════════════════════════════════

void CodeFindBar::applyFindHighlight(const QList<QTextEdit::ExtraSelection> &selections) {
  if (!m_editor) return;
  // 将查找高亮写入 CodeEditor::m_findSelections，
  // 然后调用 highlightCurrentLine() 合并所有 ExtraSelection（行高亮+括号+错误+查找）
  auto *editor = static_cast<CodeEditor *>(parentWidget());
  if (editor) {
    editor->m_findSelections = selections;
    editor->highlightCurrentLine();
  }
}

void CodeFindBar::findNext() { find(1); }

void CodeFindBar::findPrev() { find(-1); }

void CodeFindBar::find(int direction) {
  if (!m_editor || findText().isEmpty()) return;

  QTextDocument *doc = m_editor->document();
  QTextCursor current = m_editor->textCursor();

  // 查找起始位置：向下从选区末尾，向上从选区开头
  QTextCursor searchCursor = current;
  if (direction > 0) {
    searchCursor.setPosition(current.selectionEnd());
  } else {
    searchCursor.setPosition(current.selectionStart());
  }

  QTextCursor found;
  if (m_regexCheck->isChecked()) {
    QRegularExpression regex(findText(), m_caseCheck->isChecked()
                                             ? QRegularExpression::NoPatternOption
                                             : QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) return;

    QString text = doc->toPlainText();
    if (direction > 0) {
      // 向下查找
      QRegularExpressionMatchIterator it = regex.globalMatch(text, searchCursor.position());
      if (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        found = QTextCursor(doc);
        found.setPosition(match.capturedStart());
        found.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
      } else {
        // 回绕到文档开头
        QRegularExpressionMatchIterator it2 = regex.globalMatch(text, 0);
        if (it2.hasNext()) {
          QRegularExpressionMatch match = it2.next();
          found = QTextCursor(doc);
          found.setPosition(match.capturedStart());
          found.setPosition(match.capturedEnd(), QTextCursor::KeepAnchor);
        }
      }
    } else {
      // 向上查找：找最后一个 position < searchCursor.position() 的匹配
      QRegularExpressionMatchIterator it = regex.globalMatch(text, 0);
      QRegularExpressionMatch lastMatch;
      bool hasMatch = false;
      while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (match.capturedStart() < searchCursor.position()) {
          lastMatch = match;
          hasMatch = true;
        } else {
          break;
        }
      }
      if (hasMatch) {
        found = QTextCursor(doc);
        found.setPosition(lastMatch.capturedStart());
        found.setPosition(lastMatch.capturedEnd(), QTextCursor::KeepAnchor);
      } else {
        // 回绕到文档末尾：找最后一个匹配
        QRegularExpressionMatchIterator it2 = regex.globalMatch(text, 0);
        while (it2.hasNext()) {
          lastMatch = it2.next();
          hasMatch = true;
        }
        if (hasMatch) {
          found = QTextCursor(doc);
          found.setPosition(lastMatch.capturedStart());
          found.setPosition(lastMatch.capturedEnd(), QTextCursor::KeepAnchor);
        }
      }
    }
  } else {
    // 构建查找标志：向上查找时加 FindBackward
    QTextDocument::FindFlags flags = findFlags();
    if (direction < 0) flags |= QTextDocument::FindBackward;
    found = doc->find(findText(), searchCursor, flags);
    if (found.isNull()) {
      // 回绕查找
      if (direction > 0) {
        found = doc->find(findText(), 0, flags);
      } else {
        // 回绕到文档末尾，从后向前查找
        QTextCursor endCursor(doc);
        endCursor.movePosition(QTextCursor::End);
        found = doc->find(findText(), endCursor, flags);
      }
    }
  }

  if (!found.isNull()) {
    m_editor->setTextCursor(found);
    m_editor->ensureCursorVisible();
    updateMatchCount();
  }
}

void CodeFindBar::updateMatchCount() {
  QString pattern = findText();
  if (pattern.isEmpty()) {
    m_matchLabel->clear();
    m_matchCount = -1;
    return;
  }

  if (!m_editor) return;
  QTextDocument *doc = m_editor->document();
  int count = 0;
  int currentIdx = 0;

  if (m_regexCheck->isChecked()) {
    QRegularExpression regex(pattern, m_caseCheck->isChecked()
                                          ? QRegularExpression::NoPatternOption
                                          : QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) {
      m_matchLabel->setText(QStringLiteral("正则错误"));
      return;
    }
    QRegularExpressionMatchIterator it = regex.globalMatch(doc->toPlainText());
    int cursorPos = m_editor->textCursor().selectionStart();
    while (it.hasNext()) {
      QRegularExpressionMatch match = it.next();
      if (match.capturedStart() < cursorPos) currentIdx = count;
      ++count;
    }
  } else {
    QTextCursor cursor(doc);
    int cursorPos = m_editor->textCursor().selectionStart();
    while (!cursor.isNull()) {
      cursor = doc->find(pattern, cursor, findFlags());
      if (!cursor.isNull()) {
        if (cursor.selectionStart() < cursorPos) currentIdx = count;
        ++count;
      }
    }
  }

  m_matchCount = count;
  m_currentMatchIndex = count > 0 ? currentIdx + 1 : 0;
  m_matchLabel->setText(count > 0 ? QStringLiteral("%1/%2").arg(m_currentMatchIndex).arg(count)
                                  : QStringLiteral("无匹配"));
}

QTextDocument::FindFlags CodeFindBar::findFlags() const {
  QTextDocument::FindFlags flags;
  if (m_caseCheck->isChecked()) flags |= QTextDocument::FindCaseSensitively;
  if (m_wordCheck->isChecked()) flags |= QTextDocument::FindWholeWords;
  return flags;
}

QString CodeFindBar::findText() const { return m_findEdit->text(); }

QString CodeFindBar::replaceText() const { return m_replaceEdit->text(); }

// ══════════════════════════════════════════════════════════════════════════════
//  替换逻辑
// ══════════════════════════════════════════════════════════════════════════════

void CodeFindBar::replaceCurrent() {
  if (!m_editor || findText().isEmpty()) return;

  QTextCursor cursor = m_editor->textCursor();
  QString selected = cursor.selectedText();

  // 如果当前选中的文本不匹配查找内容，先查找
  bool caseSensitive = m_caseCheck->isChecked();
  if (selected.isEmpty() || (caseSensitive && selected != findText()) ||
      (!caseSensitive && selected.toLower() != findText().toLower())) {
    findNext();
    cursor = m_editor->textCursor();
    selected = cursor.selectedText();
  }

  if (!selected.isEmpty()) {
    cursor.insertText(replaceText());
    findNext();
  }
}

void CodeFindBar::replaceAll() {
  if (!m_editor || findText().isEmpty()) return;

  QTextCursor cursor(m_editor->document());
  cursor.beginEditBlock();

  int count = 0;
  // 从文档开头查找所有匹配
  cursor.movePosition(QTextCursor::Start);

  if (m_regexCheck->isChecked()) {
    QRegularExpression regex(findText(), m_caseCheck->isChecked()
                                             ? QRegularExpression::NoPatternOption
                                             : QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) return;

    QString text = m_editor->document()->toPlainText();
    // 构建替换后的文本
    QString newText = text;
    newText.replace(regex, replaceText());
    cursor.select(QTextCursor::Document);
    cursor.insertText(newText);
    // 计算替换次数
    count = 0;
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext()) {
      it.next();
      ++count;
    }
  } else {
    QTextDocument::FindFlags flags = findFlags();
    while (true) {
      cursor = m_editor->document()->find(findText(), cursor, flags);
      if (cursor.isNull()) break;
      cursor.insertText(replaceText());
      ++count;
    }
  }

  cursor.endEditBlock();
  m_matchLabel->setText(count > 0 ? QStringLiteral("替换了 %1 处").arg(count)
                                  : QStringLiteral("无匹配"));
}

// ══════════════════════════════════════════════════════════════════════════════
//  键盘事件
// ══════════════════════════════════════════════════════════════════════════════

void CodeFindBar::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hideFindBar();
    return;
  }
  // Shift+Enter = 查找上一个
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (event->modifiers() & Qt::ShiftModifier) {
      findPrev();
    } else {
      findNext();
    }
    return;
  }
  QWidget::keyPressEvent(event);
}
