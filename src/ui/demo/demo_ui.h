/**
 * @file demo_ui.h
 * @brief Demo 视图层
 *
 * 由 DemoMgr 控制器创建和管理。包含模板编辑区、JSON 数据编辑区、
 * 输出显示区以及生成/加载/保存操作按钮。
 */

#pragma once

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>

class CodeEditor;

/**
 * @class DemoUi
 * @brief Demo 示例窗口（视图层）
 *
 * MVC 架构中的视图层，负责界面布局和用户交互呈现。
 * 将信号发射给 DemoMgr 处理业务逻辑。
 */
class DemoUi : public QMainWindow {
  Q_OBJECT

public:
  explicit DemoUi(QWidget *parent = nullptr);
  ~DemoUi() override = default;

  /// 初始化界面布局
  void setupUI();

  // ════════════════════════════════════════════════════════════
  //  控件 getter
  // ════════════════════════════════════════════════════════════
  CodeEditor *templateEdit() const { return m_templateEdit; }
  CodeEditor *dataEdit() const { return m_dataEdit; }
  CodeEditor *outputEdit() const { return m_outputEdit; }

  QAction *generateAction() const { return m_generateAction; }
  QAction *loadTemplateAction() const { return m_loadTemplateAction; }
  QAction *loadDataAction() const { return m_loadDataAction; }
  QAction *saveOutputAction() const { return m_saveOutputAction; }

  void setStatusText(const QString &text, int timeout = 0);

signals:
  /// 用户点击"生成代码"
  void generateRequested();
  /// 用户点击"加载模板文件"
  void loadTemplateRequested();
  /// 用户点击"加载数据文件"
  void loadDataRequested();
  /// 用户点击"保存结果"
  void saveOutputRequested();

private:
  // ── 窗口事件 ──
#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

  CodeEditor *m_templateEdit = nullptr;  ///< 模板编辑区
  CodeEditor *m_dataEdit = nullptr;      ///< JSON 数据编辑区
  CodeEditor *m_outputEdit = nullptr;    ///< 输出显示区
  QLabel *m_statusLabel = nullptr;       ///< 状态栏标签

  QAction *m_generateAction = nullptr;      ///< 生成代码动作
  QAction *m_loadTemplateAction = nullptr;  ///< 加载模板动作
  QAction *m_loadDataAction = nullptr;      ///< 加载数据动作
  QAction *m_saveOutputAction = nullptr;    ///< 保存结果动作

  QWidget *m_titleBar = nullptr;  ///< 自定义标题栏（用于 nativeEvent 拖拽）
};