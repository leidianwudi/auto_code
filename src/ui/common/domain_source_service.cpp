#include "domain_source_service.h"

#include <QFile>
#include <QFileInfo>
#include <QStandardItem>

#include "src/ui/json_global_enum/json_global_enum_model.h"
#include "src/ui/json_source/json_source_finder.h"
#include "src/ui/json_source/json_source_model.h"
#include "src/util/common/code_constants.h"
#include "src/util/ui/component/aui_tree_combo.h"

QString jsonVueSourceScopeHelpText() {
  return QStringLiteral(
      "数据源候选包含三类：\n"
      "  1. .jsonsource 数据源（当前项目作用域内）：静态源带固定选项（条目标注 N 项），"
      "动态源选项来自接口返回（条目显示接口 url）；\n"
      "  2. .jsonglobalenum 全局枚举：项目内 + 平台共享层（crud_nest/，逐级向上就近优先）"
      "+ 兄弟后端项目（crud_nest/<项目>/）；\n"
      "  3. 全局枚举按枚举名合并展示，同名时项目级覆盖共享层。\n\n"
      "项目根的判定（.jsonsource 与项目内 .jsonglobalenum 的作用域）：从当前 .jsonvue "
      "文件所在目录逐级向上，找到的第一个含 project.acproj 标记文件的文件夹即为项目根。\n"
      "（在目录树中右键文件夹「设为项目」可写入标记，取消项目即删除标记，"
      "设为项目后文件夹图标显示为齿轮。）\n\n"
      "未设项目时（如 template 模板目录），回退为列出工作区全部 .jsonsource 数据源。");
}

QString jsonVueUploadScopeHelpText() {
  // 与 jsonVueSourceScopeHelpText 结构一致，仅措辞针对 .jsonupload
  return QStringLiteral(
      "下拉框只列出「当前项目」下的 .jsonupload 上传预设文件。\n\n"
      "项目根的判定：从当前 .jsonvue 文件所在目录逐级向上，找到的第一个含 "
      "project.acproj 标记文件的文件夹即为项目根。\n"
      "（在目录树中右键文件夹「设为项目」可写入标记，取消项目即删除标记，"
      "设为项目后文件夹图标显示为齿轮。）\n\n"
      "未设项目时（如 template 模板目录），回退为列出工作区全部 .jsonupload 上传预设。");
}

// ── 数据源函数名推导 ──────────────────────────────────────────

QString snakeToPascal(const QString &str) {
  QString res;
  const QStringList segs = str.split(QLatin1Char('_'));
  for (const QString &seg : segs) {
    if (seg.isEmpty()) continue;
    res += seg.at(0).toUpper() + seg.mid(1);
  }
  return res;
}

QString snakeToCamel(const QString &str) {
  const QString pascal = snakeToPascal(str);
  if (pascal.isEmpty()) return pascal;
  return pascal.at(0).toLower() + pascal.mid(1);
}

QString sourceUrlToFuncName(const QString &url) {
  QString norm = url;
  norm.replace(QLatin1Char('\\'), QLatin1Char('/'));
  const QStringList parts = norm.split(QLatin1Char('/'));
  QString res;
  bool first = true;
  for (const QString &seg : parts) {
    if (seg.isEmpty()) continue;
    if (first) {
      res = snakeToCamel(seg);
      first = false;
    } else {
      res += snakeToPascal(seg);
    }
  }
  return res;
}

QString staticSourceFuncName(const QString &sourceName, const QString &url) {
  QString urlName = sourceUrlToFuncName(url);
  if (!urlName.isEmpty()) {
    urlName[0] = urlName.at(0).toUpper();  // url 名首字母大写（Pascal）
    return snakeToCamel(sourceName) + QStringLiteral("Static") + urlName;
  }
  return snakeToCamel(sourceName) + QStringLiteral("Static");  // 旧数据无 url 时仅前缀
}

// ════════════════════════════════════════════════════════════
//  取值域数据源候选树（方案 B：列样式配置 / 查询设置共用）
// ════════════════════════════════════════════════════════════

/// 真/假文字提取（与 admin_data.ac 的布尔值约定一致）：
/// value 为 1/true → 真值、0/false → 假值；无法判断时按顺序（第 1 项假、第 2 项真）
static QPair<QString, QString> boolTextsOfLabelValues(
    const QList<QPair<QString, QString>> &labelValues) {
  QString trueText;
  QString falseText;
  if (labelValues.size() == 2) {
    falseText = labelValues.at(0).first;
    trueText = labelValues.at(1).first;
  }
  for (const auto &lv : labelValues) {
    const QString v = lv.second.trimmed().toLower();
    if (v == QStringLiteral("1") || v == QStringLiteral("true")) trueText = lv.first;
    if (v == QStringLiteral("0") || v == QStringLiteral("false")) falseText = lv.first;
  }
  return {trueText, falseText};
}

/// 选项预览文字（"开启=1 / 关闭=0"，超过 4 项截断）
static QString optionPreviewOfLabelValues(const QList<QPair<QString, QString>> &labelValues) {
  QStringList parts;
  int count = 0;
  for (const auto &lv : labelValues) {
    if (count >= 4) {
      parts << QStringLiteral("…");
      break;
    }
    parts << QStringLiteral("%1=%2").arg(lv.first, lv.second);
    ++count;
  }
  return parts.join(QStringLiteral(" / "));
}

void buildDomainSourceCandidates(AuiTreeCombo *combo, const QString &searchRoot, bool enumOnly,
                                 QHash<QString, QPair<QString, QString>> *boolTexts,
                                 QHash<QString, QString> *previews,
                                 QHash<QString, int> *optionCounts,
                                 QHash<QString, bool> *dynamicFlags) {
  if (!combo) return;
  combo->clear();
  if (boolTexts) boolTexts->clear();
  if (previews) previews->clear();
  if (optionCounts) optionCounts->clear();
  if (dynamicFlags) dynamicFlags->clear();
  // 顶层"未选择"条目：枚举域未选源时真假文字可自由填写（旧行为）
  combo->addEntry(nullptr, QStringLiteral("（未选择数据源）"), QString());

  // 候选一：项目作用域内的 .jsonsource 数据源（每个文件一组，可展开/收起）。
  // 静态源与动态源都可作为取值域：静态源带选项（N 项），动态源选项来自接口返回
  const QStringList srcFiles = findJsonsourceFiles(searchRoot);
  for (const QString &sf : srcFiles) {
    JsonSourceConfig cfg;
    {
      QFile f(sf);
      if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cfg = JsonSourceConfig::fromJsonString(QString::fromUtf8(f.readAll()));
        f.close();
      }
    }
    QStandardItem *group = nullptr;
    for (const auto &s : cfg.sources) {
      const bool isDyn = !s.isStatic();
      if (enumOnly && (isDyn || s.options.size() != 2)) continue;  // 枚举域仅 2 项静态源
      if (!group) {
        group = combo->addGroup(QStringLiteral("▍数据源 · ") + QFileInfo(sf).fileName());
      }
      const QString remark = s.remark.isEmpty() ? QStringLiteral("(未命名)") : s.remark;
      QList<QPair<QString, QString>> labelValues;
      for (const auto &o : s.options) labelValues.append({o.label, o.value});
      const QString ref = sf + QStringLiteral("#") + s.id;
      if (isDyn) {
        // 动态源：说明 - 接口url（选项来自接口返回，无固定选项数）
        if (previews) {
          previews->insert(ref, QStringLiteral("动态数据源：选项来自接口返回"));
        }
        if (optionCounts) optionCounts->insert(ref, -1);
        if (dynamicFlags) dynamicFlags->insert(ref, true);
        combo->addEntry(group, QStringLiteral("%1 - %2").arg(remark, s.url), ref);
      } else {
        // 静态源：说明 - url 值（N 项）；旧数据无 url 时回退为派生函数名
        const QString funcName = staticSourceFuncName(QFileInfo(sf).baseName(), s.url);
        if (boolTexts) boolTexts->insert(ref, boolTextsOfLabelValues(labelValues));
        if (previews) previews->insert(ref, optionPreviewOfLabelValues(labelValues));
        if (optionCounts) optionCounts->insert(ref, s.options.size());
        if (dynamicFlags) dynamicFlags->insert(ref, false);
        combo->addEntry(
            group,
            QStringLiteral("%1 - %2（%3 项）")
                .arg(remark, s.url.isEmpty() ? funcName : s.url, QString::number(s.options.size())),
            ref);
      }
    }
  }

  // 候选二：全局枚举（.jsonglobalenum，含平台共享层与兄弟后端项目）。
  // 引用存固定基名 global_enum.jsonsource——文件移动后生成侧按基名+id 仍可解析（不断链）；
  // id 兜底与生成侧 buildGlobalEnumJsonSource 一致
  const QStringList enumFiles = findGlobalEnumFiles(searchRoot);
  for (const QString &ef : enumFiles) {
    JsonGlobalEnumConfig cfg;
    {
      QFile f(ef);
      if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cfg = JsonGlobalEnumConfig::fromJsonString(QString::fromUtf8(f.readAll()));
        f.close();
      }
    }
    // 组标题标注层级：直接位于 crud_nest/ 下为共享层，位于 crud_nest/<项目>/ 下为项目级
    const QString parentName = QFileInfo(ef).absolutePath().section(QLatin1Char('/'), -1);
    const bool isShared = parentName == QStringLiteral("crud_nest");
    QStandardItem *group = nullptr;
    for (const auto &e : cfg.enums) {
      if (enumOnly && e.options.size() != 2) continue;
      if (!group) {
        group = combo->addGroup(
            isShared
                ? QStringLiteral("▍全局枚举（共享层） · ") + QFileInfo(ef).fileName()
                : QStringLiteral("▍全局枚举（%1） · %2").arg(parentName, QFileInfo(ef).fileName()));
      }
      const QString remark = e.remark.isEmpty() ? QStringLiteral("(未命名)") : e.remark;
      QList<QPair<QString, QString>> labelValues;
      for (const auto &o : e.options) labelValues.append({o.label, o.value});
      QString refId = e.id;
      if (refId.isEmpty()) refId = e.name;
      const QString ref = QStringLiteral("global_enum.jsonsource#") + refId;
      if (boolTexts && boolTexts->contains(ref)) continue;  // 同名枚举去重（跨文件）
      if (boolTexts) boolTexts->insert(ref, boolTextsOfLabelValues(labelValues));
      if (previews) previews->insert(ref, optionPreviewOfLabelValues(labelValues));
      if (optionCounts) optionCounts->insert(ref, e.options.size());
      combo->addEntry(
          group,
          QStringLiteral("%1 - %2（%3 项）").arg(remark, e.name, QString::number(e.options.size())),
          ref);
    }
  }
}
