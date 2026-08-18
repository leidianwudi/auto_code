/**
 * @file setting_model.cpp
 * @brief 设置界面 — 数据模型层实现
 */

#include "setting_model.h"

#include "src/util/ui/setting_store.h"

SettingModel::SettingModel() : m_store(SettingStore::ins()) {}

int SettingModel::themeIndex() const {
  switch (m_store.theme()) {
    case SettingStore::ThemeLight:
      return 0;
    case SettingStore::ThemeDark:
      return 1;
    case SettingStore::ThemeCustom:
      return 2;
  }
  return 0;
}

void SettingModel::setThemeIndex(int index) {
  switch (index) {
    case 0:
      m_store.setTheme(SettingStore::ThemeLight);
      break;
    case 1:
      m_store.setTheme(SettingStore::ThemeDark);
      break;
    default:
      m_store.setTheme(SettingStore::ThemeCustom);
      break;
  }
}

QStringList SettingModel::themeNames() const {
  return {m_store.themeName(SettingStore::ThemeLight),
          m_store.themeName(SettingStore::ThemeDark),
          m_store.themeName(SettingStore::ThemeCustom)};
}

QList<ColorEntry> SettingModel::colors() const {
  QList<ColorEntry> list;
  const QStringList keys = m_store.colorKeys();
  for (const QString &key : keys) {
    ColorEntry e;
    e.key = key;
    e.label = m_store.colorLabel(key);
    e.category = m_store.colorCategory(key);
    e.hex = m_store.color(key).name();
    e.custom = m_store.hasCustomColor(key);
    list.append(e);
  }
  return list;
}

void SettingModel::setColor(const QString &key, const QString &hex) {
  m_store.setColor(key, QColor(hex));
}

void SettingModel::resetColor(const QString &key) { m_store.resetColor(key); }

void SettingModel::resetAllColors() { m_store.resetAllColors(); }

QList<ShortcutSettingEntry> SettingModel::shortcuts() const {
  QList<ShortcutSettingEntry> list;
  const QStringList keys = m_store.shortcutKeys();
  for (const QString &key : keys) {
    ShortcutSettingEntry e;
    e.key = key;
    e.label = m_store.shortcutLabel(key);
    e.category = m_store.shortcutCategory(key);
    e.sequence = m_store.shortcut(key).toString();
    list.append(e);
  }
  return list;
}

void SettingModel::setShortcut(const QString &key, const QString &sequence) {
  m_store.setShortcut(key, QKeySequence(sequence));
}

QList<FontEntry> SettingModel::fonts() const {
  QList<FontEntry> list;
  const QStringList keys = m_store.fontKeys();
  for (const QString &key : keys) {
    FontEntry e;
    e.key = key;
    e.label = m_store.fontLabel(key);
    e.size = m_store.fontSize(key);
    e.custom = m_store.hasCustomFont(key);
    e.family = m_store.fontFamily(key);
    e.familyCustom = m_store.hasCustomFontFamily(key);
    list.append(e);
  }
  return list;
}

void SettingModel::setFontSize(const QString &key, int size) { m_store.setFontSize(key, size); }

void SettingModel::setFontFamily(const QString &key, const QString &family) {
  m_store.setFontFamily(key, family);
}

void SettingModel::resetFontSize(const QString &key) { m_store.resetFontSize(key); }

void SettingModel::resetFontFamily(const QString &key) { m_store.resetFontFamily(key); }

void SettingModel::resetAllFonts() { m_store.resetAllFonts(); }

void SettingModel::save() { m_store.save(); }
