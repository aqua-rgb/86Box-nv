/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Provides NV3 configuration
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
#include <86box/nv/vid_nv3.h>

const device_config_t nv4_config[] =
{
    // VBIOS type configuration
    {
        .name = "vbios",
        #ifndef RELEASE_BUILD
        .description = "VBIOS",
        #else
        .description = "Model",
        #endif 
        .type = CONFIG_BIOS,
        .default_string = "NV4_VBIOS_SPECTRA_2500_1_03_01",
        .default_int = 0,
        .bios = 
        {
           { 
            #ifndef RELEASE_BUILD
                .name = "[NV4] Creative Graphics Blaster CT6710", .files_no = 1,
            #else
                .name = "[RIVA TNT] Creative Graphics Blaster RIVA TNT", .files_no = 1,
            #endif
                .internal_name = "NV4_VBIOS_CREATIVE_RIVATNT",
                .files = {NV4_VBIOS_CREATIVE_RIVATNT, ""} 
           },
           { 
            #ifndef RELEASE_BUILD
                .name = "[NV4] Diamond Multimedia Systems, Inc. Viper V550 (rev A)", .files_no = 1,
            #else
                .name = "[RIVA TNT] Diamond Viper V550 (rev A)", .files_no = 1,
            #endif
                .internal_name = "NV4_VBIOS_DIAMOND_V550_REVA",
                .files = {NV4_VBIOS_DIAMOND_V550_REVA, ""} 
           },
           { 
            #ifndef RELEASE_BUILD
                .name = "[NV4] Diamond Multimedia Systems, Inc. Viper V550 (rev B)", .files_no = 1,
            #else
                .name = "[RIVA TNT] Diamond Viper V550 (rev B)", .files_no = 1,
            #endif
                .internal_name = "NV4_VBIOS_DIAMOND_V550_REVB",
                .files = {NV4_VBIOS_DIAMOND_V550_REVB, ""} 
           },
           {
            #ifndef RELEASE_BUILD
                .name = "[NV4] STB Velocity 4400 (RIVA TNT)", .files_no = 1,
            #else
                .name = "[RIVA TNT] STB Velocity 4400", .files_no = 1,
            #endif
                .internal_name = "NV4_VBIOS_STB_4400_RIVATNT",
                .files = {NV4_VBIOS_STB_4400_RIVATNT, ""},
           },
           {
            #ifndef RELEASE_BUILD
                .name = "[NV4] Canopus Spectra 2500 (ver 1.03.01)", .files_no = 1,
            #else
                .name = "[RIVA TNT] Spectra 2500", .files_no = 1,
            #endif
                .internal_name = "NV4_VBIOS_SPECTRA_2500_1_03_01",
                .files = {NV4_VBIOS_SPECTRA_2500_1_03_01, ""},
           },
        }
    },
    // Memory configuration
    {
        .name = "vram_size",
        .description = "VRAM Size",
        .type = CONFIG_SELECTION,
        .default_int = NV4_VRAM_SIZE_16MB,
        .selection = 
        {
            {
                .description = "8 MB",
                .value = NV4_VRAM_SIZE_8MB,
            },
            {
                .description = "16 MB",
                .value = NV4_VRAM_SIZE_16MB,
            },
        }

    },
    {
        .name = "chip_revision",
        .description = "Chip Revision",
        .type = CONFIG_SELECTION,
        .default_int = NV4_PCI_CFG_REVISION_B00,
        .selection = 
        {
            {
               .description = "RIVA TNT (Revision A)",
               .value = NV4_PCI_CFG_REVISION_A00,
            },
            {
               .description = "RIVA TNT (Revision B)",
               .value = NV4_PCI_CFG_REVISION_B00,
            },
            {
               .description = "RIVA TNT (Revision C)",
               .value = NV4_PCI_CFG_REVISION_C00,
            },
        }
    },
    // Multithreading configuration
    {

        .name = "pgraph_threads",
#ifndef RELEASE_BUILD
        .description = "PFIFO/PGRAPH - Number of threads to split large object method execution into",
#else
        .description = "Render threads",
#endif 
        .type = CONFIG_SELECTION,
        .default_int = 1, // todo: change later
        .selection = 
        {
            {
                .description = "1 thread (Only use if issues appear with more threads)",
                .value = 1,
            },
            {
                .description = "2 threads",
                .value = 2,
            },
            {   
                .description = "4 threads",
                .value = 4,
            },
            {
                .description = "8 threads",
                .value = 8,
            },
        },
    },
#ifndef RELEASE_BUILD
    {
        .name = "nv_debug_fulllog",
        .description = "Disable Cyclical Lines Detection for nv_log (Use for getting full context at cost of VERY large log files)",
        .type = CONFIG_BINARY,
        .default_int = 0,
    },
#endif
    {
        .type = CONFIG_END
    }
};