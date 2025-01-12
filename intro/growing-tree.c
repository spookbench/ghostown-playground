#include <intro.h>
#include <blitter.h>
#include <copper.h>
#include <fx.h>
#include <gfx.h>
#include <line.h>
#include <sprite.h>
#include <stdlib.h>
#include <sync.h>
#include <system/memory.h>

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 3
#define NSPRITES 8

extern TrackT TreeVariant;
extern TrackT TreeFade;

static CopListT *cp;
static CopInsT *bplptr[DEPTH];
static BitmapT *screen;
static CopInsT *sprptr[8];

static u_short nrPal = 0;

#include "data/tree-pal-organic.c"
#include "data/tree-pal-electric.c"
#include "data/fruit-1.c"
#include "data/fruit-2.c"
#include "data/grass.c"

typedef struct Branch {
  short pos_x, pos_y; // Q12.4
  union { // Q4.12
    short word;
    char byte;
  } _vel_x;
  union { // Q4.12
    short word;
    char byte;
  } _vel_y;
  short diameter;     // Q12.4
} BranchT;

#define vel_x _vel_x.word
#define vel_y _vel_y.word
#define vel_x_b _vel_x.byte
#define vel_y_b _vel_y.byte

#define MAXBRANCHES 256

static BranchT *branches;
static BranchT *lastBranch;

typedef struct Greets {
  // state for drawing
  char *curr;
  short n;
  short x, y;
  // provided data
  short origin_x, origin_y;
  short delay;
  char data[0];
} GreetsT;

#include "data/greets-altair.c"
#include "data/greets-appendix.c"
#include "data/greets-artway.c"
#include "data/greets-atnwhore.c"
#include "data/greets-capsule.c"
#include "data/greets-continue.c"
#include "data/greets-dekadence.c"
#include "data/greets-desire.c"
#include "data/greets-dreamweb.c"
#include "data/greets-elude.c"
#include "data/greets-tobe.c"

static GreetsT greets_empty = {
  .curr = NULL,
  .n = 0,
  .x = 0,
  .y = 0,
  .origin_x = 0,
  .origin_y = 0,
  .delay = 40,
  .data = {
    -1,
  }
};

static GreetsT *greetsArray[] = {
  // First batch
  &greets_altair,
  &greets_appendix,
  &greets_artway,
  // Second batch
  &greets_atnwhore,
  &greets_capsule,
  &greets_dekadence,
  // Third batch
  &greets_desire,
  &greets_dreamweb,
  &greets_elude,
  // Fourth batch
  &greets_empty,
  &greets_continue,
  &greets_tobe,
  // End
  NULL,
};

static GreetsT *greetsData[3];

static __code int hashTable[8] = {
  0x011bad37, 0x7a6433ee, // 3
  0x4ffa0d80, 0x23743a06, // 1
  0x273f164b, 0x9ffa9d90, // 2
  0x74a7beec, 0xb2818113, // 4 
};

static __code int hashTableIdx = 0;
static __code int fastrand_a = 0, fastrand_b = 0;

static inline int fastrand(void) {
  int *hashAddr = &hashTable[hashTableIdx * 2];

  // https://www.atari-forum.com/viewtopic.php?p=188000#p188000
  asm volatile("move.l (%2)+,%0\n"
               "move.l (%2),%1\n"
               "swap   %1\n"
               "add.l  %0,(%2)\n"
               "add.l  %1,-(%2)\n"
               : "=d" (fastrand_a), "=d" (fastrand_b)
               : "a" (hashAddr));
  
  return fastrand_a;
}

#define random fastrand

static GreetsT *GreetsFetch(void) {
  static __code GreetsT **greetsDataPtr = greetsArray;

  GreetsT *gr;

  while (!(gr = *greetsDataPtr++))
    greetsDataPtr = greetsArray;

  gr->curr = gr->data;
  gr->x = 0;
  gr->y = 0;
  gr->n = 0;
  return gr;
}

static void GreetsNextTrack(void) {
  greetsData[0] = GreetsFetch();
  greetsData[1] = GreetsFetch();
  greetsData[2] = GreetsFetch();

  hashTableIdx++;
  hashTableIdx &= 3;

  fastrand_a = fastrand_b = 0;
}

static void VBlank(void) {
  short val;

  UpdateFrameCount();

  (void)TrackValueGet(&TreeFade, frameCount);

  if ((val = FromCurrKeyFrame(&TreeFade)) < 16)
    FadeBlack(nrPal ? &tree_pal_electric : &tree_pal_organic, 0, val);
  
  if ((val = TillNextKeyFrame(&TreeFade)) < 16)
    FadeBlack(nrPal ? &tree_pal_electric : &tree_pal_organic, 0, val);
}

static void Init(void) {
  short i;

  branches = MemAlloc(sizeof(BranchT) * MAXBRANCHES, MEMF_PUBLIC);
  lastBranch = branches;

  screen = NewBitmap(WIDTH, HEIGHT, DEPTH);

  SetupPlayfield(MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);

  for (i = 0; i < NSPRITES; i++) {
    short hp = X(i * 16 + (WIDTH - 16 * NSPRITES) / 2);
    SpriteUpdatePos(&grass[i], hp, Y(HEIGHT - grass_height));
  }

  /* Move sprites into background. */
  custom->bplcon2 = BPLCON2_PF1P2;

  cp = NewCopList(50);
  CopInit(cp);
  CopSetupBitplanes(cp, bplptr, screen, DEPTH);
  CopSetupSprites(cp, sprptr);
  CopEnd(cp);

  CopListActivate(cp);

  EnableDMA(DMAF_RASTER | DMAF_SPRITE | DMAF_BLITTER);
}

static void Kill(void) {
  DisableDMA(DMAF_COPPER | DMAF_BLITTER | DMAF_RASTER | DMAF_SPRITE);

  MemFree(branches);

  DeleteBitmap(screen);
  DeleteCopList(cp);
}

static inline BranchT *NewBranch(void) {
  if (lastBranch == &branches[MAXBRANCHES]) {
    return NULL;
  } else {
    return lastBranch++;
  }
}

#define screen_bytesPerRow (WIDTH / 8)
#define fruit_bytesPerRow 2
#define fruit_width 16
#define fruit_height 16
#define fruit_bplSize (fruit_bytesPerRow * fruit_height)

static void _CopyFruit(void *srcbpt, u_short x, u_short y) {
  u_short dstmod = screen_bytesPerRow - fruit_bytesPerRow;
  u_short bltshift = rorw(x & 15, 4);
  u_short bltsize = (fruit_height << 6) | (fruit_bytesPerRow >> 1);
  void *dstbpt = screen->planes[1];
  short dstoff; 
  u_short bltcon0;

  dstoff = (x & ~15) >> 3;
  dstoff += y * screen_bytesPerRow;

  if (bltshift)
    bltsize++, dstmod -= 2;

  dstbpt += dstoff;
  bltcon0 = (SRCA | SRCB | DEST | A_OR_B) | bltshift;

  WaitBlitter();

  if (bltshift) {
    custom->bltalwm = 0;
    custom->bltamod = -2;
  } else {
    custom->bltalwm = -1;
    custom->bltamod = 0;
  }

  custom->bltbmod = dstmod;
  custom->bltdmod = dstmod;
  custom->bltcon0 = bltcon0;
  custom->bltcon1 = 0;
  custom->bltafwm = -1;
  custom->bltapt = srcbpt;
  custom->bltbpt = dstbpt;
  custom->bltdpt = dstbpt;
  custom->bltsize = bltsize;

  dstbpt = screen->planes[2];
  dstbpt += dstoff;
  WaitBlitter();

  custom->bltapt = srcbpt + fruit_bplSize;
  custom->bltbpt = dstbpt;
  custom->bltdpt = dstbpt;
  custom->bltsize = bltsize;
}

static void CopyFruit(u_short *fruit, u_short x, u_short y) {
  _CopyFruit(fruit, x - fruit_width / 2, y - fruit_height / 2);
}

static void DrawBranch(short x1, short y1, short x2, short y2) {
  u_char *data = screen->planes[0];
  short dx, dy, derr;

  u_short bltcon0 = BC0F_LINE_OR;
  u_short bltcon1 = LINEMODE;

  /* Always draw the line downwards. */
  if (y1 > y2) {
    swapr(x1, x2);
    swapr(y1, y2);
  }

  bltcon0 |= rorw(x1 & 15, 4);

  /* Word containing the first pixel of the line. */
  data += screen_bytesPerRow * y1;
  data += (x1 >> 3) & ~1;

  dx = x2 - x1;
  dy = y2 - y1;

  if (dx < 0) {
    dx = -dx;
    if (dx >= dy) {
      bltcon1 |= AUL | SUD;
    } else {
      bltcon1 |= SUL;
      swapr(dx, dy);
    }
  } else {
    if (dx >= dy) {
      bltcon1 |= SUD;
    } else {
      swapr(dx, dy);
    }
  }

  derr = dy + dy - dx;
  if (derr < 0)
    bltcon1 |= SIGNFLAG;

  {
    u_short bltamod = derr - dx;
    u_short bltbmod = dy + dy;
    u_short bltsize = (dx << 6) + 66;

    WaitBlitter();

    custom->bltadat = 0x8000;
    custom->bltbdat = -1;

    custom->bltcon0 = bltcon0;
    custom->bltcon1 = bltcon1;
    custom->bltafwm = -1;
    custom->bltalwm = -1;
    custom->bltcmod = screen_bytesPerRow;
    custom->bltbmod = bltbmod;
    custom->bltamod = bltamod;
    custom->bltdmod = screen_bytesPerRow;
    custom->bltcpt = data;
    custom->bltapt = (void *)(int)derr;
    custom->bltdpt = data;
    custom->bltsize = bltsize;
  }
}

static void MakeBranch(short x, short y) {
  BranchT *b = NewBranch();
  if (!b)
    return;
  b->pos_x = fx4i(x);
  b->pos_y = fx4i(y);
  b->vel_x = fx12i(0);
  b->vel_y = fx12i(-7);
  b->diameter = fx4i(20);
}

static inline void KillBranch(BranchT *b, BranchT **lastp) {
  BranchT *last = --(*lastp);
  if (b != last)
    *b = *last;
}

static bool SplitBranch(BranchT *parent, BranchT **lastp) {
  short newDiameter = normfx(mul16(parent->diameter, fx12f(0.58)));

  if (newDiameter >= fx4f(0.2)) {
    BranchT *b = NewBranch();
    if (!b)
      return true;
    b->pos_x = parent->pos_x;
    b->pos_y = parent->pos_y;
    b->vel_x = parent->vel_x;
    b->vel_y = parent->vel_y;
    b->diameter = newDiameter;
    parent->diameter = newDiameter;
    return true;
  }

  KillBranch(parent, lastp);
  return false;
}

static void DrawGreetings(GreetsT *gr) {
  char *curr;
  short x2, y2;

  if (gr->delay > 0) {
    gr->delay--;
    return;
  }

  if (gr->n < 0)
    return;

  curr = gr->curr;

  if (gr->n == 0) {
    gr->n = *curr++;
    if (gr->n < 0)
      return;
    gr->x = gr->origin_x + *curr++;
    gr->y = gr->origin_y + *curr++;
    gr->n--;
  }

  x2 = gr->x + *curr++;
  y2 = gr->y + *curr++;

  DrawBranch(gr->x, gr->y, x2, y2);

  gr->curr = curr;
  gr->n--;
  gr->x = x2;
  gr->y = y2;
}

static void HandleDrawingGreets(void) {
  DrawGreetings(greetsData[0]);
  DrawGreetings(greetsData[1]);
  DrawGreetings(greetsData[2]);
}

void GrowingTree(BranchT *branches, BranchT **lastp) {
  u_short *fruit = nrPal ? _fruit_1_bpl : _fruit_2_bpl;
  BranchT *b;

  for (b = *lastp - 1; b >= branches; b--) {
    short prev_x = b->pos_x;
    short prev_y = b->pos_y;
    short curr_x, curr_y;

    /*
     * Watch out! The code below is very brittle. You can easily generate
     * overflow that will cause branches to go in the opposite direction.
     * Code below was carefully crafted in Processing prototype using
     * lots of assertion on numeric results of each operation.
     */
    {
      short scale = (random() & 0x7ff) + 0x1000; // Q4.12
      short angle = random();
      short vx = b->vel_x;
      short vy = b->vel_y;
      short mag = abs(vx) / 4 + abs(vy) / 4;
      short vel_scale = div16(shift12(scale), mag);

      b->vel_x = normfx(COS(angle) * scale + vel_scale * vx);
      b->vel_y = normfx(SIN(angle) * scale + vel_scale * vy);

      curr_x = prev_x + b->vel_x_b;
      curr_y = prev_y + b->vel_y_b;
    }

    if (curr_x < fx4i(fruit_width / 2) ||
        curr_x >= fx4i(WIDTH - (fruit_width / 2)) ||
        curr_y < fx4i(fruit_height / 2) ||
        curr_y >= fx4i(HEIGHT - (fruit_height / 2)))
    {
      CopyFruit(fruit, prev_x >> 4, prev_y >> 4);
      KillBranch(b, lastp);
    } else {
      b->pos_x = curr_x;
      b->pos_y = curr_y;

      prev_x >>= 4;
      prev_y >>= 4;
      curr_x >>= 4;
      curr_y >>= 4;
      
      DrawBranch(prev_x, prev_y, curr_x, curr_y);

      if ((u_short)random() < (u_short)(65536 * 0.2f)) {
        if (!SplitBranch(b, lastp)) {
          CopyFruit(fruit, curr_x, curr_y);
        }
      }
    }
  }
}

PROFILE(GrowTree);

static void Render(void) {
  short val;

  if ((val = TrackValueGet(&TreeVariant, frameCount))) {
    short i;
    nrPal = val - 1;
    BitmapClear(screen);
    GreetsNextTrack();

    for (i = 0; i < NSPRITES; i++)
      CopInsSetSprite(sprptr[i], &grass[i]);

    MakeBranch(WIDTH / 2, HEIGHT - fruit_height / 2 - 1);
  }

  ProfilerStart(GrowTree);
  // Call function twice to make drawing trees visually faster
  GrowingTree(branches, &lastBranch);
  GrowingTree(branches, &lastBranch);

  HandleDrawingGreets();

  ProfilerStop(GrowTree);

  TaskWaitVBlank();
}

EFFECT(GrowingTree, NULL, NULL, Init, Kill, Render, VBlank);
