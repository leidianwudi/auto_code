/**
 * @file ac_symbol_table.cpp
 * @brief AC 脚本符号表实现
 */

#include "ac_symbol_table.h"

#include <QDebug>

// ──────────────────────────────────────────────────────────────
//  buildFromAst / buildFromBlock / collectStmt
// ──────────────────────────────────────────────────────────────

void AcSymbolTable::buildFromAst(const QString &filePath, const Block::Stmt &stmt) {
  m_filePath = filePath;
  collectFromStmt(stmt);
}

void AcSymbolTable::buildFromBlock(const QString &filePath, const Block &block) {
  m_filePath = filePath;
  for (const auto &stmt : block.stmts) {
    collectFromStmt(stmt);
  }
}

void AcSymbolTable::collectStmt(const Block::Stmt &stmt) { collectFromStmt(stmt); }

// ──────────────────────────────────────────────────────────────
//  collectFromStmt — 递归遍历 AST 收集符号
// ──────────────────────────────────────────────────────────────

void AcSymbolTable::collectFromStmt(const Block::Stmt &stmt) {
  switch (stmt.kind) {
    case Block::Stmt::kFuncDef: {
      const auto &fd = stmt.funcDef;
      AcSymbolEntry entry;
      entry.name = fd.name;
      entry.kind = AcSymbolKind::kFunction;
      entry.filePath = m_filePath;
      entry.line = stmt.line;
      entry.returnType = acTypeToString(fd.returnType);
      entry.params = fd.params;
      entry.signature = makeFunctionSignature(fd.name, fd.params, fd.returnType);
      entry.isExported = fd.isExported;
      m_symbols.insert(fd.name, entry);

      // 收集函数参数到符号表（支持参数跳转和悬停）
      for (const auto &param : fd.params) {
        if (!param.name.isEmpty()) {
          AcSymbolEntry paramEntry;
          paramEntry.name = param.name;
          paramEntry.kind = AcSymbolKind::kParameter;
          paramEntry.filePath = m_filePath;
          paramEntry.line = stmt.line;
          paramEntry.parentClass = fd.name;
          paramEntry.returnType = acTypeToString(param.type);
          paramEntry.signature = param.name + QStringLiteral(": ") + acTypeToString(param.type);
          m_symbols.insert(param.name, paramEntry);
        }
      }

      // 递归收集函数体内的符号
      buildFromBlock(m_filePath, fd.body);
      break;
    }

    case Block::Stmt::kInterfaceDef: {
      const auto &iface = stmt.interfaceDef;
      AcSymbolEntry entry;
      entry.name = iface.name;
      entry.kind = AcSymbolKind::kClass;
      entry.filePath = m_filePath;
      entry.line = stmt.line;
      entry.signature = QStringLiteral("interface ") + iface.name;
      if (!iface.baseInterfaces.isEmpty()) {
        entry.signature +=
            QStringLiteral(" extends ") + iface.baseInterfaces.join(QStringLiteral(", "));
      }
      m_symbols.insert(iface.name, entry);

      // 收集接口方法
      for (const auto &method : iface.methods) {
        AcSymbolEntry methodEntry;
        methodEntry.name = method.name;
        methodEntry.kind = AcSymbolKind::kMethod;
        methodEntry.filePath = m_filePath;
        methodEntry.line = stmt.line;
        methodEntry.parentClass = iface.name;
        methodEntry.returnType = acTypeToString(method.returnType);
        methodEntry.params = method.params;
        methodEntry.signature =
            makeMethodSignature(iface.name, method.name, method.params, method.returnType, false);
        m_symbols.insert(iface.name + QStringLiteral(".") + method.name, methodEntry);
      }
      break;
    }

    case Block::Stmt::kClassDef: {
      const auto &cd = stmt.classDef;
      AcSymbolEntry entry;
      entry.name = cd.name;
      entry.kind = AcSymbolKind::kClass;
      entry.filePath = m_filePath;
      entry.line = stmt.line;
      entry.comment =
          cd.baseClass.isEmpty() ? QString() : QStringLiteral("extends %1").arg(cd.baseClass);
      entry.signature =
          cd.name +
          (cd.baseClass.isEmpty() ? QString() : QStringLiteral(" extends %1").arg(cd.baseClass));
      m_symbols.insert(cd.name, entry);

      // 收集类属性
      for (const auto &prop : cd.properties) {
        AcSymbolEntry propEntry;
        propEntry.name = prop.key;
        propEntry.kind = AcSymbolKind::kProperty;
        propEntry.filePath = m_filePath;
        propEntry.parentClass = cd.name;
        propEntry.returnType = acTypeToString(prop.type);
        propEntry.signature = prop.key + QStringLiteral(": ") + acTypeToString(prop.type);
        m_symbols.insert(cd.name + QStringLiteral(".") + prop.key, propEntry);
      }

      // 收集类方法
      for (const auto &method : cd.methods) {
        AcSymbolEntry methodEntry;
        methodEntry.name = method.name;
        methodEntry.kind = AcSymbolKind::kMethod;
        methodEntry.filePath = m_filePath;
        methodEntry.line = stmt.line;
        methodEntry.parentClass = cd.name;
        methodEntry.returnType = acTypeToString(method.returnType);
        methodEntry.params = method.params;
        methodEntry.isStatic = method.isStatic;
        methodEntry.access = method.access;
        methodEntry.signature = makeMethodSignature(cd.name, method.name, method.params,
                                                    method.returnType, method.isStatic);
        m_symbols.insert(cd.name + QStringLiteral(".") + method.name, methodEntry);

        // 收集方法参数
        for (const auto &param : method.params) {
          if (!param.name.isEmpty()) {
            AcSymbolEntry paramEntry;
            paramEntry.name = param.name;
            paramEntry.kind = AcSymbolKind::kParameter;
            paramEntry.filePath = m_filePath;
            paramEntry.line = stmt.line;
            paramEntry.parentClass = cd.name + QStringLiteral(".") + method.name;
            paramEntry.returnType = acTypeToString(param.type);
            paramEntry.signature = param.name + QStringLiteral(": ") + acTypeToString(param.type);
            m_symbols.insert(param.name, paramEntry);
          }
        }

        // 递归收集方法体内的符号
        buildFromBlock(m_filePath, method.body);
      }
      break;
    }

    case Block::Stmt::kAssign: {
      const auto &as = stmt.assign;
      if (!as.name.isEmpty()) {
        AcSymbolEntry entry;
        entry.name = as.name;
        entry.kind = AcSymbolKind::kVariable;
        entry.filePath = m_filePath;
        entry.line = as.line > 0 ? as.line : stmt.line;
        if (as.hasTypeAnnotation) {
          entry.returnType = acTypeToString(as.typeAnnotation);
          entry.signature = as.name + QStringLiteral(": ") + acTypeToString(as.typeAnnotation);
        } else {
          entry.signature = as.name;
        }
        m_symbols.insert(as.name, entry);
      }
      break;
    }

    case Block::Stmt::kFor: {
      // for (var in arrayExpr) { body }
      const auto &fs = stmt.forStmt;
      if (!fs.varName.isEmpty()) {
        AcSymbolEntry entry;
        entry.name = fs.varName;
        entry.kind = AcSymbolKind::kVariable;
        entry.filePath = m_filePath;
        entry.line = fs.line > 0 ? fs.line : stmt.line;
        entry.signature = fs.varName + QStringLiteral(": iterator");
        m_symbols.insert(fs.varName, entry);
      }
      buildFromBlock(m_filePath, fs.body);
      break;
    }

    case Block::Stmt::kIf: {
      buildFromBlock(m_filePath, stmt.ifStmt.thenBlock);
      if (stmt.ifStmt.hasElse && stmt.ifStmt.elseBlock.stmts.size() > 0) {
        buildFromBlock(m_filePath, stmt.ifStmt.elseBlock);
      }
      // else if 链
      if (stmt.ifStmt.elseIfBranch) {
        collectFromIfBranch(stmt.ifStmt.elseIfBranch.get());
      }
      break;
    }

    case Block::Stmt::kWhile: {
      buildFromBlock(m_filePath, stmt.whileStmt.body);
      break;
    }

    case Block::Stmt::kUsing: {
      const auto &us = stmt.usingStmt;
      if (!us.varName.isEmpty()) {
        AcSymbolEntry entry;
        entry.name = us.varName;
        entry.kind = AcSymbolKind::kVariable;
        entry.filePath = m_filePath;
        entry.line = stmt.line;
        entry.signature = us.varName + QStringLiteral(": using");
        m_symbols.insert(us.varName, entry);
      }
      break;
    }

    default:
      break;
  }
}

void AcSymbolTable::collectFromIfBranch(const IfStmt *ifStmt) {
  if (!ifStmt) return;
  buildFromBlock(m_filePath, ifStmt->thenBlock);
  if (ifStmt->hasElse && ifStmt->elseBlock.stmts.size() > 0) {
    buildFromBlock(m_filePath, ifStmt->elseBlock);
  }
  if (ifStmt->elseIfBranch) {
    collectFromIfBranch(ifStmt->elseIfBranch.get());
  }
}

// ──────────────────────────────────────────────────────────────
//  AcType 转字符串
// ──────────────────────────────────────────────────────────────

QString AcSymbolTable::acTypeToString(const AcType &type) const {
  switch (type.kind) {
    case AcType::kNumber:
      return QStringLiteral("Number");
    case AcType::kString:
      return QStringLiteral("String");
    case AcType::kBool:
      return QStringLiteral("Boolean");
    case AcType::kAny:
      return QStringLiteral("Any");
    case AcType::kVoid:
      return QStringLiteral("void");
    case AcType::kNull:
      return QStringLiteral("null");
    case AcType::kArray:
      if (type.elementType)
        return QStringLiteral("Array<") + acTypeToString(*type.elementType) + QStringLiteral(">");
      return QStringLiteral("Array");
    case AcType::kClass:
      return type.className;
    case AcType::kInterface:
      return type.interfaceName;
    default:
      return QStringLiteral("Any");
  }
}

// ──────────────────────────────────────────────────────────────
//  查找符号
// ──────────────────────────────────────────────────────────────

AcSymbolEntry *AcSymbolTable::findSymbol(const QString &name) {
  auto it = m_symbols.find(name);
  if (it != m_symbols.end()) return &it.value();
  return nullptr;
}

const AcSymbolEntry *AcSymbolTable::findSymbol(const QString &name) const {
  auto it = m_symbols.find(name);
  if (it != m_symbols.end()) return &it.value();
  return nullptr;
}

QVector<AcSymbolEntry> AcSymbolTable::findSymbolsByPrefix(const QString &prefix) const {
  QVector<AcSymbolEntry> result;
  for (auto it = m_symbols.begin(); it != m_symbols.end(); ++it) {
    if (it.key().startsWith(prefix)) {
      result.append(it.value());
    }
  }
  return result;
}

void AcSymbolTable::clear() {
  m_symbols.clear();
  m_filePath.clear();
}

// ──────────────────────────────────────────────────────────────
//  签名生成
// ──────────────────────────────────────────────────────────────

QString AcSymbolTable::makeFunctionSignature(const QString &name, const QVector<ParamDef> &params,
                                             const AcType &returnType) const {
  QString sig = QStringLiteral("function ");
  sig += name;
  sig += QLatin1Char('(');
  for (int i = 0; i < params.size(); ++i) {
    if (i > 0) sig += QStringLiteral(", ");
    sig += params[i].name;
    if (params[i].type.kind != AcType::kAny) {
      sig += QStringLiteral(": ") + acTypeToString(params[i].type);
    }
  }
  sig += QLatin1Char(')');
  if (returnType.kind != AcType::kAny && returnType.kind != AcType::kVoid) {
    sig += QStringLiteral(": ") + acTypeToString(returnType);
  }
  return sig;
}

QString AcSymbolTable::makeMethodSignature(const QString &className, const QString &name,
                                           const QVector<ParamDef> &params,
                                           const AcType &returnType, bool isStatic) const {
  QString sig;
  if (isStatic) sig += QStringLiteral("static ");
  sig += name;
  sig += QLatin1Char('(');
  for (int i = 0; i < params.size(); ++i) {
    if (i > 0) sig += QStringLiteral(", ");
    sig += params[i].name;
    if (params[i].type.kind != AcType::kAny) {
      sig += QStringLiteral(": ") + acTypeToString(params[i].type);
    }
  }
  sig += QLatin1Char(')');
  if (returnType.kind != AcType::kAny && returnType.kind != AcType::kVoid) {
    sig += QStringLiteral(": ") + acTypeToString(returnType);
  }
  return sig;
}