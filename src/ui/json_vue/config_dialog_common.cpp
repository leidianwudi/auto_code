/**
 * @file config_dialog_common.cpp
 * @brief json_vue 配置对话框公共工具实现
 */

#include "config_dialog_common.h"

#include <QComboBox>
#include <QDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "src/util/common/code_constants.h"
#include "src/util/ui/aui_window.h"
#include "src/util/ui/component/aui_button.h"
#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  无边框对话框框架
// ════════════════════════════════════════════════════════════

ConfigDialogFrame beginConfigDialog(QDialog *dialog, const QString &title, const QMargins &margins,
                                    int spacing) {
  dialog->setWindowTitle(title);
  AuiWindow::setupFramelessDialog(dialog);

  TitleBarOptions opts;
  opts.title = title;
  opts.showMinButton = false;
  opts.showMaxButton = false;
  opts.closeRejectsDialog = true;
  TitleBarResult tb = AuiWindow::createTitleBar(dialog, opts);

  ConfigDialogFrame frame;
  frame.titleBar = tb.titleBar;
  frame.contentWidget = new QWidget;
  frame.contentLayout = new QVBoxLayout(frame.contentWidget);
  frame.contentLayout->setContentsMargins(margins);
  frame.contentLayout->setSpacing(spacing);
  return frame;
}

void finishConfigDialog(QDialog *dialog, const ConfigDialogFrame &frame) {
  // 底部按钮在业务内容之后添加，保证显示在对话框底部
  auto btns = AuiButton::createDialogButtons(dialog);
  // 确定 → accept，取消 → reject（此前遗漏连接，导致三个配置对话框按钮无效）
  QObject::connect(btns.okBtn, &QPushButton::clicked, dialog, &QDialog::accept);
  if (btns.cancelBtn)
    QObject::connect(btns.cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
  frame.contentLayout->addLayout(btns.layout);
  AuiWindow::applyWindowFrame(dialog, frame.titleBar, frame.contentWidget);
}

// ════════════════════════════════════════════════════════════
//  下拉框工具
// ════════════════════════════════════════════════════════════

void comboSelectData(QComboBox *combo, const QVariant &data, int fallback) {
  if (!combo) return;
  int idx = combo->findData(data);
  if (idx < 0) idx = fallback;
  if (idx >= 0) combo->setCurrentIndex(idx);
}

QComboBox *createNumericCombo(QWidget *parent, const QList<double> &presetValues, double current) {
  auto *combo = new QComboBox(parent);
  combo->setEditable(true);
  for (double v : presetValues) combo->addItem(QString::number(v), v);
  int idx = combo->findData(current);
  if (idx >= 0) {
    combo->setCurrentIndex(idx);
  } else {
    combo->setEditText(QString::number(current));
  }
  return combo;
}

double numericComboValue(const QComboBox *combo, double fallback) {
  if (!combo) return fallback;
  QVariant d = combo->currentData();
  return d.isValid() ? d.toDouble() : combo->currentText().toDouble();
}

// ════════════════════════════════════════════════════════════
//  表格工具
// ════════════════════════════════════════════════════════════

QPushButton *makeCompactButton(const QString &text, QWidget *parent) {
  auto *btn = new QPushButton(text, parent);
  btn->setStyleSheet(
      QStringLiteral("QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
                     "  padding: 2px 6px; font-size: 12px;"
                     "}"
                     "QPushButton:hover { background: %3; }"
                     "QPushButton:disabled { color: %4; background: %1; }")
          .arg(AuiStyle::background().name(), AuiStyle::borderColor().name(),
               AuiStyle::hoverBackground().name(), AuiStyle::mutedTextColor().name()));
  return btn;
}

QPushButton *makeTableDeleteButton(QTableWidget *table, int column, QWidget *parent) {
  auto *btn = makeCompactButton(QString::fromUtf8(CodeConstants::UiText::kDelete), parent);
  QObject::connect(btn, &QPushButton::clicked, table, [table, column, btn]() {
    for (int r = 0; r < table->rowCount(); ++r) {
      if (table->cellWidget(r, column) == btn) {
        table->removeRow(r);
        break;
      }
    }
  });
  return btn;
}
