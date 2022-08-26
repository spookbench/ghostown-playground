#include <intro.h>
#include <blitter.h>
#include <copper.h>
#include <color.h>
#include <sync.h>
#include <system/memory.h>

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 3

#include "data/electric-lifeforms-01.c"
#include "data/electric-lifeforms-02.c"

static CopListT *cp;
static BitmapT *screen;

static const PaletteT *electric_lifeforms_pal[] = {
  NULL,
  &electric_lifeforms_1_pal,
  &electric_lifeforms_2_pal
};

extern TrackT ElectricLifeformsLogoPal;

static void Init(void) {
  screen = NewBitmap(WIDTH, HEIGHT, DEPTH);

  SetupPlayfield(MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);

  {
    short i = 0;

    for (i = 0; i < (1 << DEPTH); i++)
      SetColor(i, electric_lifeforms_1_pal.colors[0]);
  }

  EnableDMA(DMAF_BLITTER);
  BitmapCopy(screen, (WIDTH - electric_lifeforms_width) / 2,
             (HEIGHT - electric_lifeforms_height) / 2, &electric_lifeforms);
  WaitBlitter();
  DisableDMA(DMAF_BLITTER);

  TrackInit(&ElectricLifeformsLogoPal);

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

static void Render(void) {
  short num = TrackValueGet(&ElectricLifeformsLogoPal, frameCount);
  short frame = FromCurrKeyFrame(&ElectricLifeformsLogoPal);

  if (num > 0) {
    if (frame < 16) {
      short i;

      for (i = 0; i < (1 << DEPTH); i++) {
        short prev = (num == 1) ? electric_lifeforms_1_pal.colors[0]
          : electric_lifeforms_pal[num - 1]->colors[i];
        short curr = electric_lifeforms_pal[num]->colors[i];
        SetColor(i, ColorTransition(prev, curr, frame));
      }
    } else {
      LoadPalette(electric_lifeforms_pal[num], 0);
    }
  }

  TaskWaitVBlank();
}

EFFECT(Vitruvian, NULL, NULL, Init, Kill, Render);