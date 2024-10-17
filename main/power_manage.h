#ifndef POWER_MANAGE_H
#define POWER_MANAGE_H

#define uS_TO_S_FACTOR 1000000
#define DEEP_SLEEP_DURATION 15

// Function to disable the RTC watchdog timer
void disable_rtc_wdt();
void enter_deep_sleep_mode(int duration);
int get_wakeup_reason();
void wifi_init();

#endif // POWER_MANAGE_H