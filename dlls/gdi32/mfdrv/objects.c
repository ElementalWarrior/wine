/*
 * GDI objects
 *
 * Copyright 1993 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wine/wingdi16.h"
#include "mfdrv/metafiledrv.h"
#include "ntgdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(metafile);

/******************************************************************
 *         MFDRV_AddHandle
 */
UINT MFDRV_AddHandle( PHYSDEV dev, HGDIOBJ obj )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    UINT16 index;

    for(index = 0; index < physDev->handles_size; index++)
        if(physDev->handles[index] == 0) break; 
    if(index == physDev->handles_size) {
        physDev->handles_size += HANDLE_LIST_INC;
        physDev->handles = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       physDev->handles,
                                       physDev->handles_size * sizeof(physDev->handles[0]));
    }
    physDev->handles[index] = get_full_gdi_handle( obj );

    physDev->cur_handles++; 
    if(physDev->cur_handles > physDev->mh->mtNoObjects)
        physDev->mh->mtNoObjects++;

    return index ; /* index 0 is not reserved for metafiles */
}

/******************************************************************
 *         MFDRV_RemoveHandle
 */
BOOL MFDRV_RemoveHandle( PHYSDEV dev, UINT index )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    BOOL ret = FALSE;

    if (index < physDev->handles_size && physDev->handles[index])
    {
        physDev->handles[index] = 0;
        physDev->cur_handles--;
        ret = TRUE;
    }
    return ret;
}

/******************************************************************
 *         MFDRV_FindObject
 */
static INT16 MFDRV_FindObject( PHYSDEV dev, HGDIOBJ obj )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    INT16 index;

    for(index = 0; index < physDev->handles_size; index++)
        if(physDev->handles[index] == obj) break;

    if(index == physDev->handles_size) return -1;

    return index ;
}


/******************************************************************
 *         METADC_DeleteObject
 */
void METADC_DeleteObject( HDC hdc, HGDIOBJ obj )
{
    METAFILEDRV_PDEVICE *metadc = get_metadc_ptr( hdc );
    METARECORD mr;
    INT16 index;

    if ((index = MFDRV_FindObject( &metadc->dev, obj )) < 0) return;
    if (obj == metadc->pen || obj == metadc->brush || obj == metadc->font)
    {
        WARN( "deleting selected object %p\n", obj );
        return;
    }

    mr.rdSize = sizeof mr / 2;
    mr.rdFunction = META_DELETEOBJECT;
    mr.rdParm[0] = index;

    MFDRV_WriteRecord( &metadc->dev, &mr, mr.rdSize*2 );

    metadc->handles[index] = 0;
    metadc->cur_handles--;
}


/***********************************************************************
 *           MFDRV_SelectObject
 */
static BOOL MFDRV_SelectObject( PHYSDEV dev, INT16 index)
{
    METARECORD mr;

    mr.rdSize = sizeof mr / 2;
    mr.rdFunction = META_SELECTOBJECT;
    mr.rdParm[0] = index;

    return MFDRV_WriteRecord( dev, &mr, mr.rdSize*2 );
}


/******************************************************************
 *         MFDRV_CreateBrushIndirect
 */

INT16 MFDRV_CreateBrushIndirect(PHYSDEV dev, HBRUSH hBrush )
{
    DWORD size;
    METARECORD *mr;
    LOGBRUSH logbrush;
    BOOL r;

    if (!GetObjectA( hBrush, sizeof(logbrush), &logbrush )) return -1;

    switch(logbrush.lbStyle)
    {
    case BS_SOLID:
    case BS_NULL:
    case BS_HATCHED:
        {
	    LOGBRUSH16 lb16;

	    lb16.lbStyle = logbrush.lbStyle;
	    lb16.lbColor = logbrush.lbColor;
	    lb16.lbHatch = logbrush.lbHatch;
	    size = sizeof(METARECORD) + sizeof(LOGBRUSH16) - 2;
	    mr = HeapAlloc( GetProcessHeap(), 0, size );
	    mr->rdSize = size / 2;
	    mr->rdFunction = META_CREATEBRUSHINDIRECT;
	    memcpy( mr->rdParm, &lb16, sizeof(LOGBRUSH16));
	    break;
	}
    case BS_PATTERN:
    case BS_DIBPATTERN:
        {
            char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
            BITMAPINFO *dst_info, *src_info = (BITMAPINFO *)buffer;
            DWORD info_size;
            char *dst_ptr;
            void *bits;
            UINT usage;

            if (!get_brush_bitmap_info( hBrush, src_info, &bits, &usage )) goto done;

            info_size = get_dib_info_size( src_info, usage );
	    size = FIELD_OFFSET( METARECORD, rdParm[2] ) + info_size + src_info->bmiHeader.biSizeImage;

            if (!(mr = HeapAlloc( GetProcessHeap(), 0, size ))) goto done;
	    mr->rdFunction = META_DIBCREATEPATTERNBRUSH;
	    mr->rdSize = size / 2;
	    mr->rdParm[0] = logbrush.lbStyle;
	    mr->rdParm[1] = usage;
            dst_info = (BITMAPINFO *)(mr->rdParm + 2);
            memcpy( dst_info, src_info, info_size );
            if (dst_info->bmiHeader.biClrUsed == 1 << dst_info->bmiHeader.biBitCount)
                dst_info->bmiHeader.biClrUsed = 0;
            dst_ptr = (char *)dst_info + info_size;

            /* always return a bottom-up DIB */
            if (dst_info->bmiHeader.biHeight < 0)
            {
                int i, width_bytes = get_dib_stride( dst_info->bmiHeader.biWidth,
                                                     dst_info->bmiHeader.biBitCount );
                dst_info->bmiHeader.biHeight = -dst_info->bmiHeader.biHeight;
                dst_ptr += (dst_info->bmiHeader.biHeight - 1) * width_bytes;
                for (i = 0; i < dst_info->bmiHeader.biHeight; i++, dst_ptr -= width_bytes)
                    memcpy( dst_ptr, (char *)bits + i * width_bytes, width_bytes );
            }
            else memcpy( dst_ptr, bits, src_info->bmiHeader.biSizeImage );
	    break;
	}

	default:
	    FIXME("Unknown brush style %x\n", logbrush.lbStyle);
	    return 0;
    }
    r = MFDRV_WriteRecord( dev, mr, mr->rdSize * 2);
    HeapFree(GetProcessHeap(), 0, mr);
    if( !r )
        return -1;
done:
    return MFDRV_AddHandle( dev, hBrush );
}


/***********************************************************************
 *           METADC_SelectBrush
 */
static HBRUSH METADC_SelectBrush( HDC hdc, HBRUSH hbrush )
{
    METAFILEDRV_PDEVICE *metadc = get_metadc_ptr( hdc );
    INT16 index;
    HBRUSH ret;

    index = MFDRV_FindObject( &metadc->dev, hbrush );
    if( index < 0 )
    {
        index = MFDRV_CreateBrushIndirect( &metadc->dev, hbrush );
        if( index < 0 )
            return 0;
        GDI_hdc_using_object( hbrush, hdc, METADC_DeleteObject );
    }
    if (!MFDRV_SelectObject( &metadc->dev, index )) return 0;
    ret = metadc->brush;
    metadc->brush = hbrush;
    return ret;
}

/******************************************************************
 *         MFDRV_CreateFontIndirect
 */

static UINT16 MFDRV_CreateFontIndirect(PHYSDEV dev, HFONT hFont, LOGFONTW *logfont)
{
    char buffer[sizeof(METARECORD) - 2 + sizeof(LOGFONT16)];
    METARECORD *mr = (METARECORD *)&buffer;
    LOGFONT16 *font16;
    INT written;

    mr->rdSize = (sizeof(METARECORD) + sizeof(LOGFONT16) - 2) / 2;
    mr->rdFunction = META_CREATEFONTINDIRECT;
    font16 = (LOGFONT16 *)&mr->rdParm;

    font16->lfHeight         = logfont->lfHeight;
    font16->lfWidth          = logfont->lfWidth;
    font16->lfEscapement     = logfont->lfEscapement;
    font16->lfOrientation    = logfont->lfOrientation;
    font16->lfWeight         = logfont->lfWeight;
    font16->lfItalic         = logfont->lfItalic;
    font16->lfUnderline      = logfont->lfUnderline;
    font16->lfStrikeOut      = logfont->lfStrikeOut;
    font16->lfCharSet        = logfont->lfCharSet;
    font16->lfOutPrecision   = logfont->lfOutPrecision;
    font16->lfClipPrecision  = logfont->lfClipPrecision;
    font16->lfQuality        = logfont->lfQuality;
    font16->lfPitchAndFamily = logfont->lfPitchAndFamily;
    written = WideCharToMultiByte( CP_ACP, 0, logfont->lfFaceName, -1, font16->lfFaceName, LF_FACESIZE - 1, NULL, NULL );
    /* Zero pad the facename buffer, so that we don't write uninitialized data to disk */
    memset(font16->lfFaceName + written, 0, LF_FACESIZE - written);

    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * 2)))
        return 0;
    return MFDRV_AddHandle( dev, hFont );
}


/***********************************************************************
 *           METADC_SelectFont
 */
static HFONT METADC_SelectFont( HDC hdc, HFONT hfont )
{
    METAFILEDRV_PDEVICE *metadc = get_metadc_ptr( hdc );
    LOGFONTW font;
    INT16 index;
    HFONT ret;

    index = MFDRV_FindObject( &metadc->dev, hfont );
    if( index < 0 )
    {
        if (!GetObjectW( hfont, sizeof(font), &font ))
            return 0;
        index = MFDRV_CreateFontIndirect( &metadc->dev, hfont, &font );
        if( index < 0 )
            return 0;
        GDI_hdc_using_object( hfont, hdc, METADC_DeleteObject );
    }
    if (!MFDRV_SelectObject( &metadc->dev, index )) return 0;
    ret = metadc->font;
    metadc->font = hfont;
    return ret;
}

/******************************************************************
 *         MFDRV_CreatePenIndirect
 */
static UINT16 MFDRV_CreatePenIndirect(PHYSDEV dev, HPEN hPen, LOGPEN16 *logpen)
{
    char buffer[sizeof(METARECORD) - 2 + sizeof(*logpen)];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = (sizeof(METARECORD) + sizeof(*logpen) - 2) / 2;
    mr->rdFunction = META_CREATEPENINDIRECT;
    memcpy(&(mr->rdParm), logpen, sizeof(*logpen));
    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * 2)))
        return 0;
    return MFDRV_AddHandle( dev, hPen );
}


/***********************************************************************
 *           METADC_SelectPen
 */
static HPEN METADC_SelectPen( HDC hdc, HPEN hpen )
{
    METAFILEDRV_PDEVICE *metadc = get_metadc_ptr( hdc );
    LOGPEN16 logpen;
    INT16 index;
    HPEN ret;

    index = MFDRV_FindObject( &metadc->dev, hpen );
    if( index < 0 )
    {
        /* must be an extended pen */
        INT size = GetObjectW( hpen, 0, NULL );

        if (!size) return 0;

        if (size == sizeof(LOGPEN))
        {
            LOGPEN pen;

            GetObjectW( hpen, sizeof(pen), &pen );
            logpen.lopnStyle   = pen.lopnStyle;
            logpen.lopnWidth.x = pen.lopnWidth.x;
            logpen.lopnWidth.y = pen.lopnWidth.y;
            logpen.lopnColor   = pen.lopnColor;
        }
        else  /* must be an extended pen */
        {
            EXTLOGPEN *elp = HeapAlloc( GetProcessHeap(), 0, size );

            GetObjectW( hpen, size, elp );
            /* FIXME: add support for user style pens */
            logpen.lopnStyle = elp->elpPenStyle;
            logpen.lopnWidth.x = elp->elpWidth;
            logpen.lopnWidth.y = 0;
            logpen.lopnColor = elp->elpColor;

            HeapFree( GetProcessHeap(), 0, elp );
        }

        index = MFDRV_CreatePenIndirect( &metadc->dev, hpen, &logpen );
        if( index < 0 )
            return 0;
        GDI_hdc_using_object( hpen, hdc, METADC_DeleteObject );
    }

    if (!MFDRV_SelectObject( &metadc->dev, index )) return 0;
    ret = metadc->pen;
    metadc->pen = hpen;
    return ret;
}


/******************************************************************
 *         MFDRV_CreatePalette
 */
static BOOL MFDRV_CreatePalette(PHYSDEV dev, HPALETTE hPalette, LOGPALETTE* logPalette, int sizeofPalette)
{
    int index;
    BOOL ret;
    METARECORD *mr;

    mr = HeapAlloc( GetProcessHeap(), 0, sizeof(METARECORD) + sizeofPalette - sizeof(WORD) );
    mr->rdSize = (sizeof(METARECORD) + sizeofPalette - sizeof(WORD)) / sizeof(WORD);
    mr->rdFunction = META_CREATEPALETTE;
    memcpy(&(mr->rdParm), logPalette, sizeofPalette);
    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * sizeof(WORD))))
    {
        HeapFree(GetProcessHeap(), 0, mr);
        return FALSE;
    }

    mr->rdSize = sizeof(METARECORD) / sizeof(WORD);
    mr->rdFunction = META_SELECTPALETTE;

    if ((index = MFDRV_AddHandle( dev, hPalette )) == -1) ret = FALSE;
    else
    {
        *(mr->rdParm) = index;
        ret = MFDRV_WriteRecord( dev, mr, mr->rdSize * sizeof(WORD));
    }
    HeapFree(GetProcessHeap(), 0, mr);
    return ret;
}


/***********************************************************************
 *           METADC_SelectPalette
 */
BOOL METADC_SelectPalette( HDC hdc, HPALETTE palette )
{
#define PALVERSION 0x0300

    METAFILEDRV_PDEVICE *metadc;
    PLOGPALETTE logPalette;
    WORD        wNumEntries = 0;
    BOOL        ret;
    int         sizeofPalette;

    if (!(metadc = get_metadc_ptr( hdc ))) return FALSE;
    GetObjectA( palette, sizeof(WORD), &wNumEntries );

    if (wNumEntries == 0) return 0;

    sizeofPalette = sizeof(LOGPALETTE) + ((wNumEntries-1) * sizeof(PALETTEENTRY));
    logPalette = HeapAlloc( GetProcessHeap(), 0, sizeofPalette );

    if (logPalette == NULL) return 0;

    logPalette->palVersion = PALVERSION;
    logPalette->palNumEntries = wNumEntries;

    GetPaletteEntries( palette, 0, wNumEntries, logPalette->palPalEntry );

    ret = MFDRV_CreatePalette( &metadc->dev, palette, logPalette, sizeofPalette );

    HeapFree( GetProcessHeap(), 0, logPalette );
    return ret;
}

/***********************************************************************
 *           METADC_RealizePalette
 */
BOOL METADC_RealizePalette( HDC hdc )
{
    return metadc_param0( hdc, META_REALIZEPALETTE );
}


HGDIOBJ METADC_SelectObject( HDC hdc, HGDIOBJ obj )
{
    switch (gdi_handle_type( obj ))
    {
    case NTGDI_OBJ_BRUSH:
        return METADC_SelectBrush( hdc, obj );
    case NTGDI_OBJ_FONT:
        return METADC_SelectFont( hdc, obj );
    case NTGDI_OBJ_PEN:
    case NTGDI_OBJ_EXTPEN:
        return METADC_SelectPen( hdc, obj );
    default:
        SetLastError( ERROR_INVALID_FUNCTION );
        return 0;
    }
}
