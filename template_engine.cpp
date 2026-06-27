/**
 * @file template_engine.cpp
 * @brief 模板引擎实现文件
 * 
 * 包含模板解析、变量替换、循环、条件判断和算术运算的完整实现。
 */

#include "template_engine.h"
#include <QRegularExpression>

/**
 * @brief 渲染模板入口函数
 * 
 * 清除之前的错误信息，然后调用 renderBlock 进行递归渲染。
 */
QString TemplateEngine::render(const QString &tmpl, const QJsonObject &data) const
{
    m_lastError.clear();
    return renderBlock(tmpl, data);
}

/**
 * @brief 递归渲染模板片段
 * 
 * 核心渲染逻辑：
 * 1. 从左到右扫描模板
 * 2. 找到 ${ 开始标记
 * 3. 提取 ${ 和 } 之间的表达式
 * 4. 根据表达式类型分别处理：
 *    - "each ..." 开头：循环处理
 *    - "if ..." 开头：条件处理
 *    - 其他：表达式求值
 * 5. 将结果拼接到输出
 */
QString TemplateEngine::renderBlock(const QString &block, const QJsonObject &context) const
{
    QString result;
    int pos = 0;

    while (pos < block.length()) {
        // 查找下一个 ${ 开始标记
        int start = block.indexOf(QStringLiteral("${"), pos);
        if (start == -1) {
            // 没有找到更多标记，将剩余文本全部追加
            result += block.mid(pos);
            break;
        }

        // 追加 ${ 之前的普通文本
        result += block.mid(pos, start - pos);

        // 查找对应的 } 结束标记
        int end = block.indexOf(QStringLiteral("}"), start + 2);
        if (end == -1) {
            // 错误：未闭合的 ${
            m_lastError = QStringLiteral("Unclosed ${ at position %1").arg(start);
            return {};
        }

        // 提取表达式内容（去掉 ${ 和 }）
        QString expr = block.mid(start + 2, end - start - 2).trimmed();
        pos = end + 1;  // 移动到 } 之后

        // ========== 处理循环语句 ${each items}...${/each} ==========
        if (expr.startsWith(QStringLiteral("each "))) {
            // 提取循环变量名（如 "items"）
            QString varName = expr.mid(5).trimmed();

            // 查找对应的结束标记 ${/each}
            int eachStart = pos;
            int eachEnd = block.indexOf(QStringLiteral("${/each}"), pos);
            if (eachEnd == -1) {
                m_lastError = QStringLiteral("Unclosed ${each %1}").arg(varName);
                return {};
            }

            // 提取循环体内容
            QString body = block.mid(eachStart, eachEnd - eachStart);
            pos = eachEnd + 8;  // 跳过 ${/each}（8个字符）

            // 执行循环渲染
            result += processEach(varName, body, context);
            continue;
        }

        // ========== 处理条件语句 ${if condition}...${else}...${/if} ==========
        if (expr.startsWith(QStringLiteral("if "))) {
            // 提取条件表达式
            QString condition = expr.mid(3).trimmed();

            // 查找 ${else} 和 ${/if} 标记
            int ifStart = pos;
            int elsePos = block.indexOf(QStringLiteral("${else}"), pos);
            int ifEnd = block.indexOf(QStringLiteral("${/if}"), pos);

            if (ifEnd == -1) {
                m_lastError = QStringLiteral("Unclosed ${if %1}").arg(condition);
                return {};
            }

            QString thenBlock;  // 条件为真时的模板
            QString elseBlock;  // 条件为假时的模板

            // 判断是否有 else 分支
            if (elsePos != -1 && elsePos < ifEnd) {
                // 有 else 分支
                thenBlock = block.mid(ifStart, elsePos - ifStart);
                elseBlock = block.mid(elsePos + 7, ifEnd - elsePos - 7);  // 跳过 ${else}（7个字符）
            } else {
                // 没有 else 分支
                thenBlock = block.mid(ifStart, ifEnd - ifStart);
                elseBlock = QString();
            }

            pos = ifEnd + 6;  // 跳过 ${/if}（6个字符）

            // 执行条件渲染
            result += processIf(condition, thenBlock, elseBlock, context);
            continue;
        }

        // ========== 处理普通表达式 ${expression} ==========
        result += evaluate(expr, context);
    }

    return result;
}

/**
 * @brief 解析嵌套属性路径
 * 
 * 支持点号分隔的属性访问，如 "user.name.email"
 * 会逐层查找：context["user"]["name"]["email"]
 * 
 * 示例：
 * - resolvePath("name", {name: "John"}) → "John"
 * - resolvePath("user.name", {user: {name: "John"}}) → "John"
 */
QJsonValue TemplateEngine::resolvePath(const QString &path, const QJsonObject &context) const
{
    // 按点号分割路径
    QStringList parts = path.split(QStringLiteral("."));
    QJsonValue current(context);

    // 逐层查找
    for (const QString &part : parts) {
        if (!current.isObject()) {
            // 当前值不是对象，无法继续查找
            return QJsonValue();
        }
        current = current.toObject().value(part);
    }

    return current;
}

/**
 * @brief 计算表达式的值
 * 
 * 处理流程：
 * 1. 特殊处理 ${this} 和 ${.}（循环中的当前元素）
 * 2. 判断是否为算术表达式
 * 3. 如果是算术表达式，调用 evalExpression 计算
 * 4. 否则作为变量名解析
 * 
 * 返回值会自动转换为字符串：
 * - 整数：去掉小数点（如 5.0 → "5"）
 * - 浮点数：保留小数（如 3.14 → "3.14"）
 * - 布尔值：转换为 "true" 或 "false"
 * - 字符串：直接返回
 */
QString TemplateEngine::evaluate(const QString &expr, const QJsonObject &context) const
{
    // 处理循环变量 ${this} 或 ${.}
    if (expr == QStringLiteral("this") || expr == QStringLiteral(".")) {
        QJsonValue val = context.value(QStringLiteral("."));
        if (val.isString()) return val.toString();
        if (val.isDouble()) return QString::number(val.toDouble());
        if (val.isBool()) return val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        return QString();
    }

    // 检查是否为算术表达式（包含 +、-、*、/）
    if (isArithmeticExpr(expr)) {
        double result = evalExpression(expr, context);
        // 如果是整数，去掉小数点
        if (result == static_cast<int>(result)) {
            return QString::number(static_cast<int>(result));
        }
        return QString::number(result);
    }

    // 作为变量名解析
    QJsonValue val = resolvePath(expr, context);

    // 根据值类型转换为字符串
    if (val.isString()) return val.toString();
    if (val.isDouble()) {
        double d = val.toDouble();
        if (d == static_cast<int>(d)) {
            return QString::number(static_cast<int>(d));
        }
        return QString::number(d);
    }
    if (val.isBool()) return val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (val.isNull()) return QString();

    return val.toString();
}

/**
 * @brief 处理循环语句
 * 
 * 实现逻辑：
 * 1. 从上下文获取数组变量
 * 2. 遍历数组中的每个元素
 * 3. 为每个元素创建新的上下文：
 *    - 如果元素是对象，将其属性合并到上下文
 *    - 如果元素是基本类型，存入特殊键 "."
 * 4. 用新上下文渲染循环体
 * 5. 拼接所有结果
 * 
 * 示例：
 * 模板：${each items}Item: ${name}\n${/each}
 * 数据：{items: [{name: "A"}, {name: "B"}]}
 * 结果：Item: A\nItem: B\n
 */
QString TemplateEngine::processEach(const QString &varName, const QString &body,
                                     const QJsonObject &context) const
{
    // 获取数组变量
    QJsonValue val = resolvePath(varName, context);
    if (!val.isArray()) {
        return QString();  // 不是数组，返回空
    }

    QJsonArray arr = val.toArray();
    QString result;

    // 遍历数组
    for (int i = 0; i < arr.size(); ++i) {
        // 复制当前上下文
        QJsonObject itemContext = context;
        QJsonValue item = arr[i];

        // 处理当前元素
        if (item.isObject()) {
            // 对象元素：将属性合并到上下文
            QJsonObject obj = item.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                itemContext.insert(it.key(), it.value());
            }
            // 同时设置 "." 为空值（对象元素通常直接用属性名访问）
            itemContext.insert(QStringLiteral("."), QJsonValue());
        } else {
            // 基本类型元素：存入特殊键 "."
            itemContext.insert(QStringLiteral("."), item);
        }

        // 用新上下文渲染循环体
        result += renderBlock(body, itemContext);
    }

    return result;
}

/**
 * @brief 处理条件语句
 * 
 * 实现逻辑：
 * 1. 从上下文获取条件变量
 * 2. 根据类型判断真假（truthy/falsy 规则）
 * 3. 根据结果渲染对应的分支
 * 
 * Truthy 规则（JavaScript 风格）：
 * - false, 0, "", null, undefined → 假
 * - 其他所有值 → 真
 * 
 * 示例：
 * 模板：${if show}Hello${else}Goodbye${/if}
 * 数据：{show: true}
 * 结果：Hello
 */
QString TemplateEngine::processIf(const QString &condition, const QString &thenBlock,
                                   const QString &elseBlock, const QJsonObject &context) const
{
    // 获取条件变量
    QJsonValue val = resolvePath(condition, context);

    // 判断真假（truthy/falsy 规则）
    bool isTrue = false;
    if (val.isBool()) {
        isTrue = val.toBool();  // 布尔值直接使用
    } else if (val.isString()) {
        isTrue = !val.toString().isEmpty();  // 非空字符串为真
    } else if (val.isDouble()) {
        isTrue = val.toDouble() != 0;  // 非零数字为真
    } else if (val.isArray()) {
        isTrue = !val.toArray().isEmpty();  // 非空数组为真
    } else if (val.isObject()) {
        isTrue = true;  // 对象总是为真
    }
    // null 和 undefined 默认为假

    // 根据条件渲染对应分支
    if (isTrue) {
        return renderBlock(thenBlock, context);
    } else {
        return renderBlock(elseBlock, context);
    }
}

/**
 * @brief 判断表达式是否包含算术运算符
 * 
 * 扫描表达式，检查是否有 +、-、*、/ 运算符。
 * 需要排除以下情况：
 * - 变量名中的点号（如 user.name）
 * - 一元运算符（如 -5, +x）
 * 
 * 实现技巧：
 * - 使用括号深度计数，只检查括号外的运算符
 * - 一元运算符的特征：出现在开头、左括号后、或其他运算符后
 * 
 * 示例：
 * - isArithmeticExpr("name") → false
 * - isArithmeticExpr("a + b") → true
 * - isArithmeticExpr("user.name") → false
 * - isArithmeticExpr("-5") → false（一元运算符）
 */
bool TemplateEngine::isArithmeticExpr(const QString &expr) const
{
    int parenDepth = 0;  // 括号深度
    for (int i = 0; i < expr.length(); ++i) {
        QChar ch = expr[i];
        
        // 跟踪括号深度
        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')') {
            --parenDepth;
        } else if ((ch == '+' || ch == '-' || ch == '*' || ch == '/') && parenDepth == 0) {
            // 检查是否为一元运算符
            if ((ch == '+' || ch == '-') && (i == 0 || expr[i-1] == '(' || expr[i-1] == '+' || expr[i-1] == '-' || expr[i-1] == '*' || expr[i-1] == '/')) {
                continue;  // 这是一元运算符，跳过
            }
            return true;  // 找到二元运算符
        }
    }
    return false;
}

/**
 * @brief 算术表达式求值入口
 * 
 * 使用递归下降解析器，支持：
 * - 运算符优先级：乘除优先于加减
 * - 括号：改变优先级
 * - 一元运算符：+x, -x
 * 
 * 语法定义（EBNF）：
 * Expression ::= Term (('+' | '-') Term)*
 * Term       ::= Factor (('*' | '/') Factor)*
 * Factor     ::= Primary | ('+' | '-') Factor
 * Primary    ::= Number | Variable | '(' Expression ')'
 * 
 * 示例：
 * - "2 + 3 * 4" → 14（先乘后加）
 * - "(2 + 3) * 4" → 20（括号优先）
 */
double TemplateEngine::evalExpression(const QString &expr, const QJsonObject &context) const
{
    int pos = 0;  // 当前解析位置
    double result = evalAddSub(expr, pos, context);
    return result;
}

/**
 * @brief 解析加减法（低优先级）
 * 
 * 语法：Term (('+' | '-') Term)*
 * 
 * 实现逻辑：
 * 1. 先解析一个 Term（乘除法表达式）
 * 2. 循环检查是否有 + 或 - 运算符
 * 3. 如果有，解析下一个 Term 并执行运算
 * 4. 重复步骤 2-3 直到没有更多运算符
 * 
 * 注意：解析前需要跳过空白字符
 * 
 * 示例：
 * - "a + b - c" → 先计算 a，然后 + b，然后 - c
 */
double TemplateEngine::evalAddSub(const QString &expr, int &pos, const QJsonObject &context) const
{
    // 先解析左操作数（乘除法）
    double left = evalMulDiv(expr, pos, context);

    // 循环处理加减运算符
    while (pos < expr.length()) {
        // 跳过空白字符
        while (pos < expr.length() && expr[pos].isSpace()) ++pos;
        if (pos >= expr.length()) break;

        QChar ch = expr[pos];
        if (ch == '+') {
            ++pos;  // 跳过 '+'
            left += evalMulDiv(expr, pos, context);  // 解析右操作数并相加
        } else if (ch == '-') {
            ++pos;  // 跳过 '-'
            left -= evalMulDiv(expr, pos, context);  // 解析右操作数并相减
        } else {
            break;  // 不是加减运算符，退出循环
        }
    }

    return left;
}

/**
 * @brief 解析乘除法（高优先级）
 * 
 * 语法：Primary (('*' | '/') Primary)*
 * 
 * 实现逻辑：
 * 1. 先解析一个 Primary（基本元素）
 * 2. 循环检查是否有 * 或 / 运算符
 * 3. 如果有，解析下一个 Primary 并执行运算
 * 4. 重复步骤 2-3 直到没有更多运算符
 * 
 * 注意：
 * - 解析前需要跳过空白字符
 * - 除法需要检查除数为零的情况
 * 
 * 示例：
 * - "a * b / c" → 先计算 a，然后 * b，然后 / c
 */
double TemplateEngine::evalMulDiv(const QString &expr, int &pos, const QJsonObject &context) const
{
    // 先解析左操作数（基本元素）
    double left = evalPrimary(expr, pos, context);

    // 循环处理乘除运算符
    while (pos < expr.length()) {
        // 跳过空白字符
        while (pos < expr.length() && expr[pos].isSpace()) ++pos;
        if (pos >= expr.length()) break;

        QChar ch = expr[pos];
        if (ch == '*') {
            ++pos;  // 跳过 '*'
            left *= evalPrimary(expr, pos, context);  // 解析右操作数并相乘
        } else if (ch == '/') {
            ++pos;  // 跳过 '/'
            double right = evalPrimary(expr, pos, context);  // 解析右操作数
            if (right == 0) {
                m_lastError = QStringLiteral("Division by zero");
                return 0;
            }
            left /= right;  // 相除
        } else {
            break;  // 不是乘除运算符，退出循环
        }
    }

    return left;
}

/**
 * @brief 解析基本元素（最高优先级）
 * 
 * 支持四种基本元素：
 * 1. 数字字面量：123, 3.14, .5
 * 2. 变量：a, user.name
 * 3. 括号表达式：(a + b)
 * 4. 一元运算符：+5, -x
 * 
 * 实现逻辑：
 * 1. 跳过前导空白
 * 2. 根据第一个字符判断元素类型
 * 3. 解析并返回对应的值
 * 
 * 注意：
 * - 变量名支持字母、数字、下划线和点号
 * - 括号表达式会递归调用 evalAddSub
 * - 一元运算符会递归调用 evalPrimary
 */
double TemplateEngine::evalPrimary(const QString &expr, int &pos, const QJsonObject &context) const
{
    // 跳过前导空白字符
    while (pos < expr.length() && expr[pos].isSpace()) {
        ++pos;
    }

    // 检查是否到达表达式末尾
    if (pos >= expr.length()) {
        m_lastError = QStringLiteral("Unexpected end of expression");
        return 0;
    }

    QChar ch = expr[pos];

    // ========== 处理一元运算符 ==========
    if (ch == '+') {
        ++pos;  // 跳过 '+'
        return evalPrimary(expr, pos, context);  // 递归解析，返回正值
    }
    if (ch == '-') {
        ++pos;  // 跳过 '-'
        return -evalPrimary(expr, pos, context);  // 递归解析，返回负值
    }

    // ========== 处理括号表达式 ==========
    if (ch == '(') {
        ++pos;  // 跳过 '('
        // 递归解析括号内的表达式（从加减法开始）
        double result = evalAddSub(expr, pos, context);
        // 跳过空白字符
        while (pos < expr.length() && expr[pos].isSpace()) {
            ++pos;
        }
        // 检查并跳过右括号
        if (pos < expr.length() && expr[pos] == ')') {
            ++pos;
        } else {
            m_lastError = QStringLiteral("Missing closing parenthesis");
        }
        return result;
    }

    // ========== 处理数字字面量 ==========
    if (ch.isDigit() || ch == '.') {
        int start = pos;
        // 读取数字（包括小数点）
        while (pos < expr.length() && (expr[pos].isDigit() || expr[pos] == '.')) {
            ++pos;
        }
        // 转换为浮点数
        bool ok;
        double result = expr.mid(start, pos - start).toDouble(&ok);
        if (!ok) {
            m_lastError = QStringLiteral("Invalid number");
            return 0;
        }
        return result;
    }

    // ========== 处理变量名 ==========
    if (ch.isLetter() || ch == '_') {
        int start = pos;
        // 读取变量名（支持字母、数字、下划线和点号）
        while (pos < expr.length() && (expr[pos].isLetterOrNumber() || expr[pos] == '_' || expr[pos] == '.')) {
            ++pos;
        }
        QString varName = expr.mid(start, pos - start);

        // 从上下文解析变量值
        QJsonValue val = resolvePath(varName, context);
        if (val.isDouble()) {
            return val.toDouble();  // 数字值直接返回
        }
        if (val.isString()) {
            bool ok;
            double result = val.toString().toDouble(&ok);
            if (ok) return result;  // 字符串尝试转换为数字
        }

        // 无法转换为数字
        m_lastError = QStringLiteral("Cannot convert '%1' to number").arg(varName);
        return 0;
    }

    // 未知字符
    m_lastError = QStringLiteral("Unexpected character '%1'").arg(ch);
    return 0;
}
