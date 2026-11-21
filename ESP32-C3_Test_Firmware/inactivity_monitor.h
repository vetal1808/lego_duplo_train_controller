#ifndef INACTIVITY_MONITOR_H
#define INACTIVITY_MONITOR_H

typedef void (*InactivityCallback)();

class InactivityMonitor
{
private:
    unsigned long lastActivityTime;
    unsigned long inactivityTimeout;
    InactivityCallback onTimeoutCallback;
    bool hasTimedOut;

public:
    InactivityMonitor();
    InactivityMonitor(unsigned long timeout, InactivityCallback callback);
    void resetActivity();
    void setTimeout(unsigned long timeout);
    void update();
    unsigned long getInactiveTime();
    bool isTimedOut();
    void resetTimeout();
};

#endif // INACTIVITY_MONITOR_H
