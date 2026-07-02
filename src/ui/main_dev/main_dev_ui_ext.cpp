/**
 * @file main_dev_ui_ext.cpp
 * @brief 编辑器面板扩展控件实现
 */

#include "main_dev_ui_ext.h"
#include "src/tool/ui/aui_style.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSplitter>
#include <QStyleOptionTab>

// ════════════════════════════════════════════════════════════
//  DraggableTabBar 实现
// ════════════════════════════════════════════════════════════

DraggableTabBar *DraggableTabBar::s_sourceBar = nullptr;
int DraggableTabBar::s_sourceIndex = -1;

DraggableTabBar::DraggableTabBar(QWidget *parent) : QTabBar(parent) {
  setAcceptDrops(true);
  setMovable(false); // 自行处理跨面板拖拽
}

void DraggableTabBar::setTabModified(int index, bool modified) {
  if (modified)
    m_modifiedTabs.insert(index);
  else
    m_modifiedTabs.remove(index);
  update(); // 触发重绘
}

bool DraggableTabBar::isTabModified(int index) const {
  return m_modifiedTabs.contains(index);
}

void DraggableTabBar::paintEvent(QPaintEvent *event) {
  // 先绘制默认标签栏
  QTabBar::paintEvent(event);

  // 在已修改标签上叠加红色 "*"
  QPainter painter(this);
  painter.setPen(AuiStyle::modifiedColor());
  QFont font = painter.font();
  font.setBold(true);
  font.setPixelSize(14);
  painter.setFont(font);

  for (int i = 0; i < count(); ++i) {
    if (!m_modifiedTabs.contains(i))
      continue;
    // 绘制在文字右侧，紧贴文字
    QStyleOptionTab opt;
    initStyleOption(&opt, i);
    QRect textRect =
        style()->subElementRect(QStyle::SE_TabBarTabText, &opt, this);
    int starX = textRect.right() + 2;
    int starY = textRect.top() + font.pixelSize() - 2;
    painter.drawText(starX, starY, QStringLiteral("*"));
  }
}

void DraggableTabBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_pressedIndex = tabAt(event->pos());
    m_dragStartPos = event->pos();
  }
  QTabBar::mousePressEvent(event);
}

void DraggableTabBar::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton) || m_pressedIndex < 0) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  if ((event->pos() - m_dragStartPos).manhattanLength() <
      QApplication::startDragDistance()) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  // 开始跨面板拖拽
  s_sourceBar = this;
  s_sourceIndex = m_pressedIndex;

  auto *mime = new QMimeData;
  mime->setData(QStringLiteral("application/x-auto-code-tab"),
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
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
}

void DraggableTabBar::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
}

void DraggableTabBar::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")) ||
      !s_sourceBar) {
    QTabBar::dropEvent(event);
    return;
  }

  int toIndex = tabAt(event->position().toPoint());
  if (toIndex < 0)
    toIndex = count(); // 追加到末尾

  emit tabDropped(s_sourceIndex, s_sourceBar, toIndex);
  s_sourceBar = nullptr;
  s_sourceIndex = -1;

  event->acceptProposedAction();
}

// ════════════════════════════════════════════════════════════
//  DimmableTabWidget 实现
// ════════════════════════════════════════════════════════════

DimmableTabWidget::DimmableTabWidget(QWidget *parent) : QTabWidget(parent) {
  setAcceptDrops(true); // 内容区也接受拖放

  // 用 DraggableTabBar 替换默认标签栏
  auto *bar = new DraggableTabBar;
  setTabBar(bar);

  // setTabBar 之后设置，确保作用到 DraggableTabBar
  setTabsClosable(true);

  // 强制 Fusion 风格使 setTabTextColor 生效
  AuiStyle::ensureFusionTabBar(bar);

  // tab 样式
  bar->setStyleSheet(QStringLiteral("QTabBar::tab {"
                                    "  padding: 6px 2px 6px 4px;"
                                    "  border: 1px solid #c8c8c8;"
                                    "  border-bottom: none;"
                                    "  border-top-left-radius: 4px;"
                                    "  border-top-right-radius: 4px;"
                                    "  background: #e8e8e8;"
                                    "  margin-right: 0px;"
                                    "}"
                                    "QTabBar::tab:selected {"
                                    "  background: #ffffff;"
                                    "}"
                                    "QTabBar::tab:hover:!selected {"
                                    "  background: #dcdcdc;"
                                    "}"
                                    "QTabBar::close-button {"
                                    "  padding: 0px;"
                                    "  margin: 0px;"
                                    "}"));

  // 跨面板拖拽：标签移动
  connect(bar, &DraggableTabBar::tabDropped, this,
          [this](int fromIndex, DraggableTabBar *fromBar, int toIndex) {
            // 定位源 DimmableTabWidget
            auto *fromWidget =
                qobject_cast<DimmableTabWidget *>(fromBar->parentWidget());
            if (!fromWidget)
              return;

            // 取出标签页内容
            QWidget *page = fromWidget->widget(fromIndex);
            if (!page)
              return;

            QString text = fromWidget->tabText(fromIndex);
            QString tip = fromWidget->tabToolTip(fromIndex);
            QIcon icon = fromWidget->tabIcon(fromIndex);

            // 从源面板移除
            fromWidget->removeTab(fromIndex);

            // 源面板没有标签页且还有其它面板 → 自动销毁
            if (fromWidget->count() == 0 && fromWidget != this) {
              auto *splitter =
                  qobject_cast<QSplitter *>(fromWidget->parentWidget());
              if (splitter && splitter->count() > 1)
                fromWidget->deleteLater();
            }

            if (fromWidget == this) {
              // 同一面板内重新排序
              int insertIdx = toIndex;
              if (insertIdx > fromIndex)
                --insertIdx;
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

// 内容区拖放

void DimmableTabWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
  else
    QTabWidget::dragEnterEvent(event);
}

void DimmableTabWidget::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab")))
    event->acceptProposedAction();
  else
    QTabWidget::dragMoveEvent(event);
}

void DimmableTabWidget::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(
          QStringLiteral("application/x-auto-code-tab"))) {
    QTabWidget::dropEvent(event);
    return;
  }

  auto *fromBar = DraggableTabBar::dragSourceBar();
  int fromIndex = DraggableTabBar::dragSourceIndex();
  DraggableTabBar::clearDragSource();

  if (!fromBar)
    return;

  auto *fromWidget = qobject_cast<DimmableTabWidget *>(fromBar->parentWidget());
  if (!fromWidget || fromIndex < 0)
    return;

  // 内容区域放下 = 追加到末尾
  QWidget *page = fromWidget->widget(fromIndex);
  if (!page)
    return;

  QString text = fromWidget->tabText(fromIndex);
  QString tip = fromWidget->tabToolTip(fromIndex);
  QIcon icon = fromWidget->tabIcon(fromIndex);

  fromWidget->removeTab(fromIndex);

  // 源面板没有标签页且还有其它面板 → 自动销毁
  if (fromWidget->count() == 0 && fromWidget != this) {
    auto *splitter = qobject_cast<QSplitter *>(fromWidget->parentWidget());
    if (splitter && splitter->count() > 1)
      fromWidget->deleteLater();
  }

  int newIdx = addTab(page, icon, text);
  setTabToolTip(newIdx, tip);

  // 如果目标面板已有标签页则插入在最后，否则追加
  setCurrentIndex(newIdx);

  event->acceptProposedAction();
}