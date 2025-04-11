/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          nv4 headers for rendering 
 *
 * 
 * 
 * Authors: Connor Hyde, <mario64crashed@gmail.com> I need a better email address ;^)
 *
 *          Copyright 2024-2025 Connor Hyde
 */

#pragma once

/* Core */
void nv4_render_current_bpp(svga_t *svga, nv4_position_16_t position, nv4_size_16_t size, nv4_grobj_t grobj);
void nv4_render_current_bpp_dfb_8(uint32_t address);
void nv4_render_current_bpp_dfb_16(uint32_t address);
void nv4_render_current_bpp_dfb_32(uint32_t address);

void nv4_render_write_pixel(nv4_position_16_t position, uint32_t color, nv4_grobj_t grobj);
uint8_t nv4_render_read_pixel_8(nv4_position_16_t position, nv4_grobj_t grobj);
uint16_t nv4_render_read_pixel_16(nv4_position_16_t position, nv4_grobj_t grobj);
uint32_t nv4_render_read_pixel_32(nv4_position_16_t position, nv4_grobj_t grobj);


uint32_t nv4_render_get_vram_address(nv4_position_16_t position, nv4_grobj_t grobj);

uint32_t nv4_render_to_chroma(nv4_color_expanded_t expanded);
nv4_color_expanded_t nv4_render_expand_color(uint32_t color, nv4_grobj_t grobj);            // Convert a colour to full RGB10 format from the current working format.
uint32_t nv4_render_downconvert_color(nv4_grobj_t grobj, nv4_color_expanded_t color);       // Convert a colour from the current working format to RGB10 format.

/* Pattern */
uint32_t nv4_render_set_pattern_color(nv4_color_expanded_t pattern_colour, bool use_color1);

/* Primitives */
void nv4_render_rect(nv4_position_16_t position, nv4_size_16_t size, uint32_t color, nv4_grobj_t grobj);                    // Render an A (unclipped) GDI rect
void nv4_render_rect_clipped(nv4_clip_16_t clip, uint32_t color, nv4_grobj_t grobj);                                        // Render a B (clipped) GDI rect.

/* Chroma */
bool nv4_render_chroma_test(uint32_t color, nv4_grobj_t grobj);

/* Blit */
void nv4_render_blit_image(uint32_t color, nv4_grobj_t grobj);
void nv4_render_blit_screen2screen(nv4_grobj_t grobj);

/* GDI */
void nv4_render_gdi_transparent_bitmap(bool clip, uint32_t color, uint32_t bitmap_data, nv4_grobj_t grobj);
void nv4_render_gdi_1bpp_bitmap(uint32_t color0, uint32_t color1, uint32_t bitmap_data, nv4_grobj_t grobj);                               /* GDI Type-E: Clipped 1bpp colour-expanded bitmap */