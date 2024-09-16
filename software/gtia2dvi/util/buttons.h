
#include "hardware/gpio.h"

#ifndef BUTTONS_H
#define BUTTONS_H

// buttons has to be continous GPIO numbers
#define BTN_A 10
#define BTN_B 11

#define BTN_FIRST BTN_A
#define BTN_COUNT 2

#define BTN_DOWN false

#define BTN_DEBOUNCE_TIME 4
#define BTN_DEBOUNCE_TIME_LONG 100
#define BTN_DOUBLE_CLICK_TIME 100

#define BTN_EVENT_TYPE_NONE 0
#define BTN_EVENT_TYPE_CLICK_SHORT 1
#define BTN_EVENT_TYPE_CLICK_LONG 2
#define BTN_EVENT_TYPE_DOUBLE_CLICK 3

#define BTN_EVENT_TYPE_SHIFT 5

#define BTN_EVENT(x, y) ((x) << BTN_EVENT_TYPE_SHIFT) + (y)

static int _btn_down_frames[BTN_COUNT];
static volatile int _btn_last_event = BTN_EVENT(BTN_EVENT_TYPE_NONE, 0);

static inline void _btn_setup(int i)
{
    gpio_init(BTN_FIRST + i);
    gpio_pull_up(BTN_FIRST + i);
    gpio_set_dir(BTN_FIRST + i, GPIO_IN);
}
static inline void _btn_handle(int i)
{
    if (gpio_get(BTN_FIRST + i) == BTN_DOWN)
    {
        _btn_down_frames[i]++;
    }
    else
    {
        int current_time = _btn_down_frames[i];
        if (current_time > BTN_DEBOUNCE_TIME_LONG)
            _btn_last_event = BTN_EVENT(BTN_EVENT_TYPE_CLICK_LONG, BTN_FIRST + i);
        else if (current_time > BTN_DEBOUNCE_TIME)
            _btn_last_event = BTN_EVENT(BTN_EVENT_TYPE_CLICK_SHORT, BTN_FIRST + i);
        _btn_down_frames[i] = 0;
    }
}

static inline void btn_init()
{
    for (int i = 0; i < BTN_COUNT; i++)
        _btn_setup(i);
}

static inline void __not_in_flash_func(btn_debounce)()
{
    for (int i = 0; i < BTN_COUNT; i++)
        _btn_handle(i);
}

static inline int btn_event_type(int event)
{
    return (event >> BTN_EVENT_TYPE_SHIFT) & 0xf;
}

static inline int btn_event_index(int event)
{
    return event & 0x1f;
}

static inline int btn_last_event()
{
    int last = _btn_last_event;
    // clear
    _btn_last_event = BTN_EVENT(BTN_EVENT_TYPE_NONE, 0);
    return last;
}

static inline bool btn_is_down(uint btn)
{
    return gpio_get(btn) == BTN_DOWN;
}

#endif
