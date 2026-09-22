/**
 * @file combobox_config_dialog.h
 * @brief 下拉框数据源配置对话框
 *
 * 配置 ApiSelect 组件的远程数据源参数。
 *
 * 支持两种方式：
 *   - 从 .jsonsource 数据源文件中选择一条数据源（推荐，可跨界面复用）：
 *     数据源下拉框显示「说明 - URL」。选中后请求 URL / 请求方式 / 加载方式 /
 *     分页参数完全跟随数据源，不可修改；仅提供两种使用方式：
 *       - 全部使用数据源：显示文本/实际值也锁定为数据源配置
 *       - 部分使用数据源：可修改显示文本/实际值字段（可点「测试」从返回示例中选择）
 *   - 手动输入请求 URL（向后兼容，不引用 .jsonsource），所有字段均可编辑。
 *
 * 提供"测试"按钮发送 HTTP 请求，自动提取返回数据的字段名。
 */

#pragma once

#include <QDialog>

#include "json_vue_model.h"
#include "src/ui/json_source/json_source_model.h"

class QLabel;
class QPushButton;
class QRadioButton;
class QTableWidget;
class SelectSourcePanel;
class AuiTreeCombo;

/**
 * @class ComboboxConfigDialog
 * @brief 下拉框数据源配置对话框
 */
class ComboboxConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit ComboboxConfigDialog(QWidget *parent = nullptr);
  ~ComboboxConfigDialog() override = default;

  /// 设置初始配置（URL 与字段）
  void setConfig(const QString &url, const QString &valueField, const QString &labelField);

  /// 设置初始配置（含查询分页）
  void setPagedConfig(bool paged, const QString &pageKey, const QString &pageSizeKey, int pageSize,
                      const QString &searchTitle, const QString &searchField, const QString &method);

  /// 设置引用的 .jsonsource 数据源（文件路径 + 数据源 id，可为空）
  void setSourceRef(const QString &sourceFile, const QString &sourceId);

  /// 设置 .jsonsource 文件搜索根目录（当前编辑文件所在目录，可为空）
  void setSearchRoot(const QString &dir);

  /// 设置 HTTP 请求参数（baseUrl、authHeader、postData）
  void setHttpConfig(const QString &baseUrl, const QString &authHeader, const QString &postData);

  // ── 获取配置结果 ──
  QString url() const;
  QString valueField() const;
  QString labelField() const;
  /// 引用的 .jsonsource 文件路径（空表示未引用）
  QString sourceFile() const;
  /// 引用的数据源 id（空表示未引用）
  QString sourceId() const;
  /// 是否启用查询分页加载
  bool paged() const;
  QString pageKey() const;
  QString pageSizeKey() const;
  int pageSize() const;
  QString searchTitle() const;
  QString searchField() const;
  QString method() const;

private:
  /// 数据源候选（统一 .jsonsource 与全局枚举两种来源）
  struct SourceCandidate {
    QString fileRef;    ///< 写入 selectSourceFile 的引用（.jsonsource=绝对路径；全局枚举=global_enum.jsonsource）
    QString filePath;   ///< 实际读取的文件路径（用于解析内容）
    QString sourceId;   ///< 数据源 id（全局枚举缺省用 name 兜底，与生成侧一致）
    QString display;    ///< 下拉列表中的显示文本
    bool isGlobal = false;  ///< 是否来自全局枚举
    JsonSource source;  ///< 统一为 JsonSource 形态（全局枚举转换而来，恒为静态）
  };

  /// 构建界面
  void setupUI();
  /// 重建数据源树形下拉（.jsonsource 文件分组 + 全局枚举分组）并填充候选
  void rebuildSourceTree();
  /// 应用选中候选的数据源到配置面板（idx<0 为手动模式）
  void applySelectedSource(int candidateIdx);
  /// 按已存储的覆盖值确定使用方式（全部/部分使用数据源）并应用到面板
  void reconcileOverrides();
  /// 按当前模式更新面板锁定状态（引用数据源时基础配置始终锁定）
  void updatePanelLocks();

  // ── 控件 ──
  AuiTreeCombo *m_sourceTree = nullptr;  ///< 数据源树形下拉（按文件分组，含全局枚举）
  QWidget *m_modeWidget = nullptr;    ///< 使用方式行（引用动态数据源时显示）
  QRadioButton *m_fullRadio = nullptr;    ///< 全部使用数据源
  QRadioButton *m_partialRadio = nullptr; ///< 部分使用数据源（可改显示文本/实际值）
  QLabel *m_staticHint = nullptr;     ///< 静态数据源提示
  QTableWidget *m_staticOptions = nullptr;  ///< 静态数据源选项只读预览（静态源/全局枚举选中时显示）
  SelectSourcePanel *m_panel = nullptr;  ///< 动态数据源配置面板

  // ── 状态 ──
  QVector<SourceCandidate> m_candidates;  ///< 全部数据源候选（重建下拉时填充）
  int m_selectedCandidate = -1;           ///< 当前选中的候选索引（-1=手动模式）
  QString m_fallbackFile;                 ///< 候选中未找到引用时的保留引用（供保存）
  QString m_fallbackId;                   ///< 候选中未找到引用时的保留 id（供保存）
  QString m_searchRoot;   ///< jsonsource 文件搜索根目录
  QString m_baseUrl;      ///< baseUrl
  QString m_authHeader;   ///< Authorization 请求头
  QString m_postData;     ///< POST 请求数据
  bool m_loading = false; ///< 加载配置时抑制信号
  JsonSource m_appliedSource;  ///< 当前已应用到面板的动态数据源
  bool m_hasDynamicSource = false; ///< 是否选中了动态数据源（决定锁定策略）
  QString m_storedValue;  ///< 打开对话框时已保存的实际值覆盖（空=未覆盖）
  QString m_storedLabel;  ///< 打开对话框时已保存的显示文本覆盖（空=未覆盖）
};
