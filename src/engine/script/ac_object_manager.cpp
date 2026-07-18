/**
 * @file ac_object_manager.cpp
 * @brief 对象管理器实现 — 引用计数 + 自动析构
 */

#include "ac_object_manager.h"

#include <QUuid>

#include "../ac_language.h"

QJsonObject AcObjectManager::registerInstance(const QJsonObject &instance,
                                              const QString &className) {
  QString objId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  // 初始引用计数为 0，由调用方（setVar/retainIfInstance）负责增加
  m_refCount[objId] = 0;
  m_objects[objId] = instance;
  m_classNames[objId] = className;

  QJsonObject result = instance;
  result[QString::fromLatin1(AcRuntime::kObjId)] = objId;
  return result;
}

void AcObjectManager::retain(const QString &objId) {
  if (objId.isEmpty() || !m_refCount.contains(objId)) return;
  m_refCount[objId]++;
}

bool AcObjectManager::release(const QString &objId) {
  if (objId.isEmpty() || !m_refCount.contains(objId)) return false;

  m_refCount[objId]--;
  if (m_refCount[objId] <= 0) {
    // 引用计数归零，加入待析构列表
    m_pendingDestructs.append({m_objects[objId], m_classNames[objId]});
    m_refCount.remove(objId);
    m_objects.remove(objId);
    m_classNames.remove(objId);
    return true;
  }
  return false;
}

bool AcObjectManager::isManagedInstance(const QJsonValue &val) {
  if (!val.isObject()) return false;
  return val.toObject().contains(QString::fromLatin1(AcRuntime::kObjId));
}

QString AcObjectManager::getObjId(const QJsonValue &val) {
  if (!val.isObject()) return QString();
  return val.toObject().value(QString::fromLatin1(AcRuntime::kObjId)).toString();
}

QVector<AcObjectManager::DestructInfo> AcObjectManager::takePendingDestructs() {
  QVector<DestructInfo> result = std::move(m_pendingDestructs);
  m_pendingDestructs.clear();
  return result;
}

void AcObjectManager::cleanup() {
  // 程序退出时，所有剩余实例直接清理，不调用析构器
  m_refCount.clear();
  m_objects.clear();
  m_classNames.clear();
  m_pendingDestructs.clear();
  m_marked.clear();
}

// ── 标记-清扫（处理循环引用） ──

void AcObjectManager::clearMarks() { m_marked.clear(); }

void AcObjectManager::mark(const QString &objId) { m_marked.insert(objId); }

bool AcObjectManager::isMarked(const QString &objId) const { return m_marked.contains(objId); }

bool AcObjectManager::contains(const QString &objId) const { return m_objects.contains(objId); }

QJsonObject AcObjectManager::getObject(const QString &objId) const {
  return m_objects.value(objId);
}

QVector<AcObjectManager::DestructInfo> AcObjectManager::collectUnmarked() {
  QVector<DestructInfo> result;
  // 遍历所有实例，收集未标记的（循环引用垃圾）
  for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
    if (!m_marked.contains(it.key())) {
      result.append({it.value(), m_classNames.value(it.key())});
    }
  }
  // 从管理器中移除所有未标记实例
  for (const auto &info : result) {
    QString objId = getObjId(QJsonValue(info.instance));
    m_refCount.remove(objId);
    m_objects.remove(objId);
    m_classNames.remove(objId);
  }
  return result;
}