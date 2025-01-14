// ****************************************************************************
//  program.cc                                                    DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of RPL programs and blocks
//
//     Programs are lists with a special way to execute
//
//
//
//
//
// ****************************************************************************
//   (C) 2023 Christophe de Dinechin <christophe@dinechin.org>
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

#include "program.h"
#include "parser.h"

RECORDER(program, 16, "Program evaluation");


// ============================================================================
//
//    Program
//
// ============================================================================

EVAL_BODY(program)
// ----------------------------------------------------------------------------
//   Normal evaluation from a program places it on stack
// ----------------------------------------------------------------------------
{
    return rt.push(o) ? OK : ERROR;
}


EXEC_BODY(program)
// ----------------------------------------------------------------------------
//   Execution of a program evaluates all items in turn
// ----------------------------------------------------------------------------
{
    result r = OK;

    for (object_g obj : *o)
    {
        record(program, "Evaluating %+s at %p, size %u\n",
               obj->fancy(), (object_p) obj, obj->size());
        if (interrupted() || r != OK)
            break;
        r = obj->evaluate();
    }

    return r;
}


PARSE_BODY(program)
// ----------------------------------------------------------------------------
//    Try to parse this as a program
// ----------------------------------------------------------------------------
{
    return list_parse(ID_program, p, L'«', L'»');
}


RENDER_BODY(program)
// ----------------------------------------------------------------------------
//   Render the program into the given program buffer
// ----------------------------------------------------------------------------
{
    return o->list_render(r, L'«', L'»');
}


program_p program::parse(utf8 source, size_t size)
// ----------------------------------------------------------------------------
//   Parse a program without delimiters (e.g. command line)
// ----------------------------------------------------------------------------
{
    record(program, ">Parsing command line [%s]", source);
    parser p(source, size);
    result r = list_parse(ID_program, p, 0, 0);
    record(program, "<Command line [%s], end at %u, result %p",
           utf8(p.source), p.end, object_p(p.out));
    if (r != OK)
        return nullptr;
    object_p  obj  = p.out;
    if (!obj)
        return nullptr;
    program_p prog = obj->as<program>();
    return prog;
}



// ============================================================================
//
//    Block
//
// ============================================================================

EVAL_BODY(block)
// ----------------------------------------------------------------------------
//   Normal evaluation of a block executes it
// ----------------------------------------------------------------------------
{
    return do_execute(o);
}


PARSE_BODY(block)
// ----------------------------------------------------------------------------
//  Blocks are parsed in structures like loops, not directly
// ----------------------------------------------------------------------------
{
    return SKIP;
}


RENDER_BODY(block)
// ----------------------------------------------------------------------------
//   Render the program into the given program buffer
// ----------------------------------------------------------------------------
{
    return o->list_render(r, 0, 0);
}
