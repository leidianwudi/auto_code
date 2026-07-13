/**
 * @file ac_object_manager.cpp
 * @brief 对象管理器实现 — 引用计数 + 自动析构
 */

#include "ac_object_manager.h"

#include <QUuid>

#include "../ac_language.h"

AcObjectManager &AcObjectManager::ins() {
  static AcObjectManager s_inst;
  return s_inst;
}

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
}