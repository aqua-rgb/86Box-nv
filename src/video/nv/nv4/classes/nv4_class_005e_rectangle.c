/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          NV4: Methods for class 0x005E (Rectangle)
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

void nv4_class_005e_method(uint32_t param, uint32_t method_id, nv4_ramin_context_t context, nv4_grobj_t grobj)
{
    switch (method_id)
    {
        case NV4_RECTANGLE_COLOR:
            nv4->pgraph.rectangle.color = param;
            break; 
        default:
            /* Check for any rectangle point or size method. */
            if (method_id >= NV4_RECTANGLE_START && method_id <= NV4_RECTANGLE_END)
            {
                uint32_t index = (method_id - NV4_RECTANGLE_START) >> 3;

                // If the size is submitted, render it.
                if (method_id & 0x04)
                {
                    nv4->pgraph.rectangle.size[index].w = param & 0xFFFF;
                    nv4->pgraph.rectangle.size[index].h = (param >> 16) & 0xFFFF;   
                    
                    nv_log("Method Execution: Rect%d Size=%d,%d Color=0x%08x\n", index, nv4->pgraph.rectangle.size[index].w, nv4->pgraph.rectangle.size[index].h, nv4->pgraph.rectangle.color);

                    nv4_render_rect(nv4->pgraph.rectangle.position[index], nv4->pgraph.rectangle.size[index], nv4->pgraph.rectangle.color, grobj);
                }
                else // position
                {
                    nv4->pgraph.rectangle.position[index].x = param & 0xFFFF;
                    nv4->pgraph.rectangle.position[index].y = (param >> 16) & 0xFFFF;
                    
                    nv_log("Method Execution: Rect%d Position=%d,%d\n", index, nv4->pgraph.rectangle.position[index].x, nv4->pgraph.rectangle.position[index].y);
                }

                return;
            }

            warning("%s: Invalid or unimplemented method 0x%04x\n", nv4_class_names[context.class_id], method_id);
            nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_SOFTWARE_METHOD_PENDING);
            return;
    }
}