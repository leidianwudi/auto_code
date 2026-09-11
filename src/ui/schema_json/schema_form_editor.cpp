/**
 * @file schema_form_editor.cpp
 * @brief 通用 schema 驱动的 JSON 可视化表单编辑器实现
 */

#include "schema_form_editor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

#include "src/util/common/code_constants.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_combo_box.h"

/// 让标签文本可选中复制（QLabel 默认不可选，不便于复制字段名/说明/标题）
static inline void makeSelectable(QLabel *l) {
  if (l) l->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

// ════════════════════════════════════════════════════════════
//  构造 / 加载
// ════════════════════════════════════════════════════════════

SchemaFormEditor::SchemaFormEditor(QWidget *parent) : QWidget(parent) {
  auto *scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  m_content = new QWidget;
  new QVBoxLayout(m_content);
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->addWidget(scroll);
  scroll->setWidget(m_content);
  // 顶部留一小段空隙，视觉上不与编辑区贴死
  static_cast<QVBoxLayout *>(m_content->layout())->setContentsMargins(12, 12, 12, 12);
  static_cast<QVBoxLayout *>(m_content->layout())->setSpacing(8);
}

void SchemaFormEditor::setSchema(const SchemaValidator &schema) {
  m_schema = schema;
  m_renderHash.clear();  // schema 变化后强制重建表单，避免命中旧哈希跳过
  // 无条件重建：schema 失效（无 root）时也要清掉旧表单，显示「未找到可用的 schema」提示
  rebuild();
}

void SchemaFormEditor::applySchema(const SchemaValidator &schema, const QString &schemaRef,
                                   bool skeletonIfEmpty) {
  const bool wasEmpty = m_root.isEmpty();
  m_schema = schema;
  m_renderHash.clear();
  if (!schemaRef.isEmpty()) m_root[QStringLiteral("$schema")] = schemaRef;
  // 根对象为空 → 按必填字段生成默认值骨架，让表单立即有完整结构可编辑
  if (skeletonIfEmpty && wasEmpty && m_schema.hasRoot())
    fillRequiredSkeleton(m_schema.rootClassName(), QStringList());
  rebuild();
  emit contentChanged();
}

void SchemaFormEditor::loadJson(const QJsonObject &root) {
  // 内容未变化时跳过整树重建（切换代码/可视化来回点击时避免重复构建表单）
  const QByteArray hash = UtilJson::fingerprint(root);
  if (hash == m_renderHash) {
    m_root = root;
    return;
  }
  m_root = root;
  m_renderHash = hash;
  rebuild();
}

void SchemaFormEditor::rebuild() {
  m_busy = true;
  QLayout *lay = m_content->layout();
  // 清空旧表单
  while (QLayoutItem *item = lay->takeAt(0)) {
    if (item->widget()) item->widget()->deleteLater();
    delete item;
  }

  if (!m_schema.hasRoot()) {
    auto *hint = new QLabel(QStringLiteral("未找到可用的 schema（根类未定义）"));
    makeSelectable(hint);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    m_busy = false;
    return;
  }

  QString title;
  QWidget *form = buildObjectForm(m_schema.rootClassName(), QStringList(), &title);
  if (form) lay->addWidget(form);
  if (auto *vb = qobject_cast<QVBoxLayout *>(lay)) vb->addStretch(1);
  m_busy = false;
}

// ════════════════════════════════════════════════════════════
//  JSON 路径读写（在 m_root 上原地增删改）
// ════════════════════════════════════════════════════════════

namespace {
/// 判断路径段是否为数组索引（用于分支 object/array 导航）
inline bool isIndexSegment(const QString &seg) {
  bool ok = false;
  seg.toInt(&ok);
  return ok;
}

/// 在容器的 JSON 值里递归写入叶子值；遇到缺失的中间层按对象补
QJsonValue writeBackRecursive(const QJsonValue &node, const QStringList &path, int idx,
                              const QJsonValue &value) {
  if (idx == path.size()) return value.isUndefined() ? QJsonValue(QJsonValue::Null) : value;
  const QString &seg = path[idx];
  if (node.isArray()) {
    QJsonArray a = node.toArray();
    bool ok = false;
    int i = seg.toInt(&ok);
    if (ok && i >= 0 && i < a.size()) {
      QJsonValue child = a.at(i);
      a[i] = writeBackRecursive(child.isUndefined() ? QJsonValue(QJsonObject()) : child, path,
                                idx + 1, value);
    } else if (ok && i == a.size()) {
      // 数组末尾追加一个元素占位（新增数组项走 insertArrayItem，这里兜底）
      a.append(writeBackRecursive(QJsonValue(QJsonObject()), path, idx + 1, value));
    }
    return a;
  }
  // 对象节点
  QJsonObject o = node.isObject() ? node.toObject() : QJsonObject();
  QJsonValue child = o.value(seg);
  o.insert(seg, writeBackRecursive(child.isUndefined() ? QJsonValue(QJsonObject()) : child, path,
                                   idx + 1, value));
  return o;
}
}  // namespace

bool SchemaFormEditor::nodeAt(const QJsonObject &root, const QStringList &path, QJsonValue *out) {
  QJsonValue cur(root);
  for (const QString &seg : path) {
    if (isIndexSegment(seg) && cur.isArray()) {
      QJsonArray a = cur.toArray();
      bool ok = false;
      int i = seg.toInt(&ok);
      if (i < 0 || i >= a.size()) {
        if (out) *out = QJsonValue(QJsonValue::Undefined);
        return true;
      }
      cur = a.at(i);
    } else if (cur.isObject()) {
      QJsonObject o = cur.toObject();
      if (!o.contains(seg)) {
        if (out) *out = QJsonValue(QJsonValue::Undefined);
        return true;
      }
      cur = o.value(seg);
    } else {
      if (out) *out = QJsonValue(QJsonValue::Undefined);
      return true;
    }
  }
  if (out) *out = cur;
  return true;
}

void SchemaFormEditor::setNodeValue(const QStringList &path, const QJsonValue &value) {
  if (path.isEmpty()) {
    // 空路径 = 替换整个根对象（根级字段增删时 removeNodeValue 走到这里）
    if (value.isObject()) m_root = value.toObject();
  } else {
    m_root = writeBackRecursive(QJsonValue(m_root), path, 0, value).toObject();
  }
  if (!m_busy) emit contentChanged();
}

void SchemaFormEditor::removeNodeValue(const QStringList &path) {
  QStringList parentPath = path;
  const QString leaf = parentPath.takeLast();
  QJsonValue parent;
  nodeAt(m_root, parentPath, &parent);
  if (parent.isArray()) {
    QJsonArray a = parent.toArray();
    bool ok = false;
    int i = leaf.toInt(&ok);
    if (ok && i >= 0 && i < a.size()) a.removeAt(i);
    setNodeValue(parentPath, a);
  } else if (parent.isObject()) {
    QJsonObject o = parent.toObject();
    o.remove(leaf);
    setNodeValue(parentPath, o);
  }
}

void SchemaFormEditor::insertArrayItem(const QStringList &arrayPath, int index,
                                       const QJsonValue &element) {
  QJsonValue cur;
  nodeAt(m_root, arrayPath, &cur);
  QJsonArray a = cur.isArray() ? cur.toArray() : QJsonArray();
  int i = index < 0 ? a.size() : qMin(index, a.size());
  a.insert(i, element);
  setNodeValue(arrayPath, a);
  rebuild();
}

void SchemaFormEditor::removeArrayItem(const QStringList &arrayPath, int index) {
  QJsonValue cur;
  nodeAt(m_root, arrayPath, &cur);
  if (!cur.isArray()) return;
  QJsonArray a = cur.toArray();
  if (index >= 0 && index < a.size()) a.removeAt(index);
  setNodeValue(arrayPath, a);
  rebuild();
}

void SchemaFormEditor::moveArrayItem(const QStringList &arrayPath, int index, int delta) {
  QJsonValue cur;
  nodeAt(m_root, arrayPath, &cur);
  if (!cur.isArray()) return;
  QJsonArray a = cur.toArray();
  int to = index + delta;
  if (index < 0 || index >= a.size() || to < 0 || to >= a.size()) return;
  QJsonValue tmp = a.at(index);
  a[index] = a.at(to);
  a[to] = tmp;
  setNodeValue(arrayPath, a);
  rebuild();
}

// ════════════════════════════════════════════════════════════
//  UI 控件构造
// ════════════════════════════════════════════════════════════

namespace {

/// 字段标签列：字段名（+必填*）。说明文字通过悬停提示显示，不常驻界面，保持界面清爽
QWidget *makeLabelColumn(const QString &name, const QString &desc, bool required) {
  auto *col = new QWidget;
  auto *v = new QVBoxLayout(col);
  v->setContentsMargins(0, 2, 0, 2);
  v->setSpacing(2);
  auto *nameRow = new QHBoxLayout;
  nameRow->setContentsMargins(0, 0, 0, 0);
  nameRow->setSpacing(0);
  auto *key = new QLabel(name);
  key->setTextFormat(Qt::PlainText);
  makeSelectable(key);
  if (!desc.isEmpty()) key->setToolTip(desc);  // 鼠标悬停属性名时提示说明
  QFont f = key->font();
  f.setWeight(QFont::Medium);
  key->setFont(f);
  nameRow->addWidget(key);
  if (required) {
    auto *star = new QLabel(QStringLiteral("*"));
    star->setStyleSheet(QStringLiteral("color: #e8463a; font-weight: 600;"));
    nameRow->addWidget(star);
  }
  nameRow->addStretch(1);
  v->addLayout(nameRow);
  return col;
}

/// 小型图标按钮（数组行内增删/上移下移）
QPushButton *makeSMButton(const QString &text) {
  auto *b = new QPushButton(text);
  b->setFixedSize(20, 20);  // 与普通行输入框(~22px)同高量级，避免撑高数组/对象标题行
  b->setFocusPolicy(Qt::NoFocus);
  return b;
}

/// 依据 schema 属性类型生成"添加属性"时的默认值
QJsonValue defaultForType(const SchemaValidator::SchemaPropInfo &prop) {
  const QString &t = prop.type;
  if (t == QLatin1String("bool")) return QJsonValue(false);
  if (t == QLatin1String("int") || t == QLatin1String("double")) return QJsonValue(0);
  if (t == QLatin1String("array")) return QJsonValue(QJsonArray());
  if (t == QLatin1String("object")) return QJsonValue(QJsonObject());
  if (t == QLatin1String("string") && !prop.enumValues.isEmpty())
    return QJsonValue(prop.enumValues.first());  // enum 默认取第一个
  return QJsonValue(QString());                  // string / any / 未知 → 空串
}
}  // namespace

// 递归为类的必填字段生成默认值骨架（写入 m_root 对应路径）
void SchemaFormEditor::fillRequiredSkeleton(const QString &cls, const QStringList &path) {
  SchemaValidator::SchemaClassInfo info;
  if (!m_schema.classInfo(cls, &info)) return;
  for (const QString &name : info.required) {
    const SchemaValidator::SchemaPropInfo *p = info.findProperty(name);
    if (!p) continue;
    QStringList childPath = path;
    childPath.append(name);
    if (p->type == QLatin1String("object") && !p->className.isEmpty()) {
      // 嵌套对象：先建空对象占位，再继续填其必填字段
      setNodeValue(childPath, QJsonValue(QJsonObject()));
      fillRequiredSkeleton(p->className, childPath);
      continue;
    }
    setNodeValue(childPath, defaultForType(*p));
  }
}

QWidget *SchemaFormEditor::buildObjectForm(const QString &schemaClass, const QStringList &path,
                                           QString *titleOut, QWidget *titleActions,
                                           bool showTitle) {
  SchemaValidator::SchemaClassInfo info;
  if (!m_schema.classInfo(schemaClass, &info)) {
    // 类不存在：退化为「内嵌 JSON 兜底」编辑整个对象
    auto *box = new QFrame;
    auto *v = new QVBoxLayout(box);
    auto *tag = new QLabel(QStringLiteral("%1 · 未知类型，编辑原始 JSON").arg(schemaClass));
    makeSelectable(tag);
    tag->setProperty("muted", true);
    QFont df = tag->font();
    df.setPixelSize(12);
    tag->setFont(df);
    v->addWidget(tag);
    QJsonValue cur;
    nodeAt(m_root, path, &cur);
    v->addWidget(makeRawJsonFallback(path, cur));
    if (titleOut) *titleOut = schemaClass;
    return box;
  }

  auto *box = new QFrame;
  box->setObjectName(QStringLiteral("schemaObjectBox"));
  auto *v = new QVBoxLayout(box);
  // 较大左缩进区分嵌套层级（子元素更清晰）；根对象不缩进
  const int indent = path.isEmpty() ? 0 : 20;
  v->setContentsMargins(indent, 4, 6, 6);
  v->setSpacing(6);

  // 标题：嵌套对象显示"类名 + 路径摘要"；根对象不渲染标题行
  // （顶部已有「模板」行标识整个表单，根标题行冗余）。
  // showTitle=false（单值 object 字段，如 joinColumn）时不显示嵌套第二行标题，
  // 避免与外层字段名冗余成两行。
  if (showTitle && !path.isEmpty()) {
    const QString summary = schemaClass + QStringLiteral("[") + path.back() + QStringLiteral("]");
    auto *title = new QLabel(summary);
    makeSelectable(title);
    QFont tf = title->font();
    tf.setWeight(QFont::DemiBold);
    title->setFont(tf);
    // 标题行：类名居左，操作按钮（如数组对象的 ↑↓×）可挂在同一行右侧
    auto *titleRow = new QWidget;
    auto *titleLay = new QHBoxLayout(titleRow);
    titleLay->setContentsMargins(0, 0, 0, 0);
    titleLay->setSpacing(4);
    titleLay->addWidget(title);
    titleLay->addStretch(1);
    if (titleActions) titleLay->addWidget(titleActions);
    v->addWidget(titleRow);
    if (titleOut) *titleOut = summary;
  } else if (titleOut) {
    *titleOut = schemaClass;
  }

  // 当前对象在 m_root 中的实际键集合（用于判断已写/未写，以及 schema 未定义的未知键）
  QSet<QString> writtenKeys;
  QJsonValue self;
  nodeAt(m_root, path, &self);
  if (self.isObject()) {
    const QStringList keys = self.toObject().keys();
    writtenKeys.reserve(keys.size());
    for (const QString &k : keys) writtenKeys.insert(k);
  }

  // ── 1) 渲染已写 / 必填的 schema 属性（properties 已按 schema 声明顺序保存）──
  QStringList addable;  // 未写且非必填的属性名 → 放入"添加属性"下拉候选
  for (const auto &kv : info.properties) {
    const QString &pname = kv.first;
    if (pname.startsWith(QLatin1Char('$'))) continue;  // $schema 等元键不渲染为字段
    const bool hasWritten = writtenKeys.contains(pname);
    const bool isRequired = info.required.contains(pname);
    if (!hasWritten && !isRequired) {
      addable.append(pname);  // 未写且非必填 → 通过"添加属性"补齐
      continue;
    }
    auto *ctrl = buildPropertyControl(pname, kv.second, path, info.required);
    // 多行字段（array/object）：字段级移除按钮已放在标题行内（见 buildPropertyControl），
    // 此处不再包装，避免与数组行内 × 同排混淆。
    const bool multilineField =
        (kv.second.type == QLatin1String("array") || kv.second.type == QLatin1String("object"));
    // 已写且非多行的普通字段：附加外层移除按钮
    if (hasWritten && !multilineField) {
      auto *wrapRow = new QWidget;
      auto *hr = new QHBoxLayout(wrapRow);
      hr->setContentsMargins(0, 0, 0, 0);
      hr->setSpacing(4);
      hr->addWidget(ctrl, 1);
      auto *del = makeSMButton(QStringLiteral("×"));
      del->setToolTip(QStringLiteral("移除该字段"));
      connect(del, &QPushButton::clicked, this, [this, path, pname]() {
        removeNodeValue(propertyPath(path, pname));
        rebuild();
      });
      hr->addWidget(del);
      v->addWidget(wrapRow);
    } else {
      v->addWidget(ctrl);
    }
  }

  // ── 2) 未知字段（含 schema 未定义、用户自定义键）单独分组 ──
  QStringList unknownKeys;
  for (const QString &k : writtenKeys) {
    if (k.startsWith(QLatin1Char('$'))) continue;  // $ 元键省略
    if (info.hasProperty(k)) continue;             // schema 已定义
    unknownKeys.append(k);
  }
  if (!unknownKeys.isEmpty()) {
    auto *unknownBox = new QFrame;
    auto *uv = new QVBoxLayout(unknownBox);
    uv->setContentsMargins(8, 6, 8, 6);
    uv->setSpacing(2);
    auto *utag = new QLabel(QStringLiteral("未在 schema 中定义的字段"));
    makeSelectable(utag);
    QFont utf = utag->font();
    utf.setPixelSize(12);
    utag->setFont(utf);
    uv->addWidget(utag);
    for (const QString &uk : unknownKeys) {
      auto *urow = new QWidget;
      auto *uh = new QHBoxLayout(urow);
      uh->setContentsMargins(0, 0, 0, 0);
      uh->setSpacing(4);
      QJsonValue ucur;
      nodeAt(m_root, propertyPath(path, uk), &ucur);
      uh->addWidget(makeRawJsonFallback(propertyPath(path, uk), ucur), 1);
      auto *udel = makeSMButton(QStringLiteral("×"));
      udel->setToolTip(QStringLiteral("移除该字段"));
      connect(udel, &QPushButton::clicked, this, [this, path, uk]() {
        removeNodeValue(propertyPath(path, uk));
        rebuild();
      });
      uh->addWidget(udel);
      uv->addWidget(urow);
    }
    v->addWidget(unknownBox);
  }

  // ── 3) 添加属性下拉（从 schema 候选） ──
  if (!addable.isEmpty()) {
    std::sort(addable.begin(), addable.end());
    auto *addRow = new QWidget;
    auto *ah = new QHBoxLayout(addRow);
    ah->setContentsMargins(0, 0, 0, 0);
    ah->setSpacing(6);
    auto *addLabel = new QLabel(QStringLiteral("＋ 添加属性:"));
    makeSelectable(addLabel);
    QFont af = addLabel->font();
    af.setPixelSize(12);
    addLabel->setFont(af);
    ah->addWidget(addLabel);
    auto *combo = AuiComboBox::create();
    combo->addItem(QStringLiteral("— 选择要添加的属性 —"));
    for (const QString &a : addable) combo->addItem(a);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(combo, &QComboBox::activated, this, [this, combo, path, info](int idx) {
      if (idx <= 0) return;  // 占位项
      const QString pname = combo->itemText(idx);
      const SchemaValidator::SchemaPropInfo *candidate = info.findProperty(pname);
      if (!candidate) return;
      // 依据类型生成默认值后写入
      QJsonValue def = defaultForType(*candidate);
      setNodeValue(propertyPath(path, pname), def);
      rebuild();
    });
    ah->addWidget(combo);
    v->addWidget(addRow);
  }
  return box;
}

QWidget *SchemaFormEditor::buildPropertyControl(const QString &name,
                                                const SchemaValidator::SchemaPropInfo &prop,
                                                const QStringList &parentPath,
                                                const QStringList &required) {
  const QStringList path = propertyPath(parentPath, name);
  QJsonValue cur;
  nodeAt(m_root, path, &cur);
  const bool req = required.contains(name);

  auto *row = new QWidget;
  auto *grid = new QGridLayout(row);
  grid->setContentsMargins(0, 2, 0, 2);
  grid->setHorizontalSpacing(12);
  grid->setVerticalSpacing(4);

  // 多行类型（array/object）：分隔线 + 标签说明在上、内容整行铺满在下，避免内容被挤窄；
  // 普通字段保持左右两列（标签左、控件右），最省空间、易扫视。
  const QString t = prop.type;
  const bool multiline = (t == QLatin1String("array") || t == QLatin1String("object"));
  // 不再为多行字段添加顶部横线：层级已由缩进表达，横线反而显得拥挤
  auto placeContent = [&](QWidget *c) {
    if (!c) return;
    if (multiline) {
      grid->addWidget(c, 1, 0, 1, 2);
    } else {
      grid->addWidget(c, 0, 1);
    }
  };
  auto placeLabelCol = [&](QWidget *l) {
    if (multiline)
      grid->addWidget(l, 0, 0, 1, 2);
    else
      grid->addWidget(l, 0, 0);
  };

  // 多行类型：标签与说明在同一行（左侧紧凑），右侧留给下方数组内容，避免头部单独一整行留白
  if (multiline) {
    auto *head = new QWidget;
    auto *headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(0, 0, 0, 0);
    headLay->setSpacing(8);
    auto *key = new QLabel(name);
    key->setTextFormat(Qt::PlainText);
    makeSelectable(key);
    if (!prop.description.isEmpty()) key->setToolTip(prop.description);  // 悬停属性名显示说明
    QFont kf = key->font();
    kf.setWeight(QFont::Medium);
    key->setFont(kf);
    headLay->addWidget(key);
    if (req) {
      auto *star = new QLabel(QStringLiteral("*"));
      star->setStyleSheet(QStringLiteral("color: #e8463a; font-weight: 600;"));
      headLay->addWidget(star);
    }
    headLay->addStretch(1);
    // 字段级移除按钮：删除整个数组/对象字段。放在标题行右侧（与数组行内 ↑↓× 垂直分离，
    // 避免像原先那样与行内按钮同排导致误判/误点）。
    // 必填字段不提供移除按钮：schema 要求该字段必须存在，移除后重建会立即重新渲染，
    // 点击看起来毫无效果，徒增困惑；清空内容用行内 × 即可。
    if (!req) {
      auto *fieldDel = makeSMButton(QStringLiteral("×"));
      fieldDel->setToolTip(QStringLiteral("移除该字段"));
      connect(fieldDel, &QPushButton::clicked, this, [this, path]() {
        removeNodeValue(path);
        rebuild();
      });
      headLay->addWidget(fieldDel);
    }
    placeLabelCol(head);
  } else {
    placeLabelCol(makeLabelColumn(name, prop.description, req));
  }

  // 1/2) string（含 enum）
  if (t == QLatin1String("string")) {
    if (!prop.enumValues.isEmpty()) {
      auto *combo = AuiComboBox::create();
      combo->setEditable(true);
      QString curText = cur.isString() || cur.isDouble()
                            ? cur.toString()
                            : (cur.toVariant().isValid() ? cur.toVariant().toString() : QString());
      combo->addItems(prop.enumValues);
      if (!curText.isEmpty() && !prop.enumValues.contains(curText)) combo->addItem(curText);
      combo->setCurrentText(curText);
      connect(combo, &QComboBox::currentTextChanged, this,
              [this, path](const QString &s) { setNodeValue(path, QJsonValue(s)); });
      placeContent(combo);
    } else {
      auto *line = new QLineEdit;
      line->setText(cur.isString() || cur.isDouble() ? cur.toString() : QString());
      connect(line, &QLineEdit::textEdited, this,
              [this, path](const QString &s) { setNodeValue(path, QJsonValue(s)); });
      placeContent(line);
    }
  }
  // 3) bool
  else if (t == QLatin1String("bool")) {
    auto *chk = new QCheckBox;
    chk->setChecked(cur.toBool());
    connect(chk, &QCheckBox::toggled, this,
            [this, path](bool on) { setNodeValue(path, QJsonValue(on)); });
    auto *wrap = new QWidget;
    auto *w = new QHBoxLayout(wrap);
    w->setContentsMargins(0, 0, 0, 0);
    w->addWidget(chk);
    w->addStretch(1);
    placeContent(wrap);
  }
  // 4) int / double
  else if (t == QLatin1String("int") || t == QLatin1String("double")) {
    auto *line = new QLineEdit;
    double dv = cur.isDouble() ? cur.toDouble() : 0;
    if (cur.isString()) {
      bool ok = false;
      dv = cur.toString().toDouble(&ok);
      if (!ok) dv = 0;
    }
    line->setText(cur.isUndefined() ? QString() : QString::number(dv, 'g', 15));
    if (t == QLatin1String("int")) {
      line->setValidator(new QIntValidator(line));
    } else {
      line->setValidator(new QDoubleValidator(line));
    }
    connect(line, &QLineEdit::editingFinished, this, [this, line, path]() {
      bool ok = false;
      double n = line->text().toDouble(&ok);
      if (ok) setNodeValue(path, QJsonValue(n));
    });
    placeContent(line);
  }
  // 5) array
  else if (t == QLatin1String("array")) {
    const QString itemsType = prop.items;
    const bool objItems = m_schema.hasClass(itemsType);  // items 引用类名 → 对象元素
    auto *wrap = new QWidget;
    auto *w = new QVBoxLayout(wrap);
    // 数组项内容相对数组标题左侧缩进。普通元素(如 selCols)用 20px 显示层级；
    // 对象数组项的每个对象块(buildObjectForm)自身已带 20px 缩进，外层不再加，避免双重缩进。
    w->setContentsMargins(objItems ? 0 : 20, 2, 0, 0);
    w->setSpacing(4);

    QJsonArray arr = cur.isArray() ? cur.toArray() : QJsonArray();

    // 新增项的默认值
    QJsonValue defaultElement;
    if (objItems)
      defaultElement = QJsonValue(QJsonObject());
    else if (itemsType == QLatin1String("bool"))
      defaultElement = QJsonValue(false);
    else if (itemsType == QLatin1String("int") || itemsType == QLatin1String("double"))
      defaultElement = QJsonValue(0);
    else
      defaultElement = QJsonValue(QString());

    // 每一项
    for (int i = 0; i < arr.size(); ++i) {
      // 数组项之间不再加横线：已有缩进区分层级，横线反而显得拥挤
      QWidget *content = nullptr;
      const QStringList itemPath = propertyPath(path, QString::number(i));

      if (objItems) {
        // 对象元素：↑↓× 放到对象标题行右侧（"类名[索引]" 同一行），操作该数组元素本身；
        // 不再单独占一行，避免与对象内部字段/数组按钮混叠。
        auto *titleActions = new QWidget;
        auto *taLay = new QHBoxLayout(titleActions);
        taLay->setContentsMargins(0, 0, 0, 0);
        taLay->setSpacing(4);
        auto *oup = makeSMButton(QStringLiteral("↑"));
        auto *odn = makeSMButton(QStringLiteral("↓"));
        auto *odel = makeSMButton(QStringLiteral("×"));
        oup->setToolTip(QStringLiteral("上移该对象"));
        odn->setToolTip(QStringLiteral("下移该对象"));
        odel->setToolTip(QStringLiteral("删除该对象"));
        taLay->addWidget(oup);
        taLay->addWidget(odn);
        taLay->addWidget(odel);
        content = buildObjectForm(itemsType, itemPath, nullptr, titleActions);
        if (!content) continue;
        w->addWidget(content);
        connect(oup, &QPushButton::clicked, this,
                [this, path, i]() { moveArrayItem(path, i, -1); });
        connect(odn, &QPushButton::clicked, this, [this, path, i]() { moveArrayItem(path, i, 1); });
        connect(odel, &QPushButton::clicked, this, [this, path, i]() { removeArrayItem(path, i); });
        continue;
      }

      // 普通元素（string/number/bool）：行内控件 + 右侧操作按钮
      if (itemsType == QLatin1String("bool")) {
        auto *ic = new QCheckBox;
        ic->setChecked(arr.at(i).toBool());
        connect(ic, &QCheckBox::toggled, this,
                [this, itemPath](bool on) { setNodeValue(itemPath, QJsonValue(on)); });
        content = ic;
      } else {
        auto *il = new QLineEdit;
        QJsonValue iv = arr.at(i);
        QString txt = iv.isString() || iv.isDouble()
                          ? iv.toString()
                          : (iv.toVariant().isValid() ? iv.toVariant().toString() : QString());
        il->setText(txt);
        connect(il, &QLineEdit::textEdited, this,
                [this, itemPath](const QString &s) { setNodeValue(itemPath, QJsonValue(s)); });
        content = il;
      }
      if (!content) continue;

      auto *rowItem = new QWidget;
      auto *h = new QHBoxLayout(rowItem);
      h->setContentsMargins(0, 0, 0, 0);
      h->setSpacing(4);
      h->addWidget(content, 1);
      auto *up = makeSMButton(QStringLiteral("↑"));
      auto *dn = makeSMButton(QStringLiteral("↓"));
      auto *del = makeSMButton(QStringLiteral("×"));
      up->setToolTip(QStringLiteral("上移该项"));
      dn->setToolTip(QStringLiteral("下移该项"));
      del->setToolTip(QStringLiteral("删除该项"));
      h->addWidget(up);
      h->addWidget(dn);
      h->addWidget(del);
      connect(up, &QPushButton::clicked, this, [this, path, i]() { moveArrayItem(path, i, -1); });
      connect(dn, &QPushButton::clicked, this, [this, path, i]() { moveArrayItem(path, i, 1); });
      connect(del, &QPushButton::clicked, this, [this, path, i]() { removeArrayItem(path, i); });
      w->addWidget(rowItem);
    }

    // 新增按钮
    auto *add = new QPushButton(QStringLiteral("＋ 添加项"));
    connect(add, &QPushButton::clicked, this,
            [this, path, defaultElement]() { insertArrayItem(path, -1, defaultElement); });
    w->addWidget(add, 0, Qt::AlignLeft);
    placeContent(wrap);
  }
  // 6) object（嵌套类 / additionalProperties）
  else if (t == QLatin1String("object")) {
    if (!prop.className.isEmpty() && m_schema.hasClass(prop.className)) {
      // 单值 object 字段：不显示嵌套第二行标题（外层自身字段名已是标题），缩进并入
      placeContent(buildObjectForm(prop.className, path, nullptr, nullptr, false));
    } else {
      placeContent(makeRawJsonFallback(path, cur));
    }
  }
  // 7) any / 未知 → 内嵌 JSON 兜底
  else {
    placeContent(makeRawJsonFallback(path, cur));
  }

  // 列宽策略：多行类型内容占第 0 列（跨两格）→ 第 0 列拉伸铺满；
  // 普通字段标签在第 0 列、控件在第 1 列 → 第 1 列拉伸。
  if (multiline) {
    grid->setColumnStretch(0, 1);
  } else {
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
  }
  grid->setColumnMinimumWidth(0, multiline ? 0 : 120);
  return row;
}

// 「内嵌 JSON 兜底框」：any/未知类型/additionalProperties 用，附警示标签
QWidget *SchemaFormEditor::makeRawJsonFallback(const QStringList &path, const QJsonValue &cur) {
  auto *wrap = new QWidget;
  auto *v = new QVBoxLayout(wrap);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(4);

  auto *tag = new QLabel(QStringLiteral("any → 内嵌 JSON 兜底"));
  makeSelectable(tag);
  auto tf = tag->font();
  tf.setPixelSize(11);
  tag->setFont(tf);
  tag->setStyleSheet(
      QStringLiteral("color: %1;").arg(palette().color(QPalette::PlaceholderText).name()));
  v->addWidget(tag);

  auto *raw = new QPlainTextEdit;
  QString text;
  if (cur.isArray())
    text = QString::fromUtf8(QJsonDocument(cur.toArray()).toJson(QJsonDocument::Indented));
  else if (cur.isObject())
    text = QString::fromUtf8(QJsonDocument(cur.toObject()).toJson(QJsonDocument::Indented));
  else if (!cur.isUndefined() && !cur.isNull())
    text = cur.toString();
  raw->setPlainText(text);
  raw->setMaximumHeight(120);
  connect(raw, &QPlainTextEdit::textChanged, this, [this, raw, path]() {
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(raw->toPlainText().toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && (d.isObject() || d.isArray())) {
      if (d.isObject())
        setNodeValue(path, d.object());
      else
        setNodeValue(path, d.array());
    }
    // 非法 JSON：不写回，等用户修正
  });
  v->addWidget(raw);
  return wrap;
}