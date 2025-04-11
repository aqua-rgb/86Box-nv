/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          NV4: Methods for class 0x0039 (Reformat image in memory)
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

void nv4_class_0039_method(uint32_t param, uint32_t method_id, nv4_ramin_context_t context, nv4_grobj_t grobj)
{
    switch (method_id)
    {
        case NV4_M2MF_IN_CTXDMA_OFFSET:
            nv4->pgraph.m2mf.offset_in = param;
            nv_log("Method Execution: M2MF Offset In = 0x%08x", param);
            break;  
        case NV4_M2MF_OUT_CTXDMA_OFFSET:
            nv4->pgraph.m2mf.offset_out = param;
            nv_log("Method Execution: M2MF Offset Out = 0x%08x", param);
            break;  
        case NV4_M2MF_IN_PITCH:
            nv4->pgraph.m2mf.pitch_in = param;
            nv_log("Method Execution: M2MF Pitch In = 0x%08x", param);
            break;  
        case NV4_M2MF_OUT_PITCH:
            nv4->pgraph.m2mf.pitch_out = param;
            nv_log("Method Execution: M2MF Pitch Out = 0x%08x", param);
            break;  
        case NV4_M2MF_SCANLINE_LENGTH_IN_BYTES:
            nv4->pgraph.m2mf.line_length_in = param;
            nv_log("Method Execution: M2MF Scanline Length in Bytes = 0x%08x", param);
            break;  
        case NV4_M2MF_NUM_SCANLINES:
            nv4->pgraph.m2mf.line_count = param;
            nv_log("Method Execution: M2MF Num Scanlines = 0x%08x", param);
            break; 
        case NV4_M2MF_FORMAT:
            nv4->pgraph.m2mf.format = param; 
            nv_log("Method Execution: M2MF Format = 0x%08x", param);
            break;
        case NV4_M2MF_NOTIFY:
            /* This is technically its own thing, but I don't know if it's ever a problem with how we've designed it */
            if (nv4->pgraph.notify_pending)
            {
                nv_log("WARNING: M2MF notification with notify_pending already set. param=0x%08x, method=0x%04x, grobj=0x%08x 0x%08x 0x%08x 0x%08x\n");
                nv_log("IF THIS BUILD WAS COMPILED WITH NV_LOG_ENABLE_ULTRA, YOU SHOULD SEE A CONTEXT BELOW");
                nv4_debug_ramin_print_context_info(param, context);
                nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_DOUBLE_NOTIFY);
                
                // disable
                nv4->pgraph.notify_pending = false;
                nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_DOUBLE_NOTIFY);
                /* may need to disable fifo in this state */
                return; 
            }

            nv_log("Method Execution: TODO: ACTUALLY IMPLEMENT M2MF!!!!");
            // set a notify as pending.
            nv4->pgraph.notifier = param; 
            nv4->pgraph.notify_pending = true; 
            break;                            
        default:
            warning("%s: Invalid or unimplemented method 0x%04x\n", nv4_class_names[context.class_id], method_id);
            nv4_pgraph_interrupt_invalid(NV4_PGRAPH_INTR_1_SOFTWARE_METHOD_PENDING);
            break;;
    }
}