

#ifndef BUTTONS_H
#define BUTTONS_H

// buttons has to be continous GPIO numbers
#define BTN_A 8
#define BTN_B 9

#define BTN_DOWN false

#define BTN_DEBOUNCE_TIME 10
#define BTN_DEBOUNCE_TIME_LONG 180

enum BtnEvent
{
    NONE = 0,
    BTN_A_SHORT = 10,
    BTN_B_SHORT,
    BTN_A_LONG = 20,
    BTN_B_LONG,
};

int btn_down_frames[2] = {0, 0};
enum BtnEvent _btn_last_event = NONE;

static inline void
btn_init()
{
    gpio_init(BTN_A);
    gpio_init(BTN_B);

    gpio_pull_up(BTN_A);
    gpio_pull_up(BTN_B);
}

inline void btn_debounce()
{
    for (int i = 0; i < 2; i++)
    {
        if (gpio_get(i + BTN_A) == BTN_DOWN)
        {
            btn_down_frames[i]++;
        }
        else
        {
            int current_time = btn_down_frames[i];
            if (current_time > BTN_DEBOUNCE_TIME)
            {
                _btn_last_event = BTN_A_SHORT + i;
                btn_down_frames[i] = 0;
                continue;
            }
            btn_down_frames[i] = 0;
        }
    }
}

static inline enum BtnEvent btn_last_event()
{
    enum BtnEvent last = _btn_last_event;
    // clear
    _btn_last_event = NONE;
    return last;
}

static inline bool btn_pushed(uint btn)
{
    return btn_down_frames[btn - BTN_A] >= BTN_DEBOUNCE_TIME;
}

static inline bool btn_is_down(uint btn)
{
    return gpio_get(btn) == BTN_DOWN;
}

#endif