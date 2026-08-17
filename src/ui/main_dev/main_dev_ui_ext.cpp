/**
 * @file main_dev_ui_ext.cpp
 * @brief 编辑器面板扩展控件实现
 */

#include "main_dev_ui_ext.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSplitter>

#include "src/util/common/code_constants.h"
#include "src/util/common/util_file.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  SplitOverlay 实现 — 拆分高亮覆盖层
// ════════════════════════════════════════════════════════════

SplitOverlay::SplitOverlay(QWidget *parent) : QWidget(parent) {
  // 覆盖层不拦截鼠标事件（拖拽期间由 QDrag 管理，这里兜底避免误拦截）
  setAttribute(Qt::WA_TransparentForMouseEvents);
  hide();
}

void SplitOverlay::showForSide(SplitSide side, const QRect &area) {
  m_side = side;
  setGeometry(area);
  show();
  raise();
  update();
}

void SplitOverlay::paintEvent(QPaintEvent * /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  // 高亮色与选中标签顶部指示条一致（VSCode 风格的蓝色）
  const QColor fill(14, 122, 254, 70);
  const QColor border(14, 122, 254, 220);
  p.setPen(QPen(border, 2));
  p.setBrush(fill);
  p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
}

// ════════════════════════════════════════════════════════════
//  DraggableTabBar 实现
// ════════════════════════════════════════════════════════════

DraggableTabBar *DraggableTabBar::s_sourceBar = nullptr;
int DraggableTabBar::s_sourceIndex = -1;

DraggableTabBar::DraggableTabBar(QWidget *parent) : AuiCodeTabBar(parent) {
  setAcceptDrops(true);
  setMovable(false);  // 自行处理跨面板拖拽
}

void DraggableTabBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) {
    int index = tabAt(event->pos());
    if (index >= 0) {
      QMenu menu(this);
      QAction *showInExplorerAct = menu.addAction(QStringLiteral("在文件资源管理器中显示"));
      menu.addSeparator();
      QAction *closeOthers = menu.addAction(QStringLiteral("关闭其它"));
      QAction *closeAll = menu.addAction(QStringLiteral("关闭全部"));
      QAction *chosen = menu.exec(event->globalPos());
      if (chosen == showInExplorerAct) {
        auto *tw = qobject_cast<QTabWidget *>(parentWidget());
        if (tw) {
          QString path = tw->tabToolTip(index);
          if (!path.isEmpty()) UtilFile::showInExplorer(path);
        }
      } else if (chosen == closeOthers) {
        emit closeOthersRequested(index);
      } else if (chosen == closeAll) {
        emit closeAllRequested();
      }
    }
    return;
  }
  if (event->button() == Qt::LeftButton) {
    // 关闭按钮点击由基类（自绘关闭区）处理
    if (handleClosePress(event->pos())) return;
    const int index = tabAt(event->pos());
    m_pressedIndex = index;
    m_dragStartPos = event->pos();
  }
  QTabBar::mousePressEvent(event);
}

void DraggableTabBar::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton) || m_pressedIndex < 0) {
    AuiCodeTabBar::mouseMoveEvent(event);  // 悬停刷新 + 基类
    return;
  }

  if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
    AuiCodeTabBar::mouseMoveEvent(event);
    return;
  }

  // 开始跨面板拖拽
  s_sourceBar = this;
  s_sourceIndex = m_pressedIndex;

  auto *mime = new QMimeData;
  mime->setData(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab),
                QByteArray::number(reinterpret_cast<quintptr>(this)));

  auto *drag = new QDrag(this);
  drag->setMimeData(mime);

  // 拖拽半透明缩略图
  QPixmap px = grab(tabRect(m_pressedIndex));
  if (!px.isNull()) {
    drag->setPixmap(px);
    drag->setHotSpot(event->pos() - tabRect(m_pressedIndex).topLeft());
  }

  if (drag->exec(Qt::MoveAction) != Qt::MoveAction) {
    s_sourceBar = nullptr;
    s_sourceIndex = -1;
  }
}

void DraggableTabBar::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)))
    event->acceptProposedAction();
}

/// 根据横向位置判断是否命中左/右拆分区域（靠近左右边界 25% 内）
static SplitSide splitSideAt(const QPoint &pos, const QWidget *w) {
  const int width = w->width();
  if (width <= 0) return SplitSide::None;
  const int x = pos.x();
  if (x < width * 0.25) return SplitSide::Left;
  if (x > width * 0.75) return SplitSide::Right;
  return SplitSide::None;
}

void DraggableTabBar::dragMoveEvent(QDragMoveEvent *event) {
  if (!event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab))) {
    QTabBar::dragMoveEvent(event);
    return;
  }
  event->acceptProposedAction();

  // 拖到 tab 头左/右边缘 → 显示拆分高亮（覆盖整个所属面板）
  m_splitSide = splitSideAt(event->position().toPoint(), this);
  if (auto *panel = qobject_cast<DimmableTabWidget *>(parentWidget())) {
    if (m_splitSide != SplitSide::None) {
      panel->showSplitOverlay(m_splitSide);
    } else {
      panel->hideSplitOverlay();
    }
  }
}

void DraggableTabBar::dragLeaveEvent(QDragLeaveEvent *event) {
  m_splitSide = SplitSide::None;
  if (auto *panel = qobject_cast<DimmableTabWidget *>(parentWidget())) panel->hideSplitOverlay();
  QTabBar::dragLeaveEvent(event);
}

void DraggableTabBar::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)) ||
      !s_sourceBar) {
    m_splitSide = SplitSide::None;
    QTabBar::dropEvent(event);
    return;
  }

  // 命中拆分区域 → 触发拆分（由 MainDevMgr 创建新面板并移动标签）
  if (m_splitSide != SplitSide::None) {
    SplitSide side = m_splitSide;
    m_splitSide = SplitSide::None;
    if (auto *panel = qobject_cast<DimmableTabWidget *>(parentWidget())) panel->hideSplitOverlay();
    emit tabSplitDropped(s_sourceIndex, s_sourceBar, side);
    s_sourceBar = nullptr;
    s_sourceIndex = -1;
    event->acceptProposedAction();
    return;
  }

  int toIndex = tabAt(event->position().toPoint());
  if (toIndex < 0) toIndex = count();  // 追加到末尾

  emit tabDropped(s_sourceIndex, s_sourceBar, toIndex);
  s_sourceBar = nullptr;
  s_sourceIndex = -1;

  event->acceptProposedAction();
}

// ════════════════════════════════════════════════════════════
//  DimmableTabWidget 实现
// ════════════════════════════════════════════════════════════

DimmableTabWidget::DimmableTabWidget(QWidget *parent) : QTabWidget(parent) {
  setAcceptDrops(true);  // 内容区也接受拖放

  // 用 DraggableTabBar 替换默认标签栏
  auto *bar = new DraggableTabBar;
  setTabBar(bar);

  // 拆分高亮覆盖层（子控件，覆盖在标签栏 + 内容区之上）
  m_splitOverlay = new SplitOverlay(this);

  // 关闭按钮不启用 Qt 原生按钮（AuiCodeTabBar 自绘接管，构造时已 setTabsClosable(false)），
  // 这里显式保持关闭，避免误开原生 X 覆盖自绘内容。
  setTabsClosable(false);

  // 自绘关闭按钮点击由 AuiCodeTabBar 直接发出 QTabBar::tabCloseRequested，
  // 原生模式下 QTabWidget 会自动转发该信号，自绘模式下需手动转发，
  // 否则 MainDevMgr 连接的 QTabWidget::tabCloseRequested 收不到关闭请求。
  connect(bar, &QTabBar::tabCloseRequested, this, &QTabWidget::tabCloseRequested);

  // tab 样式：指定编辑框 tab 头四边空白
  // 文字颜色由 paintEvent 直接自绘，不依赖 setTabTextColor 或代理样式
  AuiStyle::applyTabBarPadding(bar, 4, 4, 0, 4);

  // 标签拖到 tab 头左/右边缘 → 转发为面板的拆分请求信号
  connect(bar, &DraggableTabBar::tabSplitDropped, this, &DimmableTabWidget::splitDropped);

  // 跨面板拖拽：标签移动
  connect(bar, &DraggableTabBar::tabDropped, this,
          [this](int fromIndex, DraggableTabBar *fromBar, int toIndex) {
            // 定位源 DimmableTabWidget
            auto *fromWidget = qobject_cast<DimmableTabWidget *>(fromBar->parentWidget());
            if (!fromWidget) return;

            // 取出标签页内容
            QWidget *page = fromWidget->widget(fromIndex);
            if (!page) return;

            QString text = fromWidget->tabText(fromIndex);
            QString tip = fromWidget->tabToolTip(fromIndex);
            QIcon icon = fromWidget->tabIcon(fromIndex);

            // 从源面板移除
            fromWidget->removeTab(fromIndex);

            // 源面板没有标签页且还有其它面板 → 自动销毁
            if (fromWidget->count() == 0 && fromWidget != this) {
              auto *splitter = qobject_cast<QSplitter *>(fromWidget->parentWidget());
              if (splitter && splitter->count() > 1) fromWidget->deleteLater();
            }

            if (fromWidget == this) {
              // 同一面板内重新排序
              int insertIdx = toIndex;
              if (insertIdx > fromIndex) --insertIdx;
              int newIdx = insertTab(insertIdx, page, icon, text);
              setTabToolTip(newIdx, tip);
              setCurrentIndex(newIdx);
            } else {
              // 跨面板移动
              int newIdx = insertTab(toIndex, page, icon, text);
              setTabToolTip(newIdx, tip);
              setCurrentIndex(newIdx);
            }
          });
}

// ──────────────────────────────────────────────────────────────
//  拆分高亮覆盖层管理
// ──────────────────────────────────────────────────────────────

void DimmableTabWidget::showSplitOverlay(SplitSide side) {
  if (!m_splitOverlay) return;
  m_splitSide = side;
  // 覆盖层只覆盖左半区（拆分到左）或右半区（拆分到右），VSCode 风格
  QRect area = rect();
  if (side == SplitSide::Left) {
    area.setWidth(area.width() / 2);
  } else if (side == SplitSide::Right) {
    const int half = area.width() / 2;
    area.setLeft(area.width() - half);
    area.setWidth(half);
  }
  m_splitOverlay->showForSide(side, area);
}

void DimmableTabWidget::hideSplitOverlay() {
  m_splitSide = SplitSide::None;
  if (m_splitOverlay) m_splitOverlay->hide();
}

// 内容区拖放

void DimmableTabWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)))
    event->acceptProposedAction();
  else
    QTabWidget::dragEnterEvent(event);
}

void DimmableTabWidget::dragMoveEvent(QDragMoveEvent *event) {
  if (!event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab))) {
    QTabWidget::dragMoveEvent(event);
    return;
  }
  event->acceptProposedAction();

  // 拖到内容区左/右边缘 → 显示拆分高亮
  SplitSide side = splitSideAt(event->position().toPoint(), this);
  if (side != m_splitSide) {
    if (side != SplitSide::None) {
      showSplitOverlay(side);
    } else {
      hideSplitOverlay();
    }
  }
}

void DimmableTabWidget::dragLeaveEvent(QDragLeaveEvent *event) {
  hideSplitOverlay();
  QTabWidget::dragLeaveEvent(event);
}

void DimmableTabWidget::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab))) {
    QTabWidget::dropEvent(event);
    return;
  }

  // 命中拆分区域 → 触发拆分（先取出方向，再隐藏覆盖层重置状态）
  if (m_splitSide != SplitSide::None) {
    SplitSide side = m_splitSide;
    hideSplitOverlay();
    auto *fromBar = DraggableTabBar::dragSourceBar();
    int fromIndex = DraggableTabBar::dragSourceIndex();
    DraggableTabBar::clearDragSource();
    if (fromBar) emit splitDropped(fromIndex, fromBar, side);
    event->acceptProposedAction();
    return;
  }

  hideSplitOverlay();

  auto *fromBar = DraggableTabBar::dragSourceBar();
  int fromIndex = DraggableTabBar::dragSourceIndex();
  DraggableTabBar::clearDragSource();

  if (!fromBar) return;

  auto *fromWidget = qobject_cast<DimmableTabWidget *>(fromBar->parentWidget());
  if (!fromWidget || fromIndex < 0) return;

  // 内容区域放下 = 追加到末尾
  QWidget *page = fromWidget->widget(fromIndex);
  if (!page) return;

  QString text = fromWidget->tabText(fromIndex);
  QString tip = fromWidget->tabToolTip(fromIndex);
  QIcon icon = fromWidget->tabIcon(fromIndex);

  fromWidget->removeTab(fromIndex);

  // 源面板没有标签页且还有其它面板 → 自动销毁
  if (fromWidget->count() == 0 && fromWidget != this) {
    auto *splitter = qobject_cast<QSplitter *>(fromWidget->parentWidget());
    if (splitter && splitter->count() > 1) fromWidget->deleteLater();
  }

  int newIdx = addTab(page, icon, text);
  setTabToolTip(newIdx, tip);

  // 如果目标面板已有标签页则插入在最后，否则追加
  setCurrentIndex(newIdx);

  event->acceptProposedAction();
}
