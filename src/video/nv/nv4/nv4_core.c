/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          NV4 bringup and device emulation.
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
#include <86box/io.h>
#include <86box/pci.h>
#include <86box/rom.h> // DEPENDENT!!!
#include <86box/video.h>
#include <86box/nv/vid_nv.h>
#include <86box/nv/vid_nv4.h>

/* Main device object pointer */
nv4_t* nv4;

/* These are a ****PLACEHOLDER**** and are copied from 3dfx VoodooBanshee/Voodoo3*/
static video_timings_t timing_nv4_pci = { .type = VIDEO_PCI, .write_b = 2, .write_w = 2, .write_l = 1, .read_b = 20, .read_w = 20, .read_l = 21 };
static video_timings_t timing_nv4_agp = { .type = VIDEO_AGP, .write_b = 2, .write_w = 2, .write_l = 1, .read_b = 20, .read_w = 20, .read_l = 21 };

// Prototypes for functions only used in this translation unit
void nv4_init_mappings_mmio(void);
void nv4_init_mappings_svga(void);
bool nv4_is_svga_redirect_address(uint32_t addr);

uint8_t nv4_svga_in(uint16_t addr, void* priv);
void nv4_svga_out(uint16_t addr, uint8_t val, void* priv);

// Determine if this address needs to be redirected to the SVGA subsystem.

bool nv4_is_svga_redirect_address(uint32_t addr)
{
    return (addr >= NV4_PRMVIO_START && addr <= NV4_PRMVIO_END)     // VGA
    || (addr >= NV4_PRMCIO_START && addr <= NV4_PRMCIO_END)         // CRTC
    || (addr >= NV4_VGA_DAC_START && addr <= NV4_VGA_DAC_END);      // Legacy RAMDAC support(?)
}

// All MMIO regs are 32-bit i believe internally
// so we have to do some munging to get this to read

// Read 8-bit MMIO
uint8_t nv4_mmio_read8(uint32_t addr, void* priv)
{
    uint32_t ret = 0x00;

    // Some of these addresses are Weitek VGA stuff and we need to mask it to this first because the weitek addresses are 8-bit aligned.
    addr &= 0xFFFFFF;

    if (nv4_is_svga_redirect_address(addr))
    {
        // svga writes are not logged anyway rn
        uint32_t real_address = addr & 0x3FF;

        ret = nv4_svga_in(real_address, nv4);

        nv_log_verbose_only("Redirected MMIO read8 to SVGA: addr=0x%04x returned 0x%04x\n", addr, ret);

        return ret; 
    }

    // see if unaligned reads are a problem
    ret = nv4_mmio_read32(addr, priv);
    return (uint8_t)(ret >> ((addr & 3) << 3) & 0xFF);
}

// Read 16-bit MMIO
uint16_t nv4_mmio_read16(uint32_t addr, void* priv)
{
    uint32_t ret = 0x00;

    // Some of these addresses are Weitek VGA stuff and we need to mask it to this first because the weitek addresses are 8-bit aligned.
    addr &= 0xFFFFFF;

    if (nv4_is_svga_redirect_address(addr))
    {
        // svga writes are not logged anyway rn
        uint32_t real_address = addr & 0x3FF;

        ret = nv4_svga_in(real_address, nv4)
        | (nv4_svga_in(real_address + 1, nv4) << 8);
        
        nv_log_verbose_only("Redirected MMIO read16 to SVGA: addr=0x%04x returned 0x%04x\n", addr, ret);

        return ret; 
    }

    ret = nv4_mmio_read32(addr, priv);
    return (uint8_t)(ret >> ((addr & 3) << 3) & 0xFFFF);
}

// Read 32-bit MMIO
uint32_t nv4_mmio_read32(uint32_t addr, void* priv)
{
    uint32_t ret = 0x00;

    // Some of these addresses are Weitek VGA stuff and we need to mask it to this first because the weitek addresses are 8-bit aligned.
    addr &= 0xFFFFFF;

    if (nv4_is_svga_redirect_address(addr))
    {
        // svga writes are not logged anyway rn
        uint32_t real_address = addr & 0x3FF;

        ret = nv4_svga_in(real_address, nv4)
        | (nv4_svga_in(real_address + 1, nv4) << 8)
        | (nv4_svga_in(real_address + 2, nv4) << 16)
        | (nv4_svga_in(real_address + 3, nv4) << 24);

        nv_log_verbose_only("Redirected MMIO read32 to SVGA: addr=0x%04x returned 0x%04x\n", addr, ret);

        return ret; 
    }


    ret = nv4_mmio_arbitrate_read(addr);


    return ret; 

}

// Write 8-bit MMIO
void nv4_mmio_write8(uint32_t addr, uint8_t val, void* priv)
{
    addr &= 0xFFFFFF;

    // This is weitek vga stuff
    // If we need to add more of these we can convert these to a switch statement
    if (nv4_is_svga_redirect_address(addr))
    {
        // svga writes are not logged anyway rn
        uint32_t real_address = addr & 0x3FF;

        nv_log_verbose_only("Redirected MMIO write8 to SVGA: addr=0x%04x val=0x%02x\n", addr, val);

        nv4_svga_out(real_address, val & 0xFF, nv4);

        return; 
    }
    
    // overwrite first 8bits of a 32 bit value
    uint32_t new_val = nv4_mmio_read32(addr, NULL);

    new_val &= (~0xFF << (addr & 3) << 3);
    new_val |= (val << ((addr & 3) << 3));

    nv4_mmio_write32(addr, new_val, priv);
}

// Write 16-bit MMIO
void nv4_mmio_write16(uint32_t addr, uint16_t val, void* priv)
{
    addr &= 0xFFFFFF;

    // This is weitek vga stuff
    if (nv4_is_svga_redirect_address(addr))
    {
        // svga writes are not logged anyway rn
        uint32_t real_address = addr & 0x3FF;

        nv_log_verbose_only("Redirected MMIO write16 to SVGA: addr=0x%04x val=0x%02x\n", addr, val);


        nv4_svga_out(real_address, val & 0xFF, nv4);
        nv4_svga_out(real_address + 1, (val >> 8) & 0xFF, nv4);
        
        return; 
    }

    // overwrite first 16bits of a 32 bit value
    uint32_t new_val = nv4_mmio_read32(addr, NULL);

    new_val &= (~0xFFFF << (addr & 3) << 3);
    new_val |= (val << ((addr & 3) << 3));

    nv4_mmio_write32(addr, new_val, priv);
}

// Write 32-bit MMIO
void nv4_mmio_write32(uint32_t addr, uint32_t val, void* priv)
{
    addr &= 0xFFFFFF;

    // This is weitek vga stuff
    if (nv4_is_svga_redirect_address(addr))
    {
        // svga writes are not logged anyway rn
        uint32_t real_address = addr & 0x3FF;

        nv_log_verbose_only("Redirected MMIO write32 to SVGA: addr=0x%04x val=0x%02x\n", addr, val);

        nv4_svga_out(real_address, val & 0xFF, nv4);
        nv4_svga_out(real_address + 1, (val >> 8) & 0xFF, nv4);
        nv4_svga_out(real_address + 2, (val >> 16) & 0xFF, nv4);
        nv4_svga_out(real_address + 3, (val >> 24) & 0xFF, nv4);
        
        return; 
    }

    nv4_mmio_arbitrate_write(addr, val);
}

// PCI stuff
// BAR0         Pointer to MMIO space
// BAR1         Pointer to Linear Framebuffer (NV_USER)

uint8_t nv4_pci_read(int32_t func, int32_t addr, void* priv)
{
    uint8_t ret = 0x00;

    // sanity check
    if (!nv4)
        return ret; 

    // figure out what size this gets read as first
    // seems func does not matter at least here?
    switch (addr) 
    {
        // Get the pci vendor id..

        case NV4_PCI_CFG_VENDOR_ID:
            ret = (PCI_VENDOR_NV & 0xFF);
            break;
        
        case NV4_PCI_CFG_VENDOR_ID + 1: // all access 8bit
            ret = (PCI_VENDOR_NV >> 8);
            break;

        // device id

        case NV4_PCI_CFG_DEVICE_ID:
            ret = (NV_PCI_DEVICE_NV4 & 0xFF);
            break;
        
        case NV4_PCI_CFG_DEVICE_ID+1:
            ret = (NV_PCI_DEVICE_NV4 >> 8);
            break;
        
        // various capabilities
        // IO space         enabled
        // Memory space     enabled
        // Bus master       enabled
        // Write/inval      enabled
        // Pal snoop        enabled
        // Capabiliies list enabled
        // 66Mhz FSB        capable

        case PCI_REG_COMMAND_L:
            ret = (nv4->pci_config.pci_regs[PCI_REG_COMMAND_L]) & (NV4_PCI_COMMAND_L_IO | NV4_PCI_COMMAND_L_MEMORY | NV4_PCI_COMMAND_L_BUSMASTER);
            break;
        
        case PCI_REG_COMMAND_H:
            ret = nv4->pci_config.pci_regs[PCI_REG_COMMAND_H];
            break;

        // pci status register
        case PCI_REG_STATUS_L:
            ret = (nv4->pci_config.pci_regs[PCI_REG_STATUS_L] | NV4_PCI_STATUS_L_66MHZ_CAPABLE | NV4_PCI_STATUS_L_FAST_BACK2BACK | NV4_PCI_STATUS_L_CAP_LIST);
            break;

        case PCI_REG_STATUS_H:
            ret = (nv4->pci_config.pci_regs[PCI_REG_STATUS_H]) & (NV4_PCI_STATUS_H_MEDIUM_DEVSEL_TIMING);
            break;
        
        case NV4_PCI_CFG_REVISION:
            ret = nv4->nvbase.gpu_revision; // Commercial release
            break;
       
        case PCI_REG_PROG_IF:
            ret = 0x00;
            break;
            
        case NV4_PCI_CFG_SUBCLASS_CODE:
            ret = 0x00; // nothing
            break;
        
        case NV4_PCI_CFG_CLASS_CODE:
            ret = NV4_PCI_CFG_CLASS_CODE_VGA; // CLASS_CODE_VGA 
            break;
        
        case NV4_PCI_CFG_CACHE_LINE_SIZE:
            ret = NV4_PCI_CFG_CACHE_LINE_SIZE_DEFAULT_FROM_VBIOS;
            break;
        
        case NV4_PCI_CFG_LATENCY_TIMER:
        case NV4_PCI_CFG_HEADER_TYPE:
        case NV4_PCI_CFG_BIST:
            ret = 0x00;
            break;

        // BARs are marked as prefetchable per the datasheet
        case NV4_PCI_CFG_BAR0_L:
        case NV4_PCI_CFG_BAR1_L:
            // only bit that matters is bit 3 (prefetch bit)
            ret =(NV4_PCI_CFG_BAR_PREFETCHABLE_ENABLED << NV4_PCI_CFG_BAR_PREFETCHABLE);
            break;

        // These registers are hardwired to zero per the datasheet
        // Writes have no effect, we can just handle it here though
        case NV4_PCI_CFG_BAR0_BYTE1 ... NV4_PCI_CFG_BAR0_BYTE2:
        case NV4_PCI_CFG_BAR1_BYTE1 ... NV4_PCI_CFG_BAR1_BYTE2:
            ret = 0x00;
            break;

        // MMIO base address
        case NV4_PCI_CFG_BAR0_BASE_ADDRESS:
            ret = nv4->nvbase.bar0_mmio_base >> 24;//8bit value
            break; 

        case NV4_PCI_CFG_BAR1_BASE_ADDRESS:
            ret = nv4->nvbase.bar1_lfb_base >> 24; //8bit value
            break;

        case NV4_PCI_CFG_ENABLE_VBIOS:
            ret = nv4->pci_config.vbios_enabled;
            break;
        
        case NV4_PCI_CFG_INT_LINE:
            ret = nv4->pci_config.int_line;
            break;
        
        case NV4_PCI_CFG_INT_PIN:
            ret = PCI_INTA;
            break;

        case NV4_PCI_CFG_CAPPTR:
            ret = 0x60;
            break;

        case 0x60:
            ret = 0x01;
            break;

        case 0x61:
            ret = 0x44;
            break;

        case 0x62:
            ret = 0x01;
            break;

        case 0x63:
            ret = 0x00;
            break;

        case 0x44:
            ret = 0x02;
            break;

        case 0x45:
            ret = 0x00;
            break;

        case 0x46:
            ret = 0x10;
            break;

        case 0x47:
            ret = 0x00;
            break;

        case NV4_PCI_CFG_MIN_GRANT:
            ret = NV4_PCI_CFG_MIN_GRANT_DEFAULT;
            break;

        case NV4_PCI_CFG_MAX_LATENCY:
            ret = NV4_PCI_CFG_MAX_LATENCY_DEFAULT;
            break;

        //bar2-5 are not used and hardwired to 0
        case NV4_PCI_CFG_BAR_INVALID_START ... NV4_PCI_CFG_BAR_INVALID_END:
            ret = 0x00;
            break;
            
        case NV4_PCI_CFG_SUBSYSTEM_ID_MIRROR_START:
        case NV4_PCI_CFG_SUBSYSTEM_ID_MIRROR_END:
            ret = nv4->pci_config.pci_regs[NV4_PCI_CFG_SUBSYSTEM_ID + (addr & 0x03)];
            break;

        default: // by default just return pci_config.pci_regs
            ret = nv4->pci_config.pci_regs[addr];
            break;
        
    }

    nv_log("nv4_pci_read func=0x%04x addr=0x%04x ret=0x%04x\n", func, addr, ret);
    return ret; 
}

void nv4_pci_write(int32_t func, int32_t addr, uint8_t val, void* priv)
{

    // sanity check
    if (!nv4)
        return; 

    // some addresses are not writable so can't have any effect and can't be allowed to be modified using this code
    // as an example, only the most significant byte of the PCI BARs can be modified
    if (addr >= NV4_PCI_CFG_BAR0_L && addr <= NV4_PCI_CFG_BAR0_BYTE2
    && addr >= NV4_PCI_CFG_BAR1_L && addr <= NV4_PCI_CFG_BAR1_BYTE2)
        return;

    nv_log("nv4_pci_write func=0x%04x addr=0x%04x val=0x%04x\n", func, addr, val);

    nv4->pci_config.pci_regs[addr] = val;

    switch (addr)
    {
        // standard pci command stuff
        case PCI_REG_COMMAND_L:
            nv4->pci_config.pci_regs[PCI_REG_COMMAND_L] = val;
            // actually update the mappings
            nv4_update_mappings();
            break;
        case PCI_REG_COMMAND_H:
            nv4->pci_config.pci_regs[PCI_REG_COMMAND_H] = val;
            // actually update the mappings
            nv4_update_mappings();          
            break;
        // pci status register
        case PCI_REG_STATUS_L:
            nv4->pci_config.pci_regs[PCI_REG_STATUS_L] = val | (NV4_PCI_STATUS_L_66MHZ_CAPABLE);
            break;
        case PCI_REG_STATUS_H:
            nv4->pci_config.pci_regs[PCI_REG_STATUS_H] = val | (NV4_PCI_STATUS_H_FAST_DEVSEL_TIMING << NV4_PCI_STATUS_H_DEVSEL_TIMING);
            break;
        //TODO: ACTUALLY REMAP THE MMIO AND NV_USER
        case NV4_PCI_CFG_BAR0_BASE_ADDRESS:
            nv4->nvbase.bar0_mmio_base = val << 24;
            nv4_update_mappings();
            break; 
        case NV4_PCI_CFG_BAR1_BASE_ADDRESS:
            nv4->nvbase.bar1_lfb_base = val << 24;
            nv4_update_mappings();
            break;
        case NV4_PCI_CFG_ENABLE_VBIOS:
        case NV4_PCI_CFG_VBIOS_BASE:
            
            // make sure we are actually toggling the vbios, not the rom base
            if (addr == NV4_PCI_CFG_ENABLE_VBIOS)
                nv4->pci_config.vbios_enabled = (val & 0x01);

            if (nv4->pci_config.vbios_enabled)
            {
                // First see if we simply wanted to change the VBIOS location

                // Enable it in case it was disabled before
                mem_mapping_enable(&nv4->nvbase.vbios.mapping);

                if (addr != NV4_PCI_CFG_ENABLE_VBIOS)
                {
                    uint32_t old_addr = nv4->nvbase.vbios.mapping.base;
                    // 9bit register
                    uint32_t new_addr = nv4->pci_config.pci_regs[NV4_PCI_CFG_VBIOS_BASE_H] << 24 |
                    nv4->pci_config.pci_regs[NV4_PCI_CFG_VBIOS_BASE_L] << 16;

                    // move it
                    mem_mapping_set_addr(&nv4->nvbase.vbios.mapping, new_addr, 0x10000);

                    nv_log("...i like to move it move it (VBIOS Relocation) 0x%04x -> 0x%04x\n", old_addr, new_addr);

                }
                else
                {
                    nv_log("...VBIOS Enable\n");
                }
            }
            else
            {
                nv_log("...VBIOS Disable\n");
                mem_mapping_disable(&nv4->nvbase.vbios.mapping);

            }
            break;
        case NV4_PCI_CFG_INT_LINE:
            nv4->pci_config.int_line = val;
            break;
        //bar2-5 are not used and can't be written to
        case NV4_PCI_CFG_BAR_INVALID_START ... NV4_PCI_CFG_BAR_INVALID_END:
            break;

        // these are mirrored to the subsystem id and also stored in the ROMBIOS
        case NV4_PCI_CFG_SUBSYSTEM_ID_MIRROR_START:
        case NV4_PCI_CFG_SUBSYSTEM_ID_MIRROR_END:
            nv4->pci_config.pci_regs[NV4_PCI_CFG_SUBSYSTEM_ID + (addr & 0x03)] = val;
            break;

        default:

    }
}


//
// SVGA functions
//
void nv4_recalc_timings(svga_t* svga)
{    
    // sanity check
    if (!nv4)
        return; 

    nv4_t* nv4 = (nv4_t*)svga->priv;
    uint32_t pixel_mode = svga->crtc[NV4_CRTC_REGISTER_PIXELMODE] & 0x03;

    svga->ma_latch += (svga->crtc[NV4_CRTC_REGISTER_RPC0] & 0x1F) << 16;

    // should these actually use separate values?
    // i don't we should force the top 2 bits to 1...

    // required for VESA resolutions, force parameters higher
    // only fuck around with any of this in VGAmode?

    if (pixel_mode == NV4_CRTC_REGISTER_PIXELMODE_VGA)
    {
        if (svga->crtc[NV4_CRTC_REGISTER_PIXELMODE] & 1 << (NV4_CRTC_REGISTER_FORMAT_VDT10)) svga->vtotal += 0x400;
        if (svga->crtc[NV4_CRTC_REGISTER_PIXELMODE] & 1 << (NV4_CRTC_REGISTER_FORMAT_VDE10)) svga->dispend += 0x400;
        if (svga->crtc[NV4_CRTC_REGISTER_PIXELMODE] & 1 << (NV4_CRTC_REGISTER_FORMAT_VRS10)) svga->vblankstart += 0x400;
        if (svga->crtc[NV4_CRTC_REGISTER_PIXELMODE] & 1 << (NV4_CRTC_REGISTER_FORMAT_VBS10)) svga->vsyncstart += 0x400;
        if (svga->crtc[NV4_CRTC_REGISTER_PIXELMODE] & 1 << (NV4_CRTC_REGISTER_FORMAT_HBE6)) svga->hdisp += 0x400; 

        if (svga->crtc[NV4_CRTC_REGISTER_HEB] & 0x01)
            svga->hdisp += 0x100; // large screen bit
    }
 
    /* Turn off override if we are in VGA mode */
    svga->override = !(pixel_mode == NV4_CRTC_REGISTER_PIXELMODE_VGA);

    /* NOTE: The RIVA 128 draws in a way almost completely separate to any other 86Box GPU.
    
    Basically, we only blit to buffer32 when something changes and we don't even bother using a timer. We only render when there is something to actually render.

    This is because there is no linear relationship between the contents of VRAM and the contents of the display which 86box's SVGA subsystem cannot tolerate.
    In fact, the position in VRAM and pitch can be changed at any time via an NV_IMAGE_IN_MEMORY object.

    Therefore, we need to completely bypass it using svga->override and draw our own rendering functions. This allows us to use a neat optimisation trick
    to only ever actually draw when we need to do something. This shouldn't be a problem in games, because the drivers will read the current refresh rate from 
    the Windows settings, and then, just submit objects at that pace for anything that changes on the screen.
    */

    // Set the pixel mode
    switch (pixel_mode)
    {
        case NV4_CRTC_REGISTER_PIXELMODE_8BPP:
            svga->rowoffset += (svga->crtc[NV4_CRTC_REGISTER_RPC0] & 0xE0) << 1; // ?????
            svga->bpp = 8;
            svga->lowres = 0;
            svga->map8 = svga->pallook;
            break;
        case NV4_CRTC_REGISTER_PIXELMODE_16BPP:
            /* This is some sketchy shit that is an attempt at an educated guess
            at pixel clock differences between 9x and NT only in 16bpp. If there is ever an error on 9x with "interlaced" looking graphics,
            this is what's causing it. Possibly fucking *ReactOS* of all things */
            if ((svga->crtc[NV4_CRTC_REGISTER_VRETRACESTART] >> 1) & 0x01)
                svga->rowoffset += (svga->crtc[NV4_CRTC_REGISTER_RPC0] & 0xE0) << 2;
            else 
                svga->rowoffset += (svga->crtc[NV4_CRTC_REGISTER_RPC0] & 0xE0) << 3;

            /* sometimes it really renders in 15bpp, so you need to do this */
            if ((nv4->pramdac.general_control >> NV4_PRAMDAC_GENERAL_CONTROL_565_MODE) & 0x01)
            {
                svga->bpp = 16;
                svga->lowres = 0;
            }
            else
            {
                svga->bpp = 15;
                svga->lowres = 0;
                
            }
        
            break;
        case NV4_CRTC_REGISTER_PIXELMODE_32BPP:
            svga->rowoffset += (svga->crtc[NV4_CRTC_REGISTER_RPC0] & 0xE0) << 3;
            
            svga->bpp = 32;
            svga->lowres = 0;
            //svga->render = nv4_render_32bpp;
            break;
    }

    // from nv_riva128
    if (((svga->miscout >> 2) & 2) == 2)
    {
        // set clocks
        nv4_pramdac_set_pixel_clock();
        nv4_pramdac_set_vram_clock();
    }
}

void nv4_speed_changed(void* priv)
{
    // sanity check
    if (!nv4)
        return; 
        
    nv4_recalc_timings(&nv4->nvbase.svga);
}

// Force Redraw
// Reset etc.
void nv4_force_redraw(void* priv)
{
    // sanity check
    if (!nv4)
        return; 

    nv4->nvbase.svga.fullchange = changeframecount; 
}

// Read from SVGA core memory
uint8_t nv4_svga_in(uint16_t addr, void* priv)
{

    nv4_t* nv4 = (nv4_t*)priv;

    uint8_t ret = 0x00;

    // sanity check
    if (!nv4)
        return ret; 

    // If we need to RMA from GPU MMIO, go do that
    if (addr >= NV4_RMA_REGISTER_START
    && addr <= NV4_RMA_REGISTER_END)
    {
        if (!(nv4->pbus.rma.mode & 0x01))
            return ret;

        // must be dword aligned
        uint32_t real_rma_read_addr = ((nv4->pbus.rma.mode & NV4_CRTC_REGISTER_RMA_MODE_MAX - 1) << 1) + (addr & 0x03); 
        ret = nv4_pbus_rma_read(real_rma_read_addr);
        return ret;
    }

    // mask off b0/d0 registers 
    if ((((addr & 0xFFF0) == 0x3D0 
    || (addr & 0xFFF0) == 0x3B0) && addr < 0x3de) 
    && !(nv4->nvbase.svga.miscout & 1))
        addr ^= 0x60;

    switch (addr)
    {
        // Alias for "get current SVGA CRTC register ID"
        case NV4_CRTC_REGISTER_INDEX:
            ret = nv4->nvbase.svga.crtcreg;
            break;
        case NV4_CRTC_REGISTER_CURRENT:
            // Support the extended NVIDIA CRTC register range
            switch (nv4->nvbase.svga.crtcreg)
            {
                case NV4_CRTC_REGISTER_RL0:
                    ret = nv4->nvbase.svga.displine & 0xFF; 
                    break;
                    /* Is rl1?*/
                case NV4_CRTC_REGISTER_RL1:
                    ret = (nv4->nvbase.svga.displine >> 8) & 7;
                    break;
                case NV4_CRTC_REGISTER_I2C:
                    ret = i2c_gpio_get_sda(nv4->nvbase.i2c) << 3
                    | i2c_gpio_get_scl(nv4->nvbase.i2c) << 2;

                    break;
                default:
                    ret = nv4->nvbase.svga.crtc[nv4->nvbase.svga.crtcreg];
            }
            break;
        default:
            ret = svga_in(addr, &nv4->nvbase.svga);
            break;
    }

    return ret; //TEMP
}

// Write to SVGA core memory
void nv4_svga_out(uint16_t addr, uint8_t val, void* priv)
{
    // sanity check
    if (!nv4)
        return; 

    // If we need to RMA to GPU MMIO, go do that
    if (addr >= NV4_RMA_REGISTER_START
    && addr <= NV4_RMA_REGISTER_END)
    {
        // we don't need to store these registers...
        nv4->pbus.rma.rma_regs[addr & 3] = val;

        if (!(nv4->pbus.rma.mode & 0x01)) // we are halfway through sending something
            return;

        uint32_t real_rma_write_addr = ((nv4->pbus.rma.mode & (NV4_CRTC_REGISTER_RMA_MODE_MAX - 1)) << 1) + (addr & 0x03); 

        nv4_pbus_rma_write(real_rma_write_addr, nv4->pbus.rma.rma_regs[addr & 3]);
        return;
    }

    // mask off b0/d0 registers 
    if ((((addr & 0xFFF0) == 0x3D0 || (addr & 0xFFF0) == 0x3B0) 
    && addr < 0x3de) 
    && !(nv4->nvbase.svga.miscout & 1))//miscout bit 7 controls mappping
        addr ^= 0x60;

    uint8_t crtcreg = nv4->nvbase.svga.crtcreg;
    uint8_t old_value;

    // todo:
    // Pixel formats (8bit vs 555 vs 565)
    // VBE 3.0?
    
    switch (addr)
    {
        case NV4_CRTC_REGISTER_INDEX:
            // real mode access to GPU MMIO space...
            nv4->nvbase.svga.crtcreg = val;
            break;
        // support the extended crtc regs and debug this out
        case NV4_CRTC_REGISTER_CURRENT:

            // Implements the VGA Protect register
            if ((nv4->nvbase.svga.crtcreg < NV4_CRTC_REGISTER_OVERFLOW) && (nv4->nvbase.svga.crtc[0x11] & 0x80))
                return;

            // Ignore certain bits when VGA Protect register set and we are writing to CRTC register=07h
            if ((nv4->nvbase.svga.crtcreg == NV4_CRTC_REGISTER_OVERFLOW) && (nv4->nvbase.svga.crtc[0x11] & 0x80))
                val = (nv4->nvbase.svga.crtc[NV4_CRTC_REGISTER_OVERFLOW] & ~0x10) | (val & 0x10);

            // set the register value...
            old_value = nv4->nvbase.svga.crtc[crtcreg];

            nv4->nvbase.svga.crtc[crtcreg] = val;
            // ...now act on it

            // Handle nvidia extended Bank0/Bank1 IDs
            switch (crtcreg)
            {
                case NV4_CRTC_REGISTER_READ_BANK:
                        nv4->nvbase.cio_read_bank = val;
                        if (nv4->nvbase.svga.chain4) // chain4 addressing (planar?)
                            nv4->nvbase.svga.read_bank = nv4->nvbase.cio_read_bank << 15;
                        else
                            nv4->nvbase.svga.read_bank = nv4->nvbase.cio_read_bank << 13; // extended bank numbers
                    break;
                case NV4_CRTC_REGISTER_WRITE_BANK:
                    nv4->nvbase.cio_write_bank = val;
                        if (nv4->nvbase.svga.chain4)
                            nv4->nvbase.svga.write_bank = nv4->nvbase.cio_write_bank << 15;
                        else
                            nv4->nvbase.svga.write_bank = nv4->nvbase.cio_write_bank << 13;
                    break;
                case NV4_CRTC_REGISTER_RMA:
                    nv4->pbus.rma.mode = val & NV4_CRTC_REGISTER_RMA_MODE_MAX;
                    break;
                case NV4_CRTC_REGISTER_I2C_GPIO:
                    uint8_t scl = !!(val & 0x20);
                    uint8_t sda = !!(val & 0x10);
                    // Set an I2C GPIO register
                    i2c_gpio_set(nv4->nvbase.i2c, scl, sda);
                    break;
            }

            /* Recalculate the timings if we actually changed them 
            Additionally only do it if the value actually changed*/
            if (old_value != val)
            {
                // Thx to Fuel who basically wrote most of the SVGA compatibility code already (although I fixed some issues), because VGA is boring 
                // and in the words of an ex-Rendition/3dfx/NVIDIA engineer, "VGA was basically an undocumented bundle of steaming you-know-what.   
                // And it was essential that any cores the PC 3D startups acquired had to work with all the undocumented modes and timing tweaks (mode X, etc.)"
                if (nv4->nvbase.svga.crtcreg < 0xE
                && nv4->nvbase.svga.crtcreg > 0x10)
                {
                    nv4->nvbase.svga.fullchange = changeframecount;
                    nv4_recalc_timings(&nv4->nvbase.svga);
                }
            }

            break;
        default:
            svga_out(addr, val, &nv4->nvbase.svga);
            break;
    }

}

/* DFB, sets up a dumb framebuffer */
uint8_t nv4_dfb_read8(uint32_t addr, void* priv)
{
    addr &= (nv4->nvbase.svga.vram_mask);
    return nv4->nvbase.svga.vram[addr];
}

uint16_t nv4_dfb_read16(uint32_t addr, void* priv)
{
    addr &= (nv4->nvbase.svga.vram_mask);
    return (nv4->nvbase.svga.vram[addr + 1] << 8) | nv4->nvbase.svga.vram[addr];
}

uint32_t nv4_dfb_read32(uint32_t addr, void* priv)
{
    addr &= (nv4->nvbase.svga.vram_mask);
    return (nv4->nvbase.svga.vram[addr + 3] << 24) | (nv4->nvbase.svga.vram[addr + 2] << 16) +
    (nv4->nvbase.svga.vram[addr + 1] << 8) | nv4->nvbase.svga.vram[addr];
}

void nv4_dfb_write8(uint32_t addr, uint8_t val, void* priv)
{
    addr &= (nv4->nvbase.svga.vram_mask);
    nv4->nvbase.svga.vram[addr] = val;
    nv4->nvbase.svga.changedvram[addr >> 12] = val;
    nv4_render_current_bpp_dfb_8(addr);
}

void nv4_dfb_write16(uint32_t addr, uint16_t val, void* priv)
{
    addr &= (nv4->nvbase.svga.vram_mask);
    nv4->nvbase.svga.vram[addr + 1] = (val >> 8) & 0xFF;
    nv4->nvbase.svga.vram[addr] = (val) & 0xFF;
    nv4->nvbase.svga.changedvram[addr >> 12] = val;
    nv4_render_current_bpp_dfb_16(addr);

}

void nv4_dfb_write32(uint32_t addr, uint32_t val, void* priv)
{
    addr &= (nv4->nvbase.svga.vram_mask);
    nv4->nvbase.svga.vram[addr + 3] = (val >> 24) & 0xFF;
    nv4->nvbase.svga.vram[addr + 2] = (val >> 16) & 0xFF;
    nv4->nvbase.svga.vram[addr + 1] = (val >> 8) & 0xFF;
    nv4->nvbase.svga.vram[addr] = (val) & 0xFF;
    nv4->nvbase.svga.changedvram[addr >> 12] = val;

    nv4_render_current_bpp_dfb_32(addr);

}

/* Cursor shit */
void nv4_draw_cursor(svga_t* svga, int32_t drawline)
{
    // sanity check
    if (!nv4)
        return; 
    
    // On windows, this shows up using NV_IMAGE_IN_MEMORY.
    // Do we need to emulate it?

    nv_log("nv4_draw_cursor drawline=0x%04x", drawline);
}

// MMIO 0x110000->0x111FFF is mapped to a mirror of the VBIOS.
// Note this area is 64kb and the vbios is only 32kb. See below..

uint8_t nv4_prom_read(uint32_t address)
{
    // prom area is 64k, so...
    // first see if we even have a rom of 64kb in size
    uint32_t max_rom_size = NV4_PROM_END - NV4_PROM_START;
    uint32_t real_rom_size = max_rom_size;

    // set it
    if (nv4->nvbase.vbios.sz < max_rom_size)
        real_rom_size = nv4->nvbase.vbios.sz;

    //get our real address
    uint8_t rom_address = address & max_rom_size;

    // Does this mirror on real hardware?
    if (rom_address >= real_rom_size)
    {
        nv_log("PROM VBIOS Read to INVALID address 0x%05x, returning 0xFF", rom_address);
        return 0xFF;
    }
    else
    {
        uint8_t val = nv4->nvbase.vbios.rom[rom_address];
        nv_log("PROM VBIOS Read 0x%05x <- 0x%05x", val, rom_address);
        return val;
    }
}

void nv4_prom_write(uint32_t address, uint32_t value)
{
    uint32_t real_addr = address & 0x1FFFF;
    nv_log("What's going on here? Tried to write to the Video BIOS ROM? (Address=0x%05x, value=0x%02x)", address, value);
}

// Initialise the MMIO mappings
void nv4_init_mappings_mmio(void)
{
    nv_log("Initialising MMIO mapping\n");

    // 0x0 - 1000000: regs
    // 0x1000000-2000000

    // initialize the mmio mapping
    mem_mapping_add(&nv4->nvbase.mmio_mapping, 0, 0, 
        nv4_mmio_read8,
        nv4_mmio_read16,
        nv4_mmio_read32,
        nv4_mmio_write8,
        nv4_mmio_write16,
        nv4_mmio_write32,
        NULL, MEM_MAPPING_EXTERNAL, nv4);
}

void nv4_init_mappings_svga(void)
{
    nv_log("Initialising SVGA core memory mapping\n");

    
    // setup the svga mappings
    mem_mapping_add(&nv4->nvbase.framebuffer_mapping, 0, 0,
        nv4_dfb_read8,
        nv4_dfb_read16,
        nv4_dfb_read32,
        nv4_dfb_write8,
        nv4_dfb_write16,
        nv4_dfb_write32,
        nv4->nvbase.svga.vram, 0, &nv4->nvbase.svga);

    // the SVGA/LFB mapping is also mirrored
    mem_mapping_add(&nv4->nvbase.framebuffer_mapping_mirror, 0, 0, 
        nv4_dfb_read8,
        nv4_dfb_read16,
        nv4_dfb_read32,
        nv4_dfb_write8,
        nv4_dfb_write16,
        nv4_dfb_write32,
        nv4->nvbase.svga.vram, 0, &nv4->nvbase.svga);

    io_sethandler(0x03c0, 0x0020, 
    nv4_svga_in, NULL, NULL, 
    nv4_svga_out, NULL, NULL, 
    nv4);
}

void nv4_init_mappings(void)
{
    nv4_init_mappings_mmio();
    nv4_init_mappings_svga();
}

// Updates the mappings after initialisation. 
void nv4_update_mappings(void)
{
    // sanity check
    if (!nv4)
        return; 

    // setting this to 0 doesn't seem to disable it, based on the datasheet

    nv_log("\nMemory Mapping Config Change:\n");

    (nv4->pci_config.pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO) ? nv_log("Enable I/O\n") : nv_log("Disable I/O\n");

    io_removehandler(0x03c0, 0x0020, 
        nv4_svga_in, NULL, NULL, 
        nv4_svga_out, NULL, NULL, 
        nv4);

    if (nv4->pci_config.pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO)
        io_sethandler(0x03c0, 0x0020, 
        nv4_svga_in, NULL, NULL, 
        nv4_svga_out, NULL, NULL, 
        nv4);   
    
    if (!(nv4->pci_config.pci_regs[PCI_REG_COMMAND]) & PCI_COMMAND_MEM)
    {
        nv_log("The memory was turned off, not much is going to happen.\n");
        return;
    }

    // turn off bar0 and bar1 by defualt
    mem_mapping_disable(&nv4->nvbase.mmio_mapping);
    mem_mapping_disable(&nv4->nvbase.framebuffer_mapping);
    mem_mapping_disable(&nv4->nvbase.framebuffer_mapping_mirror);

    // Setup BAR0 (MMIO)

    nv_log("BAR0 (MMIO Base) = 0x%08x\n", nv4->nvbase.bar0_mmio_base);

    
    if (nv4->nvbase.bar0_mmio_base)
        mem_mapping_set_addr(&nv4->nvbase.mmio_mapping, nv4->nvbase.bar0_mmio_base, NV4_MMIO_SIZE);

    // if this breaks anything, remove it
    nv_log("BAR1 (Linear Framebuffer / NV_USER Base & RAMIN) = 0x%08x\n", nv4->nvbase.bar1_lfb_base);

    if (nv4->nvbase.bar1_lfb_base)
    {
        if (nv4->nvbase.vram_amount == NV4_VRAM_SIZE_8MB)
        {
            mem_mapping_set_addr(&nv4->nvbase.framebuffer_mapping, nv4->nvbase.bar1_lfb_base, NV4_VRAM_SIZE_8MB);
            mem_mapping_set_addr(&nv4->nvbase.framebuffer_mapping_mirror, nv4->nvbase.bar1_lfb_base + NV4_LFB_MIRROR_START, NV4_VRAM_SIZE_8MB);
        }
        else if (nv4->nvbase.vram_amount == NV4_VRAM_SIZE_16MB)
        {    
            mem_mapping_set_addr(&nv4->nvbase.framebuffer_mapping, nv4->nvbase.bar1_lfb_base, NV4_VRAM_SIZE_16MB);
            mem_mapping_set_addr(&nv4->nvbase.framebuffer_mapping_mirror, nv4->nvbase.bar1_lfb_base + NV4_LFB_MIRROR_START, NV4_VRAM_SIZE_16MB);
        }
        else
        {
            fatal("NV4 4MB not implemented yet"); 
        }
    }

    // Did we change the banked SVGA mode?
    switch (nv4->nvbase.svga.gdcreg[0x06] & 0x0c)
    {
        case NV4_CRTC_BANKED_128K_A0000:
            nv_log("SVGA Banked Mode = 128K @ A0000h\n");
            mem_mapping_set_addr(&nv4->nvbase.svga.mapping, 0xA0000, 0x20000); // 128kb @ 0xA0000
            nv4->nvbase.svga.banked_mask = 0x1FFFF;
            break;
        case NV4_CRTC_BANKED_64K_A0000:
            nv_log("SVGA Banked Mode = 64K @ A0000h\n");
            mem_mapping_set_addr(&nv4->nvbase.svga.mapping, 0xA0000, 0x10000); // 64kb @ 0xA0000
            nv4->nvbase.svga.banked_mask = 0xFFFF;
            break;
        case NV4_CRTC_BANKED_32K_B0000:
            nv_log("SVGA Banked Mode = 32K @ B0000h\n");
            mem_mapping_set_addr(&nv4->nvbase.svga.mapping, 0xB0000, 0x8000); // 32kb @ 0xB0000
            nv4->nvbase.svga.banked_mask = 0x7FFF;
            break;
        case NV4_CRTC_BANKED_32K_B8000:
            nv_log("SVGA Banked Mode = 32K @ B8000h\n");
            mem_mapping_set_addr(&nv4->nvbase.svga.mapping, 0xB8000, 0x8000); // 32kb @ 0xB8000
            nv4->nvbase.svga.banked_mask = 0x7FFF;
            break;
    }
}

// 
// Init code
//
void* nv4_init(const device_t *info)
{
    if (device_get_config_int("nv_debug_fulllog"))
        nv4->nvbase.log = log_open("nv4");
    else
        nv4->nvbase.log = log_open_cyclic("nv4");

#ifdef ENABLE_NV_LOG
    // Allows nv_log to be used for multiple nvidia devices
    nv_log_set_device(nv4->nvbase.log); 
#endif   
    nv_log("Initialising core\n");

    // this will only be logged if ENABLE_NV_LOG_ULTRA is defined
    nv_log_verbose_only("ULTRA LOGGING enabled");


    // Figure out which vbios the user selected
    const char* vbios_id = device_get_config_bios("vbios");
    const char* vbios_file = "";

    // depends on the bus we are using
    if (nv4->nvbase.bus_generation == nv_bus_pci)
        vbios_file = device_get_bios_file(&nv4_device_pci, vbios_id, 0);
    else   
        vbios_file = device_get_bios_file(&nv4_device_agp, vbios_id, 0);

    int32_t err = rom_init(&nv4->nvbase.vbios, vbios_file, 0xC0000, 0x10000, 0xffff, 0, MEM_MAPPING_EXTERNAL);
    
    if (err)
    {
        nv_log("NV4 FATAL: failed to load VBIOS err=%d\n", err);
        fatal("Nvidia NV4 init failed: Somehow selected a nonexistent VBIOS? err=%d\n", err);
        return NULL;
    }
    else    
        nv_log("Successfully loaded VBIOS %s located at %s\n", vbios_id, vbios_file);

    // set the vram amount and gpu revision
    nv4->nvbase.vram_amount = device_get_config_int("vram_size");
    nv4->nvbase.gpu_revision = device_get_config_int("chip_revision");
    
    // set up the bus and start setting up SVGA core
    if (nv4->nvbase.bus_generation == nv_bus_pci)
    {
        nv_log("using PCI bus\n");

        pci_add_card(PCI_ADD_NORMAL, nv4_pci_read, nv4_pci_write, NULL, &nv4->nvbase.pci_slot);

        svga_init(&nv4_device_pci, &nv4->nvbase.svga, nv4, nv4->nvbase.vram_amount, 
        nv4_recalc_timings, nv4_svga_in, nv4_svga_out, nv4_draw_cursor, NULL);

        video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_nv4_pci);

    }
    else if (nv4->nvbase.bus_generation == nv_bus_agp_2x)
    {
        nv_log("using AGP 1X bus\n");

        pci_add_card(PCI_ADD_AGP, nv4_pci_read, nv4_pci_write, NULL, &nv4->nvbase.pci_slot);

        svga_init(&nv4_device_agp, &nv4->nvbase.svga, nv4, nv4->nvbase.vram_amount, 
        nv4_recalc_timings, nv4_svga_in, nv4_svga_out, nv4_draw_cursor, NULL);

        video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_nv4_agp);
    }

    nv4->nvbase.svga.force_old_addr = 1;

    // set vram
    nv_log("VRAM=%d bytes\n", nv4->nvbase.svga.vram_max);

    // init memory mappings
    nv4_init_mappings();

    // make us actually exist
    nv4->pci_config.int_line = 0xFF; // per datasheet
    nv4->pci_config.pci_regs[PCI_REG_COMMAND] = PCI_COMMAND_IO | PCI_COMMAND_MEM;

    // svga is done, so now initialise the real gpu

    nv_log("Initialising GPU core...\n");

    nv4_pextdev_init();             // Initialise Straps
    nv4_pmc_init();                 // Initialise Master Control
    nv4_pbus_init();                // Initialise Bus (the 128 part of riva)
    nv4_pfb_init();                 // Initialise Framebuffer Interface
    nv4_pramdac_init();             // Initialise RAMDAC (CLUT, final pixel presentation etc)
    nv4_pfifo_init();               // Initialise FIFO for graphics object submission
    nv4_pgraph_init();              // Initialise accelerated graphics engine
    nv4_ptimer_init();              // Initialise programmable interval timer
    nv4_pvideo_init();              // Initialise video overlay engine

    nv_log("Initialising I2C...");
    nv4->nvbase.i2c = i2c_gpio_init("nv4_i2c");
    nv4->nvbase.ddc = ddc_init(i2c_gpio_get_bus(nv4->nvbase.i2c));

    return nv4;
}

// This function simply allocates ram and sets the bus to pci before initialising.
void* nv4_init_pci(const device_t* info)
{
    nv4 = (nv4_t*)calloc(1, sizeof(nv4_t));
    nv4->nvbase.bus_generation = nv_bus_pci;
    nv4_init(info);
    return nv4;
}

// This function simply allocates ram and sets the bus to agp before initialising.
void* nv4_init_agp(const device_t* info)
{
    nv4 = (nv4_t*)calloc(1, sizeof(nv4_t));
    nv4->nvbase.bus_generation = nv_bus_agp_2x;
    nv4_init(info);
    return nv4;
}

void nv4_close(void* priv)
{
    // Shut down logging
    log_close(nv4->nvbase.log);
#ifdef ENABLE_NV_LOG
    nv_log_set_device(NULL);
#endif

    // Shut down I2C and the DDC
    ddc_close(nv4->nvbase.ddc);
    i2c_gpio_close(nv4->nvbase.i2c);

    // Destroy the Rivatimers. (It doesn't matter if they are running.)
    rivatimer_destroy(nv4->nvbase.pixel_clock_timer);
    rivatimer_destroy(nv4->nvbase.memory_clock_timer);
    
    // Shut down SVGA
    svga_close(&nv4->nvbase.svga);
    free(nv4);
    nv4 = NULL;
}

// See if the bios rom is available.
int32_t nv4_available(void)
{
    return rom_present(NV4_VBIOS_CREATIVE_RIVATNT)
    || rom_present(NV4_VBIOS_DIAMOND_V550_REVA)
    || rom_present(NV4_VBIOS_DIAMOND_V550_REVB)
    || rom_present(NV4_VBIOS_STB_4400_RIVATNT)
    || rom_present(NV4_VBIOS_SPECTRA_2500_1_03_01);
}

// NV4 (RIVA TNT)
// PCI
// 8MB or 16MB VRAM
const device_t nv4_device_pci = 
{
    .name = "NVidia RIVA TNT (NV4) PCI",
    .internal_name = "nv4",
    .flags = DEVICE_PCI,
    .local = 0,
    .init = nv4_init_pci,
    .close = nv4_close,
    .speed_changed = nv4_speed_changed,
    .force_redraw = nv4_force_redraw,
    .available = nv4_available,
    .config = nv4_config,
};

// NV4 (RIVA TNT)
// AGP
// 8MB or 16MB VRAM
const device_t nv4_device_agp = 
{
    .name = "NVidia RIVA TNT (NV4) AGP",
    .internal_name = "nv4_agp",
    .flags = DEVICE_AGP,
    .local = 0,
    .init = nv4_init_agp,
    .close = nv4_close,
    .speed_changed = nv4_speed_changed,
    .force_redraw = nv4_force_redraw,
    .available = nv4_available,
    .config = nv4_config,
};