/**
 * @file http_client.cpp
 * @brief HTTP 网络请求封装实现
 */

#include "http_client.h"

#include <QJsonArray>
#include <QNetworkRequest>

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

void HttpClient::get(const QString &url, QObject *parent) { request(Get, url, {}, parent); }

void HttpClient::post(const QString &url, const QJsonObject &body, QObject *parent) {
  request(Post, url, body, parent);
}

void HttpClient::request(Method method, const QString &url, const QJsonObject &body,
                         QObject *parent) {
  request(method, url, body, {}, parent);
}

/// HTTP 请求默认超时时间（30 秒）
static constexpr int kHttpTimeoutMs = 30000;

void HttpClient::request(Method method, const QString &url, const QJsonObject &body,
                         const Headers &headers, QObject *parent) {
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

  if (!reply) {
    emit error(url, QStringLiteral("无法创建网络请求"));
    return;
  }

  // 处理响应
  connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      emit error(url, reply->errorString());
      return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = UtilJson::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
      emit error(url, QStringLiteral("JSON 解析失败: %1").arg(parseErr.errorString()));
      return;
    }
    emit finished(url, doc);
  });

  // 父对象销毁时自动取消请求
  if (parent) {
    connect(parent, &QObject::destroyed, reply, [reply]() { reply->abort(); });
  }
}
