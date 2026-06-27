/**
 * @file template_engine.h
 * @brief 模板引擎头文件
 * 
 * 实现一个简单的模板引擎，支持：
 * - 变量替换: ${variableName} 或 ${obj.property}
 * - 循环: ${each items}...${/each}
 * - 条件判断: ${if condition}...${else}...${/if}
 * - 算术运算: ${a + b * c}
 */

#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

/**
 * @class TemplateEngine
 * @brief 模板引擎类
 * 
 * 提供模板渲染功能，将模板字符串和 JSON 数据结合，生成最终输出。
 * 支持变量替换、循环、条件判断和算术运算。
 */
class TemplateEngine
{
public:
    /**
     * @brief 默认构造函数
     */
    TemplateEngine() = default;

    /**
     * @brief 渲染模板
     * @param tmpl 模板字符串，包含 ${...} 占位符
     * @param data JSON 对象，作为变量上下文
     * @return 渲染后的字符串，如果出错返回空字符串
     */
    QString render(const QString &tmpl, const QJsonObject &data) const;

    /**
     * @brief 获取最后一次渲染的错误信息
     * @return 错误信息字符串，无错误时为空
     */
    QString lastError() const { return m_lastError; }

private:
    /**
     * @brief 计算单个表达式的值
     * @param expr 表达式（不含 ${ 和 }）
     * @param context 变量上下文
     * @return 计算结果的字符串表示
     * 
     * 支持：
     * - 简单变量: ${name}
     * - 嵌套属性: ${user.name}
     * - 算术表达式: ${a + b * c}
     * - 循环变量: ${this} 或 ${.}
     */
    QString evaluate(const QString &expr, const QJsonObject &context) const;

    /**
     * @brief 解析嵌套属性路径
     * @param path 属性路径，如 "user.name.email"
     * @param context 变量上下文
     * @return 解析到的 JSON 值
     * 
     * 示例：resolvePath("user.name", {user: {name: "John"}}) 返回 "John"
     */
    QJsonValue resolvePath(const QString &path, const QJsonObject &context) const;

    /**
     * @brief 递归渲染模板片段
     * @param block 模板片段
     * @param context 变量上下文
     * @return 渲染后的字符串
     * 
     * 处理 ${...} 占位符，包括：
     * - ${each items}...${/each} 循环
     * - ${if condition}...${else}...${/if} 条件
     * - ${expression} 表达式求值
     */
    QString renderBlock(const QString &block, const QJsonObject &context) const;

    /**
     * @brief 处理循环语句
     * @param varName 循环变量名
     * @param body 循环体模板
     * @param context 变量上下文
     * @return 渲染后的字符串
     * 
     * 语法：${each items}...${/each}
     * - items 必须是数组
     * - 如果元素是对象，其属性会合并到上下文
     * - 如果元素是基本类型，可通过 ${this} 或 ${.} 访问
     */
    QString processEach(const QString &varName, const QString &body,
                        const QJsonObject &context) const;

    /**
     * @brief 处理条件语句
     * @param condition 条件表达式（变量名）
     * @param thenBlock 条件为真时的模板
     * @param elseBlock 条件为假时的模板
     * @param context 变量上下文
     * @return 渲染后的字符串
     * 
     * 语法：${if condition}...${else}...${/if}
     * 判断规则：
     * - bool: 直接使用
     * - string: 非空为真
     * - number: 非零为真
     * - array: 非空为真
     * - object: 总是为真
     * - null/undefined: 为假
     */
    QString processIf(const QString &condition, const QString &thenBlock,
                      const QString &elseBlock, const QJsonObject &context) const;

    /**
     * @brief 判断表达式是否包含算术运算符
     * @param expr 表达式字符串
     * @return 是否包含 +、-、*、/ 运算符
     * 
     * 用于区分数学表达式和简单变量名
     */
    bool isArithmeticExpr(const QString &expr) const;

    /**
     * @brief 算术表达式求值入口
     * @param expr 表达式字符串
     * @param context 变量上下文
     * @return 计算结果
     * 
     * 使用递归下降解析器，支持运算符优先级和括号
     */
    double evalExpression(const QString &expr, const QJsonObject &context) const;

    /**
     * @brief 解析加减法（低优先级）
     * @param expr 表达式字符串
     * @param pos 当前解析位置（会被修改）
     * @param context 变量上下文
     * @return 计算结果
     * 
     * 语法: Term (('+' | '-') Term)*
     */
    double evalAddSub(const QString &expr, int &pos, const QJsonObject &context) const;

    /**
     * @brief 解析乘除法（高优先级）
     * @param expr 表达式字符串
     * @param pos 当前解析位置（会被修改）
     * @param context 变量上下文
     * @return 计算结果
     * 
     * 语法: Primary (('*' | '/') Primary)*
     */
    double evalMulDiv(const QString &expr, int &pos, const QJsonObject &context) const;

    /**
     * @brief 解析基本元素（最高优先级）
     * @param expr 表达式字符串
     * @param pos 当前解析位置（会被修改）
     * @param context 变量上下文
     * @return 计算结果
     * 
     * 支持：
     * - 数字字面量: 123, 3.14
     * - 变量: a, user.name
     * - 括号表达式: (a + b)
     * - 一元运算符: -5, +x
     */
    double evalPrimary(const QString &expr, int &pos, const QJsonObject &context) const;

    /**
     * @brief 最后一次渲染的错误信息
     * 
     * 使用 mutable 允许在 const 方法中修改
     */
    mutable QString m_lastError;
};
