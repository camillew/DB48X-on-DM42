// ****************************************************************************
//  main.cc                                                       DB48X project
// ****************************************************************************
//
//   File Description:
//
//      The DB48X main RPL loop
//
//
//
//
//
//
//
//
// ****************************************************************************
//   (C) 2022 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the terms outlined in LICENSE.txt
// ****************************************************************************
//   This file is part of DB48X.
//
//   DB48X is free software: you can redistribute it and/or modify
//   it under the terms outlined in the LICENSE.txt file
//
//   DB48X is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// This code is distantly derived from the SwissMicro SDKDemo calculator

#include "dm42/main.h"

#include "dm42/sysmenu.h"
#include "font.h"
#include "graphics.h"
#include "input.h"
#include "list.h"
#include "num.h"
#include "stack.h"
#include "sysmenu.h"
#include "target.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dmcp.h>

using std::max;
using std::min;


uint last_keystroke_time = 0;

static void redraw_lcd()
// ----------------------------------------------------------------------------
//   Redraw the whole LCD
// ----------------------------------------------------------------------------
{
    uint period = 60000;
    uint now = sys_current_ms();

    // Draw the header
    Screen.fill(0, 0, LCD_W, HeaderFont->height() + 1, pattern::black);
    Screen.text(4, 0, utf8(PROGRAM_NAME), HeaderFont, pattern::white);

    // Draw the various components handled by input
    Input.draw_annunciators();
    Input.draw_battery(now, period, true);
    Input.draw_menus(now, period, true);
    if (!Input.draw_help())
    {
        Input.draw_editor();
        Input.draw_cursor(now, period, true);
        Stack.draw_stack();
        Input.draw_command();
    }
    Input.draw_error();

    // Refres the screen
    lcd_refresh_lines(0, LCD_H);

    // Refresh screen moving elements after 0.1s
    sys_timer_disable(TIMER1);
    sys_timer_start(TIMER1, period);
}


static void redraw_periodics()
// ----------------------------------------------------------------------------
//   Redraw the elements that move
// ----------------------------------------------------------------------------
{
    uint period = 60000;         // Default refresh is one minute
    uint now = sys_current_ms();

    int cy = Input.draw_cursor(now, period, false);
    if (cy >= 0)
        lcd_refresh_lines(cy, EditorFont->height());
    cy = Input.draw_battery(now, period, false);
    if (cy >= 0)
        lcd_refresh_lines(cy, HeaderFont->height());
    cy = Input.draw_menus(now, period, false);
    if (cy >= 0)
        lcd_refresh_lines(cy, LCD_H - cy);

    uint dawdle_time = sys_current_ms() - last_keystroke_time;
    if (dawdle_time > 180000)           // If inactive for 3 minutes
        period = 60000;                 // Only upate screen every minute
    else if (dawdle_time > 60000)       // If inactive for 1 minute
        period = 10000;                 // Onlyi update screen every 10s
    if (dawdle_time > 10000)            // If inactive for 10 seconds
        period = 3000;                  // Only upate screen every 3 second

    // Refresh screen moving elements after 0.1s
    sys_timer_disable(TIMER1);
    sys_timer_start(TIMER1, period);
}


static void handle_key(int key, bool repeating)
// ----------------------------------------------------------------------------
//   Handle all input keys
// ----------------------------------------------------------------------------
{
    sys_timer_disable(TIMER0);
    bool consumed = Input.key(key, repeating);
    if (!consumed)
        beep(1835, 125);

    // Key repeat timer
    if (Input.repeating())
        sys_timer_start(TIMER0, repeating ? 80 : 500);

    // Refresh screen moving elements after 0.1s
    sys_timer_disable(TIMER1);
    sys_timer_start(TIMER1, 100);
}


void program_init()
// ----------------------------------------------------------------------------
//   Initialize the program
// ----------------------------------------------------------------------------
{
    // Setup application menu callbacks
    run_menu_item_app = menu_item_run;
    menu_line_str_app = menu_item_description;

    // Setup default fonts
    font_defaults();

#ifndef DEBUG
    // Give as much as memory as possible to the runtime
    // Experimentally, this is the amount of memory we need to leave free
    size_t size = sys_free_mem() - 10 * 1024;
#else
    // Give 256 bytes to the runtime to stress-test the GC
    size_t size = 2048;
#endif
    byte *memory = (byte *) malloc(size);
    runtime::RT.memory(memory, size);

    // The following is just to link the same set of functions as DM42
    if (memory == (byte *) program_init)
    {
        double      d = *memory;
        BID_UINT64  a;
        BID_UINT128 res;
        binary64_to_bid64(&a, &d);
        bid64_to_bid128(&res, &a);
        num_add(&res, &res, &res);
        num_sub(&res, &res, &res);
        num_mul(&res, &res, &res);
        num_div(&res, &res, &res);
        num_div(&res, &res, &res);
        num_sqrt(&res, &res);
        num_log10(&res, &res);
        num_log(&res, &res);
        num_pow(&res, &res, &res);
        num_mul(&res, &res, &res);
        num_exp10(&res, &res);
        num_exp(&res, &res);
        num_sin(&res, &res);
        num_cos(&res, &res);
        num_tan(&res, &res);
        num_asin(&res, &res);
        num_acos(&res, &res);
        num_atan(&res, &res);
    }

    // Check if we have a state file to load
    if (sys_disk_ok())
    {
        // Try to load the state file, but only if it has the right
        // extension. This is necessary, because get_reset_state_file() could
        // legitimately return a .f42 file if we just switched from DM42.
        char *state = get_reset_state_file();
        if (state && *state && strstr(state, ".48S"))
            load_state_file(state);
    }
}


extern "C" void program_main()
// ----------------------------------------------------------------------------
//   DMCP main entry point and main loop
// ----------------------------------------------------------------------------
// Status flags:
// ST(STAT_PGM_END)   - Program should go to off state (set by auto off timer)
// ST(STAT_SUSPENDED) - Program signals it is ready for off
// ST(STAT_OFF)       - Program in off state (only [EXIT] key can wake it up)
// ST(STAT_RUNNING)   - OS doesn't sleep in this mode
{
    int key = 0;

    // Initialization
    program_init();
    redraw_lcd();
    last_keystroke_time = sys_current_ms();

    // Main loop
    while (true)
    {
        // Already in off mode and suspended
        if ((ST(STAT_PGM_END) && ST(STAT_SUSPENDED)) ||
            // Go to sleep if no keys available
            (!ST(STAT_PGM_END) && key_empty()))
        {
            CLR_ST(STAT_RUNNING);
            sys_sleep();
        }

        // Wakeup in off state or going to sleep
        if (ST(STAT_PGM_END) || ST(STAT_SUSPENDED))
        {
            if (!ST(STAT_SUSPENDED))
            {
                // Going to off mode
                lcd_set_buf_cleared(0); // Mark no buffer change region
                draw_power_off_image(0);

                LCD_power_off(0);
                SET_ST(STAT_SUSPENDED);
                SET_ST(STAT_OFF);
            }
            // Already in OFF -> just continue to sleep above
            continue;
        }

        // Well, we are woken-up
        SET_ST(STAT_RUNNING);

        // We definitely reached active state, clear suspended flag
        CLR_ST(STAT_SUSPENDED);

        // Get up from OFF state
        if (ST(STAT_OFF))
        {
            LCD_power_on();

            // Ensure that RTC readings after power off will be OK
            rtc_wakeup_delay();

            CLR_ST(STAT_OFF);

            // Check if we need to redraw
            if (!lcd_get_buf_cleared())
                lcd_forced_refresh();
        }

        // Key is ready -> clear auto off timer
        bool hadKey = false;
        if (!key_empty())
        {
            reset_auto_off();
            key    = key_pop();
            hadKey = true;
#if SIMULATOR
            if (key == -1)
            {
                cstring path = get_reset_state_file();
                printf("Exit: saving state to %s\n", path);
                if (path && *path)
                    save_state_file(path);
                break;
            }
#endif // SIMULATOR
        }
        bool repeating = sys_timer_timeout(TIMER0);
        if (repeating)
            hadKey = true;

        // Fetch the key (<0: no key event, >0: key pressed, 0: key released)
        if (key >= 0 && hadKey)
        {
            handle_key(key, repeating);

            // Redraw the LCD
            if (key_empty())
                redraw_lcd();

            // If user is typing, we can GC to keep other things fast
            // ... very bad idea: slows everything down a lot
            // runtime::RT.gc();

            // Record the last keystroke
            last_keystroke_time = sys_current_ms();
        }
        else
        {
            // Blink the cursor
            if (sys_timer_timeout(TIMER1))
                redraw_periodics();
        }
    }
}


bool program::interrupted()
// ----------------------------------------------------------------------------
//   Return true if the current program must be interrupted
// ----------------------------------------------------------------------------
{
    if (!key_empty())
        return key_tail() == KEY_EXIT;
    return false;
}
