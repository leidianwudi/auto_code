/**
 * @file ac_module_io.cpp
 * @brief AcModule 二进制序列化实现
 *
 * 格式：QDataStream（Qt 6）+ 魔数头；类定义只写运行时元数据子集。
 */

#include "ac_module_io.h"

#include <QDataStream>
#include <QIODevice>
#include <QJsonValue>

#include "src/core/json/ac_json_value.h"

namespace {
const char kMagic[4] = {'A', 'B', 'C', '1'};
const int kIoVersion = 1;  ///< 序列化格式版本（与 AcModule::version 解耦）
}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  通用读写
// ─────────────────────────────────────────────────────────────────────────────

static void writeStringList(QDataStream &ds, const QVector<QString> &list) {
  ds << qint32(list.size());
  for (const auto &s : list) ds << s;
}

static bool readStringList(QDataStream &ds, QVector<QString> &list) {
  qint32 n = 0;
  ds >> n;
  if (ds.status() != QDataStream::Ok || n < 0 || n > 1 << 20) return false;
  list.clear();
  list.reserve(n);
  for (qint32 i = 0; i < n; ++i) {
    QString s;
    ds >> s;
    if (ds.status() != QDataStream::Ok) return false;
    list.append(s);
  }
  return true;
}

static void writeValues(QDataStream &ds, const QVector<accore::AcJsonValue> &vals) {
  ds << qint32(vals.size());
  for (const auto &v : vals) ds << v.toQJsonValue();
}

static bool readValues(QDataStream &ds, QVector<accore::AcJsonValue> &vals) {
  qint32 n = 0;
  ds >> n;
  if (ds.status() != QDataStream::Ok || n < 0 || n > 1 << 20) return false;
  vals.clear();
  vals.reserve(n);
  for (qint32 i = 0; i < n; ++i) {
    QJsonValue jv;
    ds >> jv;
    if (ds.status() != QDataStream::Ok) return false;
    vals.append(accore::AcJsonValue::fromQJsonValue(jv));
  }
  return true;
}

static void writeBoolVec(QDataStream &ds, const QVector<bool> &flags) {
  ds << qint32(flags.size());
  for (bool f : flags) ds << qint8(f ? 1 : 0);
}

static bool readBoolVec(QDataStream &ds, QVector<bool> &flags) {
  qint32 n = 0;
  ds >> n;
  if (ds.status() != QDataStream::Ok || n < 0 || n > 1 << 20) return false;
  flags.clear();
  flags.reserve(n);
  for (qint32 i = 0; i < n; ++i) {
    qint8 b = 0;
    ds >> b;
    if (ds.status() != QDataStream::Ok) return false;
    flags.append(b != 0);
  }
  return true;
}

/// 类定义精简元数据（ClassDef 的 AST 字段不入档）
struct ClassMeta {
  QString name;
  QString baseClass;
  bool isNative = false;
  QVector<QString> propNames;   ///< 属性名（仅供调试/展示，运行时以 isStatic 顺序为准）
  QVector<bool> propStatic;     ///< 属性 isStatic 标志序（决定静态属性对数）
  QVector<QString> methodNames; ///< 方法名（编译生成的 "类.方法" 单元仍以函数名查找）
  QVector<bool> methodStatic;   ///< 方法 isStatic 标志
};

static void writeClassMeta(QDataStream &ds, const ClassMeta &m) {
  ds << m.name << m.baseClass << qint8(m.isNative ? 1 : 0);
  ds << qint32(m.propNames.size());
  for (const auto &n : m.propNames) ds << n;
  writeBoolVec(ds, m.propStatic);
  ds << qint32(m.methodNames.size());
  for (const auto &n : m.methodNames) ds << n;
  writeBoolVec(ds, m.methodStatic);
}

static bool readClassMeta(QDataStream &ds, ClassMeta &m) {
  qint8 native = 0;
  ds >> m.name >> m.baseClass >> native;
  m.isNative = native != 0;
  qint32 nProps = 0;
  ds >> nProps;
  if (ds.status() != QDataStream::Ok || nProps < 0 || nProps > 1 << 20) return false;
  m.propNames.clear();
  for (qint32 i = 0; i < nProps; ++i) {
    QString n;
    ds >> n;
    if (ds.status() != QDataStream::Ok) return false;
    m.propNames.append(n);
  }
  if (!readBoolVec(ds, m.propStatic)) return false;
  qint32 nMethods = 0;
  ds >> nMethods;
  if (ds.status() != QDataStream::Ok || nMethods < 0 || nMethods > 1 << 20) return false;
  m.methodNames.clear();
  for (qint32 i = 0; i < nMethods; ++i) {
    QString n;
    ds >> n;
    if (ds.status() != QDataStream::Ok) return false;
    m.methodNames.append(n);
  }
  if (!readBoolVec(ds, m.methodStatic)) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  序列化 / 反序列化
// ─────────────────────────────────────────────────────────────────────────────

bool AcModuleIo::save(const AcModule &module, QByteArray &out) {
  QDataStream ds(&out, QIODevice::WriteOnly);
  ds.setVersion(QDataStream::Qt_6_0);
  ds.writeRawData(kMagic, sizeof(kMagic));
  ds << qint32(kIoVersion) << module.sourceHash << qint32(module.entry);

  // ident 表
  writeStringList(ds, module.idents);

  // 类精简元数据
  ds << qint32(module.classes.size());
  for (auto it = module.classes.constBegin(); it != module.classes.constEnd(); ++it) {
    const ClassDef &cd = it.value();
    ClassMeta meta;
    meta.name = cd.name;
    meta.baseClass = cd.baseClass;
    meta.isNative = cd.isNative;
    for (const auto &p : cd.properties) {
      meta.propNames.append(p.key);
      meta.propStatic.append(p.isStatic);
    }
    for (const auto &m : cd.methods) {
      meta.methodNames.append(m.name);
      meta.methodStatic.append(m.isStatic);
    }
    writeClassMeta(ds, meta);
  }

  // 函数单元
  ds << qint32(module.funcs.size());
  for (const auto &u : module.funcs) {
    ds << u.name << qint32(u.identId) << qint8(u.isMethod ? 1 : 0) << qint32(u.numLocals);
    writeStringList(ds, u.paramNames);
    writeValues(ds, u.paramDefaults);
    writeValues(ds, u.constants);
    // 指令
    ds << qint32(u.code.size());
    for (const auto &ins : u.code) {
      ds << qint32(ins.op) << ins.a << ins.b << ins.c << ins.line;
    }
    // try 表
    ds << qint32(u.tryTable.size());
    for (const auto &t : u.tryTable) {
      ds << t.tryStart << t.tryEnd << t.catchAddr << t.finallyAddr << t.catchVarSlot << t.catchVar;
    }
  }

  // funcUnits 索引
  ds << qint32(module.funcUnits.size());
  for (auto it = module.funcUnits.constBegin(); it != module.funcUnits.constEnd(); ++it) {
    ds << it.key() << it.value();
  }

  return ds.status() == QDataStream::Ok;
}

bool AcModuleIo::load(const QByteArray &in, AcModule &module) {
  QDataStream ds(in);
  ds.setVersion(QDataStream::Qt_6_0);
  char magic[sizeof(kMagic)] = {0};
  if (ds.readRawData(magic, sizeof(kMagic)) != int(sizeof(kMagic))) return false;
  if (memcmp(magic, kMagic, sizeof(kMagic)) != 0) return false;

  qint32 ioVer = 0;
  ds >> ioVer;
  if (ioVer != kIoVersion) return false;
  ds >> module.sourceHash >> module.entry;
  if (ds.status() != QDataStream::Ok) return false;

  // ident 表
  if (!readStringList(ds, module.idents)) return false;

  // 类
  qint32 nClasses = 0;
  ds >> nClasses;
  if (ds.status() != QDataStream::Ok || nClasses < 0 || nClasses > 1 << 16) return false;
  for (qint32 i = 0; i < nClasses; ++i) {
    ClassMeta meta;
    if (!readClassMeta(ds, meta)) return false;
    ClassDef cd;
    cd.name = meta.name;
    cd.baseClass = meta.baseClass;
    cd.isNative = meta.isNative;
    for (int p = 0; p < meta.propNames.size() && p < meta.propStatic.size(); ++p) {
      ObjectEntry oe;  // 仅保留运行时可用的名称与静态标志（初始值在初始化单元中）
      oe.key = meta.propNames[p];
      oe.isStatic = meta.propStatic[p];
      cd.properties.append(std::move(oe));
    }
    for (int m = 0; m < meta.methodNames.size() && m < meta.methodStatic.size(); ++m) {
      MethodDef md;
      md.name = meta.methodNames[m];
      md.isStatic = meta.methodStatic[m];  // callStaticMethod/callMethod 按该标志分流
      md.isDeclaration = true;             // 占位：方法体不入档，调用走 funcUnits 单元
      cd.methods.append(std::move(md));
    }
    module.classes.insert(cd.name, std::move(cd));
  }

  // 函数单元
  qint32 nFuncs = 0;
  ds >> nFuncs;
  if (ds.status() != QDataStream::Ok || nFuncs < 0 || nFuncs > 1 << 20) return false;
  module.funcs.clear();
  module.funcs.reserve(nFuncs);
  for (qint32 i = 0; i < nFuncs; ++i) {
    AcFuncUnit u;
    qint32 identId = 0;
    qint8 isMethod = 0;
    qint32 numLocals = 0;
    ds >> u.name >> identId >> isMethod >> numLocals;
    u.identId = identId;
    u.isMethod = isMethod != 0;
    u.numLocals = numLocals;
    if (ds.status() != QDataStream::Ok) return false;
    if (!readStringList(ds, u.paramNames)) return false;
    if (!readValues(ds, u.paramDefaults)) return false;
    if (!readValues(ds, u.constants)) return false;
    qint32 nCode = 0;
    ds >> nCode;
    if (ds.status() != QDataStream::Ok || nCode < 0 || nCode > 1 << 24) return false;
    u.code.clear();
    u.code.reserve(nCode);
    for (qint32 k = 0; k < nCode; ++k) {
      qint32 op = 0;
      AcInstr ins;
      ds >> op >> ins.a >> ins.b >> ins.c >> ins.line;
      ins.op = AcOpcode(op);
      if (ds.status() != QDataStream::Ok) return false;
      u.code.append(ins);
    }
    qint32 nTry = 0;
    ds >> nTry;
    if (ds.status() != QDataStream::Ok || nTry < 0 || nTry > 1 << 20) return false;
    for (qint32 k = 0; k < nTry; ++k) {
      AcTryEntry t;
      ds >> t.tryStart >> t.tryEnd >> t.catchAddr >> t.finallyAddr >> t.catchVarSlot >> t.catchVar;
      if (ds.status() != QDataStream::Ok) return false;
      u.tryTable.append(t);
    }
    module.funcs.append(std::move(u));
  }

  // funcUnits
  qint32 nUnits = 0;
  ds >> nUnits;
  if (ds.status() != QDataStream::Ok || nUnits < 0 || nUnits > 1 << 20) return false;
  for (qint32 i = 0; i < nUnits; ++i) {
    QString key;
    int idx = 0;
    ds >> key >> idx;
    if (ds.status() != QDataStream::Ok) return false;
    module.funcUnits.insert(key, idx);
  }

  return ds.status() == QDataStream::Ok;
}