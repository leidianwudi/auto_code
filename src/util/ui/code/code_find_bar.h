/**
 * @file code_find_bar.h
 * @brief 代码查找/替换栏控件
 *
 * 类似 VSCode 的内嵌式查找替换栏，嵌入在 CodeEditor 上方。
 * 功能：查找（上一个/下一个）、替换/全部替换、大小写敏感、正则、全词匹配。
 * 按 Esc 关闭，按 Enter 查找下一个。
 */

#pragma once

#include <QLineEdit>
#include <QTextEdit>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;

/**
 * @class CodeFindBar
 * @brief 查找/替换栏控件（嵌入编辑器上方）
 *
 * 每个 CodeEditor 实例拥有一个 CodeFindBar，
 * 通过 showFindBar()/hideFindBar() 控制显示隐藏。
 * Ctrl+F 呼出，Esc 关闭。
 */
class CodeFindBar : public QWidget {
  Q_OBJECT

public:
  explicit CodeFindBar(QPlainTextEdit *editor, QWidget *parent = nullptr);

  /// 显示查找栏（自动聚焦查找输入框）
  void showFindBar();
  /// 隐藏查找栏（用户主动关闭，清除高亮和状态）
  void hideFindBar();
  /// 查找栏是否"应显示"（用户打开过且未关闭）
  bool isFindBarVisible() const;
  /// 暂停显示（切换标签页时临时隐藏，不改变"应显示"状态）
  void pauseVisible();
  /// 恢复显示（切换回标签页时，如果之前是"应显示"则恢复）
  void resumeVisible();

signals:
  /// 查找栏关闭时通知外部
  void findBarClosed();
  /// 查找栏布局变化（展开/收起替换区域）
  void layoutChanged();

protected:
  /// Esc 关闭查找栏
  void keyPressEvent(QKeyEvent *event) override;

private slots:
  /// 查找文本变化时实时高亮匹配
  void onFindTextChanged();
  /// 查找下一个
  void findNext();
  /// 查找上一个
  void findPrev();
  /// 替换当前选中
  void replaceCurrent();
  /// 全部替换
  void replaceAll();

private:
  /// 初始化界面布局
  void setupUI();
  /// 执行查找（direction: 1=向下, -1=向上）
  void find(int direction);
  /// 更新匹配计数标签
  void updateMatchCount();
  /// 更新编辑器中的查找高亮（写入 CodeEditor::m_findSelections，再触发合并）
  void applyFindHighlight(const QList<QTextEdit::ExtraSelection> &selections);
  /// 获取查找标志
  QTextDocument::FindFlags findFlags() const;
  /// 获取当前查找文本
  QString findText() const;
  /// 获取替换文本
  QString replaceText() const;

  QPlainTextEdit *m_editor = nullptr;

  // 查找区域
  QLineEdit *m_findEdit = nullptr;
  QLabel *m_matchLabel = nullptr;
  QPushButton *m_findPrevBtn = nullptr;
  QPushButton *m_findNextBtn = nullptr;
  QCheckBox *m_caseCheck = nullptr;
  QCheckBox *m_wordCheck = nullptr;
  QCheckBox *m_regexCheck = nullptr;

  // 替换区域
  QLineEdit *m_replaceEdit = nullptr;
  QPushButton *m_replaceBtn = nullptr;
  QPushButton *m_replaceAllBtn = nullptr;
  QPushButton *m_toggleReplaceBtn = nullptr;
  QPushButton *m_closeBtn = nullptr;  ///< 关闭查找栏按钮

  /// 替换区域是否展开
  bool m_replaceVisible = false;

  /// 当前匹配总数（0=无匹配, -1=未计算）
  int m_matchCount = -1;
  /// 当前匹配索引（1-based）
  int m_currentMatchIndex = 0;
  /// 用户意图：查找栏"应显示"（打开过且未主动关闭）
  bool m_intendedVisible = false;
};
