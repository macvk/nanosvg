/* 

* GDI ready interface for nanosvg library,

* ABOUT:
* The nanosvg library was written by Mikko Mononen memon@inside.org.
* The GDI integration module gives the ability to use the svg lib in the gdi environment (WM_PAINT)
* integration module was written by https://github.com/macvk.

* USAGE:
* You have one function SvgLoadImage():

* HANDLE SvgLoadImage(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad, HDC bgdc, int bg_x, int bg_y);

* The interface of this func is similar to LoadImage() win32 function.

*    HINSTANCE hInst - module from which the resource is loaded, this param can be NULL
*    LPCSTR *name - either the file name of svg image or the resource ID of svg image
*    UINT type - not used
*    int cx - desired width of the svg, can't be zero
*    int cy - desired high of the svg, can't be zero
*    UINT fuLoad - can be 0 or LR_LOADFROMFILE

* Example 1 (Load svg from file)

#define NANOSVGRAST_IMPLEMENTATION
#define SVG_GDI_IMPLEMENTATION
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "nanosvg_gdi.h"

HBITMAP img = SvgLoadImage(NULL, "test.svg", 0, 100, 100, LR_LOADFROMFILE);
...
DeleteObject(img);

* Example 2 (Load svg from resource)

#define NANOSVGRAST_IMPLEMENTATION
#define SVG_GDI_IMPLEMENTATION
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "nanosvg_gdi.h"

HBITMAP img = SvgLoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_SVG), 0, 100, 100, 0);
...
DeleteObject(img);

*/

#ifndef INCLUDE_NANOSVG_GDI_H
#define INCLUDE_NANOSVG_GDI_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef UNICODE
	extern HBITMAP SvgLoadImage(HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad);
#else
	extern HBITMAP SvgLoadImage(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad);
#endif
		
#ifdef __cplusplus
}
#endif

#endif

#ifdef SVG_GDI_IMPLEMENTATION

#include <windows.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef UNICODE
HBITMAP SvgLoadImage(HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad)
#else
HBITMAP SvgLoadImage(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad)
#endif

{
	UCHAR *svg_data;
	size_t svg_data_size;
	
	float scale;
	int w,h,x,y;

	NSVGrasterizer *rast;
	NSVGimage* svg;
	void *image;

	HBITMAP res;
	HDC hdesktopdc;
	BITMAPV4HEADER bh4;
	int bits;
	int colors;
	void *dib;

	if (fuLoad == LR_LOADFROMFILE)
	{
		FILE *f;
		char buf[1000];
#ifdef UNICODE
		_wfopen_s(&f, name, L"rb");
#else
		fopen_s(&f, name, "rb");
#endif
		if (!f)
		{
			return NULL;
		}
	
		svg_data_size = 0;	
		svg_data = (UCHAR*)malloc(0);

		while (!feof(f))
		{
			size_t n = fread(buf, 1, sizeof(buf), f);
			if (n == 0)
				break;
			svg_data = (UCHAR*)realloc(svg_data, svg_data_size+n);
        
			memcpy(svg_data +svg_data_size , buf, n);
			svg_data_size += n;
		}
		fclose(f);

		svg_data = (UCHAR*)realloc(svg_data, svg_data_size+1);
		svg_data[svg_data_size] = 0;

	}
	else
	{
		HRSRC hres;
		HGLOBAL hload;
		LPVOID lplock;

		hres = FindResource(hInst, name, RT_RCDATA);

		if (!hres)
		{
			return NULL;
		}

		hload = LoadResource(hInst, hres);

		if (!hload)
		{
			return NULL;
		}

		lplock = LockResource(hload); // no need to unlock resource
		if (!lplock)
		{
			return NULL;
		}

		svg_data_size = SizeofResource(hInst, hres);
		svg_data = (UCHAR*)malloc(svg_data_size+1);
		memcpy(svg_data, lplock, svg_data_size);

		svg_data[svg_data_size] = 0;
		
	}
	
	svg = nsvgParse((char*)svg_data, "px", 96);

	if (!svg || svg->height == 0 || svg->width == 0 || cx == 0 || cy == 0)
	{
		free(svg_data); // bad format
		return NULL;
	}

	scale = cx/svg->width;
	if (scale > cy/svg->height)
	{
		scale = cy/svg->height;
	}
	
	w = ceil(svg->width*scale);
	h = ceil(svg->height*scale);

	rast = nsvgCreateRasterizer();
	image = malloc(w*h*4);
	
	nsvgRasterize(rast, svg, 0, 0, scale, (UCHAR*)image, w, h, w * 4);
	
	hdesktopdc = GetDC(GetDesktopWindow());
	if (!hdesktopdc)
	{
		nsvgDeleteRasterizer(rast);
		nsvgDelete(svg);
		free(image);
		free(svg_data);
		return NULL;
	}

	bits = 32;
	colors = 1 << bits;

	ZeroMemory(&bh4, sizeof(bh4));
	bh4.bV4Size = sizeof(bh4);
	bh4.bV4BitCount = 32;
	bh4.bV4Planes = 1;
	bh4.bV4Height = -h;
	bh4.bV4Width = w;
	bh4.bV4V4Compression = BI_BITFIELDS;
	bh4.bV4RedMask =	0x00FF0000;
	bh4.bV4GreenMask =	0x0000FF00;
	bh4.bV4BlueMask =	0x000000FF;
	bh4.bV4AlphaMask =	0xFF000000;
	bh4.bV4CSType =		LCS_CALIBRATED_RGB;

	dib = (LPVOID)image;

//	Corrrect alpha channel
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			RGBQUAD *p = (RGBQUAD *)(((UINT32 *)dib)+x + y * w);
			UCHAR r, g, b, a;

			r = p->rgbBlue;
			g = p->rgbGreen;
			b = p->rgbRed;
			a = p->rgbReserved;

			b = b*a / 255;
			r = r*a / 255;
			g = g*a / 255;

			((UINT32 *)dib)[x + y * w] = (a << 24) |	// 0xaa000000 
				((UCHAR)(r) << 16) |			// 0x00rr0000 
				((UCHAR)(g) << 8) |			// 0x0000gg00 
				((UCHAR)(b));
		}
	}

//	ZeroMemory(&bi, sizeof(bi));
//	bi.bmiHeader = bh4;

	res = CreateDIBitmap(hdesktopdc, (BITMAPINFOHEADER*)&bh4, CBM_INIT, dib, (BITMAPINFO*)&bh4, DIB_RGB_COLORS);

	ReleaseDC(GetDesktopWindow(), hdesktopdc);
	
	nsvgDeleteRasterizer(rast);
	nsvgDelete(svg);
	free(image);
	free(svg_data);
	
	return res;	
}

#endif 
