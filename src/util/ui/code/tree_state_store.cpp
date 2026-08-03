/**
 * @file tree_state_store.cpp
 * @brief 目录树状态持久化数据层实现
 */

#include "tree_state_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "src/util/common/util_json.h"

namespace {
// 状态键常量，统一 save/load 使用
const char *kChecked = "checked";
const char *kStartup = "startup";
const char *kStartupSelected = "startupSelected";
const char *kVisualToggle = "visualToggle";
const char *kExpanded = "expanded";

QJsonArray toJsonArray(const QStringList &list) {
  QJsonArray arr;
  for (const QString &s : list) arr.append(s);
  return arr;
}

QStringList toStringList(const QJsonArray &arr) {
  QStringList list;
  for (const QJsonValue &v : arr)
    if (v.isString()) list.append(v.toString());
  return list;
}
}  // namespace

bool TreeStateStore::load(const QString &configPath) {
  if (configPath.isEmpty()) return false;
  QJsonDocument doc = UtilJson::loadFile(configPath);
  if (doc.isNull() || !doc.isObject()) return false;

  const QJsonObject obj = doc.object();
  checkedRelPaths = toStringList(obj[kChecked].toArray());
  startupRelPaths = toStringList(obj[kStartup].toArray());
  selectedStartupRel = obj[kStartupSelected].toString();
  visualToggle = obj[kVisualToggle].toBool(false);
  expandedRelPaths = toStringList(obj[kExpanded].toArray());
  return true;
}

void TreeStateStore::save(const QString &configPath) const {
  if (configPath.isEmpty()) return;

  QFileInfo fi(configPath);
  QDir().mkpath(fi.absolutePath());

  QJsonObject root;
  root[kChecked] = toJsonArray(checkedRelPaths);
  root[kStartup] = toJsonArray(startupRelPaths);
  if (!selectedStartupRel.isEmpty()) root[kStartupSelected] = selectedStartupRel;
  root[kVisualToggle] = visualToggle;
  root[kExpanded] = toJsonArray(expandedRelPaths);

  QFile file(configPath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}