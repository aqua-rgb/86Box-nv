/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          nv4 PFIFO (FIFO for graphics object submission)
 *          PIO object submission
 *          Gray code conversion routines
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
#include <86box/dma.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/rom.h> // DEPENDENT!!!
#include <86box/video.h>
#include <86box/nv/vid_nv.h>
#include <86box/nv/vid_nv4.h>

//
// ****** PFIFO register list START ******
//

nv_register_t pfifo_registers[] = {
    { NV4_PFIFO_INTR, "PFIFO - Interrupt Status", NULL, NULL},
    { NV4_PFIFO_INTR_EN, "PFIFO - Interrupt Enable", NULL, NULL,},
    { NV4_PFIFO_DELAY_0, "PFIFO - DMA Delay/Retry Register", NULL, NULL},
    { NV4_PFIFO_DEBUG_0, "PFIFO - Debug 0", NULL, NULL, }, 
    { NV4_PFIFO_CONFIG_0, "PFIFO - Config 0", NULL, NULL, },
    { NV4_PFIFO_CONFIG_RAMFC, "PFIFO - RAMIN RAMFC Config", NULL, NULL },
    { NV4_PFIFO_CONFIG_RAMHT, "PFIFO - RAMIN RAMHT Config", NULL, NULL },
    { NV4_PFIFO_CONFIG_RAMRO, "PFIFO - RAMIN RAMRO Config", NULL, NULL },
    { NV4_PFIFO_CACHE_REASSIGNMENT, "PFIFO - Allow Cache Channel Reassignment", NULL, NULL },
    { NV4_PFIFO_CACHE0_PULL0, "PFIFO - Cache0 Puller Control", NULL, NULL},
    { NV4_PFIFO_CACHE1_PULL0, "PFIFO - Cache1 Puller Control"},
    { NV4_PFIFO_CACHE0_PULLER_CTX_STATE, "PFIFO - Cache0 Puller State1 (Is context clean?)", NULL, NULL},
    { NV4_PFIFO_CACHE1_PULL0, "PFIFO - Cache1 Puller State0", NULL, NULL},
    { NV4_PFIFO_CACHE1_PULLER_CTX_STATE, "PFIFO - Cache1 Puller State1 (Is context clean?)", NULL, NULL},
    { NV4_PFIFO_CACHE0_PUSH0, "PFIFO - Cache0 Access", NULL, NULL, },
    { NV4_PFIFO_CACHE1_PUSH0, "PFIFO - Cache1 Access", NULL, NULL, },
    { NV4_PFIFO_CACHE0_PUSH_CHANNEL_ID, "PFIFO - Cache0 Push Channel ID", NULL, NULL, },
    { NV4_PFIFO_CACHE1_PUSH_CHANNEL_ID, "PFIFO - Cache1 Push Channel ID", NULL, NULL, },
    { NV4_PFIFO_CACHE0_ERROR_PENDING, "PFIFO - Cache0 DMA Error Pending?", NULL, NULL, },
    { NV4_PFIFO_CACHE0_STATUS, "PFIFO - Cache0 Status", NULL, NULL},
    { NV4_PFIFO_CACHE1_STATUS, "PFIFO - Cache1 Status", NULL, NULL}, 
    { NV4_PFIFO_CACHE0_GET, "PFIFO - Cache0 Get", NULL, NULL },
    { NV4_PFIFO_CACHE0_CTX, "PFIFO - Cache0 Context", NULL, NULL },
    { NV4_PFIFO_CACHE1_GET, "PFIFO - Cache1 Get", NULL, NULL },
    { NV4_PFIFO_CACHE0_PUT, "PFIFO - Cache0 Put", NULL, NULL },
    { NV4_PFIFO_CACHE1_PUT, "PFIFO - Cache1 Put", NULL, NULL },
    //Cache1 exclusive stuff
    { NV4_PFIFO_CACHE1_DMA_CONFIG_0, "PFIFO - Cache1 DMA Config0"},
    { NV4_PFIFO_CACHE1_DMA_CONFIG_1, "PFIFO - Cache1 DMA Config1"},
    { NV4_PFIFO_CACHE1_DMA_CONFIG_2, "PFIFO - Cache1 DMA Config2"},
    { NV4_PFIFO_CACHE1_DMA_CONFIG_3, "PFIFO - Cache1 DMA Config3"},
    { NV4_PFIFO_CACHE1_DMA_STATUS, "PFIFO - Cache1 DMA Status - PROBABLY TRIGGERING DMA"},
    { NV4_PFIFO_CACHE1_DMA_TLB_PT_BASE, "PFIFO - Cache1 DMA Translation Lookaside Buffer - Pagetable Base"},
    { NV4_PFIFO_CACHE1_DMA_TLB_PTE, "PFIFO - Cache1 DMA Status"},
    { NV4_PFIFO_CACHE1_DMA_TLB_TAG, "PFIFO - Cache1 DMA Status"},
    //Runout
    { NV4_PFIFO_RUNOUT_GET, "PFIFO Runout Get Address [8:3 if 512b, otherwise 12:3]"},
    { NV4_PFIFO_RUNOUT_PUT, "PFIFO Runout Put Address [8:3 if 512b, otherwise 12:3]"},
    { NV4_PFIFO_RUNOUT_STATUS, "PFIFO Runout Status"},
    { NV_REG_LIST_END, NULL, NULL, NULL}, // sentinel value 
};

// PFIFO init code
void nv4_pfifo_init(void)
{
    nv_log("Initialising PFIFO...");

    nv_log("Done!\n");    
}

uint32_t nv4_pfifo_read(uint32_t address) 
{ 
    // before doing anything, check the subsystem enablement state

    if (!(nv4->pmc.enable >> NV4_PMC_ENABLE_PFIFO)
    & NV4_PMC_ENABLE_PFIFO_ENABLED)
    {
        nv_log("Repressing PFIFO read. The subsystem is disabled according to pmc_enable, returning 0\n");
        return 0x00;
    }

    uint32_t ret = 0x00;

    nv_register_t* reg = nv_get_register(address, pfifo_registers, sizeof(pfifo_registers)/sizeof(pfifo_registers[0]));

    // todo: friendly logging
    
    nv_log_verbose_only("PFIFO Read from 0x%08x", address);

    // if the register actually exists
    if (reg)
    {

        // on-read function
        if (reg->on_read)
            ret = reg->on_read();
        else
        {   
            // Interrupt state:
            // Bit 0 - Cache Error
            // Bit 4 - RAMRO Triggered
            // Bit 8 - RAMRO Overflow (too many invalid dma objects)
            // Bit 12 - DMA Pusher 
            // Bit 16 - DMA Page Table Entry (pagefault?)
            switch (reg->address)
            {
                case NV4_PFIFO_INTR:
                    ret = nv4->pfifo.interrupt_status;
                    break;
                case NV4_PFIFO_INTR_EN:
                    ret = nv4->pfifo.interrupt_enable;
                    break;
                case NV4_PFIFO_DELAY_0:
                    ret = nv4->pfifo.dma_delay_retry;
                    break;
                // Debug
                case NV4_PFIFO_DEBUG_0:
                    ret = nv4->pfifo.debug_0;
                    break;
                case NV4_PFIFO_CONFIG_0:
                    ret = nv4->pfifo.config_0;
                    break; 
                // Some of these may need to become functions.
                case NV4_PFIFO_CONFIG_RAMFC:
                    ret = nv4->pfifo.ramfc_config;
                    break;
                case NV4_PFIFO_CONFIG_RAMHT:
                    ret = nv4->pfifo.ramht_config;
                    break;
                case NV4_PFIFO_CONFIG_RAMRO:
                    ret = nv4->pfifo.ramro_config;
                    break;
                /* These automatically trigger pulls when 1 is written */
                case NV4_PFIFO_CACHE0_PULL0:
                    ret = nv4->pfifo.cache0_settings.pull0;
                    break;
                case NV4_PFIFO_CACHE1_PULL0:
                    ret = nv4->pfifo.cache1_settings.pull0;
                    break;
                case NV4_PFIFO_CACHE0_PULLER_CTX_STATE:
                    ret = (nv4->pfifo.cache0_settings.context_is_dirty) ? (1 << NV4_PFIFO_CACHE0_PULLER_CTX_STATE_DIRTY) : 0;
                    break;
                case NV4_PFIFO_CACHE1_PULLER_CTX_STATE:
                    ret = (nv4->pfifo.cache0_settings.context_is_dirty) ? (1 << NV4_PFIFO_CACHE0_PULLER_CTX_STATE_DIRTY) : 0;
                    break;
                /* Does this automatically push? */
                case NV4_PFIFO_CACHE0_PUSH0:
                    ret = nv4->pfifo.cache0_settings.push0;
                    break;
                case NV4_PFIFO_CACHE1_PUSH0:
                    ret = nv4->pfifo.cache1_settings.push0;
                    break; 
                case NV4_PFIFO_CACHE0_PUSH_CHANNEL_ID:
                    ret = nv4->pfifo.cache0_settings.channel;
                    break;
                case NV4_PFIFO_CACHE1_PUSH_CHANNEL_ID:
                    ret = nv4->pfifo.cache1_settings.channel;
                    break;
                case NV4_PFIFO_CACHE0_STATUS:  
                    // CACHE0 has only one entry so it can only ever be empty or full

                    if (nv4->pfifo.cache0_settings.put_address == nv4->pfifo.cache0_settings.get_address)
                        ret |= 1 << NV4_PFIFO_CACHE0_STATUS_EMPTY;
                    else
                        ret |= 1 << NV4_PFIFO_CACHE0_STATUS_FULL;
    
                    break;
                case NV4_PFIFO_CACHE1_STATUS:
                    // CACHE1 doesn't...

                    if (nv4->pfifo.cache1_settings.put_address == nv4->pfifo.cache1_settings.get_address)
                        ret |= 1 << NV4_PFIFO_CACHE1_STATUS_EMPTY;

                    // Check if Cache1 (0x7C bytes in size depending on gpu?) is full
                    // Based on how the drivers do it
                    if (!nv4_pfifo_cache1_num_free_spaces())
                        ret |= 1 << NV4_PFIFO_CACHE1_STATUS_FULL;
                    
                    if (nv4->pfifo.runout_put != nv4->pfifo.runout_get)
                        ret |= 1 << NV4_PFIFO_CACHE1_STATUS_RANOUT;

                    break;
                case NV4_PFIFO_CACHE0_PUT:
                    ret = nv4->pfifo.cache0_settings.put_address;
                    break;
                case NV4_PFIFO_CACHE0_GET:
                    ret = nv4->pfifo.cache0_settings.get_address;
                    break;
                case NV4_PFIFO_CACHE1_PUT:
                    ret = nv4->pfifo.cache1_settings.put_address;
                    break; 
                case NV4_PFIFO_CACHE1_GET: 
                    ret = nv4->pfifo.cache1_settings.get_address;
                    break;
                // Reassignment
                case NV4_PFIFO_CACHE_REASSIGNMENT:
                    ret = nv4->pfifo.cache_reassignment & 0x01; //1bit meaningful
                    break;
                // Cache1 exclusive stuff
                // Control
                case NV4_PFIFO_CACHE1_DMA_CONFIG_0:
                    ret = nv4->pfifo.cache1_settings.dma_state;
                    break; 
                case NV4_PFIFO_CACHE1_DMA_CONFIG_1:
                    ret = nv4->pfifo.cache1_settings.dma_length & (NV4_VRAM_SIZE_8MB) - 4; //MAX vram size
                    break;
                case NV4_PFIFO_CACHE1_DMA_CONFIG_2:
                    ret = nv4->pfifo.cache1_settings.dma_address;
                    break;
                case NV4_PFIFO_CACHE1_DMA_CONFIG_3:
                    if (nv4->nvbase.bus_generation == nv_bus_pci)
                        return NV4_PFIFO_CACHE1_DMA_CONFIG_3_TARGET_NODE_PCI;
                    else 
                        return NV4_PFIFO_CACHE1_DMA_CONFIG_3_TARGET_NODE_AGP;
                    break;
                case NV4_PFIFO_CACHE1_DMA_STATUS:
                    ret = nv4->pfifo.cache1_settings.dma_status;
                    break;
                case NV4_PFIFO_CACHE1_DMA_TLB_PT_BASE:
                    ret = nv4->pfifo.cache1_settings.dma_tlb_pt_base;
                    break;
                case NV4_PFIFO_CACHE1_DMA_TLB_PTE:
                    ret = nv4->pfifo.cache1_settings.dma_tlb_pte;
                    break;
                case NV4_PFIFO_CACHE1_DMA_TLB_TAG:
                    ret = nv4->pfifo.cache1_settings.dma_tlb_tag;
                    break;
                // Runout
                case NV4_PFIFO_RUNOUT_GET:
                    ret = nv4->pfifo.runout_get;
                    break;
                case NV4_PFIFO_RUNOUT_PUT:
                    ret = nv4->pfifo.runout_put;
                    break;
                case NV4_PFIFO_RUNOUT_STATUS:
                    if (nv4->pfifo.runout_put == nv4->pfifo.runout_get)
                        ret |= 1 << NV4_PFIFO_RUNOUT_STATUS_EMPTY; /* good news */
                    else 
                        ret |= 1 << NV4_PFIFO_RUNOUT_STATUS_RANOUT; /* bad news */

                    /* TODO: the following code sucks (move to a functio?) */

                    uint32_t new_size_ramro = ((nv4->pfifo.ramro_config >> NV4_PFIFO_CONFIG_RAMRO_SIZE) & 0x01);

                    if (new_size_ramro == 0)
                        new_size_ramro = 0x200;
                    else if (new_size_ramro == 1)
                        new_size_ramro = 0x2000;
                    
                    // WTF?
                    if (nv4->pfifo.runout_put + 0x08 & (new_size_ramro - 0x08) == nv4->pfifo.runout_get)
                        ret |= 1 << NV4_PFIFO_RUNOUT_STATUS_FULL; /* VERY BAD news */

                    break;
                
                /* Cache1 is handled below - cache0 only has one entry */
                case NV4_PFIFO_CACHE0_CTX:
                    ret = nv4->pfifo.cache0_settings.context[0];
                    break;
                
            }
        }

        if (reg->friendly_name)
            nv_log_verbose_only(": 0x%08x <- %s\n", ret, reg->friendly_name);
        else   
            nv_log_verbose_only("\n");
    }
    /* Handle some special memory areas */
    else if (address >= NV4_PFIFO_CACHE1_CTX_START && address <= NV4_PFIFO_CACHE1_CTX_END)
    {
        uint32_t ctx_entry_id = ((address - NV4_PFIFO_CACHE1_CTX_START) / 16) % 8;
        ret = nv4->pfifo.cache1_settings.context[ctx_entry_id];

        nv_log_verbose_only("PFIFO Cache1 CTX Read Entry=%d Value=0x%04x\n", ctx_entry_id, ret);
    }
    /* Direct cache read  stuff */
    else if (address >= NV4_PFIFO_CACHE0_METHOD_START && address <= NV4_PFIFO_CACHE0_METHOD_END)
    {
        nv_log_verbose_only("PFIFO Cache0 Read\n");

        // See if we want the object name or the channel/subchannel information.
        if (address & 4)
        {
            nv_log_verbose_only("Data=0x%08x\n", nv4->pfifo.cache0_entry.data);

            return nv4->pfifo.cache0_entry.data;
        }
        else
        {
            uint32_t final = nv4->pfifo.cache0_entry.method | (nv4->pfifo.cache0_entry.subchannel << NV4_PFIFO_CACHE1_METHOD_SUBCHANNEL);

            nv_log_verbose_only("Param (subchannel=15:13, method=12:2)=0x%08x\n", final);


            return final;
        }
    }
    else if (address >= NV4_PFIFO_CACHE1_METHOD_START && address <= NV4_PFIFO_CACHE1_METHOD_END)
    {       
        // Not sure if REV C changes this. It should...
        uint32_t slot = 0;
        
        // shift right by 3, convert from address, to slot.
        if (nv4->nvbase.gpu_revision == NV4_PCI_CFG_REVISION_C00)
            slot = (address >> 3) & 0x3F;
        else 
            slot = (address >> 3) & 0x1F; 
            
        nv_log_verbose_only("PFIFO Cache1 Read slot=%d", slot);

        // See if we want the object name or the channel/subchannel information.
        if (address & 4)
        {
            nv_log_verbose_only("Data=0x%08x\n", nv4->pfifo.cache1_entries[slot].data);
            return nv4->pfifo.cache1_entries[slot].data;
        }
        else
        {
            uint32_t final = nv4->pfifo.cache1_entries[slot].method | (nv4->pfifo.cache1_entries[slot].subchannel << NV4_PFIFO_CACHE1_METHOD_SUBCHANNEL);
            nv_log_verbose_only("Param (subchannel=15:13, method=12:2)=0x%08x\n", final);
            return final;
        }
            
    }
    else
    {
        nv_log(": Unknown register read (address=0x%08x), returning 0x00\n", address);
    }

    return ret; 
}

void nv4_pfifo_trigger_dma_if_required(void)
{
    // Not a thing for cache0
    
    bool cache1_dma = false;

    /* Check that DMA is enabled */
    if (nv4->pfifo.cache1_settings.dma_state
    && nv4->pfifo.cache1_settings.dma_enabled)
    {
        uint32_t bytes_to_send = nv4->pfifo.cache1_settings.dma_length;
        uint32_t where_to_send = nv4->pfifo.cache1_settings.dma_address;
        uint32_t target_node = nv4->pfifo.cache1_settings.dma_target_node; //2=pci, 3=agp. What does this even do

        /* Pagetable information */
        uint32_t tlb_pt_base = nv4->pfifo.cache1_settings.dma_tlb_pt_base;
        uint32_t tlb_pt_entry = nv4->pfifo.cache1_settings.dma_tlb_pte;
        uint32_t tlb_pt_tag = nv4->pfifo.cache1_settings.dma_tlb_tag; // 0xFFFFFFFF usually?

        /* PUSH - System to GPU (?) */
        if (nv4->pfifo.cache1_settings.push0)
        {
            /* PULL - GPU to System */
            nv_log("Initiating System to NV DMA - Probably we are trying to notify\n");
        }
        else if (nv4->pfifo.cache1_settings.pull0)
        {
            /* PULL - GPU to System */
            nv_log("Initiating NV to System DMA - Probably we are trying to notify\n");
        }

    }
}

void nv4_pfifo_write(uint32_t address, uint32_t val) 
{
    // before doing anything, check the subsystem enablement

    if (!(nv4->pmc.enable >> NV4_PMC_ENABLE_PFIFO)
    & NV4_PMC_ENABLE_PFIFO_ENABLED)
    {
        nv_log("Repressing PFIFO write. The subsystem is disabled according to pmc_enable\n");
        return;
    }

    nv_register_t* reg = nv_get_register(address, pfifo_registers, sizeof(pfifo_registers)/sizeof(pfifo_registers[0]));

    nv_log_verbose_only("PFIFO Write 0x%08x -> 0x%08x", val, address);

    // if the register actually exists
    if (reg)
    {
        // on-read function
        if (reg->on_write)
            reg->on_write(val);
        else
        {
            switch (reg->address)
            {
                // Interrupt state:
                // Bit 0 - Cache Error
                // Bit 4 - RAMRO Triggered
                // Bit 8 - RAMRO Overflow (too many invalid dma objects)
                // Bit 12 - DMA Pusher 
                // Bit 16 - DMA Page Table Entry (pagefault?)
                case NV4_PFIFO_INTR:
                    nv4->pfifo.interrupt_status &= ~val;
                    nv4_pmc_clear_interrupts();

                    // update the internal cache error state
                    if (!nv4->pfifo.interrupt_status & NV4_PFIFO_INTR_CACHE_ERROR)
                        nv4->pfifo.debug_0 &= ~NV4_PFIFO_INTR_CACHE_ERROR;
                    break;
                case NV4_PFIFO_INTR_EN:
                    nv4->pfifo.interrupt_enable = val & 0x00011111;
                    nv4_pmc_handle_interrupts(true);
                    break;
                case NV4_PFIFO_DELAY_0:
                    nv4->pfifo.dma_delay_retry = val;
                    break;
                case NV4_PFIFO_CONFIG_0:
                    nv4->pfifo.config_0 = val;
                    break;

                case NV4_PFIFO_CONFIG_RAMHT:
                    nv4->pfifo.ramht_config = val;
// This code sucks a bit fix it later
#ifdef ENABLE_NV_LOG
                    uint32_t new_size_ramht = ((val >> 16) & 0x03);

                    if (new_size_ramht == 0)
                        new_size_ramht = 0x1000;
                    else if (new_size_ramht == 1)
                        new_size_ramht = 0x2000;
                    else if (new_size_ramht == 2)
                        new_size_ramht = 0x4000;
                    else if (new_size_ramht == 3)
                        new_size_ramht = 0x8000;  

                    nv_log("RAMHT Reconfiguration\n"
                    "Base Address in RAMIN: %d\n"
                    "Size: 0x%08x bytes\n", ((nv4->pfifo.ramht_config >> NV4_PFIFO_CONFIG_RAMHT_BASE_ADDRESS) & 0x0F) << 12, new_size_ramht); 
#endif
                    break;
                case NV4_PFIFO_CONFIG_RAMFC:
                    nv4->pfifo.ramfc_config = val;

                    nv_log("RAMFC Reconfiguration\n"
                    "Base Address in RAMIN: %d\n", ((nv4->pfifo.ramfc_config >> NV4_PFIFO_CONFIG_RAMFC_BASE_ADDRESS) & 0x7F) << 9); 
                    break;
                case NV4_PFIFO_CONFIG_RAMRO:
                    nv4->pfifo.ramro_config = val;

                    uint32_t new_size_ramro = ((val >> NV4_PFIFO_CONFIG_RAMRO_SIZE) & 0x01);

                    if (new_size_ramro == 0)
                        new_size_ramro = 0x200;
                    else if (new_size_ramro == 1)
                        new_size_ramro = 0x2000;
                    
                    nv_log("RAMRO Reconfiguration\n"
                    "Base Address in RAMIN: %d\n"
                    "Size: 0x%08x bytes\n", ((nv4->pfifo.ramro_config >> NV4_PFIFO_CONFIG_RAMRO_BASE_ADDRESS) & 0x7F) << 9, new_size_ramro); 
                    break;
                case NV4_PFIFO_DEBUG_0:
                    nv4->pfifo.debug_0 = val;
                    break;
                // Reassignment
                case NV4_PFIFO_CACHE_REASSIGNMENT:
                    nv4->pfifo.cache_reassignment = val & 0x01; //1bit meaningful
                    break;
                // Control - these can trigger pulls
                case NV4_PFIFO_CACHE0_PULL0:
                    nv4->pfifo.cache0_settings.pull0 = val; // 8bits meaningful
                    
                    if (nv4->pfifo.cache0_settings.pull0 & (1 >> NV4_PFIFO_CACHE0_PULL0_ENABLED))
                        nv4_pfifo_cache0_pull();

                    break;
                case NV4_PFIFO_CACHE1_PULL0:
                    nv4->pfifo.cache1_settings.pull0 = val; // 8bits meaningful
                    
                    if (nv4->pfifo.cache1_settings.pull0 & (1 >> NV4_PFIFO_CACHE1_PULL0_ENABLED))
                        nv4_pfifo_cache1_pull();

                    break;
                case NV4_PFIFO_CACHE0_PULLER_CTX_STATE:
                    nv4->pfifo.cache0_settings.context_is_dirty = (val >> NV4_PFIFO_CACHE0_PULLER_CTX_STATE_DIRTY) & 0x01;
                    break;
                case NV4_PFIFO_CACHE1_PULLER_CTX_STATE:
                    nv4->pfifo.cache1_settings.context_is_dirty = (val >> NV4_PFIFO_CACHE0_PULLER_CTX_STATE_DIRTY) & 0x01;
                    break;
                case NV4_PFIFO_CACHE0_PUSH0:
                    nv4->pfifo.cache0_settings.push0 = val;
                    break;
                case NV4_PFIFO_CACHE1_PUSH0:
                    nv4->pfifo.cache1_settings.push0 = val;
                    break; 
                case NV4_PFIFO_CACHE0_PUSH_CHANNEL_ID:
                    nv4->pfifo.cache0_settings.channel = val;
                    break;
                case NV4_PFIFO_CACHE1_PUSH_CHANNEL_ID:
                    nv4->pfifo.cache1_settings.channel = val;
                    break;
                // CACHE0_STATUS and CACHE1_STATUS are not writable
                // DMA configuration
                case NV4_PFIFO_CACHE1_DMA_CONFIG_0:
                    nv4->pfifo.cache1_settings.dma_state = val;
                    break; 
                case NV4_PFIFO_CACHE1_DMA_CONFIG_1:
                    nv4->pfifo.cache1_settings.dma_length = val;
                    break;
                case NV4_PFIFO_CACHE1_DMA_CONFIG_2:
                    nv4->pfifo.cache1_settings.dma_address = val;
                    break;
                case NV4_PFIFO_CACHE1_DMA_STATUS:
                    nv4->pfifo.cache1_settings.dma_status = val;
                    break;
                case NV4_PFIFO_CACHE1_DMA_TLB_PT_BASE:
                    nv4->pfifo.cache1_settings.dma_tlb_pt_base = val;
                    break;
                case NV4_PFIFO_CACHE1_DMA_TLB_PTE:
                    nv4->pfifo.cache1_settings.dma_tlb_pte = val;
                    break;
                case NV4_PFIFO_CACHE1_DMA_TLB_TAG:
                    nv4->pfifo.cache1_settings.dma_tlb_tag = val;
                    break;
                /* Put and Get addresses */
                case NV4_PFIFO_CACHE0_PUT:
                    nv4->pfifo.cache0_settings.put_address = val;
                    break;
                case NV4_PFIFO_CACHE0_GET:
                    nv4->pfifo.cache0_settings.get_address = val;
                    break;
                case NV4_PFIFO_CACHE1_PUT:
                    nv4->pfifo.cache1_settings.put_address = val;
                    break; 
                case NV4_PFIFO_CACHE1_GET: 
                    nv4->pfifo.cache1_settings.get_address = val;
                    break;
                case NV4_PFIFO_RUNOUT_GET:
                    
                    uint32_t size_get = ((nv4->pfifo.ramro_config >> NV4_PFIFO_CONFIG_RAMRO_SIZE) & 0x01);

                    if (size_get == 0) //512b
                        nv4->pfifo.runout_get = val & (NV4_RAMIN_RAMRO_SIZE_0 - 0x07);
                    else 
                        nv4->pfifo.runout_get = val & (NV4_RAMIN_RAMRO_SIZE_1 - 0x07);
                    break;
                case NV4_PFIFO_RUNOUT_PUT:
                    uint32_t size_put = ((nv4->pfifo.ramro_config >> NV4_PFIFO_CONFIG_RAMRO_SIZE) & 0x01);

                    if (size_put == 0) //512b
                        nv4->pfifo.runout_put = val & (NV4_RAMIN_RAMRO_SIZE_0 - 0x07);
                    else 
                        nv4->pfifo.runout_put = val & (NV4_RAMIN_RAMRO_SIZE_1 - 0x07);

                    break;
                /* Cache1 is handled below */
                case NV4_PFIFO_CACHE0_CTX:
                    nv4->pfifo.cache0_settings.context[0] = val;
                    break;
            }
        }

        if (reg->friendly_name)
            nv_log_verbose_only(": %s\n", reg->friendly_name);
        else   
            nv_log_verbose_only("\n");
    }
    else if (address >= NV4_PFIFO_CACHE0_METHOD_START && address <= NV4_PFIFO_CACHE0_METHOD_END)
    {
        nv_log_verbose_only("PFIFO Cache0 Write\n");

        // 3104 always written after 3100
        if (address & 4)
        {   
            nv_log_verbose_only("Name = 0x%08x\n", val);
            nv4->pfifo.cache0_entry.data = val;
            nv4_pfifo_cache0_pull(); // immediately pull out
        }
        else
        {
            nv4->pfifo.cache0_entry.method = (val & 0x1FFC);
            nv4->pfifo.cache0_entry.subchannel = (val >> NV4_PFIFO_CACHE1_METHOD_SUBCHANNEL) & 0x07;
            nv_log_verbose_only("Subchannel = 0x%08x, method = 0x%04x\n", nv4->pfifo.cache0_entry.subchannel, nv4->pfifo.cache0_entry.method);
        }

    }
    else if (address >= NV4_PFIFO_CACHE1_METHOD_START && address <= NV4_PFIFO_CACHE1_METHOD_END)
    {       
        // Not sure if REV C changes this. It should...
        uint32_t slot = 0;
        
        if (nv4->nvbase.gpu_revision == NV4_PCI_CFG_REVISION_C00)
            slot = (address >> 3) & 0x3F;
        else 
            slot = (address >> 3) & 0x1F; 

        uint32_t real_entry = nv4_pfifo_cache1_normal2gray(slot);

        nv_log_verbose_only("Cache1 Write Slot %d (Gray code)", real_entry);

        // See if we want the object name or the channel/subchannel information.
        if (address & 4)
        {
            nv_log_verbose_only("Name = 0x%08x\n", val);
            nv4->pfifo.cache1_entries[real_entry].data = val;
        }
        else
        {
            nv4->pfifo.cache1_entries[real_entry].method = (val & 0x1FFC);
            nv4->pfifo.cache1_entries[real_entry].subchannel = (val >> NV4_PFIFO_CACHE1_METHOD_SUBCHANNEL) & 0x07;
            nv_log_verbose_only("Subchannel = 0x%08x, method = 0x%04x\n", nv4->pfifo.cache1_entries[real_entry].subchannel, nv4->pfifo.cache1_entries[real_entry].method);
        }
    }
    /* Handle some special memory areas */
    else if (address >= NV4_PFIFO_CACHE1_CTX_START && address <= NV4_PFIFO_CACHE1_CTX_END)
    {
        uint32_t ctx_entry_id = ((address - NV4_PFIFO_CACHE1_CTX_START) / 16) % 8;
        nv4->pfifo.cache1_settings.context[ctx_entry_id] = val;

        nv_log_verbose_only("PFIFO Cache1 CTX Write Entry=%d value=0x%04x\n", ctx_entry_id, val);
    }
    else /* Completely unknown */
    {
        nv_log(": Unknown register write (address=0x%08x)\n", address);
    }

    /* Trigger DMA for notifications if we need to */
    nv4_pfifo_trigger_dma_if_required();
}


/* 
https://en.wikipedia.org/wiki/Gray_code
WHY?????? IT'S NOT A TELEGRAPH IT'S A GPU?????

Convert from a normal number to a total insanity number which is only used in PFIFO CACHE1 for ungodly and totally unknowable reasons 
(Possibly it just makes it easier to implement in logic)

I decided to use a lookup table to save everyone's time, also the numbers generated from the function
that existed here before didn't make any sense
*/

#define NV4_GRAY_TABLE_NUM_ENTRIES 64

uint8_t nv4_pfifo_cache1_gray_code_table[NV4_GRAY_TABLE_NUM_ENTRIES] = {
    0b000000, 0b000001, 0b000011, 0b000010, 0b000110, 0b000111, 0b000101, 0b000100, //0x07
    0b001100, 0b001101, 0b001111, 0b001110, 0b001010, 0b001011, 0b001001, 0b001000, //0x0F
    0b011000, 0b011001, 0b011011, 0b011010, 0b011110, 0b011111, 0b011101, 0b011100, //0x17
    0b010100, 0b010101, 0b010111, 0b010110, 0b010010, 0b010011, 0b010001, 0b010000, //0x1F
    0b110000, 0b110001, 0b110011, 0b110010, 0b110110, 0b110111, 0b110101, 0b110100, //0x27
    0b111100, 0b111101, 0b111111, 0b111110, 0b111010, 0b111011, 0b111001, 0b111000, //0x2F
    0b101000, 0b101001, 0b101011, 0b101010, 0b101110, 0b101111, 0b101101, 0b101100, //0x37
    0b100100, 0b100101, 0b100111, 0b100110, 0b100010, 0b100011, 0b100001, 0b100000  //0x3F
};

/* The function is called up to hundreds of thousands of times per second, it's too slow to do anything else */
uint8_t nv4_pfifo_cache1_binary_code_table[NV4_GRAY_TABLE_NUM_ENTRIES] =
{
    0x00, 0x01, 0x03, 0x02, 0x07, 0x06, 0x04, 0x05, // 0x07 (0)
    0x0F, 0x0E, 0x0C, 0x0D, 0x08, 0x09, 0x0B, 0x0A, // 0x0F (1000)
    0x1F, 0x1E, 0x1C, 0x1D, 0x18, 0x19, 0x1B, 0x1A, // 0x17 (10000)
    0x10, 0x11, 0x13, 0x12, 0x17, 0x16, 0x14, 0x15, // 0x1F (11000)
    0x3F, 0x3E, 0x3C, 0x3D, 0x38, 0x39, 0x3B, 0x3A, // 0x27 (100000)
    0x30, 0x31, 0x33, 0x32, 0x37, 0x36, 0x34, 0x35, // 0x2F (101000)
    0x20, 0x21, 0x23, 0x22, 0x27, 0x26, 0x24, 0x25, // 0x37 (110000)
    0x2F, 0x2E, 0x2C, 0x2D, 0x28, 0x29, 0x2B, 0x2A, // 0X3f (111000)
};

uint32_t nv4_pfifo_cache1_normal2gray(uint32_t val)
{
    return nv4_pfifo_cache1_gray_code_table[val];
}

/* 
Back to sanity
*/
uint32_t nv4_pfifo_cache1_gray2normal(uint32_t val)
{
    return nv4_pfifo_cache1_binary_code_table[val];
}

/* 
You can't push into cache0 on the real hardware, but it's not practically done because Cache0 is meant to be reserved for software objects,
NV_USER writes always go to CACHE1
*/

// Pulls graphics objects OUT of cache0
void nv4_pfifo_cache0_pull(void)
{
    // Do nothing if PFIFO CACHE0 is disabled
    if (!nv4->pfifo.cache0_settings.pull0 & (1 >> NV4_PFIFO_CACHE0_PULL0_ENABLED))
        return; 

    // Do nothing if there is nothing in cache0 to pull
    if (nv4->pfifo.cache0_settings.put_address == nv4->pfifo.cache0_settings.get_address)
        return;

    // There is only one entry for cache0 
    uint8_t current_channel = nv4->pfifo.cache0_settings.channel;
    uint8_t current_subchannel = nv4->pfifo.cache0_entry.subchannel;
    uint32_t current_param = nv4->pfifo.cache0_entry.data;
    uint16_t current_method = nv4->pfifo.cache0_entry.method;

    // i.e. there is no method in cache0, so we have to find the object.
    if (!current_method)
    {
        // flip the get address over
        nv4->pfifo.cache0_settings.get_address ^= 0x04;

        if (!nv4_ramin_find_object(current_param, 0, current_channel, current_subchannel))
            return; // interrupt was fired, and we went to ramro
    }

    uint32_t current_context = nv4->pfifo.cache0_settings.context[0]; // only 1 entry for CACHE0 so basically ignore the other context entries?
    uint8_t class_id = ((nv4_ramin_context_t*)&current_context)->class_id;

    // Tell the CPU if we found a software method
    if (!(current_context & 0x800000))
    {
        nv_log("The object in CACHE0 is a software object\n");

        nv4->pfifo.cache0_settings.pull0 |= NV4_PFIFO_CACHE0_PULL0_SOFTWARE_METHOD;
        nv4->pfifo.cache0_settings.pull0 &= ~NV4_PFIFO_CACHE0_PULL0_ENABLED;
        nv4_pfifo_interrupt(NV4_PFIFO_INTR_CACHE_ERROR, true);
        return;
    }

    // Is this needed?
    nv4->pfifo.cache0_settings.get_address ^= 0x04;

    #ifndef RELEASE_BUILD

    nv_log_verbose_only("***** DEBUG: CACHE0 PULLED ****** Contextual information below\n");

            
    nv4_ramin_context_t context_structure = *(nv4_ramin_context_t*)&current_context;

    nv4_debug_ramin_print_context_info(current_param, context_structure);
    
    nv4_pgraph_submit(current_param, current_method, current_channel, current_subchannel, class_id & 0x1F, context_structure);
    #endif

}

void nv4_pfifo_context_switch(uint32_t new_channel)
{
    /* Send our contexts to RAMFC. Load the new ones from RAMFC. */
    if (new_channel >= NV4_DMA_CHANNELS)
        fatal("nv4_pfifo_context_switch: Tried to switch to invalid dma channel");

    uint16_t ramfc_base = nv4->pfifo.ramfc_config >> NV4_PFIFO_CONFIG_RAMFC_BASE_ADDRESS & 0xF;

}

// NV_USER writes go here!
// Pushes graphics objects into cache1
void nv4_pfifo_cache1_push(uint32_t addr, uint32_t param)
{
    bool oh_shit = false;   // RAMRO needed
    nv4_ramin_ramro_reason oh_shit_reason = 0x00; // It's all good for now

    // bit 23 of a ramin dword means it's a write...
    uint32_t new_address = 0;

    uint32_t method_offset = (addr & 0x1FFC); // size of dma object is 0x2000 and some universal methods are implemented at this point, like free
    
    // Up to 128 per envytools?
    uint32_t channel = (addr >> NV4_OBJECT_SUBMIT_CHANNEL) & 0x7F;
    uint32_t subchannel = (addr >> NV4_OBJECT_SUBMIT_SUBCHANNEL) & (NV4_DMA_CHANNELS - 1);

    // first make sure there is even any cache available
    if (!nv4->pfifo.cache1_settings.push0)
    {
        oh_shit = true; 
        oh_shit_reason = nv4_runout_reason_no_cache_available;
        new_address |= (nv4_runout_reason_no_cache_available << NV4_PFIFO_RUNOUT_RAMIN_ERR);

    }
    
    // Check if runout is full
    if (nv4->pfifo.runout_get != nv4->pfifo.runout_put)
    {
        oh_shit = true;
        oh_shit_reason = nv4_runout_reason_cache_ran_out; // ? really ? I guess this means we already ran out..
        new_address |= (nv4_runout_reason_cache_ran_out << NV4_PFIFO_RUNOUT_RAMIN_ERR);
    }

    if (!nv4_pfifo_cache1_num_free_spaces())
    {
        oh_shit = true;
        oh_shit_reason = nv4_runout_reason_free_count_overrun;
        new_address |= (nv4_runout_reason_free_count_overrun << NV4_PFIFO_RUNOUT_RAMIN_ERR);
    }

    // 0x0 is used for creating the object.
    if (method_offset > 0 && method_offset < 0x100)
    {
        // Reserved NVIDIA Objects
        oh_shit = true; 
        oh_shit_reason = nv4_runout_reason_reserved_access;
        new_address |= (nv4_runout_reason_reserved_access << NV4_PFIFO_RUNOUT_RAMIN_ERR);

    }

    // Now check for context switching

    if (channel != nv4->pfifo.cache1_settings.channel)
    {
        // Cache reassignment required
        if (!nv4->pfifo.cache_reassignment 
        || (nv4->pfifo.cache1_settings.get_address != nv4->pfifo.cache1_settings.get_address))
        {
            oh_shit = true;
            oh_shit_reason = nv4_runout_reason_no_cache_available;
            new_address |= (nv4_runout_reason_no_cache_available << NV4_PFIFO_RUNOUT_RAMIN_ERR);
        }

        nv4_pfifo_context_switch(channel);
    }

    // Did we fuck up?
    if (oh_shit)
    {
        nv_log("OH CRAP: Runout Error=%d Channel=%d Subchannel=%d Method=0x%04x", 
            oh_shit_reason, channel, subchannel, method_offset);
         
        nv4_ramro_write(nv4->pfifo.runout_put, new_address);
        nv4_ramro_write(nv4->pfifo.runout_put + 4, param);

        nv4->pfifo.runout_put += 0x08;

        uint32_t ramro_size = (nv4->pfifo.ramro_config >> NV4_PFIFO_CONFIG_RAMRO_SIZE) & 0x01;

        /* Make sure it's valid */
        switch (ramro_size)
        {
            case 0:
                nv4->pfifo.runout_put &= (NV4_RAMIN_RAMRO_SIZE_0 - 0x07);
                break; 
            case 1:
                nv4->pfifo.runout_put &= (NV4_RAMIN_RAMRO_SIZE_1 - 0x07);
                break; 
        }

        //Fire the interrupt. Also the very bad interrupt...
        if (nv4->pfifo.runout_get == nv4->pfifo.runout_put)
            nv4_pfifo_interrupt(NV4_PFIFO_INTR_RUNOUT_OVERFLOW, true);
        else    
            nv4_pfifo_interrupt(NV4_PFIFO_INTR_RUNOUT, true);

        return;
    }

    // We didn't. Let's put it in CACHE1
    uint32_t current_put_index = nv4->pfifo.cache1_settings.put_address >> 2;
    nv4->pfifo.cache1_entries[current_put_index].subchannel = subchannel;
    nv4->pfifo.cache1_entries[current_put_index].method = method_offset;
    nv4->pfifo.cache1_entries[current_put_index].data = param;

    // now we have to recalculate the cache1 put address
    uint32_t next_put_address = nv4_pfifo_cache1_gray2normal(current_put_index);
    next_put_address++;

    next_put_address &= (NV4_PFIFO_CACHE1_SIZE_REV_AB - 1);

    nv4->pfifo.cache1_settings.put_address = nv4_pfifo_cache1_normal2gray(next_put_address) << 2;

    nv_log_verbose_only("Submitted object [PIO]: Channel %d.%d, Parameter 0x%08x, Method ID 0x%04x (Put Address is now %d)\n",
         channel, subchannel, param, method_offset, nv4->pfifo.cache1_settings.put_address);
   
    // Now we're done. Phew!
}

// Pulls graphics objects OUT of cache1
void nv4_pfifo_cache1_pull(void)
{
    // Do nothing if PFIFO CACHE1 is disabled
    if (!nv4->pfifo.cache1_settings.pull0 & (1 >> NV4_PFIFO_CACHE1_PULL0_ENABLED))
        return; 

    // Do nothing if there is nothing in cache1 to pull
    if (nv4->pfifo.cache1_settings.put_address == nv4->pfifo.cache1_settings.get_address)
        return;

    uint32_t get_index = nv4->pfifo.cache1_settings.get_address >> 2; // 32 bit aligned probably

    uint8_t current_channel = nv4->pfifo.cache1_settings.channel;
    uint8_t current_subchannel = nv4->pfifo.cache1_entries[get_index].subchannel;
    uint32_t current_param = nv4->pfifo.cache1_entries[get_index].data;
    uint16_t current_method = nv4->pfifo.cache1_entries[get_index].method;
  
    // NV_ROOT
    if (!current_method)
    {
        if (!nv4_ramin_find_object(current_param, 1, current_channel, current_subchannel))
            return; // interrupt was fired, and we went to ramro
    }

    uint32_t current_context = nv4->pfifo.cache1_settings.context[current_subchannel]; // get the current subchannel

    uint8_t class_id = ((nv4_ramin_context_t*)&current_context)->class_id;

    // Tell the CPU if we found a software method
    //bit23 unset=software
    //bit23 set=hardware
    if (!(current_context & 0x800000))
    {
        nv_log_verbose_only("The object in CACHE1 is a software object\n");

        nv4->pfifo.cache1_settings.pull0 |= NV4_PFIFO_CACHE0_PULL0_SOFTWARE_METHOD;
        nv4->pfifo.cache1_settings.pull0 &= ~NV4_PFIFO_CACHE0_PULL0_ENABLED;
        nv4_pfifo_interrupt(NV4_PFIFO_INTR_CACHE_ERROR, true);
        return;
    }

    // start by incrementing
    uint32_t next_get_address = nv4_pfifo_cache1_gray2normal(get_index) + 1;

    next_get_address &= (NV4_PFIFO_CACHE1_SIZE_REV_AB - 1);

    // Is this needed?
    nv4->pfifo.cache1_settings.get_address = nv4_pfifo_cache1_normal2gray(next_get_address) << 2;

    #ifndef RELEASE_BUILD

    nv_log_verbose_only("***** DEBUG: CACHE1 PULLED ****** Contextual information below\n");


    nv4_ramin_context_t context_structure = *(nv4_ramin_context_t*)&current_context;

    nv4_debug_ramin_print_context_info(current_param, context_structure);
    #endif
    
    nv4_pgraph_submit(current_param, current_method, current_channel, current_subchannel, class_id & 0x1F, context_structure);
    

    //Todo: finish it
}

// THIS IS PER SUBCHANNEL!
uint32_t nv4_pfifo_cache1_num_free_spaces(void)
{
    // get the index

    uint32_t get_index = nv4->pfifo.cache1_settings.get_address >> 2;
    uint32_t put_index = nv4->pfifo.cache1_settings.put_address >> 2;
    
    uint32_t real_get_address = nv4_pfifo_cache1_gray2normal(get_index) << 2;
    uint32_t real_put_address = nv4_pfifo_cache1_gray2normal(put_index) << 2;
    
    // There is no hope of being able to understand it. Nobody can understand
    return (real_get_address - real_put_address - 4) & 0x7C; // there are 64 entries what
}