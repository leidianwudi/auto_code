/**
 * @file code_editor.cpp
 * @brief 代码编辑器控件实现（重构后）
 */

#include "code_editor.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMenu>
#include <QPainter>
#include <QPolygon>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QWheelEvent>
#include <QStringListModel>
#include <QToolTip>
#include <QVBoxLayout>

#include "code_find_bar.h"
#include "src/engine/ac_language.h"
#include "src/engine/json_validator.h"
#include "src/engine/script/ac_validator.h"
#include "src/engine/tpl/tpl_validator.h"
#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/code/format_code.h"
#include "src/util/ui/component/aui_error_tool_tip.h"
#include "src/util/ui/component/aui_style.h"
#include "src/util/ui/highlighter/light_ac.h"
#include "src/util/ui/highlighter/light_json.h"
#include "src/util/ui/highlighter/light_tpl.h"
#include "src/util/ui/highlighter/light_ts.h"
#include "src/util/ui/setting_store.h"

/**
 * @class FixedLineHeightLayout
 * @brief 统一行高的文档布局（消除中文输入导致的行高“抖动”）
 *
 * 背景：QPlainTextEdit 的块高度由 QTextLayout 按行内所有字体（含中文 fallback
 * 字体）的度量计算，且 QPlainTextDocumentLayout 忽略块级 BlockLineHeight。
 * 等宽字体（Consolas/Courier New 等）不含中文字形，输入中文时触发字体 fallback
 * （如微软雅黑），其 ascent/descent 与主字体不同，使行高随内容变化。
 *
 * 方案：覆写 blockBoundingRect 强制所有块等高（末块保留原生底部边距），
 * 实现 VSCode 式的统一行高，保证输入中文前后行高一致。
 */
class FixedLineHeightLayout : public QPlainTextDocumentLayout {
public:
  explicit FixedLineHeightLayout(QTextDocument *doc) : QPlainTextDocumentLayout(doc) {}

  /// 设置统一行高（非正数表示不启用）
  void setFixedLineHeight(qreal h) {
    if (qFuzzyCompare(m_h, h)) return;
    m_h = h;
    // 强制重新布局，使新行高立即生效
    document()->markContentsDirty(0, document()->characterCount());
  }

  QRectF blockBoundingRect(const QTextBlock &block) const override {
    // 被折叠隐藏的块返回 0 高度，实现"整行消失"效果（配合 QTextBlock::setVisible(false)）
    if (!block.isVisible()) {
      return QRectF(0, 0, 0, 0);
    }
    QRectF r = QPlainTextDocumentLayout::blockBoundingRect(block);
    if (m_h > 0) {
      r.setHeight(m_h);
      if (!block.next().isValid())  // 末块保留底部边距（与原生行为一致）
        r.setHeight(m_h + document()->documentMargin());
    }
    return r;
  }

private:
  qreal m_h = 0;  ///< 统一行高（像素）
};

// ──────────────────────────────────────────────────────────────
//  构造与初始化（精简后）
// ──────────────────────────────────────────────────────────────

/// 全局文件内容提供器（默认空；由主窗口注册后用于跨文件 import 解析实时缓冲）
CodeEditor::ContentProvider CodeEditor::s_contentProvider;

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent) {
  setMouseTracking(true);

  m_lineNumberArea = new LineNumberArea(this);
  // 行号区开启鼠标跟踪：未按下也能收到移动事件，用于折叠标记悬停高亮
  m_lineNumberArea->setMouseTracking(true);

  connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

  updateLineNumberAreaWidth(0);
  highlightCurrentLine();

  // 使用统一行高布局：修复等宽字体不含中文时，输入中文触发 fallback 导致的行高变化
  m_fixedLineHeightLayout = new FixedLineHeightLayout(document());
  document()->setDocumentLayout(m_fixedLineHeightLayout);

  // 去掉文档默认左边距（默认 4px），使代码文本紧贴行号区右缘，不与行号区留缝隙
  document()->setDocumentMargin(0);
  // 强制整篇重排，确保 setDocumentMargin(0) 后的块几何与坐标换算立即同步，
  // 避免点击空拍换算到错误字符（如跳到行尾）
  document()->markContentsDirty(0, document()->characterCount());

  // 使用常量配置
  applyFontFromSetting();

  // 字体大小变化时（设置界面修改 / 重置）即时刷新
  connect(&SettingStore::ins(), &SettingStore::fontsChanged, this,
          &CodeEditor::applyFontFromSetting);

  // 禁用编辑器的拖放接受（默认 QPlainTextEdit viewport 接受 drop，
  // 会吞掉跨面板标签拖拽事件，导致 DimmableTabWidget 收不到 dragMove，
  // 拆分高亮覆盖层无法显示）。系统拖文件到编辑器打开等高级功能暂未实现。
  setAcceptDrops(false);
  viewport()->setAcceptDrops(false);

  // 禁用自动换行，启用水平滚动条
  setLineWrapMode(QPlainTextEdit::NoWrap);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  setExtraSelections(QList<QTextEdit::ExtraSelection>());

  // 初始化悬停定时器
  m_hoverTimer = new QTimer(this);
  m_hoverTimer->setSingleShot(true);
  m_hoverTimer->setInterval(CodeConstants::Performance::kHoverDebounceMs);

  // 初始化验证定时器
  m_validationTimer = new QTimer(this);
  m_validationTimer->setSingleShot(true);
  m_validationTimer->setInterval(CodeConstants::Performance::kValidationDebounceMs);
  connect(m_validationTimer, &QTimer::timeout, this, &CodeEditor::performValidation);

  // 补全防抖定时器：JSON+schema 的 completions 每次击键全量扫描文本，
  // 连续击键时合并为一次，降低输入滞顿（ac 补全同步即时，不受影响）
  m_completerTimer = new QTimer(this);
  m_completerTimer->setSingleShot(true);
  m_completerTimer->setInterval(120);  // 短暂防抖：合并连续击键，单次敲击几乎无感
  connect(m_completerTimer, &QTimer::timeout, this, &CodeEditor::showCompleter);

  // 文本变化即触发验证：监听 QTextDocument::contentsChange，
  // 覆盖打字/删除/粘贴/撤销重做/IME 输入法/拖放等所有编辑方式。
  // 不再依赖 keyPressEvent 手动触发（避免个别输入路径漏触发导致“输入不提示”）；
  // 0ms 防抖保证输入后立即验证（实测验证 <1ms），同事件循环内多次变化合并为一次验证。
  connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::scheduleValidation);

  // 文档结构变化后重建折叠区间（contentsChange 触发时 doc 仍处于变化过程中，
  // 用 0ms 单次定时器延后到变化完成后重建，避免读取到中间态）
  m_foldRebuildTimer = new QTimer(this);
  m_foldRebuildTimer->setSingleShot(true);
  m_foldRebuildTimer->setInterval(0);
  connect(m_foldRebuildTimer, &QTimer::timeout, this, &CodeEditor::rebuildFold);
  connect(document(), &QTextDocument::contentsChange, this, [this]() {
    m_foldValid = false;  // 标记折叠区间过期，延后重建
    m_foldRebuildTimer->start();
    // 整篇重载（setPlainText）会重置所有块格式，使文本布局层失去统一行高，
    // 导致「点击行间空隙 → cursorForPosition 定位到行尾」。检测首块已丢失统一行高
    // 时延后重应用；打字/删除产生的子块会继承前块格式，无需整篇处理。
    if (m_fixedBlockLineHeight > 0 &&
        !qFuzzyCompare(document()->firstBlock().blockFormat().lineHeight(),
                       m_fixedBlockLineHeight)) {
      m_lineHeightTimer->start();
    }
  });

  // 文档整篇重载后延后重应用统一行高（与折叠重建类似的 0ms 单次定时器）
  m_lineHeightTimer = new QTimer(this);
  m_lineHeightTimer->setSingleShot(true);
  m_lineHeightTimer->setInterval(0);
  connect(m_lineHeightTimer, &QTimer::timeout, this, &CodeEditor::applyFixedBlockLineHeight);

  // 统一高亮层：引用 / 查找面板 / 内嵌查找三套高亮收敛为统一结构。
  // 引用与查找面板层由本类 filler 依据关键词填充；内嵌查找层由 CodeFindBar
  // 通过 setLayerSelections 直接写入（无 filler）。
  m_highlightLayers.insert(
      QStringLiteral("reference"),
      {QStringLiteral("reference"), {}, QString(),
       [this](const QString &key) { return buildReferenceHighlights(key); }});
  m_highlightLayers.insert(
      QStringLiteral("search"),
      {QStringLiteral("search"), {}, QString(),
       [this](const QString &key) { return buildSearchHighlights(key); }});
  m_highlightLayers.insert(QStringLiteral("find"),
                           {QStringLiteral("find"), {}, QString(), nullptr});

  // 高亮层重算：编辑/删除文本时高亮位置会漂移，防抖后按各层当前关键词重新扫描
  m_refHighlightTimer = new QTimer(this);
  m_refHighlightTimer->setSingleShot(true);
  m_refHighlightTimer->setInterval(CodeConstants::Performance::kValidationDebounceMs);
  connect(m_refHighlightTimer, &QTimer::timeout, this, &CodeEditor::reapplyEnabledHighlights);
  connect(document(), &QTextDocument::contentsChange, this,
          &CodeEditor::scheduleReferenceRehighlight);

  // 初始化查找/替换栏（嵌入编辑器上方，默认隐藏）
  m_findBar = new CodeFindBar(this, this);
  connect(m_findBar, &CodeFindBar::findBarClosed, this, [this]() {
    // 查找栏关闭时恢复视口边距
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
  });
  connect(m_findBar, &CodeFindBar::layoutChanged, this, &CodeEditor::updateFindBarLayout);
}

CodeEditor::~CodeEditor() {
  // 析构时清理资源（如果有动态分配的对象）
}

// ──────────────────────────────────────────────────────────────
//  applyFontFromSetting — 从设置读取代码字体大小并应用
// ──────────────────────────────────────────────────────────────

void CodeEditor::applyFontFromSetting() {
  QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  // 字体族：跟随「代码字体」设置（未设置时用系统等宽字体）
  const QString fam = SettingStore::ins().fontFamily(QStringLiteral("font.code"));
  if (!fam.isEmpty()) font.setFamily(fam);
  font.setPointSize(SettingStore::ins().fontSize(QStringLiteral("font.code")));
  setFont(font);
  // 等宽字体变化后 Tab 宽度按新字体的空格宽度重新计算
  setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) *
                     CodeConstants::Editor::kTabWidthSpaces);
  // 字体变化后同步刷新统一行高
  updateFixedLineHeight(font);
}

// ──────────────────────────────────────────────────────────────
//  updateFixedLineHeight — 测量并设置统一行高
// ──────────────────────────────────────────────────────────────

void CodeEditor::updateFixedLineHeight(const QFont &font) {
  if (!m_fixedLineHeightLayout) return;
  // 用临时文档测量「拉丁+中文」混合行的自然行高（非末块，不含底部边距）。
  // 触发中文 fallback 的行是行内所有字体中度量最大者，作为统一行高可保证任何行不被裁剪。
  QTextDocument probe;
  probe.setDefaultFont(font);
  probe.setPlainText(QStringLiteral("a高\n"));  // 首行混合（非末块），避免末块底部边距
  auto *pl = new QPlainTextDocumentLayout(&probe);
  probe.setDocumentLayout(pl);  // probe 接管所有权
  pl->documentSize();           // 强制布局
  // 自然行高 + 额外间距，保证任何行不被裁剪且行不显得拥挤
  const qreal lineH = pl->blockBoundingRect(probe.firstBlock()).height() +
                      CodeConstants::Editor::kLineHeightExtraSpacing;
  m_fixedLineHeightLayout->setFixedLineHeight(lineH);

  // 关键：把统一行高同时写入文本布局层（块的 QTextBlockFormat）。
  // 只改绘制层（blockBoundingRect）会导致「点击行间空隙 → cursorForPosition 定位到行尾」，
  // 因为布局层的文本行仍是自然行高、顶部对齐，块底部留白被吸附成行尾。
  // 在文本层用 FixedHeight 绝对像素值，保证每条文本行均为 lineH，实现真正的统一行高。
  m_fixedBlockLineHeight = qRound(lineH);
  applyFixedBlockLineHeight();
}

// ──────────────────────────────────────────────────────────────
//  applyFixedBlockLineHeight — 把所有块重写为统一行高
// ──────────────────────────────────────────────────────────────

void CodeEditor::applyFixedBlockLineHeight() {
  if (m_fixedBlockLineHeight <= 0) return;
  // 逐块重写文本布局层行高。打字/删除产生的子块会继承前块格式，因此正常编辑无需处理；
  // 仅整篇 setPlainText 会重置全部块格式，需在此恢复统一行高。
  // 注意：setBlockFormat 会把 QTextDocument 的 modified 置为 true（被当作"内容已改"，
  // 表现为 tab 圆点/目录树文件名变黄）。行高只是显示层布局，不该改变"已保存"语义，
  // 因此调整前后保持原 modified 状态，当用户真正编辑时仍能正确标记未保存。
  // 关键：setBlockFormat 每次都会发出 contentsChanged → QPlainTextEdit::textChanged。
  // 若逐块调用（未合并编辑块），打开大文件时会在同一事件循环内连发 N 次 textChanged，
  // 主窗口的 textChanged 处理器会对每个其他编辑器各做一次全量重验 → N×M 次验证 → 打开卡死。
  // 因此这里屏蔽文档信号（纯行高格式调整不是"内容变化"，不应触发任何重验/重排），
  // 并跳过行高已一致的块；布局刷新由末尾的 markContentsDirty + viewport 重绘完成。
  const bool wasModified = document()->isModified();
  QTextBlockFormat fixedBf;
  fixedBf.setLineHeight(qRound(m_fixedBlockLineHeight), QTextBlockFormat::FixedHeight);
  document()->blockSignals(true);
  QTextBlock block = document()->firstBlock();
  for (; block.isValid(); block = block.next()) {
    if (qFuzzyCompare(block.blockFormat().lineHeight(), fixedBf.lineHeight())) continue;
    QTextCursor c(block);
    c.setBlockFormat(fixedBf);
  }
  document()->blockSignals(false);
  document()->setModified(wasModified);
  document()->markContentsDirty(0, document()->characterCount());
  viewport()->update();
}

// ──────────────────────────────────────────────────────────────
//  性能优化：文本缓存
// ──────────────────────────────────────────────────────────────

const QString &CodeEditor::cachedText() const {
  // 检查文档是否已更改（通过版本号检测）
  if (document()->revision() != m_cacheVersion) {
    m_cachedText = toPlainText();
    m_cacheVersion = document()->revision();
  }
  return m_cachedText;
}

void CodeEditor::setSyntaxHighlighter(QSyntaxHighlighter *h) { m_highlighter = h; }

void CodeEditor::reloadColors() {
  // 模板标签/彩虹括号/错误行的背景色都随主题变化，而这几类 ExtraSelection 按 revision
  // 缓存了已固化的 QColor，先统一作废缓存（廉价，任何可见性都要做）。
  m_cursorCtxRev = -1;
  m_rainbowBracketRev = -1;
  m_errorLineDirty = true;

  // 只有「可见」编辑器才立即重建语法高亮与 ExtraSelection：
  // 语法高亮 reloadColors → rehighlight 是对整篇文档 O(n) 重新分词上色，主题切换时
  // 若对藏在后台标签页的编辑器也全量重解析会非常卡。隐藏编辑器只置标记、待 showEvent
  // （标签页变为当前页）时再补做；可见编辑器立即重建，保证本次刷新即看到新主题。
  if (!isVisible()) {
    m_needsThemeReload = true;
    return;
  }

  // 按高亮器具体类型刷新颜色（主题/自定义颜色变化时调用）
  if (auto *lj = dynamic_cast<LightJson *>(m_highlighter)) {
    lj->reloadColors();
  } else if (auto *la = dynamic_cast<LightAc *>(m_highlighter)) {
    la->reloadColors();
  } else if (auto *lt = dynamic_cast<LightTpl *>(m_highlighter)) {
    lt->reloadColors();
  } else if (auto *lts = dynamic_cast<LightTs *>(m_highlighter)) {
    lts->reloadColors();
  }
  refreshExtraSelections();
}

void CodeEditor::showEvent(QShowEvent *event) {
  QPlainTextEdit::showEvent(event);
  // 隐藏期间因主题切换被跳过重建：变为可见时补做语法高亮换色。
  // （切换标签页时 QStackedLayout 会向新页面的控件发 QShowEvent，正好覆盖此场景）
  if (m_needsThemeReload) {
    m_needsThemeReload = false;
    reloadColors();
  }
}

// ──────────────────────────────────────────────────────────────
//  绘制与事件处理
// ──────────────────────────────────────────────────────────────

void CodeEditor::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  QRect cr = contentsRect();
  // 行号区域从视口内容区顶部开始（跳过查找栏占用的顶部边距）
  int topMargin = viewportMargins().top();
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top() + topMargin, lineNumberAreaWidth(), cr.height() - topMargin));

  // 查找栏定位：在视口顶部边距区域内，右对齐
  if (m_findBar && m_findBar->isVisible()) {
    int findBarH = m_findBar->sizeHint().height();
    int findBarW = qMin(m_findBar->sizeHint().width(), cr.width() - 10);
    m_findBar->setGeometry(cr.right() - findBarW - 5, cr.top() + topMargin - findBarH - 2, findBarW,
                           findBarH);
  }
}

void CodeEditor::paintEvent(QPaintEvent *event) {
  // 先调用标准绘制（背景、文本、默认波浪下划线等）
  QPlainTextEdit::paintEvent(event);

  // 绘制缩进参考线（在文本之下、错误波浪线之下）
  {
    int charWidth = fontMetrics().horizontalAdvance(QLatin1Char(' '));
    int tabW = tabStopDistance() / charWidth;
    if (tabW <= 0) tabW = 4;
    m_indentGuide.compute(cachedText(), document()->revision(), tabW);

    const auto &guideRanges = m_indentGuide.ranges();
    if (!guideRanges.isEmpty()) {
      QPainter guidePainter(viewport());

      // 可见行范围
      QTextBlock firstBlock = firstVisibleBlock();
      int firstLine = firstBlock.blockNumber() + 1;
      int lastLine = firstLine;
      {
        QTextBlock blk = firstBlock;
        while (blk.isValid()) {
          QRectF br = blockBoundingGeometry(blk).translated(contentOffset());
          if (br.top() > viewport()->rect().bottom()) break;
          lastLine = blk.blockNumber() + 1;
          blk = blk.next();
        }
      }

      // 当前行号和缩进
      int cursorLine = textCursor().blockNumber() + 1;
      int cursorIndent = IndentGuide::lineIndentLevel(textCursor().block().text(), tabW);

      // 光标矩形（视口坐标）：引导线在光标所在列让位，保证光标始终可见不被覆盖
      const QRect cr = cursorRect();
      const qreal caretX = cr.x() + cr.width() * 0.5;

      QColor normalColor = AuiStyle::indentGuideColor();
      QColor activeColor = AuiStyle::indentGuideActiveColor();

      for (const auto &range : guideRanges) {
        if (range.endLine < firstLine || range.startLine > lastLine) continue;

        int drawStart = qMax(range.startLine, firstLine);
        int drawEnd = qMin(range.endLine, lastLine);

        QTextBlock startBlk = document()->findBlockByNumber(drawStart - 1);
        QTextBlock endBlk = document()->findBlockByNumber(drawEnd - 1);
        if (!startBlk.isValid() || !endBlk.isValid()) continue;

        qreal y1 = blockBoundingGeometry(startBlk).translated(contentOffset()).top();
        qreal y2 = blockBoundingGeometry(endBlk).translated(contentOffset()).bottom();

        // 使用 QTextLayout::cursorToX 获取精确像素位置，避免 charWidth 估算误差；
        // 竖线相对缩进列左移 2 列（落在缩进空白处），避免紧贴/压住代码首字符
        qreal x = 0;
        const int guideCol = qMax(0, range.indent - 2);
        QTextLayout *layout = startBlk.layout();
        if (layout && layout->lineCount() > 0) {
          x = layout->lineAt(0).cursorToX(guideCol) + contentOffset().x();
        } else {
          x = guideCol * charWidth + contentOffset().x();
        }

        bool isActive = (cursorLine >= range.startLine && cursorLine <= range.endLine &&
                         cursorIndent >= range.indent);
        guidePainter.setPen(QPen(isActive ? activeColor : normalColor, 1, Qt::SolidLine));
        // 引导线与光标同列时，在光标所在行让位（断开一小段），避免覆盖光标
        if (qAbs(x - caretX) < 1.0 && cr.top() < y2 && cr.bottom() > y1) {
          if (y1 < cr.top()) guidePainter.drawLine(qRound(x), qRound(y1), qRound(x), cr.top());
          if (cr.bottom() < y2)
            guidePainter.drawLine(qRound(x), cr.bottom(), qRound(x), qRound(y2));
        } else {
          guidePainter.drawLine(qRound(x), qRound(y1), qRound(x), qRound(y2));
        }
      }
    }
  }

  // 统一绘制红色波浪线（唯一绘制机制：单行绘制，终点钳制在行尾，绝不跨行）
  if (m_errorRanges.isEmpty()) return;

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing);

  const int viewH = viewport()->height();
  for (const auto &err : m_errorRanges) {
    // 取错误范围起点所在行的视口横坐标（cursorRect 会把整行错误缩成行尾小锯齿，不可用）
    QTextCursor startCursor(document());
    startCursor.setPosition(err.start);
    const QTextBlock blk = startCursor.block();
    if (!blk.isValid()) continue;
    const QRectF blkRect = blockBoundingGeometry(blk).translated(contentOffset());
    // 仅绘制可见行
    if (blkRect.bottom() < 0 || blkRect.top() > viewH) continue;

    // 错误终点：取 err 终点与该行行尾的较小者，确保绝不越界画到下一行
    const int lineEndPos = blk.position() + qMax(0, blk.length() - 1);
    QTextCursor endCursor(document());
    endCursor.setPosition(qBound(err.start, err.start + err.length, lineEndPos));
    const int x1 = cursorRect(startCursor).left();
    const int x2 = cursorRect(endCursor).right();
    // 与标签栏共用 VSCode 风格波浪线（样式/粗细统一）
    AuiStyle::drawErrorUnderline(painter, x1, x2, qRound(blkRect.bottom()) - 2,
                                 AuiStyle::errorUnderlineColor());
  }
}

bool CodeEditor::viewportEvent(QEvent *event) {
  // 处理鼠标悬停事件，显示错误提示或悬停符号提示
  if (event->type() == QEvent::ToolTip) {
    QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
    QTextCursor cursor = cursorForPosition(helpEvent->pos());
    int pos = cursor.position();

    // 检查是否在错误区域内
    for (const auto &err : m_errorRanges) {
      if (pos >= err.start && pos <= err.start + err.length) {
        showErrorTooltip(helpEvent->globalPos(), err.tooltip);
        event->accept();
        return true;
      }
    }

    // 不在错误区域，隐藏错误提示
    hideErrorTooltip();
  }
  return QPlainTextEdit::viewportEvent(event);
}

void CodeEditor::showErrorTooltip(const QPoint &pos, const QString &text) {
  if (text.isEmpty()) {
    hideErrorTooltip();
    return;
  }

  // 如果弹窗已存在，先关闭（WA_DeleteOnClose 会自动删除）
  if (m_errorTooltip) {
    m_errorTooltip->close();
    m_errorTooltip = nullptr;
  }

  // 创建新弹窗
  m_errorTooltip = new AuiErrorToolTip(text, this);
  m_errorTooltip->move(pos);
  m_errorTooltip->show();
}

void CodeEditor::hideErrorTooltip() {
  if (m_errorTooltip) {
    m_errorTooltip->close();
    m_errorTooltip = nullptr;
  }
}

// ──────────────────────────────────────────────────────────────
//  当前行高亮 + 括号匹配（使用 BracketMatcher 模块）
// ──────────────────────────────────────────────────────────────

void CodeEditor::highlightCurrentLine() {
  // 合并各来源的 ExtraSelection（当前行 / 括号 / 错误 / 调试 / 持久高亮层）
  QList<QTextEdit::ExtraSelection> extra;
  appendCurrentLineHighlight(extra);
  appendRainbowBracketHighlights(extra);
  appendCursorContextHighlights(extra);
  appendErrorLineHighlights(extra);
  appendDebugLineHighlight(extra);
  appendLayerHighlights(extra);
  setExtraSelections(extra);
}

void CodeEditor::appendCurrentLineHighlight(QList<QTextEdit::ExtraSelection> &extra) {
  if (isReadOnly()) return;
  QTextEdit::ExtraSelection selection;
  selection.format.setBackground(AuiStyle::currentLineBackground());
  selection.format.setProperty(QTextFormat::FullWidthSelection, true);
  selection.cursor = textCursor();
  selection.cursor.clearSelection();
  extra.append(selection);
}

void CodeEditor::appendRainbowBracketHighlights(QList<QTextEdit::ExtraSelection> &extra) {
  // ── 彩虹括号：全文括号按嵌套深度着前景色（VSCode 风格），ac/json/tpl 通用 ──
  // 全文扫描缓存：仅当文档内容变化（revision 改变）时才重扫，光标移动 /
  // 面板切换等高频调用直接复用结果，避免每次 O(n) 扫全文造成卡顿。
  const qint64 rev = document()->revision();
  if (rev != m_rainbowBracketRev) {
    m_rainbowBracketCache = BracketMatcher::collectBrackets(cachedText(), nullptr);
    m_rainbowBracketRev = rev;
  }
  for (const auto &b : m_rainbowBracketCache) {
    QColor color = AuiStyle::rainbowBracketColor(b.depth);
    if (!color.isValid()) continue;
    QTextEdit::ExtraSelection sel;
    sel.cursor = textCursor();
    sel.cursor.setPosition(b.pos);
    sel.cursor.setPosition(b.pos + 1, QTextCursor::KeepAnchor);
    sel.format.setForeground(color);
    extra.append(sel);
  }
}

void CodeEditor::appendCursorContextHighlights(QList<QTextEdit::ExtraSelection> &extra) {
  QTextCursor cursor = textCursor();
  if (cursor.hasSelection()) return;
  if (!isVisible()) return;
  const int pos = cursor.position();
  const qint64 rev = document()->revision();

  // 光标上下文（括号配对 + 模板标签）选区缓存：切换面板时可见编辑器的光标位置与
  // 文档都未变，结果完全相同；命中直接复用，避免对全部 3 种括号各做一次全文扫描
  // （最多 3×O(n)）——这是切面板卡顿的主要来源。
  if (m_cursorCtxRev == rev && m_cursorCtxPos == pos) {
    extra.append(m_cursorCtxSels);
    return;
  }

  QList<QTextEdit::ExtraSelection> &out = m_cursorCtxSels;
  out.clear();
  const QString &text = cachedText();

  // 模板文件：识别 ${each}/${/each}、${if}/${/if} 成对控制标签并高亮。
  // 命中后不 return，与下方括号匹配/错误/调试等高亮叠加，
  // 保证光标在 ${...} 块内时，普通括号 ()[]{} 的高亮不丢失。
  if (m_validationMode == TemplateValidation) {
    auto tagMatch = BracketMatcher::findEnclosingTemplateTag(pos, text);
    if (tagMatch.isValid()) {
      // 只叠加背景色，保留 ${if}/${/if} 原有的 keyword() 前景色不变
      const QColor color = AuiStyle::templateTagColor();
      QTextEdit::ExtraSelection sel1;
      sel1.cursor = cursor;
      sel1.cursor.setPosition(tagMatch.openStart);
      sel1.cursor.setPosition(tagMatch.openEnd, QTextCursor::KeepAnchor);
      sel1.format.setBackground(color);
      sel1.format.setFontWeight(QFont::Bold);
      out.append(sel1);

      QTextEdit::ExtraSelection sel2;
      sel2.cursor = cursor;
      sel2.cursor.setPosition(tagMatch.closeStart);
      sel2.cursor.setPosition(tagMatch.closeEnd, QTextCursor::KeepAnchor);
      sel2.format.setBackground(color);
      sel2.format.setFontWeight(QFont::Bold);
      out.append(sel2);
    }
  }

  // 使用 BracketMatcher 进行括号匹配
  auto directMatch = BracketMatcher::findMatchAtCursor(pos, text);
  auto enclosingMatch =
      (directMatch.isValid()) ? directMatch : BracketMatcher::findEnclosingBrackets(pos, text);

  if (enclosingMatch.isValid()) {
    QColor color = AuiStyle::bracketColorForChar(enclosingMatch.openChar);
    // 与模板标签同款：给配对括号背景统一加透明度，柔和、只提示配对不抢眼，
    // 字色仍保持语法高亮的原色不变。透明度略高于模板标签，保证配对更易辨认。
    color.setAlpha(AuiStyle::kBracketMatchBgAlpha);

    QTextEdit::ExtraSelection sel1;
    sel1.cursor = cursor;
    sel1.cursor.setPosition(enclosingMatch.openPos);
    sel1.cursor.setPosition(enclosingMatch.openPos + 1, QTextCursor::KeepAnchor);
    sel1.format.setBackground(color);
    sel1.format.setFontWeight(QFont::Bold);
    out.append(sel1);

    QTextEdit::ExtraSelection sel2;
    sel2.cursor = cursor;
    sel2.cursor.setPosition(enclosingMatch.closePos);
    sel2.cursor.setPosition(enclosingMatch.closePos + 1, QTextCursor::KeepAnchor);
    sel2.format.setBackground(color);
    sel2.format.setFontWeight(QFont::Bold);
    out.append(sel2);
  }

  extra.append(out);
  m_cursorCtxRev = rev;
  m_cursorCtxPos = pos;
}

void CodeEditor::appendErrorLineHighlights(QList<QTextEdit::ExtraSelection> &extra) {
  // 错误行背景色高亮
  if (m_errorLines.isEmpty()) return;
  const qint64 rev = document()->revision();
  // 按 revision + 错误集脏标记缓存：文档内容或错误集变化时才重建，
  // 避免切面板等高频 highlightCurrentLine 对含错误的大文件遍历全部 block。
  if (m_errorLineDirty || m_errorLineRev != rev) {
    m_errorLineRev = rev;
    m_errorLineSels.clear();
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
      int lineNum = block.blockNumber() + 1;
      if (m_errorLines.contains(lineNum)) {
        QTextEdit::ExtraSelection errorSel;
        errorSel.format.setBackground(AuiStyle::errorLineBackground());
        errorSel.format.setProperty(QTextFormat::FullWidthSelection, true);
        errorSel.cursor = QTextCursor(block);
        errorSel.cursor.clearSelection();
        m_errorLineSels.append(errorSel);
      }
      block = block.next();
    }
    m_errorLineDirty = false;
  }
  extra.append(m_errorLineSels);
  // 错误波浪下划线由 paintEvent 依据 m_errorRanges 统一绘制（单行、钳制行尾），
  // 不在此通过 ExtraSelection 绘制，避免与自定义绘制重叠（粗/细两条线并存）
}

void CodeEditor::appendDebugLineHighlight(QList<QTextEdit::ExtraSelection> &extra) {
  // 调试当前行高亮（半透明淡色调背景，行号区标红箭头）。
  if (m_debugLine <= 0) return;
  QTextBlock block = document()->findBlockByNumber(m_debugLine - 1);
  if (!block.isValid()) return;
  QTextEdit::ExtraSelection debugSel;
  debugSel.format.setBackground(AuiStyle::debugLineBackground());
  debugSel.format.setProperty(QTextFormat::FullWidthSelection, true);
  debugSel.cursor = QTextCursor(block);
  debugSel.cursor.clearSelection();
  extra.append(debugSel);
}

void CodeEditor::appendLayerHighlights(QList<QTextEdit::ExtraSelection> &extra) {
  // 统一高亮层（引用 / 查找面板 / 内嵌查找）：按层合并选区。
  // 内嵌查找层仅在查找栏显示时可见（tab 切换暂停时隐藏，避免残留高亮）
  for (auto it = m_highlightLayers.begin(); it != m_highlightLayers.end(); ++it) {
    const HighlightLayer &layer = it.value();
    if (layer.selections.isEmpty()) continue;
    if (it.key() == QLatin1String("find") &&
        !(m_findBar && m_findBar->isFindBarVisible())) {
      continue;
    }
    extra.append(layer.selections);
  }
}

void CodeEditor::refreshExtraSelections() { highlightCurrentLine(); }

// ──────────────────────────────────────────────────────────────
//  按键事件 — Enter 自动缩进 + F12 跳转
// ──────────────────────────────────────────────────────────────

void CodeEditor::keyPressEvent(QKeyEvent *event) {
  // ── 调试快捷键：F5 启动/继续、F10 单步执行、F11 单步进入、Shift+F11 单步跳出 ──
  if (event->key() == Qt::Key_F5 && !event->modifiers()) {
    emit requestDebugStart();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F10 && !event->modifiers()) {
    emit requestDebugStepOver();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F11 && !event->modifiers()) {
    emit requestDebugStepInto();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F11 && (event->modifiers() & Qt::ShiftModifier)) {
    emit requestDebugStepOut();
    event->accept();
    return;
  }

  // Ctrl+F 查找
  if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_F) {
    showFindBar();
    event->accept();
    return;
  }

  // Ctrl+H 替换（等同 Ctrl+F + 展开替换区域）
  if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_H) {
    showFindBar();
    event->accept();
    return;
  }

  // F12 转到定义
  if (event->key() == Qt::Key_F12 && m_validationMode == AcValidation) {
    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QString identifier = identifierAtCursor(pos);
    if (!identifier.isEmpty()) {
      goToDefinition(identifier);
      event->accept();
      return;
    }
  }

  // Ctrl+T 工作区符号搜索
  if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_T &&
      m_validationMode == AcValidation) {
    emit requestWorkspaceSymbols();
    event->accept();
    return;
  }

  // Shift+F12 跨文件查找引用
  if ((event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_F12 &&
      m_validationMode == AcValidation) {
    QTextCursor cursor = textCursor();
    QString identifier = identifierAtCursor(cursor.position());
    if (!identifier.isEmpty()) {
      emit requestFindReferencesAll(objectName(), cursor.blockNumber() + 1,
                                    cursor.columnNumber(), identifier);
      event->accept();
      return;
    }
  }

  // F2 重命名符号（AC/TPL，语义级：作用域 + 跨文件 import）
  if (event->key() == Qt::Key_F2 && !event->modifiers() &&
      (m_validationMode == AcValidation || m_validationMode == TemplateValidation)) {
    QTextCursor cursor = textCursor();
    QString identifier = identifierAtCursor(cursor.position());
    if (!identifier.isEmpty()) {
      emit requestRenameSymbol(objectName(), cursor.blockNumber() + 1, cursor.columnNumber(),
                               identifier);
      event->accept();
      return;
    }
  }

  // Ctrl+M / Ctrl+] 跳转到匹配括号（P2: 快捷键功能）
  if ((event->modifiers() & Qt::ControlModifier) &&
      (event->key() == Qt::Key_M || event->key() == Qt::Key_BracketRight)) {
    jumpToMatchingBracket();
    event->accept();
    return;
  }

  // Ctrl+Shift+M 选中括号内所有内容（P2: 快捷键功能）
  if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) ==
          (Qt::ControlModifier | Qt::ShiftModifier) &&
      event->key() == Qt::Key_M) {
    selectBetweenBrackets();
    event->accept();
    return;
  }

  // 输入 ( 时显示函数签名提示
  if (event->key() == Qt::Key_ParenLeft && m_validationMode == AcValidation) {
    QPlainTextEdit::keyPressEvent(event);
    showSignatureHelp();
    if (m_completer) showCompleter();
    return;
  }

  // 补全器处理：补全弹窗可见时，Enter/Tab/Backtab 用于选中补全项
  // 必须放在 Enter 自动缩进之前，否则弹窗可见时按 Enter 会变成换行
  if (m_completer && m_completer->popup() && m_completer->popup()->isVisible() &&
      (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ||
       event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
      QModelIndex idx = m_completer->popup()->currentIndex();
      if (idx.isValid()) {
        insertCompletion(idx.data(Qt::DisplayRole).toString());
      }
    }
    m_completer->popup()->hide();
    event->accept();
    return;
  }

  // Enter 自动缩进
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    QTextCursor cursor = textCursor();
    QString currentLineText = cursor.block().text().left(cursor.positionInBlock());
    int indent = calculateNewLineIndent(currentLineText);
    QString indentStr(indent, QLatin1Char(' '));
    insertPlainText(QLatin1Char('\n') + indentStr);
    event->accept();
    return;
  }

  // Tab 插入空格（按 Tab 宽度对齐，与 kTabWidthSpaces 保持一致）
  if (event->key() == Qt::Key_Tab && !event->modifiers()) {
    QTextCursor cursor = textCursor();
    int tabW = CodeConstants::Editor::kTabWidthSpaces;
    int spaces = tabW - (cursor.positionInBlock() % tabW);
    insertPlainText(QString(spaces, QLatin1Char(' ')));
    event->accept();
    return;
  }

  QPlainTextEdit::keyPressEvent(event);

  // 显示补全列表（JSON+schema 用防抖合并连续击键，ac 即时触发）
  scheduleCompleter();
}

int CodeEditor::calculateNewLineIndent(const QString &linePrefix) const {
  int indent = 0;
  for (QChar ch : linePrefix) {
    if (ch == QLatin1Char(' '))
      ++indent;
    else
      break;
  }

  // 如果上一行以 { 结尾，增加缩进
  QString trimmed = linePrefix.trimmed();
  if (trimmed.endsWith(QLatin1Char('{'))) {
    indent += CodeConstants::Editor::kIndentSpaces;
  }

  return indent;
}

// ──────────────────────────────────────────────────────────────
//  右键菜单 — 增加「格式化代码」「转到定义」「查找引用」等
// ──────────────────────────────────────────────────────────────

/// 将 Qt 标准右键菜单的英文项本地化为中文（撤销/剪切/复制/粘贴等）
static void localizeStandardMenu(QMenu *menu) {
  struct EnZh {
    const char *en;
    const char *zh;
  };
  static const EnZh kMap[] = {
      {"Undo", "撤销"},  {"Redo", "重做"},   {"Cut", "剪切"},        {"Copy", "复制"},
      {"Paste", "粘贴"}, {"Delete", "删除"}, {"Select All", "全选"},
  };
  auto stripAmp = [](const QString &s) {
    QString r = s;
    r.remove(QLatin1Char('&'));
    r.replace(QLatin1String("..."), QString());
    return r;
  };
  for (QAction *act : menu->actions()) {
    if (act->isSeparator()) continue;
    const QString key = stripAmp(act->text());
    for (const auto &m : kMap) {
      if (key == QLatin1String(m.en)) {
        act->setText(QString::fromUtf8(m.zh));
        break;
      }
    }
  }
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event) {
  // 创建标准右键菜单
  QMenu *menu = createStandardContextMenu();
  localizeStandardMenu(menu);

  // 只在支持的验证模式下添加格式化等功能（JSON / AC / TPL）
  if (m_validationMode != NoValidation) {
    menu->addSeparator();

    // 获取光标下的标识符
    QTextCursor cursor = cursorForPosition(event->pos());
    int pos = cursor.position();
    int idStart = 0, idEnd = 0;
    QString identifier = identifierAtCursor(pos, &idStart, &idEnd);

    if (!identifier.isEmpty()) {
      // ── 转到定义 ──
      QAction *goDefAction = menu->addAction(QStringLiteral("转到定义"));
      goDefAction->setShortcut(QKeySequence(QStringLiteral("F12")));
      connect(goDefAction, &QAction::triggered, this,
              [this, identifier]() { goToDefinition(identifier); });

      // ── 转到类型定义 ──
      QAction *goTypeDefAction = menu->addAction(QStringLiteral("转到类型定义"));
      connect(goTypeDefAction, &QAction::triggered, this,
              [this, identifier]() { goToTypeDefinition(identifier); });

      // ── 查找所有引用（跨文件）──
      // 仅代码类文件（.ac/.tpl，非 JsonValidation）提供；json/jsonvue/jsonsource
      // /jsonupload 是数据定义文件，"标识符引用"语义不适用（裸 key 误报、字符串值漏报），
      // 故不提供此菜单项，避免误导（VSCode 也不对 JSON 提供 Find All References）
      if (m_validationMode != JsonValidation) {
        QAction *findRefsAction = menu->addAction(QStringLiteral("查找所有引用"));
        findRefsAction->setShortcut(QKeySequence(QStringLiteral("Shift+F12")));
        connect(findRefsAction, &QAction::triggered, this, [this, cursor, identifier]() {
          emit requestFindReferencesAll(objectName(), cursor.blockNumber() + 1,
                                        cursor.columnNumber(), identifier);
        });

        // ── 重命名符号（AC/TPL 语义级：作用域 + 类型推断 + 跨文件 import）──
        QAction *renameAction = menu->addAction(QStringLiteral("重命名符号"));
        renameAction->setShortcut(QKeySequence(QStringLiteral("F2")));
        connect(renameAction, &QAction::triggered, this, [this, cursor, identifier]() {
          emit requestRenameSymbol(objectName(), cursor.blockNumber() + 1,
                                   cursor.columnNumber(), identifier);
        });
      }
    }

    menu->addSeparator();

    QAction *fmtAction = menu->addAction(QStringLiteral("格式化代码"));
    fmtAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    connect(fmtAction, &QAction::triggered, this, &CodeEditor::formatCode);
  }

  // ── 断点操作（仅 .ac 可调试文件，模仿 VSCode）──
  if (isDebuggableFile()) {
    // 计算右键位置对应的行号（1-based）
    int line = cursorForPosition(event->pos()).blockNumber() + 1;
    bool hasBp = hasBreakpoint(line);
    bool bpEnabled = hasBp && isBreakpointEnabled(line);

    menu->addSeparator();
    QAction *toggleBpAction =
        menu->addAction(hasBp ? QString::fromUtf8(CodeConstants::UiText::kRemoveBreakpoint)
                              : QStringLiteral("添加断点"));
    toggleBpAction->setShortcut(QKeySequence(QStringLiteral("F9")));
    connect(toggleBpAction, &QAction::triggered, this,
            [this, line]() { toggleBreakpoint(line - 1); });

    // 仅在已有断点时才提供启用/禁用
    if (hasBp) {
      QAction *enableBpAction =
          menu->addAction(bpEnabled ? QString::fromUtf8(CodeConstants::UiText::kDisableBreakpoint)
                                    : QString::fromUtf8(CodeConstants::UiText::kEnableBreakpoint));
      enableBpAction->setEnabled(true);
      connect(enableBpAction, &QAction::triggered, this,
              [this, line, bpEnabled]() { setBreakpointEnabled(line, !bpEnabled); });
    }

    // 移除当前文件所有断点（仅当存在断点时）
    if (!m_breakpoints.isEmpty()) {
      QAction *removeAllBpAction = menu->addAction(QStringLiteral("移除断点..."));
      connect(removeAllBpAction, &QAction::triggered, this, &CodeEditor::clearBreakpoints);
    }
  }

  menu->exec(event->globalPos());
  delete menu;
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event) {
  QPlainTextEdit::mouseMoveEvent(event);

  if (m_validationMode == NoValidation) return;

  QTextCursor cursor = cursorForPosition(event->pos());
  int pos = cursor.position();

  // ── 统一的光标样式：悬停手形与 Ctrl+点击跳转共用 navigationTargetAt ──
  //   这样 AC / JSON 各模式下"手形 ⇔ 可跳转"严格一致
  bool canNavigate = navigationTargetAt(pos);
  if (event->modifiers() & Qt::ControlModifier) {
    viewport()->setCursor(canNavigate ? Qt::PointingHandCursor : Qt::IBeamCursor);
  } else {
    viewport()->setCursor(Qt::IBeamCursor);
  }

  // ── JSON 模式：属性悬停提示（基于 $schema）──
  if (m_validationMode == JsonValidation && m_schemaLoaded) {
    QString path = m_schema.propertyPathAt(cachedText(), pos);
    if (!path.isEmpty()) {
      m_hoverTimer->stop();
      QPoint gpos = event->globalPosition().toPoint();
      m_hoverTimer->start();
      disconnect(m_hoverTimer, &QTimer::timeout, this, nullptr);
      connect(m_hoverTimer, &QTimer::timeout, this,
              [this, path, gpos]() { showJsonPropertyHover(path, gpos); });
    } else {
      m_currentHoverSymbol.clear();
      m_hoverTimer->stop();
      QToolTip::hideText();
    }
    return;
  }

  if (m_validationMode != AcValidation) return;

  // ── AC 模式：悬停提示（不需要 Ctrl，始终显示）──
  int idStart = 0, idEnd = 0;
  QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
  if (!identifier.isEmpty()) {
    m_hoverTimer->stop();
    QPoint gpos = event->globalPosition().toPoint();
    m_hoverTimer->start();
    disconnect(m_hoverTimer, &QTimer::timeout, this, nullptr);
    connect(m_hoverTimer, &QTimer::timeout, this,
            [this, pos, gpos]() { showSymbolHover(pos, gpos); });
  } else {
    m_currentHoverSymbol.clear();
    m_hoverTimer->stop();
    QToolTip::hideText();
  }
}

// ──────────────────────────────────────────────────────────────
//  点击坐标校正 — 消除自定义统一行高造成的「点击行尾吸附」
// ──────────────────────────────────────────────────────────────
// FixedLineHeightLayout 将块的绘制高度固定为统一行高（m_h），但
// cursorForPosition 在块内做行命中时仍按"块内文本行实际高度"计算。
// 当点击落在块内文本底部以下的空隙（如纯 ASCII 行比中英混排行矮）时，
// 内部映射会退化为“吸附到行尾”。这里把点击点纵向修正到文本行的中心，
// 保证它命中正确的行，从而得到正确的列。

QPointF CodeEditor::snapClickToText(const QPointF &pos) const {
  // 用基类映射取块：块级纵向判定是正确的，错的只是块内列
  const QTextBlock block = cursorForPosition(pos.toPoint()).block();
  if (!block.isValid()) return pos;
  const QTextLayout *tl = block.layout();
  if (!tl || tl->lineCount() == 0) return pos;
  const qreal lineH = tl->lineAt(0).height();
  // 块的视口坐标 = 文档坐标 + contentOffset
  const QPointF blockViewTop = blockBoundingGeometry(block).topLeft() + contentOffset();
  // 仅修正纵向到文本行中心，横向保留原点击 x
  return QPointF(pos.x(), blockViewTop.y() + lineH * 0.5);
}

void CodeEditor::mousePressEvent(QMouseEvent *event) {
  // 仅对“普通左键单击”做纵向校正（不处理 Ctrl/Shift/中指等，避免干扰其他能力）
  const bool plainClick =
      event->button() == Qt::LeftButton &&
      (event->modifiers() &
       (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == 0;
  if (plainClick) {
    const QPointF corrected = snapClickToText(event->position());
    QMouseEvent me(event->type(), corrected, corrected, event->globalPosition(),
                   event->button(), event->buttons(), event->modifiers(),
                   event->pointingDevice());
    QPlainTextEdit::mousePressEvent(&me);
    event->accept();
    return;
  }
  QPlainTextEdit::mousePressEvent(event);
}

void CodeEditor::mouseReleaseEvent(QMouseEvent *event) {
  // Ctrl+点击跳转
  if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
    QTextCursor cursor = cursorForPosition(event->pos());
    int pos = cursor.position();

    // ── JSON 模式：Ctrl+点击属性 → 跳转到 schema 中对应字段 ──
    if (m_validationMode == JsonValidation && m_schemaLoaded) {
      // Ctrl+点击 "$schema" 的路径值 → 跳转打开 schema 文件
      if (schemaRefRangeAt(pos)) {
        if (!m_schemaPath.isEmpty()) {
          emit aboutToNavigate(m_schemaPath, 1);
          emit requestGoToLine(m_schemaPath, 1);
          return;
        }
      }
      QString path = m_schema.propertyPathAt(cachedText(), pos);
      QString className, propName;
      if (!path.isEmpty() && m_schema.propertyContext(path, &className, &propName) &&
          !propName.isEmpty()) {
        int line = findSchemaPropertyLine(className, propName);
        emit aboutToNavigate(m_schemaPath, line);
        emit requestGoToLine(m_schemaPath, line);
        return;
      }
    }

    // ── AC 模式：Ctrl+点击标识符 → 转到定义；点击 import 路径 → 打开该文件 ──
    //   （JSON 无 schema 时不再走标识符兜底，保证与手形判定一致）
    if (m_validationMode == AcValidation) {
      // 优先：光标位于 import ... from "path" 的路径字符串内 → 打开目标文件
      const QString targetPath = importPathAt(pos);
      if (!targetPath.isEmpty()) {
        emit aboutToNavigate(targetPath, 1);
        emit requestGoToLine(targetPath, 1);
        return;
      }
      int idStart = 0, idEnd = 0;
      QString identifier = identifierAtCursor(pos, &idStart, &idEnd);
      if (!identifier.isEmpty()) {
        goToDefinition(identifier);
        return;
      }
    }
  }

  QPlainTextEdit::mouseReleaseEvent(event);
}

// ══════════════════════════════════════════════════════════════════════════════
//  查找/替换栏
// ══════════════════════════════════════════════════════════════════════════════

void CodeEditor::showFindBar() {
  if (!m_findBar) return;
  m_findBar->showFindBar();
  updateFindBarLayout();
}

void CodeEditor::hideFindBar() {
  if (m_findBar) m_findBar->hideFindBar();
  setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

bool CodeEditor::isFindBarVisible() const { return m_findBar && m_findBar->isFindBarVisible(); }

CodeFindBar *CodeEditor::findBar() const { return m_findBar; }

void CodeEditor::updateFindBarLayout() {
  if (!m_findBar || !m_findBar->isFindBarVisible()) return;
  // 强制刷新布局，确保 sizeHint 反映替换区域展开/收起后的实际高度
  m_findBar->layout()->activate();
  int findBarH = m_findBar->sizeHint().height();
  int topMargin = findBarH + 4;
  setViewportMargins(lineNumberAreaWidth(), topMargin, 0, 0);
  QRect cr = contentsRect();
  int findBarW = qMin(m_findBar->sizeHint().width(), cr.width() - 10);
  // 查找栏定位在顶部边距区域内，不覆盖代码
  m_findBar->setGeometry(cr.right() - findBarW - 5, cr.top() + topMargin - findBarH - 2, findBarW,
                         findBarH);
  // 行号区域也要同步调整
  m_lineNumberArea->setGeometry(
      QRect(cr.left(), cr.top() + topMargin, lineNumberAreaWidth(), cr.height() - topMargin));
}