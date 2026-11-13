/** @file domain_source_service.h
 * @brief 数据源作用域、生成命名与枚举候选的业务规则。
 */
#pragma once
#include <QHash>
#include <QPair>
#include <QString>
class AuiTreeCombo;

/// jsonsource 数据源作用域帮助文案（供数据源类下拉框旁的问号帮助按钮共用）。
/// 说明「右键设为项目」对数据源下拉框内容的影响：项目根过滤 + 未设项目时的回退规则
QString jsonVueSourceScopeHelpText();

/// jsonupload 上传预设作用域帮助文案（供上传预设下拉框旁的问号帮助按钮共用）。
/// 结构与 jsonVueSourceScopeHelpText 相同，措辞针对 .jsonupload 上传预设文件
QString jsonVueUploadScopeHelpText();

// ── 数据源函数名推导（与 AC 脚本 tool_str.ac / admin_data.ac 保持一致）──

/// 下划线命名 → 帕斯卡命名（"user_role" → "UserRole"）
QString snakeToPascal(const QString &str);

/// 下划线命名 → 小驼峰命名（"user_role" → "userRole"）
QString snakeToCamel(const QString &str);

/// 数据源 url → 函数名（第一段小驼峰，后续段帕斯卡拼接）
/// "enableState" → "enableState"；"vipprice/getTime" → "vippriceGetTime"
QString sourceUrlToFuncName(const QString &url);

/// 静态数据源函数名（与 admin_data.ac processSources 一致）：
/// 文件名小驼峰 + "Static" + url名（帕斯卡），
/// boolean.jsonsource + enableState → booleanStaticEnableState
QString staticSourceFuncName(const QString &sourceName, const QString &url);

/// 构建取值域数据源候选树（方案 B：列样式配置与查询设置共用）。
/// 候选 = 作用域内 .jsonsource 数据源（静态源 + 动态源，动态源条目显示接口 url）
/// + 全局枚举；enumOnly=true 仅列恰好 2 项的静态源（枚举域）；
/// boolTexts 输出 ref→{真值文字,假值文字}（仅静态源），previews 输出 ref→选项预览，
/// optionCounts 输出 ref→选项数（动态源为 -1，供"恰 2 项 = 枚举能力"的推导），
/// dynamicFlags 输出 ref→是否动态源
void buildDomainSourceCandidates(AuiTreeCombo *combo, const QString &searchRoot, bool enumOnly,
                                 QHash<QString, QPair<QString, QString>> *boolTexts,
                                 QHash<QString, QString> *previews,
                                 QHash<QString, int> *optionCounts,
                                 QHash<QString, bool> *dynamicFlags = nullptr);
