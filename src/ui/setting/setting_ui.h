/**
 * @file setting_ui.h
 * @brief 设置界面 — 视图层
 *
 * 无边框对话框，左侧为分区导航（主题 / 颜色 / 快捷键），
 * 右侧使用 QStackedWidget 展示对应设置页。
 * 由 SettingMgr 控制器创建，通过 SettingModel 读写数据。
 *
 * 复用 AuiWindow / AuiStyle / AuiButton 统一外观（src/util/ui 封装）。
 */

#pragma once

#include <QDialog>
#include <QHash>

class QListWidget;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QWidget;

class SettingModel;

/**
 * @class SettingUi
 * @brief 设置对话框（视图层）
 *
 * MVC 中的视图层，负责界面布局与数据展示。
 * 三个分区：
 * - 颜色：顶部为整体配色（浅色/深色/自定义），下方为所有可配置颜色
 *          （分类分组，点击色块修改，可单个/全部重置）
 * - 字体：窗口/目录树/代码字体大小（SpinBox 修改，可单个/全部重置）
 * - 快捷键：所有可配置快捷键（双击修改）
 */
class SettingUi : public QDialog {
  Q_OBJECT

public:
  explicit SettingUi(QWidget *parent = nullptr);
  ~SettingUi() override = default;

  /// 绑定数据模型（由 SettingMgr 注入）
  void setModel(SettingModel *model);

  /// 初始化界面布局
  void setupUI();

  /// 重新加载所有页数据（主题变化后刷新颜色展示）
  void reloadAll();

  /// 主题切换后刷新本对话框所有固化的样式表（分区列表/颜色表/快捷键表/色块）
  void refreshStyle();

private slots:
  /// 左侧分区切换
  void onSectionChanged(int row);
  /// 主题单选变化
  void onThemeToggled();
  /// 颜色表点击（点击色块列弹出取色器）
  void onColorCellClicked(int row, int column);
  /// 重置单个颜色
  void onResetColor();
  /// 重置所有颜色
  void onResetAllColors();
  /// 快捷键表双击（打开捕获对话框）
  void onShortcutDoubleClicked(int row, int column);
  /// 字体大小变化
  void onFontValueChanged();
  /// 重置单个字体
  void onResetFont();
  /// 重置所有字体
  void onResetAllFonts();

private:
  /// 构建「颜色」页（含顶部主题配色 + 底部重置所有颜色）
  void buildColorPage();
  /// 构建「字体」页（含各字体大小 SpinBox + 底部重置所有字体）
  void buildFontPage();
  /// 构建「快捷键」页
  void buildShortcutPage();
  /// 刷新主题页选中状态
  void reloadTheme();
  /// 刷新颜色表数据
  void reloadColors();
  /// 刷新字体页数据
  void reloadFonts();
  /// 刷新快捷键表数据
  void reloadShortcuts();
  /// 创建颜色表（分类分组）
  void populateColorTable();
  /// 创建快捷键徽章控件
  QWidget *createKeyBadge(const QString &text);
  /// 创建快捷键捕获对话框并返回新序列（取消返回空）
  bool captureShortcut(const QString &current, QString *out);

  QWidget *m_titleBar = nullptr;                  ///< 自定义标题栏（nativeEvent 拖拽）
  QListWidget *m_sections = nullptr;              ///< 左侧分区导航
  QStackedWidget *m_stack = nullptr;              ///< 右侧设置页堆栈
  QRadioButton *m_lightRadio = nullptr;           ///< 浅色主题
  QRadioButton *m_darkRadio = nullptr;            ///< 深色主题
  QRadioButton *m_customRadio = nullptr;          ///< 自定义主题
  QPushButton *m_resetAllColorBtn = nullptr;      ///< 重置所有颜色按钮
  QTableWidget *m_colorTable = nullptr;           ///< 颜色表
  QTableWidget *m_shortcutTable = nullptr;        ///< 快捷键表
  QHash<QString, QSpinBox *> m_fontSpins;         ///< 字体 key → 字号 SpinBox
  QHash<QString, QPushButton *> m_fontResetBtns;  ///< 字体 key → 单行重置按钮
  QPushButton *m_resetAllFontBtn = nullptr;       ///< 重置所有字体按钮

  SettingModel *m_model = nullptr;  ///< 数据模型（由控制器注入）

#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
};
