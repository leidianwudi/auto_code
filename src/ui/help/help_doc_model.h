/**
 * @file help_doc_model.h
 * @brief 帮助文档模型层（MVC）
 *
 * 持有帮助文档分类数据（按钮标题 + AC 示例代码），
 * 供视图层展示。数据来源统一为 help_doc_data.h 常量。
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "help_doc_data.h"

/**
 * @class HelpDocModel
 * @brief 帮助文档数据模型
 *
 * 封装帮助文档分类数据的读取，视图层通过 getter 获取标题列表与代码内容。
 */
class HelpDocModel {
public:
  /// @brief 分类数量
  int count() const { return HelpDocData::kEntries.size(); }

  /// @brief 指定索引的分类标题（左侧单选按钮文字）
  QString titleAt(int index) const {
    if (index < 0 || index >= count()) return QString();
    return QString::fromUtf8(HelpDocData::kEntries[index].title);
  }

  /// @brief 指定索引的 AC 示例代码（右侧展示内容）
  QString codeAt(int index) const {
    if (index < 0 || index >= count()) return QString();
    return QString::fromUtf8(HelpDocData::kEntries[index].code);
  }

  /// @brief 全部分类标题列表
  QStringList titles() const {
    QStringList result;
    for (int i = 0; i < count(); ++i) result << titleAt(i);
    return result;
  }
};
