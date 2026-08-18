/**
 * @file icon_picker_dialog.cpp
 * @brief 图标选择对话框实现
 *
 * 3 套图标库：Element Plus / Ant Design / TDesign
 * 标签切换 + 翻页 + 纯图标网格（无文字）
 */

#include "icon_picker_dialog.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "icon_loader.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  图标居中绘制委托
// ════════════════════════════════════════════════════════════

/// 将图标居中绘制在单元格中
class CenterIconDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    // 绘制选中/悬停背景（不调用基类 paint 避免断言）
    if (option.state & (QStyle::State_Selected | QStyle::State_MouseOver)) {
      QStyleOptionViewItem opt = option;
      opt.text = QString();
      opt.icon = QIcon();
      QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
      style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    }

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (icon.isNull()) return;

    QSize iconSize = icon.actualSize(option.rect.size());
    int x = option.rect.left() + (option.rect.width() - iconSize.width()) / 2;
    int y = option.rect.top() + (option.rect.height() - iconSize.height()) / 2;
    icon.paint(painter, x, y, iconSize.width(), iconSize.height());
  }
};

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

IconPickerDialog::IconPickerDialog(const QString &currentIcon, QWidget *parent)
    : QDialog(parent), m_selectedIcon(currentIcon) {
  setupUI();

  // 判断当前图标属于哪个标签页
  if (!currentIcon.isEmpty()) {
    QString prefix = IconLoader::extractPrefix(currentIcon);
    if (prefix == QStringLiteral("ep")) {
      m_currentTab = 0;
    } else if (prefix == QStringLiteral("ant-design")) {
      m_currentTab = 1;
    } else if (prefix == QStringLiteral("tdesign")) {
      m_currentTab = 2;
    }
  }
  m_tabWidget->setCurrentIndex(m_currentTab);
  switchTab(m_currentTab);
}

// ════════════════════════════════════════════════════════════
//  界面构建
// ════════════════════════════════════════════════════════════

void IconPickerDialog::setupUI() {
  setWindowTitle(QStringLiteral("选择图标"));
  setMinimumSize(685, 660);

  AuiWindow::setupFramelessDialog(this);

  TitleBarOptions opts;
  opts.title = QStringLiteral("选择图标");
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  auto tb = AuiWindow::createTitleBar(this, opts);

  auto *contentWidget = new QWidget;
  auto *mainLayout = new QVBoxLayout(contentWidget);
  mainLayout->setContentsMargins(2, 4, 2, 8);
  mainLayout->setSpacing(0);

  // ── 搜索框 ──
  auto *searchLayout = new QHBoxLayout;
  searchLayout->setSpacing(6);

  auto *searchLabel = new QLabel(QStringLiteral("搜索:"));
  m_searchEdit = new QLineEdit;
  m_searchEdit->setPlaceholderText(QStringLiteral("输入关键字过滤图标"));
  m_searchEdit->setMinimumHeight(26);
  m_searchEdit->setMaximumHeight(26);
  searchLayout->addWidget(searchLabel);
  searchLayout->addWidget(m_searchEdit);
  mainLayout->addLayout(searchLayout);
  mainLayout->addSpacing(4);

  connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
    m_currentPage = 0;
    fillPage();
  });

  // ── 标签切换 ──
  m_tabWidget = new QTabWidget;
  m_tabWidget->setDocumentMode(true);
  m_tabWidget->addTab(new QWidget, QStringLiteral("Element Plus"));
  m_tabWidget->addTab(new QWidget, QStringLiteral("Ant Design"));
  m_tabWidget->addTab(new QWidget, QStringLiteral("TDesign"));
  // 仅显示 tab 栏，隐藏空白内容区
  m_tabWidget->tabBar()->setFixedHeight(30);
  m_tabWidget->setFixedHeight(32);
  m_tabWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  m_tabWidget->setStyleSheet(
      QStringLiteral("QTabWidget::pane { border: none; padding: 0px; }"
                     "QTabBar::tab {"
                     "  padding: 8px 20px;"
                     "  border: none;"
                     "  border-radius: 4px;"
                     "  background: transparent;"
                     "  color: %1;"
                     "}"
                     "QTabBar::tab:selected {"
                     "  background: %2;"
                     "  color: %3;"
                     "}"
                     "QTabBar::tab:hover:!selected {"
                     "  background: %4;"
                     "  color: %5;"
                     "}")
          .arg(AuiStyle::inactiveTabColor().name(), AuiStyle::tabHoverBackground().name(),
               AuiStyle::textColor().name(), AuiStyle::hoverBackground().name(),
               AuiStyle::secondaryTextColor().name()));
  mainLayout->addWidget(m_tabWidget);

  connect(m_tabWidget, &QTabWidget::currentChanged, this, &IconPickerDialog::switchTab);

  // ── 图标网格 ──
  m_iconTable = new QTableWidget;
  m_iconTable->setColumnCount(kCols);
  m_iconTable->horizontalHeader()->setVisible(false);
  m_iconTable->verticalHeader()->setVisible(false);
  m_iconTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  m_iconTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_iconTable->setSelectionBehavior(QAbstractItemView::SelectItems);
  m_iconTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_iconTable->setShowGrid(false);
  m_iconTable->setItemDelegate(new CenterIconDelegate(this));
  m_iconTable->setIconSize(QSize(kIconSize, kIconSize));
  for (int i = 0; i < kCols; ++i) {
    m_iconTable->setColumnWidth(i, kCellSize);
  }
  mainLayout->addWidget(m_iconTable);
  mainLayout->addSpacing(4);

  // 点击选中
  connect(m_iconTable, &QTableWidget::cellClicked, this, [this](int, int) {
    auto *item = m_iconTable->currentItem();
    if (item && !item->toolTip().isEmpty()) {
      m_selectedIcon = item->toolTip();
      m_selectedLabel->setText(QStringLiteral("已选: %1").arg(m_selectedIcon));
    }
  });

  // 双击确认
  connect(m_iconTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
    auto *item = m_iconTable->currentItem();
    if (item && !item->toolTip().isEmpty()) {
      m_selectedIcon = item->toolTip();
      accept();
    }
  });

  // ── 翻页（页码按钮） + 当前选中显示 ──
  auto *bottomLayout = new QHBoxLayout;
  bottomLayout->setSpacing(4);

  m_prevBtn = new QPushButton(QStringLiteral("<"));
  m_prevBtn->setFixedSize(28, 28);
  m_nextBtn = new QPushButton(QStringLiteral(">"));
  m_nextBtn->setFixedSize(28, 28);

  connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
    if (m_currentPage > 0) {
      m_currentPage--;
      fillPage();
    }
  });

  connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
    if (m_currentPage < m_totalPages - 1) {
      m_currentPage++;
      fillPage();
    }
  });

  bottomLayout->addWidget(m_prevBtn);

  m_pageBtnLayout = new QHBoxLayout;
  m_pageBtnLayout->setSpacing(2);
  bottomLayout->addLayout(m_pageBtnLayout);

  bottomLayout->addWidget(m_nextBtn);
  bottomLayout->addSpacing(12);

  m_selectedLabel =
      new QLabel(m_selectedIcon.isEmpty() ? QStringLiteral("未选择")
                                          : QStringLiteral("已选: %1").arg(m_selectedIcon));
  bottomLayout->addWidget(m_selectedLabel);
  bottomLayout->addStretch();
  mainLayout->addLayout(bottomLayout);
  mainLayout->addSpacing(4);

  // ── 底部按钮 ──
  auto *btnLayout = new QHBoxLayout;
  btnLayout->addStretch();
  auto *clearBtn = new QPushButton(QStringLiteral("清除"));
  auto *okBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kConfirm));
  auto *cancelBtn = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kCancel));
  clearBtn->setMinimumWidth(80);
  okBtn->setMinimumWidth(80);
  cancelBtn->setMinimumWidth(80);
  AuiButton::applyDialogButtonStyle(clearBtn);
  AuiButton::applyDialogButtonStyle(okBtn);
  AuiButton::applyDialogButtonStyle(cancelBtn);
  connect(clearBtn, &QPushButton::clicked, this, [this]() {
    m_selectedIcon.clear();
    accept();
  });
  connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
  connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(clearBtn);
  btnLayout->addSpacing(8);
  btnLayout->addWidget(okBtn);
  btnLayout->addSpacing(8);
  btnLayout->addWidget(cancelBtn);
  mainLayout->addLayout(btnLayout);

  AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);
}

// ════════════════════════════════════════════════════════════
//  标签切换
// ════════════════════════════════════════════════════════════

void IconPickerDialog::switchTab(int tabIndex) {
  m_currentTab = tabIndex;
  m_currentPage = 0;
  m_searchEdit->clear();
  fillPage();
}

// ════════════════════════════════════════════════════════════
//  填充图标网格
// ════════════════════════════════════════════════════════════

void IconPickerDialog::fillPage() {
  // 获取当前标签页的完整图标列表
  QStringList allIcons;
  switch (m_currentTab) {
    case 0:
      allIcons = getEpIcons();
      break;
    case 1:
      allIcons = getAntIcons();
      break;
    case 2:
      allIcons = getTdIcons();
      break;
  }

  // 搜索过滤
  QString filter = m_searchEdit->text().trimmed().toLower();
  if (filter.isEmpty()) {
    m_filteredIcons = allIcons;
  } else {
    m_filteredIcons.clear();
    for (const QString &icon : allIcons) {
      if (IconLoader::extractShortName(icon).toLower().contains(filter)) {
        m_filteredIcons.append(icon);
      }
    }
  }

  // 计算分页
  m_totalPages = qMax(1, (m_filteredIcons.size() + kPageSize - 1) / kPageSize);
  if (m_currentPage >= m_totalPages) m_currentPage = m_totalPages - 1;

  int startIdx = m_currentPage * kPageSize;
  int endIdx = qMin(startIdx + kPageSize, m_filteredIcons.size());
  int count = endIdx - startIdx;

  // 更新翻页按钮状态
  m_prevBtn->setEnabled(m_currentPage > 0);
  m_nextBtn->setEnabled(m_currentPage < m_totalPages - 1);

  // 重建页码按钮
  rebuildPageButtons();

  // 填充表格
  m_iconTable->setRowCount(0);
  m_iconTable->setRowCount(kRows);
  for (int r = 0; r < kRows; ++r) {
    m_iconTable->setRowHeight(r, kCellSize);
  }

  IconLoader &loader = IconLoader::instance();

  // 收集需要异步加载的图标
  QStringList iconsToLoad;

  for (int i = 0; i < count; ++i) {
    int row = i / kCols;
    int col = i % kCols;
    const QString &iconName = m_filteredIcons[startIdx + i];

    auto *item = new QTableWidgetItem();
    item->setToolTip(iconName);

    QIcon cached = loader.cached(iconName);
    if (!cached.isNull()) {
      item->setIcon(cached);
    } else {
      item->setIcon(loader.getOrCreateIcon(iconName, kIconSize));
      iconsToLoad.append(iconName);
    }
    item->setTextAlignment(Qt::AlignCenter);

    m_iconTable->setItem(row, col, item);
  }

  // 清空多余单元格
  for (int i = count; i < kPageSize; ++i) {
    int row = i / kCols;
    int col = i % kCols;
    auto *item = new QTableWidgetItem();
    item->setFlags(Qt::NoItemFlags);
    m_iconTable->setItem(row, col, item);
  }

  // 异步加载真实图标
  if (!iconsToLoad.isEmpty()) {
    loader.requestIcons(iconsToLoad, [this](const QString &iconName) {
      QIcon icon = IconLoader::instance().cached(iconName);
      if (icon.isNull()) return;
      // 更新表格中对应项
      for (int r = 0; r < m_iconTable->rowCount(); ++r) {
        for (int c = 0; c < kCols; ++c) {
          auto *item = m_iconTable->item(r, c);
          if (item && item->toolTip() == iconName) {
            item->setIcon(icon);
            item->setTextAlignment(Qt::AlignCenter);
            return;
          }
        }
      }
    });
  }
}

// ════════════════════════════════════════════════════════════
//  页码按钮
// ════════════════════════════════════════════════════════════

void IconPickerDialog::rebuildPageButtons() {
  // 清除旧按钮
  for (auto *btn : m_pageButtons) {
    m_pageBtnLayout->removeWidget(btn);
    delete btn;
  }
  m_pageButtons.clear();

  if (m_totalPages <= 1) return;

  // 显示最多 9 个页码按钮，当前页居中
  int maxVisible = 9;
  int start, end;
  if (m_totalPages <= maxVisible) {
    start = 0;
    end = m_totalPages - 1;
  } else {
    int half = maxVisible / 2;
    start = qMax(0, m_currentPage - half);
    end = start + maxVisible - 1;
    if (end >= m_totalPages) {
      end = m_totalPages - 1;
      start = qMax(0, end - maxVisible + 1);
    }
  }

  for (int i = start; i <= end; ++i) {
    auto *btn = new QPushButton(QString::number(i + 1));
    btn->setFixedSize(28, 28);
    btn->setCheckable(true);
    btn->setChecked(i == m_currentPage);
    btn->setStyleSheet(i == m_currentPage
                           ? QStringLiteral("QPushButton { font-weight: bold; border: none; }")
                           : QStringLiteral("QPushButton { border: none; }"));
    int page = i;
    connect(btn, &QPushButton::clicked, this, [this, page]() {
      m_currentPage = page;
      fillPage();
    });
    m_pageBtnLayout->addWidget(btn);
    m_pageButtons.append(btn);
  }
}

// ════════════════════════════════════════════════════════════
//  返回选中结果
// ════════════════════════════════════════════════════════════

QString IconPickerDialog::selectedIcon() const { return m_selectedIcon; }