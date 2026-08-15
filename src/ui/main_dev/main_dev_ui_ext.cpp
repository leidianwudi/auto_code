/**
 * @file main_dev_ui_ext.cpp
 * @brief 编辑器面板扩展控件实现
 */

#include "main_dev_ui_ext.h"

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QSplitter>

#include "src/util/common/code_constants.h"
#include "src/util/common/util_file.h"
#include "src/util/ui/component/aui_style.h"

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

void DraggableTabBar::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)))
    event->acceptProposedAction();
}

void DraggableTabBar::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)) ||
      !s_sourceBar) {
    QTabBar::dropEvent(event);
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

// 内容区拖放

void DimmableTabWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)))
    event->acceptProposedAction();
  else
    QTabWidget::dragEnterEvent(event);
}

void DimmableTabWidget::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab)))
    event->acceptProposedAction();
  else
    QTabWidget::dragMoveEvent(event);
}

void DimmableTabWidget::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat(QString::fromUtf8(CodeConstants::Mime::kAutoCodeTab))) {
    QTabWidget::dropEvent(event);
    return;
  }

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
