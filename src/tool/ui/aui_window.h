/**
 * @file aui_window.h
 * @brief 无边框窗口工具类
 *
 * 提供可复用的无边框窗口样式，支持自定义标题栏、Win32 边框拉伸。
 * MainDevUi、CreateUi 等窗口统一使用此工具类，确保外观一致。
 *
 * 使用场景：
 * - 主窗口（QMainWindow）：setupFramelessWindow() + createTitleBar() + applyWindowFrame()
 * - 模态对话框（QDialog）：setupFramelessDialog() + createTitleBar() + applyWindowFrame()
 */

#pragma once

#include <QIcon>
#include <QLabel>
#include <QString>
#include <QWidget>

class QDialog;
class QLabel;
class QVBoxLayout;

/// 标题栏创建选项
struct TitleBarOptions {
  QString title;                   ///< 标题文字（空则不显示标题标签）
  bool titleRightAligned = false;  ///< true = 标题在 stretch 之后（右对齐）
  bool showMinButton = true;       ///< 显示最小化按钮
  bool showMaxButton = true;       ///< 显示最大化按钮
  bool showCloseButton = true;     ///< 显示关闭按钮
  bool closeRejectsDialog =
      false;  ///< true = 关闭按钮调用 QDialog::reject()（而非 QWidget::close()）
};

/// 标题栏创建结果
struct TitleBarResult {
  QWidget *titleBar = nullptr;   ///< 标题栏容器
  QLabel *titleLabel = nullptr;  ///< 标题文字标签（可能为 nullptr）
  int contentInsertIndex = 1;    ///< 自定义控件插入位置（AC 图标之后）
};

class AuiWindow {
public:
  /// 无边框窗口基础设置：FramelessWindowHint + 基础样式表
  static void setupFramelessWindow(QWidget *window);

  /// 无边框对话框设置：保留 Qt::Dialog 标志 + FramelessWindowHint + 基础样式表
  /// @note QDialog 必须保留 Qt::Dialog 标志，否则 exec() 无法正常显示
  static void setupFramelessDialog(QDialog *dialog);

  /// 模态对话框样式设置：应用统一样式表 + 设置 DWM 标题栏颜色
  /// @note QDialog 使用 exec() 必须保留原生标题栏，不能使用 FramelessWindowHint
  static void setupDialogStyle(QDialog *dialog);

  /// 创建标准标题栏（AC 图标 + 标题 + 窗口控制按钮）
  /// @param window  窗口（用于连接按钮信号、监听窗口状态变化更新最大化图标）
  /// @param options 标题栏选项
  /// @return TitleBarResult 包含标题栏控件和插入位置
  static TitleBarResult createTitleBar(QWidget *window, const TitleBarOptions &options = {});

  /// 创建 AC 程序图标（24x24 的 Consolas 粗体字母标签）
  static QLabel *createAppIcon(QWidget *parent = nullptr, int size = 24);

  /// 获取 AC 程序图标 Pixmap，可用于 setWindowIcon() 等场景
  static QPixmap appIconPixmap(int size = 20);

  /// 创建底部状态栏（状态文字 + QSizeGrip 可拖拽三角）
  /// @param parent 父控件
  /// @param text   初始状态文字（如 "就绪"）
  /// @return 状态栏 QWidget 指针，可直接添加到布局底部
  static QWidget *createStatusBar(QWidget *parent, const QString &text = QStringLiteral("就绪"));

  /// 创建底部状态栏（自定义内容控件 + QSizeGrip 可拖拽三角）
  /// @param parent  父控件
  /// @param content 内容控件（如包含多个标签的自定义 QWidget），会以 stretch=1 添加到布局中
  /// @return 状态栏 QWidget 指针，可直接添加到布局底部
  static QWidget *createStatusBar(QWidget *parent, QWidget *content);

  /// 创建窗口外层框架（1px 边框 QFrame），将内容控件包裹在内
  /// @param window   窗口
  /// @param titleBar 标题栏（可为 nullptr）
  /// @param content  内容控件（如 QSplitter、QWidget）
  static void applyWindowFrame(QWidget *window, QWidget *titleBar, QWidget *content);

  /// 启用标题栏拖拽（使用 Qt startSystemMove 替代 HTCAPTION，避免模态对话框关闭后拖拽失效）
  /// @param window   窗口
  /// @param titleBar 标题栏（可为 nullptr）
  static void enableTitleBarDrag(QWidget *window, QWidget *titleBar);

  /// 切换窗口最大化/还原状态
  /// @param window 目标窗口
  static void toggleMaximize(QWidget *window);

  /// 添加 Win32 拉伸边框支持（WS_THICKFRAME）
  static void enableWin32Resize(QWidget *window);

  /// 为模态对话框安装父窗口遮罩层
  /// 在对话框显示前调用，父窗口会被半透明遮罩覆盖，形成"变暗"效果
  /// @param dialog 模态对话框（必须有 parentWidget）
  /// @return 遮罩 QWidget 指针，对话框关闭后传给 removeModalOverlay 移除
  static QWidget *installModalOverlay(QDialog *dialog);

  /// 移除父窗口遮罩层
  /// 在对话框关闭时调用
  /// @param overlay installModalOverlay 返回的遮罩指针
  static void removeModalOverlay(QWidget *overlay);

#if defined(Q_OS_WIN)
  /// Win32 原生事件处理（WM_NCHITTEST 边框拉伸 + 标题栏拖拽）
  /// @param window    窗口实例
  /// @param titleBar  标题栏（用于 HTCAPTION 区域判断）
  /// @param eventType 事件类型
  /// @param message   事件消息
  /// @param result    处理结果
  /// @return true 表示已处理
  static bool handleNativeEvent(QWidget *window, QWidget *titleBar, const QByteArray &eventType,
                                void *message, qintptr *result);
#endif

private:
  AuiWindow() = delete;
};