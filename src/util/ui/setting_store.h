/**
 * @file setting_store.h
 * @brief 运行时设置存储（主题、颜色、快捷键）
 *
 * 集中管理全局可配置的颜色与快捷键，供 AuiStyle / LightColor 及各组件读取。
 * 内置「浅色」「深色」两套主题，并允许用户自定义任意颜色。
 * 设置持久化到 AppData 目录下的 settings.json。
 *
 * 单例使用：SettingStore::ins()
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QPalette>
#include <QString>

class QTimer;

/**
 * @class SettingStore
 * @brief 全局设置存储（单例）
 *
 * 职责：
 * - 保存当前主题（浅色 / 深色 / 自定义）
 * - 保存所有可配置颜色（key → QColor）
 * - 保存所有可配置快捷键（key → QKeySequence）
 * - 从 AppData/settings.json 加载、保存
 * - 主题或颜色变化时发出信号，供界面实时刷新
 */
class SettingStore : public QObject {
  Q_OBJECT

public:
  /// 主题类型
  enum Theme { ThemeLight, ThemeDark, ThemeCustom };

  /// 获取单例
  static SettingStore &ins();

  /// 初始化（注册默认颜色、加载配置文件）
  void init();

  // ── 主题 ──

  Theme theme() const { return m_theme; }
  void setTheme(Theme t);

  /// 主题显示名（浅色 / 深色 / 自定义）
  QString themeName(Theme t) const;

  // ── 颜色 ──

  /// 获取颜色；若该 key 无自定义值，返回主题内置默认值
  QColor color(const QString &key) const;

  /// 设置颜色（进入自定义模式）
  void setColor(const QString &key, const QColor &c);

  /// 是否有该 key 的自定义颜色
  bool hasCustomColor(const QString &key) const;

  /// 所有已注册的颜色 key（用于设置界面展示）
  QStringList colorKeys() const;

  /// 颜色中文名（用于设置界面）
  QString colorLabel(const QString &key) const;

  /// 颜色分类（"界面" / "编辑器" / "代码高亮"）
  QString colorCategory(const QString &key) const;

  /// 恢复该 key 为主题默认色
  void resetColor(const QString &key);

  /// 恢复所有颜色为主题默认
  void resetAllColors();

  // ── 快捷键 ──

  /// 获取快捷键序列
  QKeySequence shortcut(const QString &key) const;

  /// 设置快捷键
  void setShortcut(const QString &key, const QKeySequence &seq);

  /// 所有已注册的快捷键 key
  QStringList shortcutKeys() const;

  /// 快捷键中文名
  QString shortcutLabel(const QString &key) const;

  /// 快捷键分类（"文件" / "视图" / "调试" / "编辑"）
  QString shortcutCategory(const QString &key) const;

  // ── 字体大小 ──

  /// 获取字体大小（磅值，6~40）；未自定义时返回默认值
  int fontSize(const QString &key) const;

  /// 设置字体大小（等于默认值时按恢复处理；修改「窗口字体」会立即应用到 qApp）
  void setFontSize(const QString &key, int size);

  /// 所有已注册的字体 key（用于设置界面展示）
  QStringList fontKeys() const;

  /// 字体中文名（用于设置界面）
  QString fontLabel(const QString &key) const;

  /// 该字体大小是否为用户自定义
  bool hasCustomFont(const QString &key) const;

  /// 恢复该 key 为默认字号
  void resetFontSize(const QString &key);

  /// 恢复所有字体为默认字号
  void resetAllFonts();

  /// 将「窗口字体」应用到 qApp（字号 + 字体族，未自定义字体族时沿用系统字体）
  void applyWindowFont();

  // ── 字体风格（字体族） ──

  /// 获取字体风格（字体族）；未自定义时返回空串（应用方回退到各自默认字体）
  QString fontFamily(const QString &key) const;

  /// 设置字体风格（字体族）；空串表示恢复默认
  void setFontFamily(const QString &key, const QString &family);

  /// 该字体的风格是否为用户自定义
  bool hasCustomFontFamily(const QString &key) const;

  /// 恢复该 key 为默认字体风格
  void resetFontFamily(const QString &key);

  // ── 持久化 ──

  /// 保存到 AppData/settings.json
  void save();

  /// 立即保存并广播「设置已应用」信号
  void apply();

  // ── 全局风格 ──

  /// 根据当前主题构建全局调色板（供 qApp 使用，隔离系统主题色）
  QPalette buildPalette() const;

  /// 应用全局风格到 qApp（Fusion 风格 + 调色板），使程序不随系统主题变色
  void applyGlobalStyle();

private slots:
  /// 窗口字体防抖定时器到点：合并连续修改后统一应用窗口字体并广播信号
  void onFontsDebounced();

signals:
  /// 主题变化
  void themeChanged();
  /// 任意颜色变化
  void colorsChanged();
  /// 快捷键变化
  void shortcutsChanged();
  /// 任意字体大小变化（目录树/代码等组件字体，需按各自设置即时刷新）
  void fontsChanged();
  /// 仅「窗口字体」变化（需做全窗口级刷新，代价较大，已内部防抖）
  void windowFontChanged();

private:
  SettingStore();
  ~SettingStore() override = default;
  SettingStore(const SettingStore &) = delete;
  SettingStore &operator=(const SettingStore &) = delete;

  /// 配置文件路径（AppData 目录）
  QString storePath() const;

  /// 注册一个颜色 key（含标签、分类、浅/深主题默认值）
  void registerColor(const QString &key, const QString &label, const QString &category,
                     const QColor &light, const QColor &dark);

  /// 注册一个快捷键 key（含标签、分类、默认序列）
  void registerShortcut(const QString &key, const QString &label, const QString &category,
                        const QString &defaultSeq);

  /// 注册一个字体 key（含标签、默认磅值、默认字体族；字体族空串表示跟随系统/默认）
  void registerFont(const QString &key, const QString &label, int defaultSize,
                    const QString &defaultFamily = QString());

  /// 从文件加载
  void loadFromFile();

  /// 主题内置色（light / dark）
  QHash<QString, QColor> m_themeLight;
  QHash<QString, QColor> m_themeDark;
  /// 用户自定义颜色覆盖
  QHash<QString, QColor> m_custom;
  /// 颜色标签与分类
  QHash<QString, QString> m_labels;
  QHash<QString, QString> m_categories;
  /// key 注册顺序（保证展示稳定）
  QStringList m_colorOrder;

  /// 快捷键默认值与自定义值
  QHash<QString, QKeySequence> m_shortcuts;
  QHash<QString, QString> m_shortcutLabels;
  QHash<QString, QString> m_shortcutCategories;
  QStringList m_shortcutOrder;

  /// 字体默认值与自定义值
  QHash<QString, int> m_fontDefaults;
  QHash<QString, int> m_fontCustom;
  QHash<QString, QString> m_fontLabels;
  /// 字体族默认值与自定义值（空串 = 跟随系统/默认）
  QHash<QString, QString> m_fontFamilyDefaults;
  QHash<QString, QString> m_fontFamilyCustom;
  QStringList m_fontOrder;

  Theme m_theme = ThemeLight;
  bool m_initialized = false;
  bool m_globalStyleApplied = false;  ///< 全局 Fusion 风格是否已应用（仅应用一次）
  QTimer *m_fontTimer = nullptr;      ///< 字体修改防抖定时器（合并连续修改，避免拖动卡顿）
};
