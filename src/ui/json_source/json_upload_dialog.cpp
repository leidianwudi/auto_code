/**
 * @file json_upload_dialog.cpp
 * @brief .jsonupload 单条上传预设编辑对话框实现
 */

#include "json_upload_dialog.h"

#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "src/ui/json_vue/config_dialog_common.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_combo_box.h"

// ════════════════════════════════════════════════════════════
//  附加参数表格列索引
// ════════════════════════════════════════════════════════════

namespace {
enum ParamCols {
  ParamColName = 0,   ///< 参数名
  ParamColValue,      ///< 参数值
  ParamColValueType,  ///< 值类型（下拉：字符串/数字）
  ParamColDelete,     ///< 操作（删除按钮）
  ParamColCount
};

/// 值类型下拉选项 → 存储标记（""=字符串 / "number"=数字）
inline QString comboDataToValueType(const QComboBox *combo) {
  return combo ? combo->currentData().toString() : QString();
}
}  // namespace

// ════════════════════════════════════════════════════════════
//  构造 / 界面构建
// ════════════════════════════════════════════════════════════

JsonUploadDialog::JsonUploadDialog(QWidget *parent) : QDialog(parent) { setupUI(); }

void JsonUploadDialog::setupUI() {
  ConfigDialogFrame frame =
      beginConfigDialog(this, QStringLiteral("上传预设配置"), QMargins(12, 10, 12, 10), 6);
  auto *layout = frame.contentLayout;

  auto *form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  form->setSpacing(6);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  layout->addLayout(form);

  m_remarkEdit = new QLineEdit(this);
  m_remarkEdit->setPlaceholderText(QStringLiteral("如: 商品主图上传"));
  form->addRow(QStringLiteral("说明:"), m_remarkEdit);

  m_urlEdit = new QLineEdit(this);
  m_urlEdit->setPlaceholderText(QStringLiteral("如 upload/image（相对路径，走目标项目 axios）"));
  form->addRow(QStringLiteral("上传地址:"), m_urlEdit);

  m_methodCombo = AuiComboBox::create(this);
  m_methodCombo->addItem(QStringLiteral("POST"), QStringLiteral("POST"));
  m_methodCombo->addItem(QStringLiteral("PUT"), QStringLiteral("PUT"));
  m_methodCombo->addItem(QStringLiteral("GET"), QStringLiteral("GET"));
  form->addRow(QStringLiteral("请求方式:"), m_methodCombo);

  m_fileFieldEdit = new QLineEdit(this);
  m_fileFieldEdit->setPlaceholderText(QStringLiteral("form-data 中文件字段名，默认 file"));
  m_fileFieldEdit->setText(QStringLiteral("file"));
  form->addRow(QStringLiteral("文件字段名:"), m_fileFieldEdit);

  m_responsePathEdit = new QLineEdit(this);
  m_responsePathEdit->setPlaceholderText(QStringLiteral("响应中图片路径的点分路径，默认 data.url"));
  m_responsePathEdit->setText(QStringLiteral("data.url"));
  form->addRow(QStringLiteral("响应提取路径:"), m_responsePathEdit);

  m_maxCountCombo = createNumericCombo(this, {1, 3, 5, 10, 20}, 1);
  m_maxCountCombo->setEditable(true);
  form->addRow(QStringLiteral("最多张数:"), m_maxCountCombo);

  m_valueTypeCombo = AuiComboBox::create(this);
  m_valueTypeCombo->addItem(QStringLiteral("自动（按张数推导）"), QString());
  m_valueTypeCombo->addItem(QStringLiteral("字符串（单值）"),
                            QString::fromLatin1(JsonUploadValueType::kString));
  m_valueTypeCombo->addItem(QStringLiteral("字符串数组（多图）"),
                            QString::fromLatin1(JsonUploadValueType::kArray));
  form->addRow(QStringLiteral("提交值形态:"), m_valueTypeCombo);

  // ── 缩略图显示尺寸（0=默认）──
  // 列表页与编辑/详情页的图片缩略图各自独立配置（宽高需同时 >0 才生效）
  auto makeThumbSpin = [this](QWidget *row, const QString &tip) {
    auto *spin = new QSpinBox(row);
    spin->setRange(0, 999);
    spin->setSpecialValueText(QStringLiteral("默认"));
    spin->setToolTip(tip);
    return spin;
  };
  const QString thumbTip = QStringLiteral("0=默认尺寸；宽高都为 0 时生效。建议 2:1 比例如 80×40");
  {
    // 列表缩略图行
    auto *listRow = new QWidget(this);
    auto *listLayout = new QHBoxLayout(listRow);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);
    m_listThumbWidthSpin = makeThumbSpin(listRow, QStringLiteral("列表页图片缩略图宽度，") + thumbTip);
    m_listThumbHeightSpin = makeThumbSpin(listRow, QStringLiteral("列表页图片缩略图高度，") + thumbTip);
    listLayout->addWidget(m_listThumbWidthSpin);
    listLayout->addWidget(new QLabel(QStringLiteral("×"), listRow));
    listLayout->addWidget(m_listThumbHeightSpin);
    listLayout->addStretch();
    form->addRow(QStringLiteral("列表缩略图宽高(px):"), listRow);

    // 编辑/详情缩略图行
    auto *editRow = new QWidget(this);
    auto *editLayout = new QHBoxLayout(editRow);
    editLayout->setContentsMargins(0, 0, 0, 0);
    editLayout->setSpacing(4);
    m_editThumbWidthSpin = makeThumbSpin(editRow, QStringLiteral("编辑/详情页图片卡片宽度，") + thumbTip);
    m_editThumbHeightSpin = makeThumbSpin(editRow, QStringLiteral("编辑/详情页图片卡片高度，") + thumbTip);
    editLayout->addWidget(m_editThumbWidthSpin);
    editLayout->addWidget(new QLabel(QStringLiteral("×"), editRow));
    editLayout->addWidget(m_editThumbHeightSpin);
    editLayout->addStretch();
    form->addRow(QStringLiteral("编辑缩略图宽高(px):"), editRow);
  }

  // ── 附加 form 参数表格 ──
  auto *paramHeader = new QHBoxLayout;
  auto *paramLabel = new QLabel(QStringLiteral("附加 form 参数:"), this);
  paramHeader->addWidget(paramLabel);
  paramHeader->addStretch();
  m_addParamBtn = makeCompactButton(QStringLiteral("+ 添加参数"), this);
  paramHeader->addWidget(m_addParamBtn);
  layout->addLayout(paramHeader);

  m_paramsTable = makeConfigTable(
      {{QStringLiteral("参数名"), QHeaderView::Stretch, 0},
       {QStringLiteral("参数值"), QHeaderView::Stretch, 0},
       {QStringLiteral("值类型"), QHeaderView::Interactive, 90},
       {QString::fromUtf8(CodeConstants::UiText::kDelete), QHeaderView::Interactive, 60}},
      this, 80, 160, QAbstractItemView::SelectRows);
  m_paramsTable->setMinimumWidth(360);
  layout->addWidget(m_paramsTable);

  connect(m_addParamBtn, &QPushButton::clicked, this, [this]() {
    const int row = m_paramsTable->rowCount();
    m_paramsTable->insertRow(row);
    m_paramsTable->setItem(row, ParamColName, new QTableWidgetItem(QString()));
    m_paramsTable->setItem(row, ParamColValue, new QTableWidgetItem(QString()));
    auto *typeCombo = AuiComboBox::create(this);
    typeCombo->addItem(QStringLiteral("字符串"), QString());
    typeCombo->addItem(QStringLiteral("数字"), QStringLiteral("number"));
    m_paramsTable->setCellWidget(row, ParamColValueType, typeCombo);
    m_paramsTable->setCellWidget(row, ParamColDelete,
                                 makeTableDeleteButton(m_paramsTable, ParamColDelete, this));
  });

  finishConfigDialog(this, frame);
  setMinimumSize(460, 520);
}

// ════════════════════════════════════════════════════════════
//  附加参数表格填充 / 收集
// ════════════════════════════════════════════════════════════

void JsonUploadDialog::fillParams(const QVector<JsonUploadParam> &params) {
  m_paramsTable->setRowCount(0);
  for (const auto &p : params) {
    const int row = m_paramsTable->rowCount();
    m_paramsTable->insertRow(row);
    m_paramsTable->setItem(row, ParamColName, new QTableWidgetItem(p.name));
    m_paramsTable->setItem(row, ParamColValue, new QTableWidgetItem(p.value));
    auto *typeCombo = AuiComboBox::create(this);
    typeCombo->addItem(QStringLiteral("字符串"), QString());
    typeCombo->addItem(QStringLiteral("数字"), QStringLiteral("number"));
    comboSelectData(typeCombo, p.valueType);
    m_paramsTable->setCellWidget(row, ParamColValueType, typeCombo);
    m_paramsTable->setCellWidget(row, ParamColDelete,
                                 makeTableDeleteButton(m_paramsTable, ParamColDelete, this));
  }
}

QVector<JsonUploadParam> JsonUploadDialog::collectParams() const {
  QVector<JsonUploadParam> out;
  for (int r = 0; r < m_paramsTable->rowCount(); ++r) {
    JsonUploadParam p;
    const QTableWidgetItem *nameItem = m_paramsTable->item(r, ParamColName);
    const QTableWidgetItem *valueItem = m_paramsTable->item(r, ParamColValue);
    p.name = nameItem ? nameItem->text().trimmed() : QString();
    p.value = valueItem ? valueItem->text() : QString();
    p.valueType = comboDataToValueType(
        qobject_cast<QComboBox *>(m_paramsTable->cellWidget(r, ParamColValueType)));
    if (p.name.isEmpty()) continue;  // 跳过未填参数名的行
    out.append(p);
  }
  return out;
}

// ════════════════════════════════════════════════════════════
//  预填 / 取回
// ════════════════════════════════════════════════════════════

void JsonUploadDialog::setUpload(const JsonUpload &u) {
  m_remarkEdit->setText(u.remark);
  m_urlEdit->setText(u.url);
  comboSelectData(m_methodCombo, u.method);
  m_fileFieldEdit->setText(u.fileField.isEmpty() ? QStringLiteral("file") : u.fileField);
  m_responsePathEdit->setText(u.responsePath.isEmpty() ? QStringLiteral("data.url")
                                                       : u.responsePath);
  // maxCount：可编辑下拉，未命中预设值时填入编辑框
  m_maxCountCombo->setEditText(QString::number(u.maxCount < 1 ? 1 : u.maxCount));
  comboSelectData(m_valueTypeCombo, u.valueType);
  m_listThumbWidthSpin->setValue(qMax(0, u.listThumbWidth));
  m_listThumbHeightSpin->setValue(qMax(0, u.listThumbHeight));
  m_editThumbWidthSpin->setValue(qMax(0, u.editThumbWidth));
  m_editThumbHeightSpin->setValue(qMax(0, u.editThumbHeight));
  fillParams(u.params);
}

JsonUpload JsonUploadDialog::upload() const {
  JsonUpload u;
  u.remark = m_remarkEdit->text().trimmed();
  u.url = m_urlEdit->text().trimmed();
  u.method = m_methodCombo->currentData().toString();
  u.fileField = m_fileFieldEdit->text().trimmed();
  if (u.fileField.isEmpty()) u.fileField = QStringLiteral("file");
  u.responsePath = m_responsePathEdit->text().trimmed();
  if (u.responsePath.isEmpty()) u.responsePath = QStringLiteral("data.url");
  u.maxCount = qMax(1, qRound(m_maxCountCombo->currentText().toDouble()));
  u.valueType = m_valueTypeCombo->currentData().toString();
  u.listThumbWidth = m_listThumbWidthSpin->value();
  u.listThumbHeight = m_listThumbHeightSpin->value();
  u.editThumbWidth = m_editThumbWidthSpin->value();
  u.editThumbHeight = m_editThumbHeightSpin->value();
  u.params = collectParams();
  return u;
}
