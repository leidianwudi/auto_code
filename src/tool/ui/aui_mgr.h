/**
 * @file aui_mgr.h
 * @brief UI 控制器基类
 *
 * 所有 UI 控制器的基类，提供单例管理和窗口打开/关闭功能。
 * 继承 Singleton<TDerived> 实现单例，继承 QObject 支持 Qt 信号槽。
 *
 * 使用方式：
 *   class MyMgr : public AuiMgr<MyMgr> {
 *     Q_OBJECT
 *   protected:
 *     QWidget *onCreateWindow() override {
 *       auto *w = new MyWindow;
 *       // ... 初始化
 *       return w;
 *     }
 *   };
 *
 *   MyMgr::ins().open();  // 打开/激活窗口（可重复调用）
 */

#pragma once

#include <QApplication>
#include <QObject>
#include <QWidget>

#include "src/tool/design/Singleton.h"

/**
 * @class AuiMgr
 * @brief UI 管理器基类（CRTP 单例）
 *
 * @tparam TDerived 派生类（CRTP 模式）
 *
 * 特性：
 * - 继承 Singleton<TDerived> 实现全局唯一实例
 * - 继承 QObject 使派生类支持 Q_OBJECT 宏
 * - open() 可重复调用，首次创建窗口，后续仅激活
 * - closeWindow() 隐藏窗口，不会销毁窗口对象
 */
template <typename TDerived>
class AuiMgr : public QObject, public Singleton<TDerived> {
public:
  explicit AuiMgr(QObject *parent = nullptr) : QObject(parent) {}

  /**
   * @brief 打开/激活窗口（可重复调用）
   *
   * 首次调用时通过 onCreateWindow() 创建窗口，
   * 后续调用仅将窗口显示、提升到最前、激活。
   *
   * 首次打开时，自动在最近获得焦点的窗口（activeWindow）中间居中。
   * 如果无活跃窗口，则回退到 parentWidget() 居中。
   */
  void open() {
    if (!m_window) {
      m_window = derived()->onCreateWindow();
    }
    if (m_window) {
      // 首次显示时，在父窗口中间居中
      if (!m_window->isVisible()) {
        centerOnParent();
      }
      m_window->showNormal();
      m_window->raise();
      m_window->activateWindow();
    }
  }

  /**
   * @brief 关闭窗口（仅隐藏，不销毁）
   */
  void close() {
    if (m_window) {
      m_window->close();
    }
  }

  /// 获取窗口指针
  QWidget *window() const { return m_window; }

  /// 窗口是否已打开（存在且可见）
  bool isOpen() const { return m_window && m_window->isVisible(); }

protected:
  /**
   * @brief 创建窗口对象（纯虚函数）
   *
   * 派生类在此方法中创建具体的 QWidget 子类窗口，
   * 进行初始化设置并返回。
   */
  virtual QWidget *onCreateWindow() = 0;

  /// 将窗口居中于最近获得焦点的窗口（activeWindow）中间
  /// 如果无活跃窗口，则回退到 parentWidget() 居中
  void centerOnParent() {
    // 优先以当前活跃窗口为参考居中（非模式对话框场景）
    QWidget *ref = QApplication::activeWindow();
    if (!ref) ref = m_window ? m_window->parentWidget() : nullptr;
    if (ref) {
      int x = ref->x() + (ref->width() - m_window->width()) / 2;
      int y = ref->y() + (ref->height() - m_window->height()) / 2;
      m_window->move(x, y);
    }
  }

  TDerived *derived() { return static_cast<TDerived *>(this); }
  QWidget *m_window = nullptr;
};