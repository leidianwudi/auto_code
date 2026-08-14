/**
 * @file help_doc_ui.h
 * @brief 帮助文档界面 — 视图层（MVC）
 *
 * 左侧一列单选按钮（帮助文档分类），右侧展示对应的 AC 示例代码，
 * 使用封装好的代码编辑器（语法高亮 + 可复制）。
 * 数据来源统一为 HelpDocModel（help_doc_data.h 常量）。
 *
 * 由 HelpDocMgr 控制器创建，遵循 SettingUi / DemoUi 的设计模式。
 */

#pragma once

#include <QDialog>

class QButtonGroup;
class QRadioButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;

class CodeEditor;
class HelpDocModel;

/**
 * @class HelpDocUi
 * @brief 帮助文档界面（视图层）
 *
 * MVC 中的视图层，负责界面布局与用户交互呈现：
 * - 左侧：分类单选按钮（从 HelpDocModel 获取标题）
 * - 右侧：AC 示例代码（只读 + 语法高亮 + 可复制）
 */
class HelpDocUi : public QDialog {
  Q_OBJECT

public:
  explicit HelpDocUi(QWidget *parent = nullptr);
  ~HelpDocUi() override = default;

  /// 绑定数据模型（由 HelpDocMgr 注入）
  void setModel(HelpDocModel *model);

  /// 初始化界面布局
  void setupUI();

private slots:
  /// 左侧分类切换：刷新右侧代码内容
  void onCategoryChanged(int index);
  /// 主题/自定义颜色变化：重建固化的样式表并刷新高亮
  void refreshStyle();

private:
  /// 构建左侧分类单选按钮列
  void buildCategoryList();
  /// 构建右侧代码展示区
  void buildCodePanel();
  /// 左侧分类滚动区样式表（当前主题色）
  QString categoryScrollStyle() const;
  /// 单选按钮样式表（当前主题色）
  QString radioStyle() const;
  /// 右侧代码编辑器样式表（当前主题色）
  QString codeEditStyle() const;

  HelpDocModel *m_model = nullptr;  ///< 数据模型（由控制器注入）

  QWidget *m_titleBar = nullptr;            ///< 自定义标题栏（nativeEvent 拖拽）
  QScrollArea *m_categoryScroll = nullptr;  ///< 左侧分类滚动区
  QVBoxLayout *m_categoryLayout = nullptr;  ///< 左侧单选按钮布局
  QButtonGroup *m_categoryGroup = nullptr;  ///< 分类单选按钮组
  CodeEditor *m_codeEdit = nullptr;         ///< 右侧 AC 代码编辑器

#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif
};
