/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          nv4 PVIDEO - Video Overlay
 *
 *
 *
 * Authors: Connor Hyde, <mario64crashed@gmail.com> I need a better email address ;^)
 *
 *          Copyright 2024-2025 starfrost
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/rom.h> // DEPENDENT!!!
#include <86box/video.h>
#include <86box/nv/vid_nv.h>
#include <86box/nv/vid_nv4.h>


nv_register_t pvideo_registers[] = {
    { NV4_PVIDEO_INTR, "PVIDEO - Interrupt Status", NULL, NULL},
    { NV4_PVIDEO_INTR_EN, "PVIDEO - Interrupt Enable", NULL, NULL,},
    { NV4_PVIDEO_FIFO_THRESHOLD, "PVIDEO - FIFO Fill Threshold", NULL, NULL},
    { NV4_PVIDEO_FIFO_BURST_LENGTH, "PVIDEO - FIFO Burst Length (1=32, 2=64, 3=128)", NULL, NULL},
    { NV4_PVIDEO_OVERLAY, "PVIDEO - Overlay Info (Bit0 = Video On, Bit4 = Key On, Bit8 = Format, 0=CCIR, 1=YUV2)", NULL, NULL },
    { NV_REG_LIST_END, NULL, NULL, NULL}, // sentinel value 
};

// ptimer init code
void nv4_pvideo_init(void)
{
    nv_log("Initialising PVIDEO...");

    nv_log("Done!\n");    
}

uint32_t nv4_pvideo_read(uint32_t address) 
{ 
    // before doing anything, check the subsystem enablement

    nv_register_t* reg = nv_get_register(address, pvideo_registers, sizeof(pvideo_registers)/sizeof(pvideo_registers[0]));
    uint32_t ret = 0x00;
    
    // todo: friendly logging
    
    nv_log_verbose_only("PVIDEO Read from 0x%08x", address);

    // if the register actually exists
    if (reg)
    {
        if (reg->friendly_name)
            nv_log_verbose_only(": %s\n", reg->friendly_name);
        else   
            nv_log_verbose_only("\n");

        // on-read function
        if (reg->on_read)
            ret = reg->on_read();
        else
        {   
            // Interrupt state:
            // Bit 0 - Notifier
            
            switch (reg->address)
            {
                case NV4_PVIDEO_INTR:
                    ret = nv4->pvideo.interrupt_status;
                    break;
                case NV4_PVIDEO_INTR_EN:
                    ret = nv4->pvideo.interrupt_enable;
                    break;
                case NV4_PVIDEO_FIFO_THRESHOLD:
                    ret = nv4->pvideo.fifo_threshold;
                    break;
                case NV4_PVIDEO_FIFO_BURST_LENGTH:
                    ret = nv4->pvideo.fifo_burst_size & 0x03;
                    break;
                case NV4_PVIDEO_OVERLAY:
                    ret = nv4->pvideo.overlay_settings & 0xFF;
                    break;
                
            }
        }

        if (reg->friendly_name)
            nv_log_verbose_only(": 0x%08x <- %s\n", ret, reg->friendly_name);
        else   
            nv_log_verbose_only("\n");
    }
    else
    {
        nv_log(": Unknown register read (address=0x%08x), returning 0x00\n", address);
    }

    return ret;
}

void nv4_pvideo_write(uint32_t address, uint32_t value) 
{
    // before doing anything, check the subsystem enablement
    nv_register_t* reg = nv_get_register(address, pvideo_registers, sizeof(pvideo_registers)/sizeof(pvideo_registers[0]));

    nv_log_verbose_only("PVIDEO Write 0x%08x -> 0x%08x\n", value, address);

    // if the register actually exists
    if (reg)
    {
        if (reg->friendly_name)
            nv_log_verbose_only(": %s\n", reg->friendly_name);
        else   
            nv_log_verbose_only("\n");

        // on-read function
        if (reg->on_write)
            reg->on_write(value);
        else
        {
            switch (reg->address)
            {
                // Interrupt state:
                // Bit 0 - Notifier

                case NV4_PVIDEO_INTR:
                    nv4->pvideo.interrupt_status &= ~value;
                    nv4_pmc_clear_interrupts();
                    break;
                case NV4_PVIDEO_INTR_EN:
                    nv4->pvideo.interrupt_enable = value & 0x00000001;
                    break;
                case NV4_PVIDEO_FIFO_THRESHOLD:
                    // only bits 6:3 matter
                    nv4->pvideo.fifo_threshold = ((value >> 3) & 0x0F) << 3;
                    break;
                case NV4_PVIDEO_FIFO_BURST_LENGTH:
                    nv4->pvideo.fifo_burst_size = value & 0x03;
                    break;
                case NV4_PVIDEO_OVERLAY:
                    nv4->pvideo.overlay_settings = value & 0xFF;
                    break;
            }
        }
    }
    else /* Completely unknown */
    {
        nv_log(": Unknown register write (address=0x%08x)\n", address);
    }
}