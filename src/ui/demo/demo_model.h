/**
 * @file demo_model.h
 * @brief Demo 数据模型层
 *
 * 存储示例界面的运行时状态数据，包括模板、JSON 数据和生成结果。
 * 遵循 MainDev 的 MVC 架构风格。
 */

#pragma once

#include <QJsonObject>
#include <QString>

/**
 * @class DemoModel
 * @brief Demo 数据模型
 *
 * 纯数据类，管理示例编辑器的所有输入/输出状态。
 * 由 DemoMgr 创建，DemoMgr 读写，DemoUi 通过 getter 只读访问。
 */
class DemoModel {
public:
  DemoModel() = default;

  /// 设置模板内容
  void setTemplate(const QString &text) { m_template = text; }
  /// 获取模板内容
  QString getTemplate() const { return m_template; }

  /// 设置 JSON 数据字符串
  void setJsonData(const QString &text) { m_jsonData = text; }
  /// 获取 JSON 数据字符串
  QString getJsonData() const { return m_jsonData; }

  /// 设置生成结果
  void setOutput(const QString &text) { m_output = text; }
  /// 获取生成结果
  QString getOutput() const { return m_output; }

  /// 设置上次解析的 JSON 对象
  void setDataObject(const QJsonObject &obj) { m_dataObject = obj; }
  /// 获取上次解析的 JSON 对象
  QJsonObject dataObject() const { return m_dataObject; }

  /// 设置错误消息
  void setError(const QString &error) { m_error = error; }
  /// 获取错误消息
  QString error() const { return m_error; }
  /// 是否有错误
  bool hasError() const { return !m_error.isEmpty(); }
  /// 清除错误
  void clearError() { m_error.clear(); }

  /// 设置最后保存的路径
  void setSavePath(const QString &path) { m_savePath = path; }
  /// 获取最后保存的路径
  QString savePath() const { return m_savePath; }

private:
  QString m_template;       ///< 模板文本
  QString m_jsonData;       ///< JSON 数据文本
  QString m_output;         ///< 生成结果
  QJsonObject m_dataObject; ///< 解析后的 JSON 对象
  QString m_error;          ///< 错误消息
  QString m_savePath;       ///< 最后保存/加载的文件路径
};