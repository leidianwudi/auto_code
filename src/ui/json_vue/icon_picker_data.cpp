/**
 * @file icon_picker_data.cpp
 * @brief IconPickerDialog 图标数据源实现
 *
 * 图标数据来自 IconLoader 的本地图标集（res/icons/iconify_sets.json），
 * 前缀/顺序与 shop IconSelect 一致：ep / ri / ant-design / mdi。
 */

#include "icon_picker_dialog.h"

#include "icon_loader.h"

QStringList IconPickerDialog::iconsForTab(int tabIndex) {
  static const char *prefixes[] = {"ep", "ri", "ant-design", "mdi"};
  constexpr int kSetCount = 4;
  if (tabIndex < 0 || tabIndex >= kSetCount) return {};
  QStringList list =
      IconLoader::instance().localIconNames(QString::fromLatin1(prefixes[tabIndex]));
  // 与 shop selectableIconSets 一致：Element 选择器不提供齿轮图标
  //（渲染集合仍保留，已保存的 ep:setting 菜单/按钮可正常预览）
  if (QString::fromLatin1(prefixes[tabIndex]) == QStringLiteral("ep")) {
    list.removeAll(QStringLiteral("ep:setting"));
  }
  return list;
}
