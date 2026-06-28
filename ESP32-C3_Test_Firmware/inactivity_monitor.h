#ifndef INACTIVITY_MONITOR_H
#define INACTIVITY_MONITOR_H

typedef void (*InactivityCallback)();

class InactivityMonitor {
private:
  unsigned long lastActivityTime;
  unsigned long inactivityTimeout;
  InactivityCallback onTimeoutCallback;
  bool hasTimedOut;

public:
  /**
   * @param timeout Время бездействия в миллисекундах перед срабатыванием callback
   * @param callback Функция, вызываемая при превышении времени бездействия
   */
  InactivityMonitor(unsigned long timeout, InactivityCallback callback);

  /**
   * Сбросить таймер бездействия (отметить текущий момент как активность)
   */
  void resetActivity();

  /**
   * Установить новое время таймаута
   */
  void setTimeout(unsigned long timeout);

  /**
   * Проверить, превышено ли время бездействия
   * Вызывает callback один раз при первом превышении таймаута
   */
  void update();

  /**
   * Получить время бездействия в миллисекундах
   */
  unsigned long getInactiveTime();

  /**
   * Проверить, произошло ли событие таймаута
   */
  bool isTimedOut();

  /**
   * Сбросить флаг таймаута
   */
  void resetTimeout();
};

#endif // INACTIVITY_MONITOR_H
