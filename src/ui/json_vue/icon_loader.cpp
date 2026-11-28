/**
 * @file icon_loader.cpp
 * @brief 图标异步加载器实现
 *
 * 通过 Iconify HTTP API 批量获取图标 SVG，用 QSvgRenderer 渲染为真实图标。
 * API: https://api.iconify.design/mingcute.json?icons=icon1,icon2,...
 */

#include "icon_loader.h"

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QSvgRenderer>
#include <QTimer>

#include "src/util/ui/component/aui_style.h"

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

IconLoader::IconLoader() : QObject(nullptr) {
  m_networkManager = new QNetworkAccessManager(this);
  // 使用系统代理（浏览器能访问的，Qt 也能访问）
  QNetworkProxyFactory::setUseSystemConfiguration(true);
}

IconLoader::~IconLoader() = default;

IconLoader &IconLoader::instance() {
  static IconLoader s_instance;
  return s_instance;
}

// ════════════════════════════════════════════════════════════
//  缓存查询
// ════════════════════════════════════════════════════════════

QIcon IconLoader::cached(const QString &iconName) const {
  if (m_cache.contains(iconName)) return m_cache.value(iconName);
  return m_cache.value(normalizeIconName(iconName));
}

QIcon IconLoader::getOrCreateIcon(const QString &iconName, int size) {
  if (m_cache.contains(iconName)) return m_cache.value(iconName);
  // 本地图标集（与 shop IconSelect 同源）优先，离线可用
  QIcon local = localIcon(iconName);
  if (!local.isNull()) return local;
  // 生成占位图标（不缓存，后续网络成功后替换）
  return QIcon(generatePlaceholder(iconName, size));
}

// ════════════════════════════════════════════════════════════
//  异步加载
// ════════════════════════════════════════════════════════════

void IconLoader::requestIcon(const QString &iconName,
                             std::function<void(const QString &)> callback) {
  // 已缓存真实图标：立即回调
  if (m_cache.contains(iconName)) {
    if (callback) callback(iconName);
    return;
  }
  // 本地图标集：同步渲染，立即回调，不走网络
  QIcon local = localIcon(iconName);
  if (!local.isNull()) {
    if (callback) callback(iconName);
    return;
  }

  // 立即回调占位图标（不缓存），后台加载真实图标
  if (callback) callback(iconName);

  if (!m_loading.contains(iconName)) {
    if (callback) m_pending[iconName].append(callback);
    sendBatchRequest({iconName});
  } else {
    if (callback) m_pending[iconName].append(callback);
  }
}

void IconLoader::requestIcons(const QStringList &iconNames,
                              std::function<void(const QString &)> callback) {
  QStringList toLoad;
  for (const QString &name : iconNames) {
    if (m_cache.contains(name)) {
      // 已缓存真实图标：立即回调
      if (callback) callback(name);
      continue;
    }
    // 本地图标集：同步渲染，立即回调，不进网络队列
    if (!localIcon(name).isNull()) {
      if (callback) callback(name);
      continue;
    }
    // 未缓存：加入待加载列表，注册回调等待网络加载完成
    if (callback) m_pending[name].append(callback);
    if (!m_loading.contains(name)) {
      toLoad.append(name);
    }
  }

  if (!toLoad.isEmpty()) {
    sendBatchRequest(toLoad);
  }
}

// ════════════════════════════════════════════════════════════
//  批量请求
// ════════════════════════════════════════════════════════════

void IconLoader::sendBatchRequest(const QStringList &iconNames) {
  for (const QString &fullIcon : iconNames) {
    QString prefix = extractPrefix(fullIcon);
    QString name = extractShortName(fullIcon);
    if (prefix.isEmpty() || name.isEmpty()) continue;

    m_loading.insert(fullIcon);

    QStringList urls = {
        QStringLiteral("https://api.iconify.design/%1/%2.svg").arg(prefix, name),
        QStringLiteral("https://api.simplesvg.com/%1/%2.svg").arg(prefix, name),
    };

    tryUrl(0, fullIcon, prefix, name, urls);
  }
}

void IconLoader::tryUrl(int urlIndex, const QString &fullIcon, const QString &prefix,
                        const QString &name, const QStringList &urls) {
  if (urlIndex >= urls.size()) {
    m_loading.remove(fullIcon);
    return;
  }

  QNetworkRequest request{QUrl(urls[urlIndex])};
  request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("auto_code/1.0"));
  request.setTransferTimeout(8000);

  QNetworkReply *reply = m_networkManager->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, fullIcon, urls, urlIndex]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      tryUrl(urlIndex + 1, fullIcon, extractPrefix(fullIcon), extractShortName(fullIcon), urls);
      return;
    }
    QByteArray svgContent = reply->readAll();
    if (svgContent.isEmpty()) {
      tryUrl(urlIndex + 1, fullIcon, extractPrefix(fullIcon), extractShortName(fullIcon), urls);
      return;
    }
    QPixmap pixmap = renderSvg(QString::fromUtf8(svgContent), kDefaultIconSize);
    if (pixmap.isNull()) {
      tryUrl(urlIndex + 1, fullIcon, extractPrefix(fullIcon), extractShortName(fullIcon), urls);
      return;
    }
    m_loading.remove(fullIcon);
    m_cache.insert(fullIcon, QIcon(pixmap));
    auto callbacks = m_pending.take(fullIcon);
    for (const auto &cb : callbacks) cb(fullIcon);
  });
}

// ════════════════════════════════════════════════════════════
//  SVG 渲染
// ════════════════════════════════════════════════════════════

QPixmap IconLoader::renderSvg(const QString &svgContent, int size) {
  // 图标 body 用 fill="currentColor"：替换为当前主题文字色，深色主题下也能看见
  QString svg = svgContent;
  if (svg.contains(QStringLiteral("currentColor"))) {
    svg.replace(QStringLiteral("currentColor"),
                AuiStyle::textColor().name(QColor::HexRgb));
  }
  QSvgRenderer renderer(svg.toUtf8());
  if (!renderer.isValid()) return QPixmap();

  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  renderer.render(&painter);
  return pixmap;
}

// ════════════════════════════════════════════════════════════
//  本地占位图标生成
// ════════════════════════════════════════════════════════════

QPixmap IconLoader::generatePlaceholder(const QString &iconName, int size) {
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // 根据图标名生成一致的彩色背景
  uint hash = qHash(iconName);
  int hue = hash % 360;
  QColor bgColor = QColor::fromHsl(hue, 180, 200);
  QColor textColor = QColor::fromHsl(hue, 200, 80);

  // 圆角矩形背景
  painter.setPen(Qt::NoPen);
  painter.setBrush(bgColor);
  painter.drawRoundedRect(1, 1, size - 2, size - 2, 4, 4);

  // 图标名首字母（去掉前缀后）
  QString shortName = extractShortName(iconName);
  QString firstChar = shortName.isEmpty() ? QStringLiteral("?") : shortName.left(1);

  painter.setPen(textColor);
  QFont font;
  font.setPixelSize(size * 0.65);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, firstChar);

  return pixmap;
}

// ════════════════════════════════════════════════════════════
//  本地图标集（res/icons/iconify_sets.json；与 shop IconSelect 同源）
// ════════════════════════════════════════════════════════════

void IconLoader::ensureLocalLoaded() {
  if (m_localLoaded) return;
  m_localLoaded = true;

  QFile f(QStringLiteral(":/icons/iconify_sets.json"));
  if (!f.open(QIODevice::ReadOnly)) return;
  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject()) return;

  const QJsonArray sets = doc.object().value(QStringLiteral("sets")).toArray();
  for (const QJsonValue &sv : sets) {
    const QJsonObject s = sv.toObject();
    const QString prefix = s.value(QStringLiteral("prefix")).toString();
    if (prefix.isEmpty()) continue;
    const int defW = s.value(QStringLiteral("width")).toInt(16);
    const int defH = s.value(QStringLiteral("height")).toInt(16);
    const int defL = s.value(QStringLiteral("left")).toInt(0);
    const int defT = s.value(QStringLiteral("top")).toInt(0);

    const QJsonObject icons = s.value(QStringLiteral("icons")).toObject();
    QStringList names;
    names.reserve(icons.size());
    for (auto it = icons.constBegin(); it != icons.constEnd(); ++it) {
      const QJsonObject o = it.value().toObject();
      LocalIconData d;
      d.body = o.value(QStringLiteral("body")).toString();
      d.width = o.value(QStringLiteral("width")).toInt(defW);
      d.height = o.value(QStringLiteral("height")).toInt(defH);
      d.left = o.value(QStringLiteral("left")).toInt(defL);
      d.top = o.value(QStringLiteral("top")).toInt(defT);
      const QString key = prefix + QLatin1Char(':') + it.key();
      m_localIcons.insert(key, d);
      names.append(key);
    }
    names.sort();
    m_localNames.insert(prefix, names);
    m_localPrefixes.append(prefix);
  }
}

QString IconLoader::normalizeIconName(const QString &iconName) {
  // 兼容旧配置的 vi- 前缀（old element-plus-admin 写法）
  return iconName.startsWith(QStringLiteral("vi-")) ? iconName.mid(3) : iconName;
}

QIcon IconLoader::localIcon(const QString &iconName) {
  const QString key = normalizeIconName(iconName);
  const auto cachedIt = m_cache.constFind(key);
  if (cachedIt != m_cache.constEnd()) return cachedIt.value();

  ensureLocalLoaded();
  const auto it = m_localIcons.constFind(key);
  if (it == m_localIcons.constEnd()) return QIcon();

  const LocalIconData &d = it.value();
  const QString svg =
      QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%1 %2 %3 %4\" "
                     "width=\"%3\" height=\"%4\">%5</svg>")
          .arg(d.left)
          .arg(d.top)
          .arg(d.width)
          .arg(d.height)
          .arg(d.body);
  const QPixmap pm = renderSvg(svg, kDefaultIconSize);
  if (pm.isNull()) return QIcon();

  const QIcon icon(pm);
  m_cache.insert(key, icon);
  if (key != iconName) m_cache.insert(iconName, icon);
  return icon;
}

QStringList IconLoader::localIconNames(const QString &prefix) {
  ensureLocalLoaded();
  return m_localNames.value(prefix);
}

QStringList IconLoader::localPrefixes() {
  ensureLocalLoaded();
  return m_localPrefixes;
}

// ════════════════════════════════════════════════════════════
//  工具函数
// ════════════════════════════════════════════════════════════

QString IconLoader::extractPrefix(const QString &fullIcon) {
  // "vi-ep:home-filled" → "ep"
  int viIdx = fullIcon.indexOf(QStringLiteral("vi-"));
  int colonIdx = fullIcon.indexOf(':');
  if (viIdx == 0 && colonIdx > 3) {
    return fullIcon.mid(3, colonIdx - 3);
  }
  if (colonIdx > 0) return fullIcon.left(colonIdx);
  return QString();
}

QString IconLoader::extractShortName(const QString &fullIcon) {
  // "vi-ep:home-filled" → "home-filled"
  int colonIdx = fullIcon.indexOf(':');
  if (colonIdx < 0) return fullIcon;
  return fullIcon.mid(colonIdx + 1);
}
