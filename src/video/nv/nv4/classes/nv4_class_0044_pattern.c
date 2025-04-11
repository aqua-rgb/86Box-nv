/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          NV4: Methods for class 0x0044 (Pattern)
 *
 *
 *
 * Authors: Connor Hyde, <mario64crashed@gmail.com> I need a better email address ;^)
 *
 *          Copyright 2024-2025 Connor Hyde
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/rom.h>
#include <86box/video.h>
#include <86box/nv/vid_nv.h>
#include <86box/nv/vid_nv4.h>

void nv4_class_0044_method(uint32_t param, uint32_t method_id, nv4_ramin_context_t context, nv4_grobj_t grobj)
{
    switch (method_id)
    {
        /* Valid software method, suppress logging */
        case NV4_PATTERN_FORMAT:
            nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_SOFTWARE_METHOD_PENDING);
            break; 
        case NV4_PATTERN_SHAPE:
            /* If the shape is not valid, tell the software that it's invalid */

            /* 
            Technically you are meant to do this: 

            But in practice, I don't know, because it always submits 0x20 or 0x40, which are valid when param & 0x03,
            and appear to be deliberate behaviour in the drivers rather than bugs. What
            if (param > NV4_PATTERN_SHAPE_LAST_VALID)
            {
                warning("nv4 class 0x06 (Pattern) invalid shape %d (This is a bug)", param);
                nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_INVALID_DATA);
                return; 
            }
            
            */
            nv4->pgraph.pattern_shape = param & 0x03;

            break;
        /* Seems to be "SetPatternSelect" on Riva TNT and later, but possibly called by accident on Riva 128. There is no hardware equivalent for this. So let's just suppress
        the warnings. */
        case NV4_PATTERN_UNUSED_DRIVER_BUG:
            break;
        case NV4_PATTERN_COLOR0:
            nv4_color_expanded_t expanded_colour0 = nv4_render_expand_color(param, grobj);
            nv4_render_set_pattern_color(expanded_colour0, false);
            break;
        case NV4_PATTERN_COLOR1:
            nv4_color_expanded_t expanded_colour1 = nv4_render_expand_color(param, grobj);
            nv4_render_set_pattern_color(expanded_colour1, true);
            break;
        case NV4_PATTERN_BITMAP_HIGH:
            nv4->pgraph.pattern_bitmap = 0; //reset
            nv4->pgraph.pattern_bitmap |= ((uint64_t)param << 32); 
            break;
        case NV4_PATTERN_BITMAP_LOW:
            nv4->pgraph.pattern_bitmap |= param;
            break;
        default:
            warning("%s: Invalid or unimplemented method 0x%04x\n", nv4_class_names[context.class_id], method_id);
            nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_SOFTWARE_METHOD_PENDING);
            break;
    }
}