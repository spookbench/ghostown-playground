#include <intro.h>
#include <blitter.h>
#include <copper.h>
#include <color.h>
#include <sync.h>
#include <system/memory.h> 

#define WIDTH 320
#define HEIGHT 240
#define DEPTH 3

#include "data/ecco-purple.c"

static CopListT *cp;
static BitmapT *screen;
static BitmapT ecco_purple;

static void Load(void) {
    PixmapToBitmap(&ecco_purple, ecco_purple_width, ecco_purple_height, 3,
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

    EnableDMA(DMAF_RASTER);
}

static void Kill(void) {
  DisableDMA(DMAF_COPPER | DMAF_RASTER);

  DeleteCopList(cp);

  DeleteBitmap(screen);
}

EFFECT(Ecco, Load, NULL, Init, Kill, NULL, NULL);