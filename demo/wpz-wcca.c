#include "std/debug.h"
#include "std/math.h"
#include "std/memory.h"
#include "std/resource.h"

#include "audio/stream.h"
#include "engine/object.h"
#include "engine/scene.h"
#include "uvmap/misc.h"
#include "uvmap/generate.h"
#include "uvmap/raycast.h"
#include "uvmap/render.h"
#include "uvmap/scaling.h"
#include "gfx/blit.h"
#include "gfx/colorfunc.h"
#include "gfx/colors.h"
#include "gfx/hsl.h"
#include "gfx/layers.h"
#include "gfx/palette.h"
#include "gfx/rectangle.h"

#include "system/audio.h"
#include "system/c2p.h"
#include "system/display.h"

#include "config.h"
#include "demo.h"
#include "timeline.h"

const int WIDTH = 320;
const int HEIGHT = 256;
const int DEPTH = 8;

const int H_RAYS = 41;
const int V_RAYS = 33;

const char *DemoConfigPath = "wpz-wcca.json";

static AudioStreamT *TheMusic = NULL;
static PixBufT *TheLoadImg = NULL;
static PaletteT *TheLoadPal = NULL;

PARAMETER(PixBufT *, TheCanvas, NULL);
PARAMETER(PaletteT *, ThePalette, NULL);
PARAMETER(PixBufT *, ClipartImg, NULL);
PARAMETER(PaletteT *, ClipartPal, NULL);
PARAMETER(int, ClipartX, 0);
PARAMETER(int, ClipartY, 0);

/*
 * Load demo.
 */
bool LoadDemo() {
  const char *loadImgPath = JsonQueryString(DemoConfig, "load/image");
  const char *loadPalPath = JsonQueryString(DemoConfig, "load/palette");
  const char *musicPath = JsonQueryString(DemoConfig, "music/file");

  if ((TheLoadImg = NewPixBufFromFile(loadImgPath)) &&
      (TheLoadPal = NewPaletteFromFile(loadPalPath)) &&
      (TheMusic = AudioStreamOpen(musicPath)) &&
      (TheCanvas = NewPixBuf(PIXBUF_CLUT, WIDTH, HEIGHT)))
  {
    if (InitDisplay(TheLoadImg->width, TheLoadImg->height, DEPTH)) {
      c2p1x1_8_c5_bm(TheLoadImg->data, GetCurrentBitMap(),
                     TheLoadImg->width, TheLoadImg->height, 0, 0);
      LoadPalette(TheLoadPal);
      DisplaySwap();
      return true;
    }
  }

  return false;
}

/*
 * Transition to demo.
 */
void BeginDemo() {
  ChangeDisplay(WIDTH, HEIGHT, DEPTH);

  /* Release loading screen image... */
  MemUnref(TheLoadImg);
  MemUnref(TheLoadPal);

  /* Play the audio stream... */
  AudioStreamPlay(TheMusic);
}

/*
 * Tear down demo.
 */
void KillDemo() {
  UnlinkPalettes(R_("RaycastTexturePal"));
  UnlinkPalettes(R_("CompTxt0Pal"));

  AudioStreamStop(TheMusic);

  MemUnref(TheMusic);
  MemUnref(TheCanvas);
}

/*
 * User updated the time.
 */
void DemoUpdateTime(int oldFrameNumber, int newFrameNumber) {
  AudioStreamUpdatePos(TheMusic);
}

/*
 * Set up resources.
 */
void SetupResources() {
  ResAdd("WeCanLogoObj", NewSceneObject("WeCanLogo", R_("WeCanLogoMesh")));
  ResAdd("WeCanLogoScene", NewScene());
  SceneAddObject(R_("WeCanLogoScene"), R_("WeCanLogoObj"));

  {
    MeshT *mesh = R_("PotatoMesh");

    CenterMeshPosition(mesh);
    CalculateSurfaceNormals(mesh);
    NormalizeMeshSize(mesh);
  }

  {
    MeshT *mesh = R_("WeCanLogoMesh");

    CalculateSurfaceNormals(mesh);
    NormalizeMeshSize(mesh);
    MeshApplyPalette(mesh, R_("WeCanBgPal"));
  }

  ResAdd("PotatoObj", NewSceneObject("Potato", R_("PotatoMesh")));
  ResAdd("PotatoScene", NewScene());
  SceneAddObject(R_("PotatoScene"), R_("PotatoObj"));

  ResAdd("RaycastMap", NewUVMap(H_RAYS, V_RAYS, UV_ACCURATE, 256, 256));
  ResAdd("UVMap", NewUVMap(WIDTH, HEIGHT, UV_NORMAL, 256, 256));
  ResAdd("UVMapA", NewUVMap(WIDTH, HEIGHT, UV_FAST, 256, 256));
  ResAdd("UVMapB", NewUVMap(WIDTH, HEIGHT, UV_FAST, 256, 256));
  ResAdd("UVMapC", NewUVMap(WIDTH, HEIGHT, UV_FAST, 256, 256));

  ResAdd("LayerMap", NewPixBuf(PIXBUF_GRAY, WIDTH, HEIGHT));
  ResAdd("ComposeMap", NewPixBuf(PIXBUF_GRAY, WIDTH, HEIGHT));
  ResAdd("ShadeMap", NewPixBuf(PIXBUF_GRAY, WIDTH, HEIGHT));

  LinkPalettes(R_("RaycastTexturePal"), R_("WhelpzLogoPal"), NULL);
  PixBufRemap(R_("WhelpzLogoImg"), R_("WhelpzLogoPal"));

  UVMapGenerate3(R_("UVMapA"));
  UVMapSetTexture(R_("UVMapA"), R_("CompTxt0Img"));
  UVMapGenerate4(R_("UVMapB"));
  UVMapSetTexture(R_("UVMapB"), R_("CompTxt1Img"));
  UVMapGenerate2(R_("UVMapC"));
  UVMapSetTexture(R_("UVMapC"), R_("RaycastTextureImg"));

  ResAdd("CompTxt0PalOrig", MemClone(R_("CompTxt0Pal")));

  LinkPalettes(R_("CompTxt0Pal"), R_("CompTxt1Pal"), NULL);
  PixBufRemap(R_("CompTxt1Img"), R_("CompTxt1Pal"));

  ResAdd("ColorFunc", NewColorFunc());
  ResAdd("EffectPal", NewPalette(256));
}

/*** Raycast *****************************************************************/

typedef struct {
  Vector3D Nominal[3];
  Vector3D Transformed[3];
  Matrix3D Transformation;
} CameraViewT;

static CameraViewT CameraView = {
  .Nominal = {
    { -0.5f,  0.333333f, 0.5f },
    {  0.5f,  0.333333f, 0.5f },
    { -0.5f, -0.333333f, 0.5f }
  }
};

PARAMETER(PixBufT *, RaycastTexture, NULL);

CALLBACK(RaycastSetView1) {
  float t = FrameTime(frame);
  int y = 0;

  if (t > 0.75)
    y = 4 * (t - 0.75) * 90;

  LoadRotation3D(&CameraView.Transformation,
                 0.0f, y, (frame->number + frame->first) * 0.75f);
}

CALLBACK(RaycastSetView2) {
  LoadRotation3D(&CameraView.Transformation,
                 0.0f, 90.0f, (frame->number + frame->first) * 0.75f);
}

CALLBACK(RaycastSetView3) {
  float t = FrameTime(frame);
  int y = 90;

  if (t < 0.25)
    y = 4 * t * 90;

  LoadRotation3D(&CameraView.Transformation,
                 0.0f, y + 90, (frame->number + frame->first) * 0.75f);
}

CALLBACK(RaycastCalculateView) {
  Vector3D *transformed = CameraView.Transformed;
  Vector3D *nominal = CameraView.Nominal;

  Transform3D(transformed, nominal, 3, &CameraView.Transformation);

  V3D_Sub(&transformed[1], &transformed[1], &transformed[0]);
  V3D_Sub(&transformed[2], &transformed[2], &transformed[0]);
}

CALLBACK(RenderRaycast) {
  UVMapT *map = R_("UVMap");
  UVMapT *smallMap = R_("RaycastMap");
  PixBufT *shades = R_("ShadeMap");

  RaycastTunnel(smallMap, CameraView.Transformed);
  UVMapScale8x(map, smallMap);
  UVMapSetTexture(map, RaycastTexture);
  UVMapRender(map, TheCanvas);
}

CALLBACK(CalculateShadeMap1) {
  UVMapT *uvmap = R_("UVMap");
  PixBufT *shades = R_("ShadeMap");
  int16_t *map = uvmap->map.normal.v;
  uint8_t *dst = shades->data;
  int n = shades->width * shades->height;
  int f = FrameTime(frame) * 256 - 224;

  while (n--) {
    int value = f - (*map++);

    if (value < 0) {
      value = -value;
      if (value < 128)
        value = 128;
      else if (value < 255)
        value = 255 - value;
      else
        value = 0;
    } else {
      value = 128;
    }

    *dst++ = value;
  }
}

CALLBACK(CalculateShadeMap2) {
  UVMapT *uvmap = R_("UVMap");
  PixBufT *shades = R_("ShadeMap");
  int16_t *map = uvmap->map.normal.v;
  uint8_t *dst = shades->data;
  int n = shades->width * shades->height;
  int f = FrameTime(frame) * 224.0f;

  while (n--) {
    int value = - (*map++) + f;

    if (value < 0) {
      value = 128;
    } else {
      if (value < 128)
        value = 128;
      if (value > 255)
        value = 255;
    }

    *dst++ = value;
  }
}

CALLBACK(RenderRaycastLight) {
  UVMapT *map = R_("UVMap");
  PixBufT *shades = R_("ShadeMap");

  PixBufSetColorMap(shades, R_("RaycastColorMap"), 0);
  PixBufSetBlitMode(shades, BLIT_COLOR_MAP);
  PixBufBlit(TheCanvas, 0, 0, shades, NULL);
}

PARAMETER(PixBufT *, LightMap, NULL);

CALLBACK(BlitLightMapToCanvas) {
  PixBufSetColorMap(LightMap, R_("RaycastColorMap"), 0);
  PixBufSetBlitMode(LightMap, BLIT_COLOR_MAP);
  PixBufBlit(TheCanvas, 0, 0, LightMap, NULL);
}

/*** Composition *************************************************************/

CALLBACK(ClearComposeMap) {
  PixBufClear(R_("ComposeMap"));
}

CALLBACK(CalculateComposeMap) {
  uint8_t *cfunc = R_("ColorFunc");
  UVMapT *map = R_("UVMapB");
  PixBufT *comp = NewPixBufWrapper(WIDTH, HEIGHT, map->map.fast.v);
  PixBufT *compMap = R_("ComposeMap");
  int i;

  for (i = 0; i < 256; i++)
    cfunc[i] = ((128 - (frame->number % 256 + i)) & 0xff) >= 128 ? 1 : 0;

  PixBufSetColorFunc(comp, cfunc);
  PixBufSetBlitMode(comp, BLIT_COLOR_FUNC);
  PixBufBlit(compMap, 0, 0, comp, NULL);

  MemUnref(comp);
}

CALLBACK(FadeFromWhite) {
  PaletteT *pal = R_("EffectPal");
  float t = 1.0f - FrameTime(frame);

  void Fade(RGB *dst, RGB *src) {
    dst->r = (float)(255 - src->r) * t + src->r;
    dst->g = (float)(255 - src->g) * t + src->g;
    dst->b = (float)(255 - src->b) * t + src->b;
  }

  PaletteModify(pal, ThePalette, Fade);
  LoadPalette(pal);
}

CALLBACK(FadeFromBlack) {
  PaletteT *pal = R_("EffectPal");
  float t = FrameTime(frame);

  void Fade(RGB *dst, RGB *src) {
    dst->r = src->r * t;
    dst->g = src->g * t;
    dst->b = src->b * t;
  }

  PaletteModify(pal, ThePalette, Fade);
  LoadPalette(pal);
}

CALLBACK(FadeToBlack) {
  PaletteT *pal = R_("EffectPal");
  float t = 1.0f - FrameTime(frame);

  void Fade(RGB *dst, RGB *src) {
    dst->r = src->r * t;
    dst->g = src->g * t;
    dst->b = src->b * t;
  }

  PaletteModify(pal, ThePalette, Fade);
  LoadPalette(pal);
}

CALLBACK(CycleHue) {
  float temp;
  float t = modff(frame->number / (DemoBeat * 4.0f), &temp);

  void CyclicHue(RGB *dst, RGB *src) {
    HSL hsl;

    RGB2HSL(src, &hsl);

    hsl.h += t;

    if (hsl.h >= 1.0f)
      hsl.h -= 1.0f;

    HSL2RGB(&hsl, dst);
  }

  PaletteModify(R_("CompTxt0Pal"), R_("CompTxt0PalOrig"), CyclicHue);
}

CALLBACK(ComposeMaps) {
  UVMapT *map1 = R_("UVMapA");
  UVMapT *map2 = R_("UVMapB");
  PixBufT *compMap = R_("ComposeMap");
  int du = frame->number;
  int dv = 2 * frame->number;

  UVMapSetOffset(map1, du, dv);
  UVMapSetOffset(map2, -du, -dv);
  UVMapComposeAndRender(TheCanvas, compMap, map1, map2);
}

/*** Potato ******************************************************************/

CALLBACK(ControlPotato) {
  MatrixStack3D *ms = GetObjectTranslation(R_("PotatoScene"), "Potato");

  StackReset(ms);
  PushScaling3D(ms, 1.75f, 1.75f, 1.75f);
  PushRotation3D(ms, 0, (float)(-frame->number * 2), frame->number);
  PushTranslation3D(ms, 0.0f, 0.0f, -2.0f);
}

CALLBACK(RenderPotato) {
  PixBufT *shades = R_("ShadeMap");
  RenderFlatShading = true;

  PixBufClear(shades);
  RenderScene(R_("PotatoScene"), shades);

  PixBufSetColorMap(shades, R_("RaycastColorMap"), 0);
  PixBufSetBlitMode(shades, BLIT_COLOR_MAP);

  PixBufBlit(TheCanvas, 0, 0, shades, NULL);
}

CALLBACK(RenderPotatoBackground) {
  int du = 2 * frame->number;
  int dv = 4 * frame->number;

  UVMapSetOffset(R_("UVMapC"), du, dv);
  UVMapRender(R_("UVMapC"), TheCanvas);
}

/*** We Can Screen ***********************************************************/

CALLBACK(Transition) {
  PixBufT *map = R_("LayerMap");
  PixBufT *image[] = { R_("WeCanBg1Img"), R_("WeCanBg2Img") };
  float t = FrameTime(frame);

  PixBufClear(map);
  map->fgColor = 1;
  DrawRectangle(map, 0, 0, 320 * t, 200);

  LayersCompose(TheCanvas, map, image, 2);
}

CALLBACK(RenderWeCanLogo) {
  SceneT *scene = R_("WeCanLogoScene");

  RenderFlatShading = false;

  {
    MatrixStack3D *ms = GetObjectTranslation(scene, "WeCanLogo");

    StackReset(ms);
    PushRotation3D(ms, 0, (float)(-frame->number * 2), 0);
    PushTranslation3D(ms, 0.0f, 0.0f, -2.0f);
    //PushTranslation3D(ms, -1.0f, -0.775f, -1.95f);
  }

  RenderAllFaces = true;

  RenderScene(scene, TheCanvas);
}

CALLBACK(ShowWeCanBg1) {
  PixBufBlit(TheCanvas, 0, 0, R_("WeCanBg1Img"), NULL);
}

CALLBACK(ShowWeCanBg2) {
  PixBufBlit(TheCanvas, 0, 0, R_("WeCanBg2Img"), NULL);
}

ARRAY(float, 2, DatePos, 100.0f, 200.0f);

CALLBACK(ShowDate)  {
  PixBufT *date = R_("DateImg");
  PixBufSetBlitMode(date, BLIT_TRANSPARENT);
  PixBufBlit(TheCanvas, DatePos[0], DatePos[1], date, NULL);
}

ARRAY(float, 2, CodeAgainPos, 20.0f, 20.f);

CALLBACK(ShowCodeAgain) {
  PixBufT *date = R_("CodeAgainImg");
  PixBufSetBlitMode(date, BLIT_TRANSPARENT);
  PixBufBlit(TheCanvas, CodeAgainPos[0], CodeAgainPos[1], date, NULL);
}

ARRAY(float, 2, TwoPos, 135.0f, 80.0f);

CALLBACK(ShowTwo)  {
  PixBufT *two = R_("TwoImg");
  PixBufSetColorMap(two, R_("WeCanColorMap"), 0);
  PixBufSetBlitMode(two, BLIT_COLOR_MAP);
  PixBufBlit(TheCanvas, TwoPos[0], TwoPos[1], two, NULL);
}

ARRAY(float, 2, WarnungPos, 20.0f, 10.0f);

CALLBACK(ShowWarnung)  {
  PixBufT *warnung = R_("WarnungImg");
  PixBufSetColorMap(warnung, R_("WeCanColorMap"), 0);
  PixBufSetBlitMode(warnung, BLIT_COLOR_MAP);
  PixBufBlit(TheCanvas, WarnungPos[0], WarnungPos[1], warnung, NULL);
}

ARRAY(float, 2, AttenzionePos, 100.0f, 10.0f);

CALLBACK(ShowAttenzione) {
  PixBufT *attenzione = R_("AttenzioneImg");
  PixBufSetColorMap(attenzione, R_("WeCanColorMap"), 0);
  PixBufSetBlitMode(attenzione, BLIT_COLOR_MAP);
  PixBufBlit(TheCanvas, AttenzionePos[0], AttenzionePos[1], attenzione, NULL);
}

/*****************************************************************************/

CALLBACK(BlitClipartToCanvas) {
  PixBufBlit(TheCanvas, ClipartX, ClipartY, ClipartImg, NULL);
}

CALLBACK(ClearCanvas) {
  PixBufClear(TheCanvas);
}

CALLBACK(ReloadPalette) {
  LoadPalette(ThePalette);
}

CALLBACK(RenderCanvas) {
  c2p1x1_8_c5_bm(TheCanvas->data, GetCurrentBitMap(), WIDTH, HEIGHT, 0, 0);
}

CALLBACK(FeedAudioStream) {
  AudioStreamFeed(TheMusic);
}

#include "wpz-wcca.syms"