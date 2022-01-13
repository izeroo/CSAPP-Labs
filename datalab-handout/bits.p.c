#line 144 "bits.c"
int bitXor(int x, int y) {

  return ~(~x & ~y) & ~(x & y);
#line 31 "<command-line>"
#include "/usr/include/stdc-predef.h"
#line 147 "bits.c"
}
#line 154
int tmin(void) { return 0x1 << 31; }
#line 163
int isTmax(int x) {
#line 171
  return !(((x + 1) ^( ~x)) |( !(~x ^ 0x0)));
}
#line 181
int allOddBits(int x) {
  int mask=(  0xaa << 24) +( 0xaa << 16) +( 0xaa << 8) + 0xaa;
  return !((x & mask) ^ mask);
}
#line 192
int negate(int x) {

  return ~x + 1;
}
#line 205
int isAsciiDigit(int x) {
  int Tmin=  0x1 << 31;
  int a=
      ~0x30;

  int b=
      ~(0x39 | Tmin);

  int c=  Tmin &( a + x + 1) >> 31;
  int d=  Tmin &( b + x) >> 31;
  return !(c | d);
}
#line 224
int conditional(int x, int y, int z) {
#line 228
  int mask=(  !x + ~0x00);
  return (mask & y) |( ~mask & z);
}
#line 238
int isLessOrEqual(int x, int y) {
  int signx=(  x >> 31) & 1;
  int signy=(  y >> 31) & 1;
  int sign=(  signx ^ signy) & signx;
  int tmp=  y + ~x + 1;
  tmp = !((tmp >> 31) & 1) & !(signx ^ signy);
  return (sign | tmp);
}
#line 255
int logicalNeg(int x) { return ((x |( ~x + 1)) >> 31) + 1; }
#line 268
int howManyBits(int x) {
  int flag;int bit16;int bit8;int bit4;int bit2;int bit1;int bit0;
  flag = x >> 31;
  x =( ~flag & x) |( flag & ~x);


  bit16 = !(!!(x >> 16) ^ 0x1) << 4;
  x >>= bit16;
  bit8 = !(!!(x >> 8) ^ 0x1) << 3;
  x >>= bit8;
  bit4 = !(!!(x >> 4) ^ 0x1) << 2;
  x >>= bit4;
  bit2 = !(!!(x >> 2) ^ 0x1) << 1;
  x >>= bit2;
  bit1 = !(!!(x >> 1) ^ 0x1) << 0;
  x >>= bit1;
  bit0 = x;
  return bit16 + bit8 + bit4 + bit2 + bit1 + bit0 + 1;
}
#line 299
unsigned floatScale2(unsigned uf) {
#line 323
  int sign=(  uf >> 31) & 0x1;
  int exp=(  uf >> 23) & 0xFF;
  int frac=  uf & 0x7FFFFF;
  if (exp == 0 && frac == 0) 
    return uf;
  if (exp == 0xFF) 
    return uf;
  if (exp == 0) 
    return (sign << 31) |( frac << 1);
  return (sign << 31) |( ++exp << 23) | frac;
}
#line 346
int floatFloat2Int(unsigned uf) {
  int sign=(  uf >> 31) & 0x1;
  int exp=(  uf >> 23) & 0xFF;
  int frac=  uf & 0x7FFFFF;
  int E=  exp - 127;


  if (exp == 0xFF) {
    return 1 << 31;
  }

  if (exp == 0) {
    return 0;
  }

  frac = frac |( 1 << 23);
  if (E > 31) {
    return 1 << 31;
  } else if (E < 0) {
    return 0;
  }
  if (E >= 23) {
    frac <<=( E - 23);
  } else {
    frac >>=( 23 - E);
  }

  if (sign) 
    return ~frac + 1;
  return frac;
}
#line 390
unsigned floatPower2(int x) {

  if (x <= -150) {
    return 0;
  } else if (x < -126) {
    return 1 <<( 23 + x + 126);
  } else if (x <= 127) {
    return (x + 127) << 23;
  } else {
    return 0XFF << 23;
  };
}
