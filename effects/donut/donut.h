/* Adapted from https://www.a1k0n.net/2021/01/13/optimizing-donut.html */

#define W 40
#define H 32
#define D 30

#define R(mul, shift, x, y)                                                    \
  _ = x;                                                                       \
  x -= mul * y >> shift;                                                       \
  y += mul * _ >> shift;                                                       \
  _ = (3145728 - x * x - y * y) >> 11;                                         \
  x = x * _ >> 10;                                                             \
  y = y * _ >> 10;

static char b[W * H], z[W * H];
static const char pixels[] = ".,-~:;=!*#$@";

void CalcDonut(void) {
  static short sA = 1024, cA = 0, sB = 1024, cB = 0, _;

  short sj = 0, cj = 1024;
  short i, j;

  memset(b, 32, W*H);  // text buffer
  memset(z, 127, W*H); // z buffer

  for (j = 0; j < 90; j++) {
    // sine and cosine of angle i
    short si = 0;
    short ci = 1024;
#ifdef Log
    Log("j = %d\n", j);
#endif
    for (i = 0; i < 324; i++) {
      short R1 = 1;
      short R2 = 2048;
      int K2 = 5120 * 1024;

      /* int */ short x0 = R1 * cj + R2;
      /* int */ short x1 = ci * x0 >> 10;
      /* int */ short x2 = cA * sj >> 10;
      /* int */ short x3 = si * x0 >> 10;
      /* int */ short x4 = R1 * x2 - (sA * x3 >> 10);
      /* int */ short x5 = sA * sj >> 10;
      int x6 = K2 + R1 * 1024 * x5 + cA * x3;
      /* int */ short x7 = cj * si >> 10;
      /* int */ short x = W / 2 + D * (cB * x1 - sB * x4) / x6;
      /* int */ short y = H / 2 + D / 2 * (cB * x4 + sB * x1) / x6;
      short _N1 = (-sA * x7 >> 10) + x2;
      short _N2 = cj * sB >> 10;
      int N = (((-cA * x7 - cB * _N1 - ci * _N2) >> 10) - x5) >> 7;

      int o = x + W * y;
      char zz = (x6 - K2) >> 15;

      if (H > y && y >= 0 && x >= 0 && W > x && zz < z[o]) {
        z[o] = zz;
        b[o] = pixels[N > 0 ? N : 0];
      }
      R(5, 8, ci, si) // rotate i
    }
    R(9, 7, cj, sj) // rotate j
  }

  R(5, 7, cA, sA);
  R(5, 8, cB, sB);
}