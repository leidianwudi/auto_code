/**
 * @file json_vue_editor_http.cpp
 * @brief JsonVueEditor 的 HTTP 生成与接口配置加载实现
 *
 * 从 json_vue_editor.cpp 拆分：findNearestApiAuthDataAc / loadHttpConfigFromAcFile /
 * onGenerate / onHttpFinished / onHttpError / populateColumnsFromHttp。
 */

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPoint>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidgetItem>

#include "json_vue_editor.h"
#include "json_vue_editor_helpers.h"
#include "src/util/common/http_client.h"
#include "src/util/common/util_json.h"
#include "src/util/ui/component/aui_message_box.h"

// ════════════════════════════════════════════════════════════
//  HTTP 生成
// ════════════════════════════════════════════════════════════

QString JsonVueEditor::findNearestApiAuthDataAc(const QString &jsonvueFilePath) {
  if (jsonvueFilePath.isEmpty()) return {};
  // 从 .jsonvue 文件所在目录开始，逐级向上查找 api_auth_data.ac
  QDir dir = QFileInfo(jsonvueFilePath).absoluteDir();
  while (true) {
    QFileInfo candidate(dir.absoluteFilePath(kApiAuthDataAcFile));
    if (candidate.isFile()) {
      return candidate.absoluteFilePath();
    }
    if (!dir.cdUp()) break;  // 已到根目录
  }
  return {};
}

void JsonVueEditor::loadHttpConfigFromAcFile(const QString &acFilePath) {
  if (acFilePath.isEmpty()) return;
  m_acConfigFilePath = acFilePath;  // 记住路径，供点击"生成"时重读
  QFile f(acFilePath);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QString content = QString::fromUtf8(f.readAll());
  f.close();

  // 用正则提取 AC 脚本中的常量定义
  // 匹配: let varName: String = "value"; 或 public varName: String = "value";
  // （支持转义引号 \"）
  static const QRegularExpression rx(
      QStringLiteral(R"rx((?:let|public)\s+(\w+)\s*:\s*String\s*=\s*"((?:[^"\\]|\\.)*)")rx"));
  auto it = rx.globalMatch(content);
  while (it.hasNext()) {
    auto m = it.next();
    QString name = m.captured(1);
    QString value = m.captured(2);
    // 反转义: \" → "，\\ → 反斜杠
    value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    if (name == QStringLiteral("baseUrl")) {
      m_baseUrl = value;
    } else if (name == QStringLiteral("authHeader")) {
      m_authHeader = value;
    } else if (name == QStringLiteral("postData")) {
      m_postData = value;
    }
  }
}

void JsonVueEditor::onGenerate() {
  // 每次点击"生成"时重新读取 api_auth_data.ac 的 HTTP 配置（baseUrl/authHeader/postData），
  // 保证 ac 文件被修改后无需重启程序即可生效
  if (!m_acConfigFilePath.isEmpty()) {
    loadHttpConfigFromAcFile(m_acConfigFilePath);
  }

  QString url = m_dataUrlEdit->text().trimmed();
  if (url.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("请输入生成数据URL"));
    return;
  }

  // 拼接 baseUrl
  QString fullUrl = url;
  if (m_baseUrl.isEmpty()) {
    // baseUrl 为空时直接使用 url（必须为完整 URL）
    if (!url.startsWith(QStringLiteral("http://")) && !url.startsWith(QStringLiteral("https://"))) {
      AuiMessageBox::show(this, QStringLiteral("提示"),
                          QStringLiteral("baseUrl 为空时，URL 必须以 http:// 或 https:// 开头"));
      return;
    }
  } else {
    // 拼接 baseUrl + url
    if (url.startsWith(QStringLiteral("http://")) || url.startsWith(QStringLiteral("https://"))) {
      fullUrl = url;
    } else {
      QString base = m_baseUrl;
      while (base.endsWith('/')) base.chop(1);
      if (!url.startsWith('/')) url.prepend('/');
      fullUrl = base + url;
    }
  }

  m_generateBtn->setEnabled(false);
  m_generateBtn->setText(QStringLiteral("请求中..."));

  HttpClient::Method method = HttpClient::Get;
  QString methodStr = m_methodCombo->currentText();
  if (methodStr == JsonVueHttp::kPost)
    method = HttpClient::Post;
  else if (methodStr == JsonVueHttp::kPut)
    method = HttpClient::Put;
  else if (methodStr == JsonVueHttp::kDelete)
    method = HttpClient::Delete;

  // 解析 POST 数据
  QJsonObject bodyObj;
  if (method == HttpClient::Post || method == HttpClient::Put) {
    if (!m_postData.isEmpty()) {
      QJsonParseError err;
      QJsonDocument postDoc = UtilJson::fromJson(m_postData, &err);
      if (err.error == QJsonParseError::NoError && postDoc.isObject()) {
        bodyObj = postDoc.object();
      }
    }
  }

  // 构建请求头
  HttpClient::Headers headers;
  if (!m_authHeader.isEmpty()) {
    headers[QStringLiteral("Authorization")] = m_authHeader;
  }

  // 只回调给本编辑器绑定的回调（HttpClient 区分发起方，不广播）
  HttpClient::instance().request(
      method, fullUrl, bodyObj, headers, [this](const QJsonDocument &doc) { onHttpFinished(doc); },
      [this, fullUrl](const QString &errorMsg) { onHttpError(fullUrl, errorMsg); }, this);
}

void JsonVueEditor::onHttpFinished(const QJsonDocument &doc) {
  m_generateBtn->setEnabled(true);
  m_generateBtn->setText(QStringLiteral("生成"));

  int added = populateColumnsFromHttp(doc);
  if (added < 0) return;  // 解析/校验失败，错误提示已在 populateColumnsFromHttp 中弹出

  // 生成成功提示
  QString msg;
  if (added > 0) {
    msg = QStringLiteral("生成成功，已从接口获取 %1 个新列并追加到列配置。").arg(added);
  } else {
    msg = QStringLiteral("生成成功，但网络返回的列已全部存在于列配置中，未新增任何列。");
  }
  AuiMessageBox::show(this, QStringLiteral("生成成功"), msg);
}

void JsonVueEditor::onHttpError(const QString &url, const QString &errorMsg) {
  m_generateBtn->setEnabled(true);
  m_generateBtn->setText(QStringLiteral("生成"));

  // 详细错误信息，便于用户定位问题
  QString method = m_methodCombo->currentText();
  QString msg =
      QStringLiteral("请求地址：%1\n请求方法：%2\n错误详情：%3\n\n").arg(url, method, errorMsg);
  msg += QStringLiteral("排查建议：\n");
  msg += QStringLiteral("1. 确认后端服务已启动，且端口未被占用；\n");
  msg +=
      QStringLiteral("2. 检查 baseUrl 配置是否正确（可在 %1 中修改）；\n").arg(kApiAuthDataAcFile);
  msg += QStringLiteral("3. 检查 Authorization 令牌是否过期或无效；\n");
  msg += QStringLiteral("4. 可在浏览器中直接访问上述地址，验证接口是否可用。");
  AuiMessageBox::show(this, QStringLiteral("请求失败"), msg);
}

int JsonVueEditor::populateColumnsFromHttp(const QJsonDocument &doc) {
  // 解析 { code, msg, data: { list: [ { col1, col2, ... } ] } }
  QJsonObject root = doc.object();
  QJsonValue dataVal = root.value(QStringLiteral("data"));
  QJsonArray listArr;

  if (dataVal.isObject()) {
    listArr = dataVal.toObject().value(QStringLiteral("list")).toArray();
  } else if (dataVal.isArray()) {
    listArr = dataVal.toArray();
  } else {
    // 没有包裹层，直接是数组
    if (doc.isArray()) listArr = doc.array();
  }

  if (listArr.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("返回数据中未找到 list 数组"));
    return -1;
  }

  // 取第一行的列名
  QJsonObject firstRow = listArr.at(0).toObject();
  if (firstRow.isEmpty()) {
    AuiMessageBox::show(this, QStringLiteral("提示"), QStringLiteral("返回数据第一行为空"));
    return -1;
  }

  // 收集当前列配置中已有的 dataName（已有列不允许任何修改）
  QSet<QString> existingNames;
  for (int row = 0; row < m_columnTable->rowCount(); ++row) {
    auto *item = m_columnTable->item(row, ColDataName);
    if (item && !item->text().trimmed().isEmpty()) {
      existingNames.insert(item->text().trimmed());
    }
  }

  // 仅把网络数据中新增（当前列配置里没有）的列追加到表格末尾，已有列保持原顺序不变
  int addedCount = 0;
  m_loading = true;
  for (auto it = firstRow.begin(); it != firstRow.end(); ++it) {
    QString colName = it.key();
    if (existingNames.contains(colName)) continue;  // 已有列跳过，不修改

    ColumnConfig col;
    col.dataName = colName;
    // 默认查询列名和编辑列名等于数据列名
    col.queryName = colName;
    col.editName = colName;

    int row = m_columnTable->rowCount();
    m_columnTable->insertRow(row);

    m_columnTable->setItem(row, ColDataName, new QTableWidgetItem(col.dataName));

    // 标题：优先用列表页列标题（queryName），为空则用编辑页标签（editName）
    QString title = col.queryName.isEmpty() ? col.editName : col.queryName;
    m_columnTable->setItem(row, ColTitle, new QTableWidgetItem(title));

    auto *qVis = new QCheckBox;
    qVis->setChecked(col.queryVisible);
    m_columnTable->setCellWidget(row, ColQueryVisible, qVis);
    connectCellWidgetSignals(qVis);

    auto *eVis = new QCheckBox;
    eVis->setChecked(col.editVisible);
    m_columnTable->setCellWidget(row, ColEditVisible, eVis);
    connectCellWidgetSignals(eVis);

    // 配置按钮（⚙ + 摘要文本，含显示类型/编辑样式/通用配置）
    auto *configBtn = new QPushButton(columnConfigSummary(col), this);
    storeColumnConfig(configBtn, col);
    m_columnTable->setCellWidget(row, ColConfig, configBtn);
    configBtn->installEventFilter(this);  // 双击打开样式配置
    // 右键菜单：复制配置 / 粘贴配置
    configBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(configBtn, &QPushButton::customContextMenuRequested, this,
            [this, configBtn](const QPoint &pos) {
              for (int r = 0; r < m_columnTable->rowCount(); ++r) {
                if (m_columnTable->cellWidget(r, ColConfig) == configBtn) {
                  showColumnConfigMenu(r, configBtn->mapToGlobal(pos));
                  break;
                }
              }
            });

    ++addedCount;  // 记录新增列数
  }
  m_loading = false;

  // 刷新查询字段下拉框
  refreshQueryFieldDataNames();

  emit configChanged();
  return addedCount;
}
