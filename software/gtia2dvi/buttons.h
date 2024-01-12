

#ifndef BUTTONS_H
#define BUTTONS_H

#define BTN_A 8
#define BTN_B 9

#define BTN_DOWN false

#define BTN_DEBOUNCE_TIME 4

int btn_down_frames[2] = {0, 0};

static inline void btn_init()
{
    gpio_init(BTN_A);
    gpio_init(BTN_B);

    gpio_pull_up(BTN_A);
    gpio_pull_up(BTN_B);
}

static inline void btn_debounce()
{
    for (int i = 0; i < 2; i++)
    {
        if (gpio_get(i + BTN_A) == BTN_DOWN)
        {
            btn_down_frames[i]++;
        }
        else
        {
            btn_down_frames[i] = 0;
        }
    }
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