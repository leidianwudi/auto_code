/**
 * @file aui_message_box.cpp
 * @brief 消息对话框工具类实现
 */

#include "aui_message_box.h"

#include <QCursor>
#include <QDialog>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include "../aui_window.h"
#include "aui_button.h"
#include "aui_style.h"

namespace {

/// 消息对话框三选结果
enum class MsgChoice { kFirst, kSecond, kCancel };

/// TextAlign → Qt 对齐标志（垂直方向始终保持居中）
inline Qt::Alignment toQtAlignment(AuiMessageBox::TextAlign align) {
  return align == AuiMessageBox::TextAlign::kCenter
             ? Qt::Alignment(Qt::AlignCenter)
             : Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter);
}

/// 左对齐模式下为每个段落（非空行）首行加两个全角空格缩进（中文排版首行缩进两字习惯）
QString indentParagraphs(const QString &text) {
  const QString kIndent = QStringLiteral("\u3000\u3000");  // 全角空格 ×2 = 两个汉字宽
  QStringList lines = text.split(QLatin1Char('\n'));
  for (QString &line : lines) {
    if (!line.trimmed().isEmpty()) line.prepend(kIndent);
  }
  return lines.join(QLatin1Char('\n'));
}

/// 消息对话框 — 私有实现，不暴露给外部
class MessageBoxDialog : public QDialog {
public:
  MessageBoxDialog(const QString &title, const QString &text, bool showCancel, QWidget *parent,
                   AuiMessageBox::TextAlign align = AuiMessageBox::TextAlign::kLeft)
      : QDialog(parent), m_showCancel(showCancel), m_textAlign(toQtAlignment(align)) {
    setupUI(title, text);
  }

  /// 三选对话框：首按钮 / 次按钮 / 取消
  MessageBoxDialog(const QString &title, const QString &text, const QString &firstText,
                   const QString &secondText, QWidget *parent,
                   AuiMessageBox::TextAlign align = AuiMessageBox::TextAlign::kLeft)
      : QDialog(parent),
        m_threeButtons(true),
        m_firstText(firstText),
        m_secondText(secondText),
        m_textAlign(toQtAlignment(align)) {
    setupUI(title, text);
  }

  /// 三选结果（仅三选对话框有效；两键对话框固定为 kCancel 以外的值由 accept/reject 判定）
  MsgChoice choice() const { return m_choice; }

private:
  void setupUI(const QString &title, const QString &text) {
    setWindowTitle(title);

    AuiWindow::setupFramelessDialog(this);

    TitleBarOptions opts;
    opts.title = title;
    opts.showMinButton = false;
    opts.showMaxButton = false;
    opts.closeRejectsDialog = true;
    auto tb = AuiWindow::createTitleBar(this, opts);

    auto *contentWidget = new QWidget;
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(16, 12, 16, 12);
    contentLayout->setSpacing(5);

    // 左对齐时按中文排版习惯为每个段落首行加两个汉字缩进；居中模式保持原样
    const QString bodyText = m_textAlign.testFlag(Qt::AlignLeft) ? indentParagraphs(text) : text;

    auto *label = new QLabel(bodyText, this);
    label->setWordWrap(true);
    label->setAlignment(m_textAlign);
    // 允许鼠标/键盘选中文本，方便复制错误内容
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    contentLayout->addWidget(label);

    QHBoxLayout *btnLayout = nullptr;
    if (m_threeButtons) {
      btnLayout = setupThreeButtons();
    } else {
      auto btns = AuiButton::createDialogButtons(this, m_showCancel);
      connect(btns.okBtn, &QPushButton::clicked, this, &QDialog::accept);
      if (btns.cancelBtn) connect(btns.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
      btnLayout = btns.layout;
      btns.okBtn->setFocus();
    }
    contentLayout->addLayout(btnLayout);

    AuiWindow::applyWindowFrame(this, tb.titleBar, contentWidget);

    applyAutoSize(bodyText, label, contentLayout, tb.titleBar, btnLayout);
  }

  /// 三选按钮行：首（默认高亮）/ 次 / 取消
  QHBoxLayout *setupThreeButtons() {
    auto *layout = new QHBoxLayout;
    layout->addStretch();

    auto *first = new QPushButton(m_firstText, this);
    first->setMinimumWidth(80);
    first->setDefault(true);
    AuiButton::applyDialogButtonStyle(first);
    connect(first, &QPushButton::clicked, this, [this]() {
      m_choice = MsgChoice::kFirst;
      accept();
    });
    layout->addWidget(first);

    layout->addSpacing(12);
    auto *second = new QPushButton(m_secondText, this);
    second->setMinimumWidth(80);
    AuiButton::applyDialogButtonStyle(second);
    connect(second, &QPushButton::clicked, this, [this]() {
      m_choice = MsgChoice::kSecond;
      accept();
    });
    layout->addWidget(second);

    layout->addSpacing(12);
    auto *cancel = new QPushButton(QString::fromUtf8(CodeConstants::UiText::kCancel), this);
    cancel->setMinimumWidth(80);
    AuiButton::applyDialogButtonStyle(cancel);
    connect(cancel, &QPushButton::clicked, this, [this]() {
      m_choice = MsgChoice::kCancel;
      reject();
    });
    layout->addWidget(cancel);

    layout->addStretch();
    first->setFocus();
    return layout;
  }

  /// 根据内容自动缩放窗口：内容少时缩小，避免固定大窗口留白过多；内容多时受 maxW 约束并换行。
  void applyAutoSize(const QString &text, QLabel *label, QLayout *contentLayout, QWidget *titleBar,
                     QLayout *btnsLayout) {
    constexpr int kMinW = 400;       // 最小宽度
    constexpr int kMaxW = 620;       // 最大宽度（超长文本在此宽度内换行）
    constexpr int frameHMargin = 2;  // WindowFrame 左右边框 1px×2
    constexpr int frameVMargin = 1;  // WindowFrame 底部边框 1px（顶部被标题栏覆盖）

    const QMargins cm = contentLayout->contentsMargins();
    const int hMargins = cm.left() + cm.right();                 // 水平内边距
    const int vMargins = cm.top() + cm.bottom() + frameVMargin;  // 垂直内边距 + 底部边框

    // 计算文本在多行情况下的最宽单行宽度，作为理想内容宽度
    QStringList lines = text.split(QLatin1Char('\n'));
    QFontMetrics fm = label->fontMetrics();
    int maxLineW = 0;
    for (const QString &line : lines) maxLineW = qMax(maxLineW, fm.horizontalAdvance(line));

    // 文本可用宽度：介于下限与上限之间（不足下限则宁可在下限宽度内换行居中）
    const int availTextW =
        qBound(kMinW - frameHMargin - hMargins, maxLineW, kMaxW - frameHMargin - hMargins);

    // 固定文本渲染宽度并据此取换行后高度
    label->setFixedWidth(availTextW);
    int textH = label->heightForWidth(availTextW);
    if (textH <= 0) textH = fm.height() * qMax(1, lines.size());

    const int titleH = titleBar->sizeHint().height();
    const int buttonsH = btnsLayout->sizeHint().height();
    const int spacing = contentLayout->spacing();

    int desiredW = qBound(kMinW, availTextW + frameHMargin + hMargins, kMaxW);
    int desiredH = titleH + vMargins + textH + spacing + buttonsH + 6;  // 额外 6px 视觉留白

    // 防止在低分辨率屏幕上超高
    if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
      desiredH = qMin(desiredH, screen->availableGeometry().height());
    }

    resize(desiredW, desiredH);
  }

  bool m_showCancel = false;
  bool m_threeButtons = false;                                   ///< 是否为三选对话框
  QString m_firstText;                                           ///< 三选：首按钮文字
  QString m_secondText;                                          ///< 三选：次按钮文字
  Qt::Alignment m_textAlign = Qt::AlignLeft | Qt::AlignVCenter;  ///< 正文对齐方式
  MsgChoice m_choice = MsgChoice::kCancel;                       ///< 三选结果
};

}  // namespace

void AuiMessageBox::show(QWidget *parent, const QString &title, const QString &text,
                         TextAlign align) {
  MessageBoxDialog dlg(title, text, false, parent, align);
  dlg.exec();
}

bool AuiMessageBox::confirm(QWidget *parent, const QString &title, const QString &text,
                            TextAlign align) {
  MessageBoxDialog dlg(title, text, true, parent, align);
  return dlg.exec() == QDialog::Accepted;
}

AuiMessageBox::Choice AuiMessageBox::question3(QWidget *parent, const QString &title,
                                               const QString &text, const QString &firstText,
                                               const QString &secondText, TextAlign align) {
  MessageBoxDialog dlg(title, text, firstText, secondText, parent, align);
  dlg.exec();
  switch (dlg.choice()) {
    case MsgChoice::kFirst:
      return Choice::kFirst;
    case MsgChoice::kSecond:
      return Choice::kSecond;
    default:
      return Choice::kCancel;
  }
}