#ifndef SETTINGS_H
#define SETTINGS_H
// ****************************************************************************
//  settings.h                                                    DB48X project
// ****************************************************************************
//
//   File Description:
//
//     List of system-wide settings
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
// ****************************************************************************

#include "command.h"
#include "menu.h"
#include "renderer.h"

#include <types.h>


struct settings
// ----------------------------------------------------------------------------
//    Internal representation of settings
// ----------------------------------------------------------------------------
{
    enum { STD_DISPLAYED = 20 };

    settings()
        : precision(BID128_MAXDIGITS),
          display_mode(NORMAL),
          displayed(STD_DISPLAYED),
          decimal_mark('.'),
          exponent_mark(L'⁳'),
          standard_exp(9),
          angle_mode(DEGREES),
          base(16),
          wordsize(64),
          command_fmt(LONG_FORM),
          show_decimal(true),
          fancy_exponent(true)
    {}

    enum angles
    // ------------------------------------------------------------------------
    //   The base used for angles
    // ------------------------------------------------------------------------
    {
        DEGREES,
        RADIANS,
        GRADS,
        NUM_ANGLES,
    };

    enum display
    // ------------------------------------------------------------------------
    //   The display mode for numbers
    // ------------------------------------------------------------------------
    {
        NORMAL, FIX, SCI, ENG
    };

    enum commands
    // ------------------------------------------------------------------------
    //   Display of commands
    // ------------------------------------------------------------------------
    {
        LOWERCASE,              // Display the short name in lowercase
        UPPERCASE,              // Display the short name in uppercase
        CAPITALIZED,            // Display the short name capitalized
        LONG_FORM,              // Display the long form
    };


    void save(renderer &out, bool show_defaults = false);


public:
    uint16_t precision;         // Internal precision for numbers
    display  display_mode;      // Display mode
    uint16_t displayed;         // Number of displayed digits
    char     decimal_mark;      // Character used for decimal separator
    unicode  exponent_mark;     // The character used to represent exponents
    uint16_t standard_exp;      // Maximum exponent before switching to sci
    angles   angle_mode;        // Angle mode ( degrees, radians or grads)
    uint8_t  base;              // The default base for #numbers
    uint16_t wordsize;          // Wordsize for binary numbers (in bits)
    commands command_fmt;       // How we prefer to display commands
    bool     show_decimal   :1; // Show decimal dot for integral real numbers
    bool     fancy_exponent :1; // Show exponent with fancy superscripts
};


extern settings Settings;


// Macro to defined a simple command handler for derived classes
#define SETTINGS_COMMAND_DECLARE(derived)                       \
struct derived : command                                        \
{                                                               \
    derived(id i = ID_##derived) : command(i) { }               \
                                                                \
    OBJECT_HANDLER(derived)                                     \
    {                                                           \
        switch(op)                                              \
        {                                                       \
        case EVAL:                                              \
        case EXEC:                                              \
            RT.command(fancy(ID_##derived));                    \
            Input.menuNeedsRefresh();                           \
            return ((derived *) obj)->evaluate();               \
        case MENU_MARKER:                                       \
            return ((derived *) obj)->marker();                 \
        default:                                                \
            return DELEGATE(command);                           \
        }                                                       \
    }                                                           \
    static result evaluate();                                   \
    static unicode marker();                                    \
    static cstring menu_label(menu::info &mi);                  \
}

#define SETTINGS_COMMAND_BODY(derived, mkr)                     \
    unicode derived::marker() { return mkr; }                   \
    object::result derived::evaluate()

#define SETTINGS_COMMAND_NOLABEL(derived, mkr)                  \
    unicode derived::marker() { return mkr; }                   \
    cstring derived::menu_label(menu::info UNUSED &mi)          \
    {                                                           \
        return #derived;                                        \
    }                                                           \
    object::result derived::evaluate()

#define SETTINGS_COMMAND_LABEL(derived)                         \
    cstring derived::menu_label(menu::info UNUSED &mi)


COMMAND_DECLARE(Modes);

SETTINGS_COMMAND_DECLARE(Std);
SETTINGS_COMMAND_DECLARE(Fix);
SETTINGS_COMMAND_DECLARE(Sci);
SETTINGS_COMMAND_DECLARE(Eng);
SETTINGS_COMMAND_DECLARE(Sig);

SETTINGS_COMMAND_DECLARE(Deg);
SETTINGS_COMMAND_DECLARE(Rad);
SETTINGS_COMMAND_DECLARE(Grad);

SETTINGS_COMMAND_DECLARE(LowerCase);
SETTINGS_COMMAND_DECLARE(UpperCase);
SETTINGS_COMMAND_DECLARE(Capitalized);
SETTINGS_COMMAND_DECLARE(LongForm);

SETTINGS_COMMAND_DECLARE(DecimalDot);
SETTINGS_COMMAND_DECLARE(DecimalComma);
SETTINGS_COMMAND_DECLARE(NoTrailingDecimal);
SETTINGS_COMMAND_DECLARE(TrailingDecimal);
SETTINGS_COMMAND_DECLARE(Precision);
SETTINGS_COMMAND_DECLARE(StandardExponent);
SETTINGS_COMMAND_DECLARE(FancyExponent);
SETTINGS_COMMAND_DECLARE(ClassicExponent);

SETTINGS_COMMAND_DECLARE(Base);
SETTINGS_COMMAND_DECLARE(Bin);
SETTINGS_COMMAND_DECLARE(Oct);
SETTINGS_COMMAND_DECLARE(Dec);
SETTINGS_COMMAND_DECLARE(Hex);

SETTINGS_COMMAND_DECLARE(stws);
COMMAND_DECLARE(rcws);

#endif // SETTINGS_H
