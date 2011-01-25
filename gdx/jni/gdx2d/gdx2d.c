/*
 * Copyright 2010 Mario Zechner (contact@badlogicgames.com), Nathan Sweet (admin@esotericsoftware.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS"
 * BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#include "gdx2d.h"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBI_HEADER_FILE_ONLY
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.c"
#include "stb_truetype.h"

static uint32_t gdx2d_blend = GDX2D_BLEND_NONE;
static uint32_t gdx2d_scale = GDX2D_SCALE_NEAREST;

static uint32_t* lu4 = 0;
static uint32_t* lu5 = 0;
static uint32_t* lu6 = 0;

typedef void(*set_pixel_func)(unsigned char* pixel_addr, uint32_t color);
typedef uint32_t(*get_pixel_func)(unsigned char* pixel_addr);

inline void generate_look_ups() {
	uint32_t i = 0;
	lu4 = malloc(sizeof(uint32_t) * 16);
	lu5 = malloc(sizeof(uint32_t) * 32);
	lu6 = malloc(sizeof(uint32_t) * 64);

	for(i = 0; i < 16; i++) {
		lu4[i] = (uint32_t) i / 15.0f * 255;
		lu5[i] = (uint32_t) i / 31.0f * 255;
		lu6[i] = (uint32_t) i / 63.0f * 255;
	}

	for(i = 16; i < 32; i++) {
		lu5[i] = (uint32_t) i / 31.0f * 255;
		lu6[i] = (uint32_t) i / 63.0f * 255;
	}

	for(i = 32; i < 64; i++) {
		lu6[i] = (uint32_t) i / 63.0f * 255;
	}
}

inline uint32_t to_format(uint32_t format, uint32_t color) {
	uint32_t r, g, b, a, l;

	switch(format) {
		case GDX2D_FORMAT_ALPHA: 
			return color & 0xff;
		case GDX2D_FORMAT_LUMINANCE_ALPHA: 
			r = (color & 0xff000000) >> 24;
			g = (color & 0xff000000) >> 16;
			b = (color & 0xff000000) >> 8;
			a = (color & 0xff);
			l = ((uint32_t)(0.2126f * r + 0.7152 * g + 0.0722 * b) & 0xff) << 8;
			return l | a;
		case GDX2D_FORMAT_RGB888:
			return color >> 8;
		case GDX2D_FORMAT_RGBA8888:
			return color;
		case GDX2D_FORMAT_RGB565: 
			r = (((color & 0xff000000) >> 27) << 11) & 0xf800;
			g = (((color & 0xff0000) >> 18) << 5) & 0x7e0;
			b = ((color & 0xff00) >> 11) & 0x1f;
			return r | g | b;
		case GDX2D_FORMAT_RGBA4444:
			r = (((color & 0xff000000) >> 28) << 12) & 0xf000;
			g = (((color & 0xff0000) >> 20) << 8) & 0xf00;
			b = (((color & 0xff00) >> 12) << 4) & 0xf0;
			a = ((color & 0xff) >> 4) & 0xf;
			return r | g | b | a;
		default: 
			return 0;
	}
}

inline uint32_t to_RGBA8888(uint32_t format, uint32_t color) {
	uint32_t r, g, b, a;

	if(!lu5) generate_look_ups();

	switch(format) {
		case GDX2D_FORMAT_ALPHA: 
			return (color & 0xff) | (color & 0xff) << 8 | ((color & 0xff) << 16) | ((color & 0xff) << 24);
		case GDX2D_FORMAT_LUMINANCE_ALPHA: 
			return ((color & 0xff00) << 16) | ((color & 0xff00) << 8) | (color & 0xffff);
		case GDX2D_FORMAT_RGB888:
			return (color << 8) | 0x000000ff;
		case GDX2D_FORMAT_RGBA8888:
			return color;
		case GDX2D_FORMAT_RGB565:
			r = lu5[(color & 0xf800) >> 11] << 24;
			g = lu6[(color & 0x7e0) >> 5] << 16;
			b = lu5[(color & 0x1f)] << 8;
			return r | g | b | 0xff;
		case GDX2D_FORMAT_RGBA4444:
			r = lu4[(color & 0xf000) >> 12] << 24;
			g = lu4[(color & 0xf00) >> 8] << 16;
			b = lu4[(color & 0xf0) >> 4] << 8;
			a = lu4[(color & 0xf)];
			return r | g | b | a;
		default: 
			return 0;
	}
}

inline void set_pixel_alpha(unsigned char *pixel_addr, uint32_t color) {
	*pixel_addr = (unsigned char)(color & 0xff);
}

inline void set_pixel_luminance_alpha(unsigned char *pixel_addr, uint32_t color) {	
	*(unsigned short*)pixel_addr = (unsigned short)color;
}

inline void set_pixel_RGB888(unsigned char *pixel_addr, uint32_t color) {	
	//*(unsigned short*)pixel_addr = (unsigned short)(((color & 0xff0000) >> 16) | (color & 0xff00));
	pixel_addr[0] = (color & 0xff0000) >> 16;
	pixel_addr[1] = (color & 0xff00) >> 8;
	pixel_addr[2] = (color & 0xff);
}

inline void set_pixel_RGBA8888(unsigned char *pixel_addr, uint32_t color) {			
	*(uint32_t*)pixel_addr = ((color & 0xff000000) >> 24) |
							((color & 0xff0000) >> 8) |
							((color & 0xff00) << 8) |
							((color & 0xff) << 24);
}

inline void set_pixel_RGB565(unsigned char *pixel_addr, uint32_t color) {
	*(uint16_t*)pixel_addr = (uint16_t)(color);
}

inline void set_pixel_RGBA4444(unsigned char *pixel_addr, uint32_t color) {	
	*(uint16_t*)pixel_addr = (uint16_t)(color);	
}

inline set_pixel_func set_pixel_func_ptr(uint32_t format) {
	switch(format) {
		case GDX2D_FORMAT_ALPHA:			return &set_pixel_alpha;
		case GDX2D_FORMAT_LUMINANCE_ALPHA:	return &set_pixel_luminance_alpha;
		case GDX2D_FORMAT_RGB888:			return &set_pixel_RGB888;
		case GDX2D_FORMAT_RGBA8888:			return &set_pixel_RGBA8888;
		case GDX2D_FORMAT_RGB565:			return &set_pixel_RGB565;
		case GDX2D_FORMAT_RGBA4444:			return &set_pixel_RGBA4444;
		default: return &set_pixel_alpha; // better idea for a default?
	}
}

inline uint32_t blend(uint32_t src, uint32_t dst) {
	int32_t src_r = (src & 0xff000000) >> 24;
	int32_t src_g = (src & 0xff0000) >> 16;
	int32_t src_b = (src & 0xff00) >> 8;
	int32_t src_a = (src & 0xff);
		
	int32_t dst_r = (dst & 0xff000000) >> 24;
	int32_t dst_g = (dst & 0xff0000) >> 16;
	int32_t dst_b = (dst & 0xff00) >> 8;
		
	dst_r = dst_r + src_a * (src_r - dst_r) / 255;
	dst_g = dst_g + src_a * (src_g - dst_g) / 255;
	dst_b = dst_b + src_a * (src_b - dst_b) / 255;
	return (uint32_t)((dst_r << 24) | (dst_g << 16) | (dst_b << 8) | src_a);
}

inline uint32_t get_pixel_alpha(unsigned char *pixel_addr) {
	return *pixel_addr;
}

inline uint32_t get_pixel_luminance_alpha(unsigned char *pixel_addr) {
	return (((uint32_t)pixel_addr[0]) << 8) | pixel_addr[1];
}

inline uint32_t get_pixel_RGB888(unsigned char *pixel_addr) {
	return (((uint32_t)pixel_addr[0]) << 16) | (((uint32_t)pixel_addr[1]) << 8) | (pixel_addr[2]);
}

inline uint32_t get_pixel_RGBA8888(unsigned char *pixel_addr) {	
	return (((uint32_t)pixel_addr[0]) << 24) | (((uint32_t)pixel_addr[1]) << 16) | (((uint32_t)pixel_addr[2]) << 8) | pixel_addr[3];
}

inline uint32_t get_pixel_RGB565(unsigned char *pixel_addr) {
	return *(uint16_t*)pixel_addr;
}

inline uint32_t get_pixel_RGBA4444(unsigned char *pixel_addr) {
	return *(uint16_t*)pixel_addr;
}

inline get_pixel_func get_pixel_func_ptr(uint32_t format) {
	switch(format) {
		case GDX2D_FORMAT_ALPHA:			return &get_pixel_alpha;
		case GDX2D_FORMAT_LUMINANCE_ALPHA:	return &get_pixel_luminance_alpha;
		case GDX2D_FORMAT_RGB888:			return &get_pixel_RGB888;
		case GDX2D_FORMAT_RGBA8888:			return &get_pixel_RGBA8888;
		case GDX2D_FORMAT_RGB565:			return &get_pixel_RGB565;
		case GDX2D_FORMAT_RGBA4444:			return &get_pixel_RGBA4444;
		default: return &get_pixel_alpha; // better idea for a default?
	}
}

gdx2d_pixmap* gdx2d_load(const unsigned char *buffer, uint32_t len, uint32_t req_format) {
	int32_t width, height, format;
	// TODO fix this! Add conversion to requested format
	if(req_format > GDX2D_FORMAT_RGBA8888) 
		req_format = GDX2D_FORMAT_RGBA8888;
	const unsigned char* pixels = stbi_load_from_memory(buffer, len, &width, &height, &format, req_format);
	if(pixels == NULL)
		return NULL;

	gdx2d_pixmap* pixmap = (gdx2d_pixmap*)malloc(sizeof(gdx2d_pixmap));
	pixmap->width = (uint32_t)width;
	pixmap->height = (uint32_t)height;
	pixmap->format = (uint32_t)format;
	pixmap->pixels = pixels;
	return pixmap;
}

inline uint32_t bytes_per_pixel(uint32_t format) {
	switch(format) {
		case GDX2D_FORMAT_ALPHA:
			return 1;
		case GDX2D_FORMAT_LUMINANCE_ALPHA:
		case GDX2D_FORMAT_RGB565:
		case GDX2D_FORMAT_RGBA4444:
			return 2;
		case GDX2D_FORMAT_RGB888:
			return 3;
		case GDX2D_FORMAT_RGBA8888:
			return 4;
		default:
			return 4;
	}
}

gdx2d_pixmap* gdx2d_new(uint32_t width, uint32_t height, uint32_t format) {
	gdx2d_pixmap* pixmap = (gdx2d_pixmap*)malloc(sizeof(gdx2d_pixmap));
	pixmap->width = width;
	pixmap->height = height;
	pixmap->format = format;
	pixmap->pixels = (unsigned char*)malloc(width * height * bytes_per_pixel(format));
	return pixmap;
}
void gdx2d_free(const gdx2d_pixmap* pixmap) {
	free((void*)pixmap->pixels);
	free((void*)pixmap);
}

void gdx2d_set_blend (uint32_t blend) {
	gdx2d_blend = blend;
}

void gdx2d_set_scale (uint32_t scale) {
	gdx2d_scale = scale;
}

inline void clear_alpha(const gdx2d_pixmap* pixmap, uint32_t col) {
	int pixels = pixmap->width * pixmap->height;
	memset((void*)pixmap->pixels, col, pixels);
}

inline void clear_luminance_alpha(const gdx2d_pixmap* pixmap, uint32_t col) {
	int pixels = pixmap->width * pixmap->height;
	unsigned short* ptr = (unsigned short*)pixmap->pixels;
	unsigned short l = (col & 0xff) << 8 | (col >> 8);	

	for(; pixels > 0; pixels--) {
		*ptr = l;				
		ptr++;
	}
}

inline void clear_RGB888(const gdx2d_pixmap* pixmap, uint32_t col) {
	int pixels = pixmap->width * pixmap->height;
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	unsigned char r = (col & 0xff0000) >> 16;
	unsigned char g = (col & 0xff00) >> 8;
	unsigned char b = (col & 0xff);

	for(; pixels > 0; pixels--) {
		*ptr = r;
		ptr++;
		*ptr = g;
		ptr++;
		*ptr = b;
		ptr++;
	}
}

inline void clear_RGBA8888(const gdx2d_pixmap* pixmap, uint32_t col) {
	int pixels = pixmap->width * pixmap->height;
	uint32_t* ptr = (uint32_t*)pixmap->pixels;
	unsigned char r = (col & 0xff000000) >> 24;
	unsigned char g = (col & 0xff0000) >> 16;
	unsigned char b = (col & 0xff00) >> 8;
	unsigned char a = (col & 0xff);
	col = (a << 24) | (b << 16) | (g << 8) | r;

	for(; pixels > 0; pixels--) {
		*ptr = col;
		ptr++;		
	}
}

inline void clear_RGB565(const gdx2d_pixmap* pixmap, uint32_t col) {
	uint32_t pixels = pixmap->width * pixmap->height;
	uint32_t left = pixels % 2;
	pixels >>= 1;
	uint32_t* ptr = (uint32_t*)pixmap->pixels;
	uint32_t c = ((col & 0xffff) << 16) | (col & 0xffff);

	for(; pixels > 0; pixels--, ptr++) {
		*ptr = c;		
	}
	if(left) {
		*((uint16_t*)pixmap->pixels + pixmap->width * pixmap->height * 2) = (uint16_t)col;
	}
}

inline void clear_RGBA4444(const gdx2d_pixmap* pixmap, uint32_t col) {
	uint32_t pixels = pixmap->width * pixmap->height;
	uint32_t left = pixels % 2;
	pixels >>= 1;
	uint32_t* ptr = (uint32_t*)pixmap->pixels;
	uint32_t c = ((col & 0xffff) << 16) | (col & 0xffff);

	for(; pixels > 0; pixels--, ptr++) {
		*ptr = c;		
	}
	if(left) {
		*((uint16_t*)pixmap->pixels + pixmap->width * pixmap->height * 2) = (uint16_t)col;
	}
}

void gdx2d_clear(const gdx2d_pixmap* pixmap, uint32_t col) {	
	col = to_format(pixmap->format, col);

	switch(pixmap->format) {
		case GDX2D_FORMAT_ALPHA:
			clear_alpha(pixmap, col);
			break;
		case GDX2D_FORMAT_LUMINANCE_ALPHA:
			clear_luminance_alpha(pixmap, col);
			break;
		case GDX2D_FORMAT_RGB888:
			clear_RGB888(pixmap, col);
			break;
		case GDX2D_FORMAT_RGBA8888:
			clear_RGBA8888(pixmap, col);
			break;
		case GDX2D_FORMAT_RGB565:
			clear_RGB565(pixmap, col);
			break;
		case GDX2D_FORMAT_RGBA4444:
			clear_RGBA4444(pixmap, col);
			break;
		default:
			break;
	}
}

inline int32_t in_pixmap(const gdx2d_pixmap* pixmap, int32_t x, int32_t y) {
	if(x < 0 || y < 0)
		return 0;
	if(x >= pixmap->width || y >= pixmap->height)
		return 0;
	return -1;
}

inline void set_pixel(unsigned char* pixels, uint32_t width, uint32_t height, uint32_t bpp, set_pixel_func pixel_func, int32_t x, int32_t y, uint32_t col) {
	if(x < 0 || y < 0) return;
	if(x >= width || y >= height) return;
	pixels = pixels + (x + height * y) * bpp;
	pixel_func(pixels, col);
}

uint32_t gdx2d_get_pixel(const gdx2d_pixmap* pixmap, int32_t x, int32_t y) {
	if(!in_pixmap(pixmap, x, y))
		return 0;
	unsigned char* ptr = (unsigned char*)pixmap->pixels + (x + pixmap->height * y) * bytes_per_pixel(pixmap->format);
	return to_RGBA8888(pixmap->format, get_pixel_func_ptr(pixmap->format)(ptr));
}

void gdx2d_set_pixel(const gdx2d_pixmap* pixmap, int32_t x, int32_t y, uint32_t col) {
	if(gdx2d_blend) {
		uint32_t dst = gdx2d_get_pixel(pixmap, x, y);
		col = blend(col, dst);
		col = to_format(pixmap->format, col);
		set_pixel((unsigned char*)pixmap->pixels, pixmap->width, pixmap->height, bytes_per_pixel(pixmap->format), set_pixel_func_ptr(pixmap->format), x, y, col);
	} else {
		col = to_format(pixmap->format, col);
		set_pixel((unsigned char*)pixmap->pixels, pixmap->width, pixmap->height, bytes_per_pixel(pixmap->format), set_pixel_func_ptr(pixmap->format), x, y, col);
	}
}

void gdx2d_draw_line(const gdx2d_pixmap* pixmap, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t col) {	
    int32_t dy = y1 - y0;
    int32_t dx = x1 - x0;
	int32_t fraction = 0;
    int32_t stepx, stepy;
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	uint32_t bpp = bytes_per_pixel(pixmap->format);
	set_pixel_func pset = set_pixel_func_ptr(pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(pixmap->format);
	uint32_t col_format = to_format(pixmap->format, col);
	void* addr = ptr + (x0 + y0) * bpp;

    if (dy < 0) { dy = -dy;  stepy = -1; } else { stepy = 1; }
    if (dx < 0) { dx = -dx;  stepx = -1; } else { stepx = 1; }
    dy <<= 1;
    dx <<= 1;    

    if(gdx2d_blend) {
    	col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(addr))));
    }
	pset(addr, col_format);
    if (dx > dy) {
        fraction = dy - (dx >> 1);
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx;
            }
            x0 += stepx;
            fraction += dy;
			if(in_pixmap(pixmap, x0, y0)) {
				addr = ptr + (x0 + y0 * pixmap->width) * bpp;
				if(gdx2d_blend) {
					col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(addr))));
				}
				pset(addr, col_format);
			}
        }
    } else {
		fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			if(in_pixmap(pixmap, x0, y0)) {
				addr = ptr + (x0 + y0 * pixmap->width) * bpp;
				if(gdx2d_blend) {
					col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(addr))));
				}
				pset(addr, col_format);
			}
		}
	}
}

inline void hline(const gdx2d_pixmap* pixmap, int32_t x1, int32_t x2, int32_t y, uint32_t col) {
	int32_t tmp = 0;
	set_pixel_func pset = set_pixel_func_ptr(pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(pixmap->format);
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	uint32_t bpp = bytes_per_pixel(pixmap->format);
	uint32_t col_format = to_format(pixmap->format, col);

	if(y < 0 || y >= pixmap->height) return;

	if(x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	if(x1 >= pixmap->width) return;
	if(x2 < 0) return;

	if(x1 < 0) x1 = 0;
	if(x2 >= pixmap->width) x2 = pixmap->width - 1;	
	x2 += 1;
	
	ptr += (x1 + y * pixmap->width) * bpp;

	while(x1 != x2) {
		if(gdx2d_blend) {
			col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(ptr))));
		}
		pset(ptr, col_format);
		x1++;
		ptr += bpp;
	}
}

inline void vline(const gdx2d_pixmap* pixmap, int32_t y1, int32_t y2, int32_t x, uint32_t col) {
	int32_t tmp = 0;
	set_pixel_func pset = set_pixel_func_ptr(pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(pixmap->format);
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	uint32_t bpp = bytes_per_pixel(pixmap->format);
	uint32_t stride = bpp * pixmap->width;
	uint32_t col_format = to_format(pixmap->format, col);

	if(x < 0 || x >= pixmap->width) return;

	if(y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	if(y1 >= pixmap->height) return;
	if(y2 < 0) return;

	if(y1 < 0) y1 = 0;
	if(y2 >= pixmap->height) y2 = pixmap->height - 1;	
	y2 += 1;

	ptr += (x + y1 * pixmap->width) * bpp;

	while(y1 != y2) {
		if(gdx2d_blend) {
			col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(ptr))));
		}
		pset(ptr, col_format);
		y1++;
		ptr += stride;
	}
}

void gdx2d_draw_rect(const gdx2d_pixmap* pixmap, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t col) {
	hline(pixmap, x, x + width - 1, y, col);
	hline(pixmap, x, x + width - 1, y + height - 1, col);
	vline(pixmap, y, y + height - 1, x, col);
	vline(pixmap, y, y + height - 1, x + width - 1, col);
}

inline void circle_points(unsigned char* pixels, uint32_t width, uint32_t height, uint32_t bpp, set_pixel_func pixel_func, int32_t cx, int32_t cy, int32_t x, int32_t y, uint32_t col) {	        
    if (x == 0) {
        set_pixel(pixels, width, height, bpp, pixel_func, cx, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + y, cy, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - y, cy, col);
    } else 
    if (x == y) {
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy - y, col);
    } else 
    if (x < y) {
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + y, cy + x, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - y, cy + x, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + y, cy - x, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - y, cy - x, col);
    }
}

void gdx2d_draw_circle(const gdx2d_pixmap* pixmap, int32_t x, int32_t y, uint32_t radius, uint32_t col) {	
    int32_t px = 0;
    int32_t py = radius;
    int32_t p = (5 - (int32_t)radius*4)/4;
	unsigned char* pixels = (unsigned char*)pixmap->pixels;
	uint32_t width = pixmap->width;
	uint32_t height = pixmap->height;
	uint32_t bpp = bytes_per_pixel(pixmap->format);
	set_pixel_func pixel_func = set_pixel_func_ptr(pixmap->format);
	col = to_format(pixmap->format, col);

    circle_points(pixels, width, height, bpp, pixel_func, x, y, px, py, col);
    while (px < py) {
        px++;
        if (p < 0) {
            p += 2*px+1;
        } else {
            py--;
            p += 2*(px-py)+1;
        }
        circle_points(pixels, width, height, bpp, pixel_func, x, y, px, py, col);
    }
}

void gdx2d_fill_rect(const gdx2d_pixmap* pixmap, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t col) {
	int32_t x2 = x + width - 1;
	int32_t y2 = y + height - 1;

	if(x >= pixmap->width) return;
	if(y >= pixmap->height) return;
	if(x2 < 0) return;
	if(y2 < 0) return;

	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(x2 >= pixmap->width) x2 = pixmap->width - 1;
	if(y2 >= pixmap->height) y2 = pixmap->height - 1;

	y2++;
	while(y!=y2) {
		hline(pixmap, x, x2, y, col);
		y++;
	}
}

void gdx2d_fill_circle(const gdx2d_pixmap* pixmap, int32_t x0, int32_t y0, uint32_t radius, uint32_t col) {
	int32_t f = 1 - (int32_t)radius;
	int32_t ddF_x = 1;
	int32_t ddF_y = -2 * (int32_t)radius;
	int32_t px = 0;
	int32_t py = (int32_t)radius;

	hline(pixmap, x0, x0, y0 + (int32_t)radius, col);
	hline(pixmap, x0, x0, y0 - (int32_t)radius, col);
	hline(pixmap, x0 - (int32_t)radius, x0 + (int32_t)radius, y0, col);


	while(px < py)
	{	
		if(f >= 0) 
		{
			py--;
			ddF_y += 2;
			f += ddF_y;
		}
		px++;
		ddF_x += 2;
		f += ddF_x;
		hline(pixmap, x0 - px, x0 + px, y0 + py, col);
		hline(pixmap, x0 - px, x0 + px, y0 - py, col);
		hline(pixmap, x0 - py, x0 + py, y0 + px, col);
		hline(pixmap, x0 - py, x0 + py, y0 - px, col);
	}
}

void blit_same_size(const gdx2d_pixmap* src_pixmap, const gdx2d_pixmap* dst_pixmap, 
						 			 int32_t src_x, int32_t src_y, 
									 int32_t dst_x, int32_t dst_y, 
									 uint32_t width, uint32_t height) {	
	set_pixel_func pset = set_pixel_func_ptr(dst_pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(src_pixmap->format);
	get_pixel_func dpget = get_pixel_func_ptr(dst_pixmap->format);
	uint32_t sbpp = bytes_per_pixel(src_pixmap->format);
	uint32_t dbpp = bytes_per_pixel(dst_pixmap->format);
	uint32_t spitch = sbpp * src_pixmap->width;
	uint32_t dpitch = dbpp * dst_pixmap->width;

	int sx = src_x;
	int sy = src_y;
	int dx = dst_x;
	int dy = dst_y;

	for(;sy < src_y + height; sy++, dy++) {
		if(sy < 0 || dy < 0) continue;
		if(sy >= src_pixmap->height || dy >= dst_pixmap->height) break;
		
		for(sx = src_x, dx = dst_x; sx < src_x + width; sx++, dx++) {
			if(sx < 0 || dx < 0) continue;
			if(sx >= src_pixmap->width || dx >= dst_pixmap->width) break;

			const void* src_ptr = src_pixmap->pixels + sx * sbpp + sy * spitch;
			const void* dst_ptr = dst_pixmap->pixels + dx * dbpp + dy * dpitch;
			uint32_t src_col = to_RGBA8888(src_pixmap->format, pget((void*)src_ptr));

			if(gdx2d_blend) {
				uint32_t dst_col = to_RGBA8888(dst_pixmap->format, dpget((void*)dst_ptr));
				src_col = to_format(dst_pixmap->format, blend(src_col, dst_col));
			} else {
				src_col = to_format(dst_pixmap->format, src_col); 
			}
			
			pset((void*)dst_ptr, src_col);
		}
	}
}

void blit(const gdx2d_pixmap* src_pixmap, const gdx2d_pixmap* dst_pixmap,
					   int32_t src_x, int32_t src_y, uint32_t src_width, uint32_t src_height,
					   int32_t dst_x, int32_t dst_y, uint32_t dst_width, uint32_t dst_height) {
}

void gdx2d_draw_pixmap(const gdx2d_pixmap* src_pixmap, const gdx2d_pixmap* dst_pixmap,
					   int32_t src_x, int32_t src_y, uint32_t src_width, uint32_t src_height,
					   int32_t dst_x, int32_t dst_y, uint32_t dst_width, uint32_t dst_height) {
	if(src_width == dst_width && src_height == dst_height) {
		blit_same_size(src_pixmap, dst_pixmap, src_x, src_y, dst_x, dst_y, src_width, src_height);
	} else {
		blit(src_pixmap, dst_pixmap, src_x, src_y, src_width, src_height, dst_x, dst_y, dst_width, dst_height);
	}
}
