#include <effect.h>
#include <blitter.h>
#include <copper.h>
#include <color.h>
#include <system/memory.h>
#include <c2p_1x1_4.h>

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 4

#include "data/ecco-purple.c"

static CopListT *cp;
static BitmapT *screen;
static BitmapT ecco_purple;

void PixmapToBitmap(BitmapT *bm, short width, short height, short depth, void *pixels) {
  short bytesPerRow = ((short)(width + 15) & ~15) / 8;
  int bplSize = bytesPerRow * height;

  bm->width = width;
  bm->height = height;
  bm->depth = depth;
  bm->bytesPerRow = bytesPerRow;
  bm->bplSize = bplSize;
  bm->flags = BM_DISPLAYABLE | BM_STATIC;

  BitmapSetPointers(bm, pixels);

  {
    void *planes = MemAlloc(bplSize * 4, MEMF_PUBLIC);
    c2p_1x1_4(pixels, planes, width, height, bplSize);
    memcpy(pixels, planes, bplSize * 4);
    MemFree(planes);
  }
}

static void Load(void) {
    PixmapToBitmap(&ecco_purple, ecco_purple_width, ecco_purple_height, 4,
    ecco_purple_pixels);
}

static void Init(void) {
    screen = NewBitmap(WIDTH, HEIGHT, DEPTH);

    memcpy(screen->planes[0], ecco_purple.planes[0],
            WIDTH * HEIGHT * DEPTH / 8);
    
    SetupPlayfield(MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);

    // {
    //     short i = 0;

    //     for (i = 0; i < (1 << DEPTH); i++)
    //     SetColor(i, ecco_purple_pal.colors[0]);
    // }

    cp = NewCopList(40);
    CopInit(cp);
    CopSetupBitplanes(cp, NULL, screen, DEPTH);
    CopEnd(cp);

    CopListActivate(cp);

    LoadPalette(&ecco_purple_pal, 0);

    EnableDMA(DMAF_RASTER);
}

static void Kill(void) {
  DisableDMA(DMAF_COPPER | DMAF_RASTER);

  DeleteCopList(cp);

  DeleteBitmap(screen);
}

EFFECT(Ecco, Load, NULL, Init, Kill, NULL, NULL);