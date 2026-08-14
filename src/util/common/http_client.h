/**
 * @file http_client.h
 * @brief HTTP 网络请求封装
 *
 * 基于 QNetworkAccessManager 的异步 HTTP 客户端。
 * 提供 GET / POST / PUT / DELETE 请求，返回 JSON 响应。
 *
 * 设计要点：单例维护一个 QNetworkAccessManager，但每个请求独立绑定
 * 自己的成功/失败回调（request() 时传入），响应返回时只调用该请求
 * 对应的回调，不会广播给其他发起方，天然区分请求归属。
 */

#pragma once

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

#include <functional>

/**
 * @class HttpClient
 * @brief HTTP 异步请求客户端（单例）
 *
 * 单例模式，全局共享一个 QNetworkAccessManager。
 * 发起方通过 request() 传入自己的回调（std::function），
 * 请求完成后 HttpClient 只回调该请求绑定的回调，不广播。
 */
class HttpClient : public QObject {
  Q_OBJECT

public:
  /// HTTP 请求方法
  enum Method {
    Get,   ///< GET 方法
    Post,  ///< POST 方法
    Put,   ///< PUT 方法
    Delete ///< DELETE 方法
  };

  /// 自定义请求头（header name → value）
  using Headers = QHash<QString, QString>;

  /// 请求成功回调（参数为返回的 JSON 文档）
  using SuccessCallback = std::function<void(const QJsonDocument &doc)>;
  /// 请求失败回调（参数为错误信息）
  using ErrorCallback = std::function<void(const QString &errorMsg)>;

  /// 获取单例实例
  static HttpClient &instance();

  /// 发起指定方法的请求，响应只回调给本请求绑定的回调
  /// @param method HTTP 方法
  /// @param url 完整 URL
  /// @param body JSON 请求体（POST/PUT 时使用）
  /// @param headers 自定义请求头（如 Authorization）
  /// @param onSuccess 本请求的成功回调（仅本请求触发）
  /// @param onError 本请求的失败回调（仅本请求触发）
  /// @param context 发起方对象（用于发起方销毁时自动取消本请求）
  void request(Method method, const QString &url, const QJsonObject &body,
               const Headers &headers, SuccessCallback onSuccess, ErrorCallback onError,
               QObject *context = nullptr);

private:
  HttpClient(QObject *parent = nullptr);
  ~HttpClient() override = default;

  QNetworkAccessManager *m_manager;
};
