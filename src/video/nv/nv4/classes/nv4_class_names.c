/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          NV4: Defines core class names for debugging purposes
 *
 *
 *
 * Authors: Connor Hyde, <mario64crashed@gmail.com> I need a better email address ;^)
 *
 *          Copyright 2024-2025 Connor Hyde
 */
#include <stdio.h>
#include <stdint.h>
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/rom.h> // DEPENDENT!!!
#include <86box/video.h>
#include <86box/nv/vid_nv.h>
#include <86box/nv/vid_nv4.h>

/* These are the object classes AS RECOGNISED BY THE GRAPHICS HARDWARE. */
/* The drivers implement a COMPLETELY DIFFERENT SET OF CLASSES. */

/* THERE CAN ONLY BE 32 CLASSES IN NV4 BECAUSE THE CLASS ID PART OF THE CONTEXT OF A GRAPHICS OBJECT IN PFIFO RAM HASH TABLE IS ONLY 5 BITS LONG! */

const char* nv4_class_names[] = 
{
    "NV4 class 0x0012: Beta factor",
    "NV4 class 0x0043: Render operation",
    "NV4 class 0x0057: Chroma key",
    "NV4 class 0x0019: Clipping rectangle",
    "NV4 class 0x0044: Pattern",
    "NV4 class 0x005E: Rectangle",
    "NV4 class 0x005C: Lin (line without starting or ending pixel)",
    "NV4 class 0x005D: Triangle",
    "NV4 class 0x0039: Memory to memory format",
    "NV4 class 0x005F: Blit",
    "NV4 class 0x0061: Image",
    "NV4 class 0x0054: Direct3D 5.0 textured triangle",
    "NV4 class 0x0055: Direct3D 6.0 multitextured triangle",
};