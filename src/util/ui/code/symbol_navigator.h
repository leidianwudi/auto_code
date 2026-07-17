/**
 * @file symbol_navigator.h
 * @brief 符号导航器（从 CodeEditor 中拆分）
 *
 * 负责符号表的存储、查询和导航功能，
 * 支持悬停提示、转到定义、查找引用等操作。
 */

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPoint>
#include <QString>
#include <QTextEdit>
#include <QVector>

class AcSymbolEntry;
class AuiErrorToolTip;

/**
 * @class SymbolNavigator
 * @brief 符号导航器
 *
 * 独立的符号导航逻辑实现，支持：
 * - 符号表管理（存储和查询）
 * - 悬停提示显示
 * - 转到定义跳转
 * - 查找所有引用
 * - 符号引用高亮
 */
class SymbolNavigator : public QObject {
  Q_OBJECT
public:
  explicit SymbolNavigator(QObject *parent = nullptr);
  ~SymbolNavigator();

  /// 设置符号表数据
  void setSymbolTable(const QHash<QString, AcSymbolEntry> &symbols);

  /// 获取符号表（只读）
  const QHash<QString, AcSymbolEntry> &symbolTable() const { return m_symbolTable; }

  /**
   * @brief 提取光标下的标识符文本
   * @param pos 光标位置（0-based）
   * @param text 文本内容
   * @param startPos 输出：标识符起始位置
   * @param endPos 输出：标识符结束位置
   * @return 标识符文本
   */
  static QString identifierAtCursor(int pos, const QString &text, int *startPos = nullptr,
                                    int *endPos = nullptr);

  /**
   * @brief 查找符号定义
   * @param name 符号名
   * @return 符号条目指针（未找到返回 nullptr）
   */
  const AcSymbolEntry *findDefinition(const QString &name) const;

  /**
   * @brief 查找符号的所有引用位置
   * @param name 符号名
   * @param text 文本内容
   * @return 引用位置列表（行号, 上下文文本）
   */
  QVector<QPair<int, QString>> findReferences(const QString &name, const QString &text) const;

  /**
   * @brief 显示悬停符号提示
   * @param pos 光标位置
   * @param globalPos 全局坐标（用于定位 Tooltip）
   * @param text 文本内容
   */
  void showHover(int pos, const QPoint &globalPos, const QString &text);

  /// 隐藏悬停提示
  void hideHover();

  /// 高亮当前符号的所有引用
  QList<QTextEdit::ExtraSelection> highlightReferences(const QString &name) const;

  /**
   * @brief 生成悬停提示文本
   * @param name 符号名
   * @return 格式化的提示文本
   */
  QString generateHoverText(const QString &name) const;

private:
  /// 通过源码搜索定位符号行号（当 AST 行号不可用时）
  int findSymbolLineByName(const QString &name, const QString &text) const;

  QHash<QString, AcSymbolEntry> m_symbolTable;
  AuiErrorToolTip *m_hoverTooltip;
  QString m_currentHoverSymbol;
};