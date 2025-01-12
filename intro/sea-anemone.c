#include <intro.h>
#include <blitter.h>
#include <copper.h>
#include <color.h>
#include <fx.h>
#include <gfx.h>
#include <line.h>
#include <stdlib.h>
#include <sprite.h>
#include <sync.h>

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 4
#define NSPRITES 4

#define DIAMETER 32
#define NARMS 15 /* must be power of two minus one */

static __code short active = 0;
static __code CopListT *cp[2];
static CopInsT *sprptr[2][8];
static CopInsT *bplptr[2][DEPTH];
static BitmapT *screen;

#include "palettes.h"

#include "data/anemone-gradient.c"
#include "data/anemone-gradient-data.c"

static CopInsT *colins[2][anemone_gradient_length];

#include "data/circles.c"
#include "data/squares.c"
#include "data/whirl.c"

typedef const BitmapT *ArmShapeT[DIAMETER / 2]; 

static const ArmShapeT circles = {
  &circle1,
  &circle2,
  &circle3,
  &circle4,
  &circle5,
  &circle6,
  &circle7,
  &circle8,
  &circle9,
  &circle10,
  &circle11,
  &circle12,
  &circle13,
  &circle14,
  &circle15,
  &circle16
};

static const ArmShapeT squares = {
  &square1,
  &square2,
  &square3,
  &square4,
  &square5,
  &square6,
  &square7,
  &square8,
  &square9,
  &square10,
  &square11,
  &square12,
  &square13,
  &square14,
  &square15,
  &square16
};

static const ArmShapeT *shapes[4] = {
  NULL,
  &circles,
  &squares,
  &circles,
};

static const ArmShapeT *active_shape = &circles;

extern TrackT SeaAnemoneVariant;
extern TrackT SeaAnemonePal;
extern TrackT SeaAnemonePalPulse;
extern TrackT SeaAnemoneGradient;
extern TrackT SeaAnemoneFade;

typedef const PaletteT *SeaAnemonePalT[5];

static const SeaAnemonePalT sea_anemone_palettes = {
  NULL, 
  &pal_green,
  &pal_blue,
  &pal_red,
  &pal_gold,
};

static const SeaAnemonePalT anemone1_pal = {
  NULL,
  &pal_green_light,
  &pal_green,
  &pal_green_dark,
};

static const SeaAnemonePalT anemone4_pal = {
  NULL,
  &pal_gold_light,
  &pal_gold,
  &pal_gold_dark,
};

static const SeaAnemonePalT anemone2_pal = {
  NULL,
  &pal_blue_light,
  &pal_blue,
  &pal_blue_dark,
};

static const SeaAnemonePalT anemone3_pal = {
  NULL,
  &pal_red_light,
  &pal_red,
  &pal_red_dark,
};

static const SeaAnemonePalT *sea_anemone_pal[5] = {
  NULL,
  &anemone1_pal,
  &anemone2_pal,
  &anemone3_pal,
  &anemone4_pal
};

static const SeaAnemonePalT *active_pal = &anemone1_pal;
static const short blip_sequence[] = {
  0, 2,
  1, 1, 1, 1, 1, 1, 1,
  3, 3, 3, 3, 3, 3, 3
};

static const short gradient_envelope[] = {
  0, 0, 
  0, 1, 2, 3, 4, 5, 6, 7, 8,
  9, 10, 11, 12, 13, 14,
  14, 13, 12, 11, 10, 9,
  8, 7, 6, 5, 4, 3, 2, 1, 0,
};

// For the background gradient
static __code const PaletteT *currPal = &pal_red;

static inline int fastrand(void) {
  static int m[2] = { 0x3E50B28C, 0xD461A7F9 };

  int a, b;

  // https://www.atari-forum.com/viewtopic.php?p=188000#p188000
  asm volatile("move.l (%2)+,%0\n"
               "move.l (%2),%1\n"
               "swap   %1\n"
               "add.l  %0,(%2)\n"
               "add.l  %1,-(%2)\n"
               : "=d" (a), "=d" (b)
               : "a" (m));
  
  return a;
}

#define random fastrand

typedef struct Arm {
  short diameter;
  union { // Q4.12
    short word;
    char byte;
  } _vel_x;
  union { // Q4.12
    short word;
    char byte;
  } _vel_y;
  short pos_x, pos_y; // Q12.4
  short angle;
  short pad[2];
} ArmT;

#define vel_x _vel_x.word
#define vel_y _vel_y.word
#define vel_x_b _vel_x.byte
#define vel_y_b _vel_y.byte

typedef struct ArmQueue {
  short head; // points to first unused entry
  short tail; // points to the last element
  ArmT elems[NARMS + 1]; // contains one empty guard element
} ArmQueueT;

static ArmQueueT AnemoneArms;

static inline ArmT *ArmGet(ArmQueueT *arms, short i) {
  return &arms->elems[i & NARMS];
}

static inline ArmT *ArmFirst(ArmQueueT *arms) {
  return ArmGet(arms, arms->head - 1);
}

static inline ArmT *ArmLast(ArmQueueT *arms) {
  return ArmGet(arms, arms->tail);
}

static inline ArmT *ArmPrev(ArmQueueT *arms, ArmT *arm) {
  arm--;
  return arm < arms->elems ? &arms->elems[NARMS] : arm;
}

static inline void ArmsReset(ArmQueueT *arms) {
  arms->head = arms->tail = 0;
}

static inline bool ArmsNonEmpty(ArmQueueT *arms) {
  return arms->head != arms->tail;
}

static inline bool ArmsFull(ArmQueueT *arms) {
  return ((arms->head + 1) & NARMS) == arms->tail;
}

static short ArmVariant = 0;

static void MakeArm(ArmQueueT *arms, ArmT *arm) {
  u_int arand = random();

  if (ArmVariant == 1) {
    arm->pos_x = fx4i((arand & 255) + 32);
    if (arms->head % 2 == 0) {
      arm->pos_y = fx4i(DIAMETER);
    } else {
      arm->pos_y = fx4i(HEIGHT - DIAMETER);
    }
  } else if (ArmVariant == 2) {
    arm->angle = arand;
    arm->pos_x = fx4i(WIDTH / 2) + (short)((COS(arm->angle) * 120) >> 8);
    arm->pos_y = fx4i(HEIGHT - 60) + (short)((SIN(arm->angle) * 50) >> 8);
  } else if (ArmVariant == 3 || ArmVariant == 4) {
    arm->angle = arand;
    arm->pos_x = fx4i(WIDTH / 2) + (short)((COS(arm->angle) * 60) >> 8);
    arm->pos_y = fx4i(HEIGHT / 2) + (short)((SIN(arm->angle) * 60) >> 8);
  }

  arm->vel_x = 0;
  arm->vel_y = 0;
  arm->diameter = mod16(random() & 0x7fff, DIAMETER / 3) + DIAMETER * 2 / 3;
}

static void ArmsAdd(ArmQueueT *arms, ArmT *arm) {
  short prev, curr;

  if (ArmsFull(arms))
    return;

  prev = (arms->head - 1) & NARMS;
  curr = arms->head;

  while (curr != arms->tail && arm->diameter <= arms->elems[prev].diameter) {
    arms->elems[curr] = arms->elems[prev];      
    curr = prev;
    prev = (prev - 1) & NARMS;
  }

  arms->elems[curr] = *arm;
  arms->head = (arms->head + 1) & NARMS;
}

static inline void ArmsPop(ArmQueueT *arms) {
  arms->tail = (arms->tail + 1) & NARMS;
}

static void ArmMove(ArmT *arm, short angle) {
  short vx = arm->vel_x;
  short vy = arm->vel_y;
  int magSq;
  int diameter;

  vx += COS(angle) >> 1;
  vy += SIN(angle) >> 1;

  magSq = vx * vx + vy * vy; // Q8.24
  diameter = arm->diameter << 24;

  if (magSq > diameter) {
    // short scale = div16(diameter, normfx(magSq));
    // `magSq` does get truncated if converted to 16-bit,
    // so it's not eligible for 16-bit division.
    short scale = diameter / (magSq >> 12);
    vx = normfx(vx * scale);
    vy = normfx(vy * scale);
  }

  arm->vel_x = vx;
  arm->vel_y = vy;

  arm->pos_x += arm->vel_x_b;
  arm->pos_y += arm->vel_y_b;

  arm->diameter--;
}

static __code u_short gradient[anemone_gradient_pal_count];

static void GradientUpdate(short step) {
  const u_char *ptr = anemone_gradient_color;
  const short from = currPal->colors[0];
  short i;

  for (i = 0; i < anemone_gradient_pal_count; i++)
    gradient[i] = ColorTransition(from, anemone_gradient_pal.colors[i], step);

  {
    CopInsT **insptr0 = colins[0];
    CopInsT **insptr1 = colins[1];

    for (i = 0; i < anemone_gradient_length; i++) {
      u_short col = gradient[*ptr++];
      CopInsSet16(*insptr0++, col);
      CopInsSet16(*insptr1++, col);
    }
  }
}

static void GradientClear(short step) {
  const short to = currPal->colors[0];
  short bgcol = ColorTransition(0, to, step);
  short i;

  {
    CopInsT **insptr0 = colins[0];
    CopInsT **insptr1 = colins[1];

    for (i = 0; i < anemone_gradient_length; i++) {
      CopInsSet16(*insptr0++, bgcol);
      CopInsSet16(*insptr1++, bgcol);
    }
  }
}

static void VBlank(void) {
  short val;

  UpdateFrameCount();

  (void)TrackValueGet(&SeaAnemoneFade, frameCount);

  if ((val = FromCurrKeyFrame(&SeaAnemoneFade)) < 16) {
    GradientClear(val);
    FadeBlack(currPal, 0, val);
    FadeBlack(&whirl_pal, 16, val);
  } else if ((val = TillNextKeyFrame(&SeaAnemoneFade)) < 16) {
    GradientClear(val);
    FadeBlack(currPal, 0, val);
    FadeBlack(&whirl_pal, 16, val);
  } else if ((val = TrackValueGet(&SeaAnemonePalPulse, frameFromStart))) {
    LoadPalette((*active_pal)[blip_sequence[val]], 0);
  } else if ((val = TrackValueGet(&SeaAnemoneGradient, frameFromStart))) {
    GradientUpdate(gradient_envelope[val]);
  }
}

static void MakeCopperList(CopListT *cp, CopInsT **bplptr, CopInsT **sprptr,
                           CopInsT **insptr)
{
  const u_char *ptr = anemone_gradient_y;
  short i;

  CopInit(cp);
  CopSetupBitplanes(cp, bplptr, screen, DEPTH);
  CopSetupSprites(cp, sprptr);

  for (i = 0; i < anemone_gradient_length; i++) {
    short y = *ptr++;
    CopWaitSafe(cp, Y(y), 0);
    *insptr++ = CopSetColor(cp, 0, 0);
  }

  CopEnd(cp);
}

static void Init(void) {
  short i;

  for (i = 0; i < NSPRITES; i++) {
    short hp = X(i * 16 + (WIDTH - NSPRITES * 16) / 2);
    SpriteUpdatePos(&whirl[i], hp, Y((HEIGHT - whirl_height) / 2));
  }

  screen = NewBitmap(WIDTH, HEIGHT * 4, DEPTH);

  SetupPlayfield(MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);
  LoadPalette(currPal, 0);
  LoadPalette(&whirl_pal, 16);

  cp[0] = NewCopList(50 + 2 * anemone_gradient_length);
  cp[1] = NewCopList(50 + 2 * anemone_gradient_length);
  MakeCopperList(cp[0], bplptr[0], sprptr[0], colins[0]);
  MakeCopperList(cp[1], bplptr[1], sprptr[1], colins[1]);
  GradientUpdate(0);
  CopListActivate(cp[0]);

  EnableDMA(DMAF_RASTER | DMAF_BLITTER | DMAF_BLITHOG);

  ArmsReset(&AnemoneArms);

  /* Moved from DrawCircle, since we use only one type of blit. */
  {
    WaitBlitter();
    custom->bltcon1 = 0;
    custom->bltafwm = -1;
  }
}

static void Kill(void) {
  DisableDMA(DMAF_COPPER | DMAF_BLITTER | DMAF_RASTER | DMAF_BLITHOG);
  ResetSprites();

  DeleteCopList(cp[0]);
  DeleteCopList(cp[1]);
  DeleteBitmap(screen);
}

#define screen_bytesPerRow (WIDTH / 8)

static __code int vShift = 0;

static void DrawShape(const BitmapT *shape, short x, short y, short c) {
  u_short dstmod = screen_bytesPerRow - shape->bytesPerRow;
  u_short bltshift = rorw(x & 15, 4);
  u_short bltsize = (shape->height << 6) | (shape->bytesPerRow >> 1);
  void *srcbpt = shape->planes[0];
  void **dstbpts = screen->planes;
  int start;

  start = (y + vShift) * screen_bytesPerRow;
  start += (x & ~15) >> 3;

  WaitBlitter();

  if (bltshift) {
    bltsize++, dstmod -= 2; 

    custom->bltalwm = 0;
    custom->bltamod = -2;
  } else {
    custom->bltalwm = -1;
    custom->bltamod = 0;
  }

  custom->bltbmod = dstmod;
  custom->bltdmod = dstmod;
  
  {
    short n = DEPTH - 1;

    do {
      void *dstbpt = (*dstbpts++) + start;
      u_short bltcon0 = bltshift;

      if (c & 1) {
        bltcon0 |= SRCA | SRCB | DEST | A_OR_B;
      } else {
        bltcon0 |= SRCA | SRCB | DEST | NOT_A_AND_B;
      }

      WaitBlitter();

      custom->bltcon0 = bltcon0;
      custom->bltapt = srcbpt;
      custom->bltbpt = dstbpt;
      custom->bltdpt = dstbpt;
      custom->bltsize = bltsize;

      c >>= 1;
    } while (--n != -1);
  }
}

static void SeaAnemone(ArmQueueT *arms, const ArmShapeT shapes) {
  static ArmT arm;

  MakeArm(arms, &arm);
  ArmsAdd(arms, &arm);

  if (ArmsNonEmpty(arms)) {
    ArmT *curr = ArmFirst(arms);
    ArmT *last = ArmLast(arms);

    while (true) {
      short angle;

      {
        short qy = curr->pos_y >> 4;
        short arand = random();

        if (ArmVariant == 1) {
          angle = arand & 0x7ff;
          if (qy > HEIGHT / 2)
            angle += 0x800;
        } else if (ArmVariant == 3) {
          angle = ((arand & 0x7ff) - 0x400) + curr->angle;
        } else if (ArmVariant == 4) {
          angle = (arand & 0x7ff) + curr->angle;
        } else {
          angle = (arand & 0x7ff) + 0x800;
        }
      }

      ArmMove(curr, angle);

      if (curr->diameter > 1) {
        short d = curr->diameter;
        short r = d / 2;
        short x = (curr->pos_x >> 4) - r;
        short y = (curr->pos_y >> 4) - r;
        if ((x < 0) || (y < 0) || (x >= WIDTH - d) || (y >= HEIGHT - d))
          continue;
        if (r < 16)
          DrawShape(shapes[r - 1], x, y, 16 - r);
      }
      if (curr == last)
        break;
      curr = ArmPrev(arms, curr);
    }

    while (ArmsNonEmpty(arms) && ArmLast(arms)->diameter < 1) {
      ArmsPop(arms);
    }
  }
}

PROFILE(SeaAnemone);

static void Render(void) {
  int lineOffset = 0;
  short val;

  active ^= 1;

  if ((val = TrackValueGet(&SeaAnemoneVariant, frameFromStart))) { 
    BitmapClear(screen);
    ArmsReset(&AnemoneArms);
    ArmVariant = val;
  }

  if ((val = TrackValueGet(&SeaAnemonePal, frameFromStart))) {
    currPal = sea_anemone_palettes[val];
    active_pal = sea_anemone_pal[val];
    active_shape = shapes[val];
  }

  ProfilerStart(SeaAnemone);

  // Scroll the screen vertically
  if (ArmVariant == 4) {
    short i;

    vShift = frameCount % (HEIGHT * 3);
    lineOffset = vShift * screen_bytesPerRow;

    for (i = 0; i < DEPTH; i++)
      CopInsSet32(bplptr[active][i], screen->planes[i] + lineOffset);
  }

  if (ArmVariant == 3) {
    short i;

    EnableDMA(DMAF_SPRITE);
    for (i = 0; i < NSPRITES; i++) {
      CopInsSetSprite(sprptr[active][i], &whirl[i]);
    }
  } else {
    ResetSprites();
  }

  SeaAnemone(&AnemoneArms, *active_shape);
  CopListRun(cp[active]); 
  ProfilerStop(SeaAnemone);

  TaskWaitVBlank();
}

EFFECT(SeaAnemone, NULL, NULL, Init, Kill, Render, VBlank);
