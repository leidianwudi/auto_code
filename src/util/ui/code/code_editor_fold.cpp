/**
 * @file code_editor_fold.cpp
 * @brief 代码编辑器折叠与行号区实现（从 code_editor.cpp 拆分）
 *
 * 包含以下功能：
 * - 行号区宽度与滚动（updateLineNumberAreaWidth / updateLineNumberArea / lineNumberAreaWidth）
 * - 代码折叠区间计算与折叠态应用（computeFoldRanges / applyFoldVisibility / toggleFold ...）
 * - 行号区绘制（lineNumberAreaPaintEvent，含折叠标记与断点圆点）
 * - 行号区事件（LineNumberArea 鼠标/滚轮/右键，含折叠点击与断点切换）
 */

#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include "code_editor.h"
#include "code_find_bar.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_style.h"

// ──────────────────────────────────────────────────────────────
//  行号区域
// ──────────────────────────────────────────────────────────────

void CodeEditor::updateLineNumberAreaWidth(int newBlockCount) {
  Q_UNUSED(newBlockCount);
  // 保留查找栏的顶部边距
  int topMargin =
      (m_findBar && m_findBar->isFindBarVisible()) ? m_findBar->sizeHint().height() + 4 : 0;
  setViewportMargins(lineNumberAreaWidth(), topMargin, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy) {
  if (dy)
    m_lineNumberArea->scroll(0, dy);
  else
    m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

  if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth(0);
}

int CodeEditor::lineNumberAreaWidth() const {
  int digits = 1;
  int max = qMax(1, blockCount());
  while (max >= 10) {
    max /= 10;
    ++digits;
  }

  // 左侧预留断点圆点 gutter；行号右对齐后仍需 6px 尾随空隙，避免与折叠标记相碰
  // 行号区右侧预留折叠标记 gutter，避免折叠图标与行号重叠
  int space = kBreakpointGutterWidth + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
  space += 6;
  space += kFoldMarkerWidth;
  return space;
}

// ════════════════════════════════════════════════════════════
//  代码折叠（VSCode 风格）
// ════════════════════════════════════════════════════════════

/// 计算一行缩进量（空格/制表，制表按 tabWidth 计）
static int countIndent(const QString &line, int tabWidth) {
  int indent = 0;
  for (int i = 0; i < line.size(); ++i) {
    const QChar &c = line[i];
    if (c == QLatin1Char(' '))
      ++indent;
    else if (c == QLatin1Char('\t'))
      indent += tabWidth;
    else
      break;
  }
  return indent;
}

void CodeEditor::computeFoldRanges() {
  m_foldRanges.clear();
  m_foldValid = true;
  if (m_validationMode == NoValidation) return;

  QStringList lines = cachedText().split(QLatin1Char('\n'));
  const int count = lines.size();
  if (count < 2) return;

  const int tabWidth =
      qMax(1, qRound(tabStopDistance() / fontMetrics().horizontalAdvance(QLatin1Char(' '))));

  // 计算每行缩进；空行/纯空白行标记为 -1（折叠计算时跳过，不改变层级、不作为回退点）
  QVector<int> indent(count);
  for (int i = 0; i < count; ++i) {
    if (lines[i].trimmed().isEmpty())
      indent[i] = -1;
    else
      indent[i] = countIndent(lines[i], tabWidth);
  }

  // 折叠起始块 = 「块头」行（如 function / if / 开 `{`）：其下一个非空行缩进更深。
  // 折叠区间从块头延伸到「第一个缩进 <= 块头缩进的非空行」之前结束。
  // 关键：回退判定用 <=（父级/同级都终止），空行跳过不结束块，
  // 避免内层块错误地把后面的所有代码一并收起。
  for (int i = 0; i < count - 1; ++i) {
    if (indent[i] < 0) continue;
    // 找下一个非空行作为「块头是否有更深子级」的依据
    int j = i + 1;
    while (j < count && indent[j] < 0) ++j;
    if (j >= count) continue;
    if (indent[j] > indent[i]) {
      const int base = indent[i];
      int end = i;  // 折叠结束（隐藏 i+1..end）
      for (int m = j; m < count; ++m) {
        if (indent[m] < 0) continue;   // 空行不结束块
        if (indent[m] <= base) break;  // 回到父级/同级缩进 → 块结束
        end = m;
      }
      if (end > i) m_foldRanges.insert(i, end);
    }
  }
}

void CodeEditor::applyFoldVisibility() {
  if (m_applyingFold) return;  // 防重入：visible 变化再触 contentsChange → timer → 再进来
  m_applyingFold = true;

  // 锚块定位：记录折叠前「视口顶部可见块」在文档坐标系中的 top 及其滚动值。
  // 折叠后重新计算滚动值，使该块仍停留在屏幕同一位置——这样即使滚到最底部，
  // 被点击/折叠的标记行也不会相对鼠标位置上蹿下跳（类似 Trae IDE）。
  const double s0 = verticalScrollBar()->value();
  int anchorBlock = -1;
  double docTop0 = 0;
  {
    QTextBlock tb = firstVisibleBlock();
    if (tb.isValid()) {
      anchorBlock = tb.blockNumber();
      docTop0 = blockBoundingGeometry(tb).top();
    }
  }

  // 先全部恢复可见，再按折叠态隐藏
  QTextBlock block = document()->firstBlock();
  for (int n = 0; block.isValid(); ++n, block = block.next()) {
    bool hidden = false;
    for (auto it = m_collapsedStarts.begin(); it != m_collapsedStarts.end(); ++it) {
      if (n > *it && n <= m_foldRanges.value(*it, *it)) {
        hidden = true;
        break;
      }
    }
    if (block.isVisible() == hidden) block.setVisible(!hidden);
  }
  m_applyingFold = false;

  // 触发布局重算（高度 0 + 位置重新排布）
  document()->markContentsDirty(0, document()->characterCount());

  // 折叠后目标滚动值；默认退回仅夹取原滚动值
  int targetScroll = qBound(verticalScrollBar()->minimum(), qRound(s0),
                            verticalScrollBar()->maximum());
  // 锚块回到屏幕原位置：scroll1 = scroll0 + (docTop1 - docTop0)
  if (anchorBlock >= 0) {
    QTextBlock t1 = document()->findBlockByNumber(anchorBlock);
    // 若锚块因折叠被隐藏，向上找最近的可见块作为新锚
    while (t1.isValid() && !t1.isVisible()) t1 = t1.previous();
    if (t1.isValid()) {
      const double docTop1 = blockBoundingGeometry(t1).top();
      targetScroll = qRound(s0 + (docTop1 - docTop0));
      targetScroll = qBound(verticalScrollBar()->minimum(), targetScroll,
                            verticalScrollBar()->maximum());
    }
  }
  verticalScrollBar()->setValue(targetScroll);
  viewport()->update();
  m_lineNumberArea->update();
}

void CodeEditor::rebuildFold() {
  // 重新按当前文本计算可折叠区间，并应用已有折叠态
  computeFoldRanges();
  applyFoldVisibility();
}

void CodeEditor::restoreFoldState(const QSet<int> &blocks) {
  // 供会话还原调用：先按当前文本重算可折叠区间（光标/折叠生效前提），
  // 再覆盖持久化的折叠块号并应用。若验证模式为 NoValidation（如 .jsonvue 的
  // codeEditor），computeFoldRanges 不会产生折叠区间，这里自然无折叠，符合预期。
  computeFoldRanges();
  m_collapsedStarts = blocks;
  applyFoldVisibility();
}

bool CodeEditor::isFoldStart(int block) const { return m_foldRanges.contains(block); }

void CodeEditor::toggleFold(int block) {
  if (!m_foldRanges.contains(block)) return;
  if (m_collapsedStarts.contains(block))
    m_collapsedStarts.remove(block);
  else
    m_collapsedStarts.insert(block);
  applyFoldVisibility();
}

QRect CodeEditor::foldMarkerRect(int block) const {
  // 折叠标记位于行号区右侧（紧贴代码左侧）。坐标用行号区局部坐标系，
  // y 取该 block 在视口中的 top（行号区与视口同高、同步滚动，y 一致）。
  QTextBlock b = document()->findBlockByNumber(block);
  if (!b.isValid() || !b.isVisible()) return QRect();
  QRectF g = blockBoundingGeometry(b).translated(contentOffset());
  if (g.height() <= 0) return QRect();
  const int w = m_lineNumberArea ? m_lineNumberArea->width() : 0;
  const int x = qMax(0, w - kFoldMarkerWidth);  // 紧贴行号区右缘（代码左侧），减少右缘留白、图标更靠近代码
  return QRect(x, qRound(g.top()), kFoldMarkerWidth, qRound(g.height()));
}

void CodeEditor::setFoldHoverBlock(int block) {
  if (m_foldHoverBlock == block) return;
  m_foldHoverBlock = block;
  // 整段重绘行号区，刷新折叠标记的悬停高亮
  if (m_lineNumberArea) m_lineNumberArea->update();
}

void CodeEditor::setFoldAreaActive(bool active) {
  if (m_foldShowAll == active) return;
  m_foldShowAll = active;
  m_foldHoverBlock = -1;  // 清除悬停高亮
  // 整段重绘行号区，使所有可折叠标记随灰色区进入/离开显示或隐藏
  if (m_lineNumberArea) m_lineNumberArea->update();
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event, const QRect &area) {
  QPainter painter(m_lineNumberArea);
  painter.fillRect(event->rect(), AuiStyle::lineNumberBackground());

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      int line = blockNumber + 1;
      QString number = QString::number(line);

      // 错误行号显示红色，否则用默认颜色
      if (m_errorLines.contains(line)) {
        painter.setPen(AuiStyle::errorTextColor());
      } else {
        painter.setPen(AuiStyle::lineNumberTextColor());
      }

      painter.drawText(kBreakpointGutterWidth, top,
                       m_lineNumberArea->width() - kFoldMarkerWidth - kBreakpointGutterWidth,
                       fontMetrics().height(), Qt::AlignRight, number);

      // 绘制断点圆点（生效=实心红圆，失效=空心红圆，位于行号左侧的专属 gutter）
      if (m_breakpoints.contains(line)) {
        int dotR = 5;
        int cx = kBreakpointGutterWidth / 2;
        int cy = top + fontMetrics().height() / 2;
        QColor red(0xf4, 0x47, 0x47);
        if (m_breakpoints.value(line)) {
          // 生效：实心红圆
          painter.setPen(Qt::NoPen);
          painter.setBrush(red);
          painter.drawEllipse(QPoint(cx, cy), dotR, dotR);
        } else {
          // 失效：空心红圆（仅描边）
          painter.setPen(QPen(red, 1.5));
          painter.setBrush(Qt::NoBrush);
          painter.drawEllipse(QPoint(cx, cy), dotR, dotR);
        }
      }

      // ── 折叠标记 `>`：位于行号区右侧（紧贴代码）。折叠态常显；未折叠时鼠标悬停该行，或在灰色行号区（任意行）时才显示全部 ──
      if (isFoldStart(blockNumber) &&
          (isCollapsed(blockNumber) || m_foldShowAll || m_foldHoverBlock == blockNumber)) {
        QRect mr = foldMarkerRect(blockNumber);
        if (mr.isValid()) {
          // 自绘 v 形箭头（非文字），save/restore 隔离画笔状态
          painter.save();
          // 未折叠向下「v」，折叠后向右「▸」。样式由 AuiStyle 统一管理。
          // 两方向开口张角一致（均为 100°，half=50°→开口跨度≈9.6），
          // 保证折叠/展开图标开口同样"开"，且大小一致。
          const QColor arrowColor = m_foldHoverBlock == blockNumber
                                        ? AuiStyle::errorTextColor()
                                        : AuiStyle::secondaryTextColor();
          const qreal foldAngle = 100.0;
          AuiStyle::drawFoldArrow(painter, mr.center(), !isCollapsed(blockNumber), arrowColor, 6.5, foldAngle);
          painter.restore();
        }
      }
    }

    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
}

// ── 行号区点击：切换断点（折叠标记已移入代码区右缘，行号区不再处理折叠）──
void LineNumberArea::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    // 折叠标记（行号区右侧）优先：命中该行的 `>` 图标则折叠/展开
    const int line = m_codeEditor->lineAtY(event->pos().y());
    const int block = line - 1;
    if (line > 0 && m_codeEditor->isFoldStart(block) &&
        m_codeEditor->foldMarkerRect(block).contains(event->pos())) {
      m_codeEditor->toggleFold(block);
      event->accept();
      return;
    }
    m_codeEditor->toggleBreakpointAtY(event->pos().y());
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

// ── 行号区鼠标移动：更新折叠标记悬停高亮 ──
void LineNumberArea::mouseMoveEvent(QMouseEvent *event) {
  QWidget::mouseMoveEvent(event);
  // 鼠标进入灰色行号区：显示全部可折叠图标
  m_codeEditor->setFoldAreaActive(true);
  const int line = m_codeEditor->lineAtY(event->pos().y());
  const int block = line - 1;
  // 鼠标在该灰色区域任意列，只要所在行可折叠就显示图标
  m_codeEditor->setFoldHoverBlock((line > 0 && m_codeEditor->isFoldStart(block)) ? block : -1);
}

void LineNumberArea::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  m_codeEditor->setFoldAreaActive(false);
  m_codeEditor->setFoldHoverBlock(-1);
}

// ── 行号区滚轮：转发到编辑器视口，让鼠标在行号区也能滚动代码 ──
void LineNumberArea::wheelEvent(QWheelEvent *event) {
  QCoreApplication::sendEvent(m_codeEditor->viewport(), event);
  event->accept();
}

// ── 根据行号区 y 坐标返回所在行（1-based），无命中返回 0 ──
int CodeEditor::lineAtY(int y) const {
  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + qRound(blockBoundingRect(block).height());

  while (block.isValid()) {
    if (y >= top && y < bottom) return blockNumber + 1;
    block = block.next();
    top = bottom;
    bottom = top + qRound(blockBoundingRect(block).height());
    ++blockNumber;
  }
  return 0;
}

// ── 根据行号区 y 坐标切换断点 ──
void CodeEditor::toggleBreakpointAtY(int y) {
  const int line = lineAtY(y);
  if (line > 0) toggleBreakpoint(line - 1);
}

// ── 行号区右键：添加/移除/启用/禁用断点（模仿 VSCode）──
void LineNumberArea::contextMenuEvent(QContextMenuEvent *event) {
  // 仅 .ac 可调试文件支持断点操作
  if (!m_codeEditor->isDebuggableFile()) {
    event->ignore();
    return;
  }
  const int line = m_codeEditor->lineAtY(event->pos().y());
  if (line <= 0) {
    event->ignore();
    return;
  }
  const bool hasBp = m_codeEditor->hasBreakpoint(line);
  const bool bpEnabled = hasBp && m_codeEditor->isBreakpointEnabled(line);

  QMenu menu(this);
  QAction *toggleAction =
      menu.addAction(hasBp ? QString::fromUtf8(CodeConstants::UiText::kRemoveBreakpoint)
                           : QStringLiteral("添加断点"));
  connect(toggleAction, &QAction::triggered, this,
          [this, line]() { m_codeEditor->toggleBreakpoint(line - 1); });

  if (hasBp) {
    QAction *enableAction =
        menu.addAction(bpEnabled ? QString::fromUtf8(CodeConstants::UiText::kDisableBreakpoint)
                                 : QString::fromUtf8(CodeConstants::UiText::kEnableBreakpoint));
    connect(enableAction, &QAction::triggered, this,
            [this, line, bpEnabled]() { m_codeEditor->setBreakpointEnabled(line, !bpEnabled); });
  }

  if (!m_codeEditor->breakpoints().isEmpty()) {
    QAction *removeAllAction = menu.addAction(QStringLiteral("移除断点..."));
    connect(removeAllAction, &QAction::triggered, m_codeEditor, &CodeEditor::clearBreakpoints);
  }

  menu.exec(event->globalPos());
  event->accept();
}
