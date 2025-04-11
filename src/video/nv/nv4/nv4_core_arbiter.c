/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          The insane NV4 MMIO arbiter.
 *          Writes to ALL sections of the GPU based on the write position
 *          All writes are internally considered to be 32-bit! Be careful...
 * 
 *          Also handles interrupt dispatch
 *
 *          
 *
 * Authors: Connor Hyde, <mario64crashed@gmail.com> I need a better email address ;^)
 *
 *          Copyright 2024-2025 starfrost
 */

// STANDARD NV4 includes
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

// Gets a register...
// move this somewhere else when we have more models
nv_register_t* nv_get_register(uint32_t address, nv_register_t* register_list, uint32_t num_regs)
{
    for (int32_t reg_num = 0; reg_num < num_regs; reg_num++)
    {
        if (register_list[reg_num].address == NV_REG_LIST_END)
            break; //unimplemented

        if (register_list[reg_num].address == address)
            return &register_list[reg_num];
    }

    return NULL;
}

// Arbitrates an MMIO read
uint32_t nv4_mmio_arbitrate_read(uint32_t address)
{
    // sanity check
    if (!nv4)
        return 0x00; 

    uint32_t ret = 0x00;

    // note: some registers are byte aligned not dword aligned
    // only very few are though, so they can be handled specially, using the register list most likely
    address &= 0xFFFFFC;

    // gigantic set of if statements to send the write to the right subsystem
    if (address >= NV4_PMC_START && address <= NV4_PMC_END)
        ret = nv4_pmc_read(address);
    else if (address >= NV4_CIO_START && address <= NV4_CIO_END)
        ret = nv4_cio_read(address);
    else if (address >= NV4_PBUS_PCI_START && address <= NV4_PBUS_PCI_END)
        ret = nv4_pci_read(0x00, address & 0xFF, NULL);
    else if (address >= NV4_PBUS_START && address <= NV4_PBUS_END)
        ret = nv4_pbus_read(address);
    else if (address >= NV4_PFIFO_START && address <= NV4_PFIFO_END)
        ret = nv4_pfifo_read(address);
    else if (address >= NV4_PFB_START && address <= NV4_PFB_END)
        ret = nv4_pfb_read(address);
    else if (address >= NV4_PRM_START && address <= NV4_PRM_END)
        ret = nv4_prm_read(address);
    else if (address >= NV4_PRMIO_START && address <= NV4_PRMIO_END)
        ret = nv4_prmio_read(address);
    else if (address >= NV4_PTIMER_START && address <= NV4_PTIMER_END)
        ret = nv4_ptimer_read(address);
    else if (address >= NV4_PFB_START && address <= NV4_PFB_END)
        ret = nv4_pfb_read(address);
    else if (address >= NV4_PEXTDEV_START && address <= NV4_PEXTDEV_END)
        ret = nv4_pextdev_read(address);
    else if (address >= NV4_PROM_START && address <= NV4_PROM_END)
        ret = nv4_prom_read(address);
    else if (address >= NV4_PME_START && address <= NV4_PME_END)
        ret = nv4_pme_read(address);
    else if (address >= NV4_PGRAPH_START && address <= NV4_PGRAPH_REAL_END) // what we're actually doing here determined by nv4_pgraph_* func
        ret = nv4_pgraph_read(address);
    else if (address >= NV4_PRMCIO_START && address <= NV4_PRMCIO_END)
        ret = nv4_prmcio_read(address);    
    else if (address >= NV4_PVIDEO_START && address <= NV4_PVIDEO_END)
        ret = nv4_pvideo_read(address);
    else if (address >= NV4_PRAMDAC_START && address <= NV4_PRAMDAC_END)
        ret = nv4_pramdac_read(address);
    else if (address >= NV4_VRAM_START && address <= NV4_VRAM_END)
        ret = nv4_dfb_read32(address & nv4->nvbase.svga.vram_mask, &nv4->nvbase.svga);
    else if (address >= NV4_USER_START && address <= NV4_USER_END)
        ret = nv4_user_read(address);
    else 
    {
        warning("MMIO read arbitration failed, INVALID address NOT mapped to any GPU subsystem 0x%08x [returning 0x00]\n", address);
        return 0x00;
    }

    return ret;
}

void nv4_mmio_arbitrate_write(uint32_t address, uint32_t value)
{
    // sanity check
    if (!nv4)
        return; 

    // Some of these addresses are Weitek VGA stuff and we need to mask it to this first because the weitek addresses are 8-bit aligned.
    address &= 0xFFFFFF;

    // note: some registers are byte aligned not dword aligned
    // only very few are though, so they can be handled specially, using the register list most likely
    address &= 0xFFFFFC;

    // gigantic set of if statements to send the write to the right subsystem
    if (address >= NV4_PMC_START && address <= NV4_PMC_END)
        nv4_pmc_write(address, value);
    else if (address >= NV4_CIO_START && address <= NV4_CIO_END)
        nv4_cio_write(address, value);
    else if (address >= NV4_PBUS_PCI_START && address <= NV4_PBUS_PCI_END)              // PCI mirrored at 0x1800 in MMIO
        nv4_pci_write(0x00, address & 0xFF, value, NULL); // priv does not matter
    else if (address >= NV4_PBUS_START && address <= NV4_PBUS_END)
        nv4_pbus_write(address, value);
    else if (address >= NV4_PFIFO_START && address <= NV4_PFIFO_END)
        nv4_pfifo_write(address, value);
    else if (address >= NV4_PRM_START && address <= NV4_PRM_END)
        nv4_prm_write(address, value);
    else if (address >= NV4_PRMIO_START && address <= NV4_PRMIO_END)
        nv4_prmio_write(address, value);
    else if (address >= NV4_PTIMER_START && address <= NV4_PTIMER_END)
        nv4_ptimer_write(address, value);
    else if (address >= NV4_PFB_START && address <= NV4_PFB_END)
        nv4_pfb_write(address, value);
    else if (address >= NV4_PEXTDEV_START && address <= NV4_PEXTDEV_END)
        nv4_pextdev_write(address, value);
    else if (address >= NV4_PROM_START && address <= NV4_PROM_END)
        nv4_prom_write(address, value);
    else if (address >= NV4_PME_START && address <= NV4_PME_END)
        nv4_pme_write(address, value);
    else if (address >= NV4_PGRAPH_START && address <= NV4_PGRAPH_REAL_END) // what we're actually doing here is determined by the nv4_pgraph_* functions
        nv4_pgraph_write(address, value);
    else if (address >= NV4_PRMCIO_START && address <= NV4_PRMCIO_END)
        nv4_prmcio_write(address, value);
    else if (address >= NV4_PVIDEO_START && address <= NV4_PVIDEO_END)
        nv4_pvideo_write(address, value);
    else if (address >= NV4_PRAMDAC_START && address <= NV4_PRAMDAC_END)
        nv4_pramdac_write(address, value);
    else if (address >= NV4_VRAM_START && address <= NV4_VRAM_END)
        nv4_dfb_write32(address, value, &nv4->nvbase.svga);
    else if (address >= NV4_USER_START && address <= NV4_USER_END)
        nv4_user_write(address, value);
    //RAMIN is its own thing
    else 
    {
        warning("MMIO write arbitration failed, INVALID address NOT mapped to any GPU subsystem 0x%08x\n", address);
        return;
    }
}


//                                                              //
// ******* DUMMY FUNCTIONS FOR UNIMPLEMENTED SUBSYSTEMS ******* //
//                                                              //

// Read and Write functions for GPU subsystems
// Remove the ones that aren't used here eventually, have all of htem for now
uint32_t    nv4_cio_read(uint32_t address) { return 0; };
void        nv4_cio_write(uint32_t address, uint32_t value) {};
uint32_t    nv4_prm_read(uint32_t address) { return 0; };
void        nv4_prm_write(uint32_t address, uint32_t value) {};
uint32_t    nv4_prmio_read(uint32_t address) { return 0; };
void        nv4_prmio_write(uint32_t address, uint32_t value) {};

// TODO: PGRAPH class registers
uint32_t    nv4_prmcio_read(uint32_t address) { return 0; };
void        nv4_prmcio_write(uint32_t address, uint32_t value) {};
