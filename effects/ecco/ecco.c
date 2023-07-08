#include <effect.h>
#include <blitter.h>
#include <copper.h>
#include <color.h>
#include <fx.h>
#include <system/memory.h>
#include <c2p_1x1_4.h>

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 4

#define YSTART ((256 - HEIGHT) / 2)

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
    short i, sh;
    screen = NewBitmap(WIDTH, HEIGHT, DEPTH);

    memcpy(screen->planes[0], ecco_purple.planes[0],
            WIDTH * HEIGHT * DEPTH / 8);
    
    SetupPlayfield(MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);

    cp = NewCopList(700);
    CopInit(cp);
    CopSetupBitplanes(cp, NULL, screen, DEPTH);

    // CopWait(cp, 0, 0);
    // CopMove16(cp, bplcon1, 0);

    // CopWait(cp, 100, 0);
    // CopMove16(cp, bplcon1, 255);

    // CopWait(cp, 220, 0);
    // CopMove16(cp, bplcon1, 0);

    for (i = 0; i < HEIGHT; i++) {
      CopWaitSafe(cp, Y(YSTART + i), 0);
      if (i % 2 == 0) {
        sh = normfx(SIN(i*64));
        CopMove16(cp, bplcon1, sh | sh);
      } else {
        sh = normfx(COS(i*32));
        CopMove16(cp, bplcon1, (sh << 4) | sh);
      }
    }

    CopEnd(cp);

    CopListActivate(cp);

    LoadPalette(&ecco_purple_pal, 0);

    EnableDMA(DMAF_RASTER);
}

static void Render(void) {
  Log("eeee");
}

static void Kill(void) {
  DisableDMA(DMAF_COPPER | DMAF_RASTER);

  DeleteCopList(cp);

  DeleteBitmap(screen);
}

EFFECT(Ecco, Load, NULL, Init, Kill, Render, NULL);