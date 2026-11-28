/**
 * @file icon_loader.h
 * @brief 图标异步加载器（单例）
 *
 * 通过 Iconify HTTP API 加载图标 SVG 并渲染为 QIcon。
 * 全局共享缓存，多个对话框复用同一份图标资源。
 * 图标格式：vi-{prefix}:{name}，如 vi-ep:home-filled
 *
 * 支持的图标库：
 *   - Element Plus (ep)
 *   - Ant Design (ant-design)
 *   - TDesign (tdesign)
 *   - Mingcute (mingcute)
 */

#pragma once

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <functional>

class QNetworkAccessManager;

class IconLoader : public QObject {
  Q_OBJECT

public:
  static IconLoader &instance();

  QIcon cached(const QString &iconName) const;
  QIcon getOrCreateIcon(const QString &iconName, int size = 20);

  /// 本地图标集（与 shop IconSelect 一致）：prefix → 图标名列表（prefix:name）
  QStringList localIconNames(const QString &prefix);
  /// 本地图标集前缀顺序（ep/ri/ant-design/mdi）
  QStringList localPrefixes();

  void requestIcon(const QString &iconName, std::function<void(const QString &)> callback);
  void requestIcons(const QStringList &iconNames,
                    std::function<void(const QString &)> callback);

  static QPixmap renderSvg(const QString &svgContent, int size);
  static QPixmap generatePlaceholder(const QString &iconName, int size);

  /// 从完整图标名提取前缀，"vi-ep:home-filled" → "ep"
  static QString extractPrefix(const QString &fullIcon);
  /// 从完整图标名提取图标名，"vi-ep:home-filled" → "home-filled"
  static QString extractShortName(const QString &fullIcon);

private:
  IconLoader();
  ~IconLoader() override;
  IconLoader(const IconLoader &) = delete;
  IconLoader &operator=(const IconLoader &) = delete;

  /// 本地图标（body + 坐标）数据，来自 res/icons/iconify_sets.json
  struct LocalIconData {
    QString body;
    int left = 0;
    int top = 0;
    int width = 16;
    int height = 16;
  };
  void ensureLocalLoaded();
  QIcon localIcon(const QString &iconName);
  /// "vi-ep:user" → "ep:user"（本地数据键统一无 vi- 前缀）
  static QString normalizeIconName(const QString &iconName);

  void sendBatchRequest(const QStringList &iconNames);
  void tryUrl(int urlIndex, const QString &fullIcon, const QString &prefix,
              const QString &name, const QStringList &urls);

  QNetworkAccessManager *m_networkManager;
  QHash<QString, QIcon> m_cache;
  QSet<QString> m_loading;
  QHash<QString, QVector<std::function<void(const QString &)>>> m_pending;

  bool m_localLoaded = false;
  QHash<QString, LocalIconData> m_localIcons;   ///< "ep:user" → 数据
  QHash<QString, QStringList> m_localNames;     ///< "ep" → ["ep:add-location", ...]
  QStringList m_localPrefixes;                  ///< ep/ri/ant-design/mdi

  static constexpr int kDefaultIconSize = 32;
};
