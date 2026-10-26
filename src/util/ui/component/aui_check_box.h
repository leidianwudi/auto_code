/**
 * @file aui_check_box.h
 * @brief 居中绘制的复选框（表格"纯勾选"列的 cell widget 用）
 */

#pragma once

#include <QCheckBox>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>

/**
 * @class AuiCenterCheckBox
 * @brief 指示器水平居中的复选框
 *
 * QCheckBox 的指示器默认贴控件左缘；作为表格纯勾选列的 cell widget 时，
 * 控件会被拉伸到整格，指示器便偏向左侧、视觉失衡。本控件（用于无文字
 * 场景）把指示器绘制在水平中心、垂直仍居中；点击命中保持整个控件矩形
 * ——cell 拉伸后即整格可点，交互面积不受影响。
 *
 * 注意：仅适合"无文字纯勾选"场景；带文字的复选框应使用普通 QCheckBox
 * （文字与指示器成组靠左是标准形态）。
 */
class AuiCenterCheckBox : public QCheckBox {
 public:
  using QCheckBox::QCheckBox;

 protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    QPainter p(this);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    const int iw = style()->pixelMetric(QStyle::PM_IndicatorWidth, &opt, this);
    const int ih = style()->pixelMetric(QStyle::PM_IndicatorHeight, &opt, this);
    opt.rect = QRect((width() - iw) / 2, (height() - ih) / 2, iw, ih);
    style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &opt, &p, this);
  }
};
