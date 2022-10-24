// ****************************************************************************
//  runtime.cc                                                    DB48X project
// ****************************************************************************
//
//   File Description:
//
//     Implementation of the RPL runtime
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

#include "runtime.h"
#include "object.h"
#include <cstring>

// The one and only runtime
runtime runtime::RT(nullptr, 0);

RECORDER(runtime,       16, "RPL runtime");
RECORDER(runtime_error, 16, "RPL runtime error (anomalous behaviors)");
RECORDER(editor,        16, "Text editor (command line)");
RECORDER(errors,        16, "Runtime errors)");
RECORDER(gc,            16, "Garbage collection events");
RECORDER(gc_details,    32, "Details about garbage collection (noisy)");


static void dump_object_list(cstring  message,
                             object_p first,
                             object_p last,
                             object_p *stack,
                             object_p *stackEnd)
// ----------------------------------------------------------------------------
//   Dump all objects in a given range
// ----------------------------------------------------------------------------
{
    uint count = 0;
    size_t sz = 0;
    object_p next;

    record(gc, "%+s object list", message);
    for (object_p obj = first; obj < last; obj = next)
    {
        next = obj->skip();
        object::id i = obj->type();
        record(gc, " %p-%p: %+s (%d) uses %d bytes)",
               obj, next-1, object::name(i), i, next - obj);
        sz += next - obj;
        count++;
    }
    record(gc, "%+s stack", message);
    for (object_p *s = stack; s < stackEnd; s++)
        record(gc, " %u: %p (%+s)", s - stack, *s, object::name((*s)->type()));
    record(gc, "%+s: %u objects using %u bytes", message, count, sz);
}


size_t runtime::gc()
// ----------------------------------------------------------------------------
//   Recycle unused temporaries
// ----------------------------------------------------------------------------
//   Temporaries can only be referenced from the stack
//   Objects in the global area are copied there, so they need no recycling
//   This algorithm is linear in number of objects and moves only live data
{
    size_t   recycled = 0;
    object_p first    = (object_p) Globals;
    object_p last     = Temporaries;
    object_p free     = first;
    object_p next;

    record(gc, "Garbage collection, available %u, range %p-%p",
           available(), first, last);
    if (RECORDER_TRACE(gc) > 1)
        dump_object_list("Pre-collection",
                         first, last, StackTop, StackBottom);

    for (object_p obj = first; obj < last; obj = next)
    {
        bool found = false;
        next = skip(obj);
        record(gc_details, "Scanning object %p (ends at %p)", obj, next);
        for (object_p *s = StackTop; s < StackBottom && !found; s++)
        {
            found = *s >= obj && *s < next;
            if (found)
                record(gc_details, "Found %p at stack level %u",
                       obj, s - StackTop);
        }
        if (!found)
        {
            for (gcptr *p = GCSafe; p && !found; p = p->next)
            {
                found = p->safe >= (byte *) obj && p->safe <= (byte *) next;
                if (found)
                    record(gc_details, "Found %p in GC-safe pointer %p (%p)",
                           obj, p->safe, p);
            }
        }
        if (found)
        {
            // Move object to free space
            record(gc_details, "Moving %p-%p to %p", obj, next, free);
            move(obj, next, free);
            free += next - obj;
        }
        else
        {
            recycled += next - obj;
            record(gc_details, "Recycling %p size %u total %u",
                   obj, next - obj, recycled);
        }
    }

    // Move the command line
    if (Editing)
    {
        object_p edit = Temporaries;
        move(edit, edit + Editing, edit - recycled);
    }

    // Adjust Temporaries
    Temporaries -= recycled;


    if (RECORDER_TRACE(gc) > 1)
        dump_object_list("Post-collection",
                         (object_p) Globals, Temporaries,
                         StackTop, StackBottom);
    record(gc, "Garbage collection done, purged %u, available %u",
           recycled, available());
    return recycled;
}


void runtime::move(object_p first, object_p last, object_p to)
// ----------------------------------------------------------------------------
//   Move objects in memory to a new location, adjusting pointers
// ----------------------------------------------------------------------------
{
    int delta = to - first;
    if (!delta)
        return;

    int size  = last - first;
    if (size <= 0)
    {
        if (size < 0)
            record(runtime_error, "GC move with range %p-%p", first, last);
        return;
    }

    // Move the object in memory
    memmove((byte *) to, (byte *) first, size);

    // Adjust the stack pointers
    for (object_p *s = StackTop; s < StackBottom; s++)
    {
        if (*s >= first && *s < last)
        {
            record(gc_details, "Adjusting stack level %u from %p to %p",
                   s - StackTop, *s, *s + delta);
            *s += delta;
        }
    }

    // Adjust the protected pointers
    for (gcptr *p = GCSafe; p; p = p->next)
    {
        if (p->safe >= (byte *) first && p->safe < (byte *) last)
        {
            record(gc_details, "Adjusting GC-safe %p from %p to %p",
                   p, p->safe, p->safe + delta);
            p->safe += delta;
        }
    }
}


size_t runtime::size(object_p obj)
// ----------------------------------------------------------------------------
//   Delegate the size to the object
// ----------------------------------------------------------------------------
{
    return obj->size(*this);
}


utf8 runtime::close_editor()
// ----------------------------------------------------------------------------
//   Close the editor and encapsulate its content into a string
// ----------------------------------------------------------------------------
//   This will move the editor below the temporaries, encapsulated as
//   a string. After that, it is safe to allocate temporaries without
//   overwriting the editor
{
    // Compute the extra size we need for a string header
    size_t hdrsize = leb128size(object::ID_string) + leb128size(Editing + 1);
    if (available(hdrsize) < hdrsize)
        return nullptr;

    // Move the editor data above that header
    char *ed = (char *) Temporaries;
    char *str = ed + hdrsize;
    memmove(str, ed, Editing);

    // Null-terminate that string for safe use by C code
    str[Editing] = 0;
    record(editor, "Closing editor size %u at %p [%s]", Editing, ed, str);

    // Write the string header
    ed = leb128(ed, object::ID_string);
    ed = leb128(ed, Editing + 1);

    // Move Temporaries past that newly created string
    Temporaries = (object_p) str + Editing + 1;

    // We are no longer editing
    Editing = 0;

    // Return a pointer to a valid C string safely wrapped in a RPL string
    return utf8(str);
}
