#include "memory.h"
#include "pixmap.h"

__regargs LONG PixmapSize(PixmapT *pixmap) {
  WORD width = pixmap->width;
  if (pixmap->type == PM_RGB)
    width *= 3;
  else if (pixmap->type == PM_RGB4)
    width *= 2;
  else if (pixmap->type == PM_GRAY4 || pixmap->type == PM_CMAP4)
    width /= 2;
  return width * pixmap->height;
}

__regargs PixmapT *NewPixmap(WORD width, WORD height, 
                             PixmapTypeT type, ULONG memoryAttributes)
{
  PixmapT *pixmap = MemAlloc(sizeof(PixmapT), MEMF_PUBLIC|MEMF_CLEAR);

  pixmap->type = type;
  pixmap->width = width;
  pixmap->height = height;
  pixmap->pixels = MemAlloc(PixmapSize(pixmap), memoryAttributes);

  return pixmap;
}

__regargs PixmapT *ClonePixmap(PixmapT *pixmap) {
  PixmapT *clone = NewPixmap(pixmap->width, pixmap->height,
                             pixmap->type, MemTypeOf(pixmap->pixels));

  memcpy(clone->pixels, pixmap->pixels, PixmapSize(pixmap));

  return clone;
}

__regargs void DeletePixmap(PixmapT *pixmap) {
  if (pixmap) {
    MemFree(pixmap->pixels);
    MemFree(pixmap);
  }
}

__regargs void PixmapScramble_4_1(PixmapT *pixmap) {
  if (pixmap->type == PM_GRAY4 || pixmap->type == PM_CMAP4) {
    ULONG *data = pixmap->pixels;
    WORD n = pixmap->width * pixmap->height / 8;
    register ULONG m0 asm("d6") = 0xa5a5a5a5;
    register ULONG m1 asm("d7") = 0x0a0a0a0a;

    /* [a0 a1 a2 a3 b0 b1 b2 b3] => [a0 b0 a2 b1 a1 b2 a3 b3] */
    while (--n >= 0) {
      ULONG c = *data;
      *data++ = (c & m0) | ((c >> 3) & m1) | ((c & m1) << 3);
    }
  }
}

__regargs void PixmapScramble_4_2(PixmapT *pixmap) {
  if (pixmap->type == PM_GRAY4 || pixmap->type == PM_CMAP4) {
    ULONG *data = pixmap->pixels;
    WORD n = pixmap->width * pixmap->height / 8;
    register ULONG m0 asm("d6") = 0xc3c3c3c3;
    register ULONG m1 asm("d7") = 0x0c0c0c0c;

    /* [a0 a1 a2 a3 b0 b1 b2 b3] => [a0 a1 b0 b1 a2 a3 b2 b3] */
    while (--n >= 0) {
      ULONG c = *data;
      *data++ = (c & m0) | ((c >> 2) & m1) | ((c & m1) << 2);
    }
  }
}

__regargs void PixmapConvert(PixmapT *pixmap, PixmapTypeT type) {
  if ((pixmap->type == PM_GRAY4 && type == PM_GRAY) ||
      (pixmap->type == PM_CMAP4 && type == PM_CMAP) ||
      (pixmap->type == PM_GRAY && type == PM_GRAY4) ||
      (pixmap->type == PM_CMAP && type == PM_CMAP4) ||
      (pixmap->type == PM_RGB && type == PM_RGB4))
  {
    UBYTE *pixels = pixmap->pixels;

    if (pixmap->type == PM_GRAY4)
      pixmap->type = PM_GRAY;
    else if (pixmap->type == PM_CMAP4)
      pixmap->type = PM_CMAP;
    else if (pixmap->type == PM_RGB)
      pixmap->type = PM_RGB4;
    else if (pixmap->type == PM_GRAY)
      pixmap->type = PM_GRAY4;
    else if (pixmap->type == PM_CMAP)
      pixmap->type = PM_CMAP4;

    pixmap->pixels = MemAlloc(PixmapSize(pixmap), MemTypeOf(pixmap->pixels));

    {
      UBYTE *src = pixels;
      LONG n = pixmap->width * pixmap->height;

      if (pixmap->type == PM_RGB4) {
        UWORD *dst = pixmap->pixels;
        do {
          UBYTE r = *src++;
          UBYTE g = *src++;
          UBYTE b = *src++;
          UBYTE lo = (g & 0xf0) | ((b & 0xf0) >> 4);
          *dst++ = ((r & 0xf0) << 4) | lo;
        } while (--n);
      } else if (pixmap->type == PM_GRAY4 || pixmap->type == PM_CMAP4) {
        UBYTE *dst = pixmap->pixels;
        n /= 2;
        do {
          UBYTE c = *src++;
          UBYTE d = *src++;
          *dst++ = c << 4 | d;
        } while (--n);
      } else {
        UBYTE *dst = pixmap->pixels;
        n /= 2;
        do {
          UBYTE c = *src++;
          *dst++ = c >> 4;
          *dst++ = c & 15;
        } while (--n);
      }
    }

    MemFree(pixels);
  }
}
