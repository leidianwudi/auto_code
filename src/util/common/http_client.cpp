/**
 * @file http_client.cpp
 * @brief HTTP 网络请求封装实现
 */

#include "http_client.h"

#include <QJsonArray>
#include <QNetworkRequest>
#include <QPointer>

#include "util_json.h"

// ════════════════════════════════════════════════════════════
//  单例
// ════════════════════════════════════════════════════════════

HttpClient &HttpClient::instance() {
  static HttpClient inst;
  return inst;
}

HttpClient::HttpClient(QObject *parent)
    : QObject(parent), m_manager(new QNetworkAccessManager(this)) {}

// ════════════════════════════════════════════════════════════
//  公开接口
// ════════════════════════════════════════════════════════════

/// HTTP 请求默认超时时间（30 秒）
static constexpr int kHttpTimeoutMs = 30000;

void HttpClient::request(Method method, const QString &url, const QJsonObject &body,
                         const Headers &headers, SuccessCallback onSuccess, ErrorCallback onError,
                         QObject *context) {
  QNetworkRequest req((QUrl(url)));
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  req.setTransferTimeout(kHttpTimeoutMs);

  // 设置自定义请求头
  for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
    req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
  }

  QNetworkReply *reply = nullptr;
  switch (method) {
    case Get:
      reply = m_manager->get(req);
      break;
    case Post: {
      QJsonDocument doc(body);
      reply = m_manager->post(req, doc.toJson(QJsonDocument::Compact));
      break;
    }
    case Put: {
      QJsonDocument doc(body);
      reply = m_manager->put(req, doc.toJson(QJsonDocument::Compact));
      break;
    }
    case Delete:
      reply = m_manager->deleteResource(req);
      break;
  }

  // 发起方对象（用于销毁时取消请求）；QPointer 防止发起方析构后悬垂
  QPointer<QObject> ctx(context);

  if (!reply) {
    if (onError) onError(QStringLiteral("无法创建网络请求"));
    return;
  }

  // 响应只回调给本请求绑定的回调，不广播给其他发起方。
  // 若发起方已销毁（ctx 为空），则丢弃该响应。
  connect(reply, &QNetworkReply::finished, this, [reply, url, ctx, onSuccess, onError]() {
    reply->deleteLater();

    if (ctx.isNull()) {
      return;  // 发起方已销毁，不再回调
    }

    if (reply->error() != QNetworkReply::NoError) {
      if (onError) onError(reply->errorString());
      return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = UtilJson::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
      if (onError) onError(QStringLiteral("JSON 解析失败: %1").arg(parseErr.errorString()));
      return;
    }
    if (onSuccess) onSuccess(doc);
  });

  // 发起方销毁时自动取消本请求
  if (context) {
    connect(context, &QObject::destroyed, reply, [reply]() { reply->abort(); });
  }
}
