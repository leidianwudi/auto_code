/**
 * @file http_client.h
 * @brief HTTP 网络请求封装
 *
 * 基于 QNetworkAccessManager 的异步 HTTP 客户端。
 * 提供 GET / POST 请求，返回 JSON 响应。
 */

#pragma once

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

/**
 * @class HttpClient
 * @brief HTTP 异步请求客户端
 *
 * 单例模式，全局共享一个 QNetworkAccessManager。
 * 提供 GET / POST 请求，返回 JSON 响应。
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

  /// 获取单例实例
  static HttpClient &instance();

  /// 发起 GET 请求
  /// @param url 完整的请求 URL
  /// @param parent 关联的父对象（用于在父对象销毁时自动取消请求）
  void get(const QString &url, QObject *parent = nullptr);

  /// 发起 POST 请求（JSON body）
  /// @param url 完整的请求 URL
  /// @param body JSON 请求体
  /// @param parent 关联的父对象
  void post(const QString &url, const QJsonObject &body, QObject *parent = nullptr);

  /// 发起指定方法的请求
  /// @param method HTTP 方法
  /// @param url 完整 URL
  /// @param body JSON 请求体（POST/PUT 时使用）
  /// @param parent 关联的父对象
  void request(Method method, const QString &url, const QJsonObject &body = {},
               QObject *parent = nullptr);

  /// 发起带自定义请求头的请求
  /// @param method HTTP 方法
  /// @param url 完整 URL
  /// @param body JSON 请求体（POST/PUT 时使用）
  /// @param headers 自定义请求头（如 Authorization）
  /// @param parent 关联的父对象
  void request(Method method, const QString &url, const QJsonObject &body,
               const Headers &headers, QObject *parent = nullptr);

signals:
  /// 请求成功时发射
  /// @param url 请求的 URL
  /// @param doc 返回的 JSON 文档
  void finished(const QString &url, const QJsonDocument &doc);

  /// 请求失败时发射
  /// @param url 请求的 URL
  /// @param errorMsg 错误信息
  void error(const QString &url, const QString &errorMsg);

private:
  HttpClient(QObject *parent = nullptr);
  ~HttpClient() override = default;

  QNetworkAccessManager *m_manager;
};
