/* Fpu087.c
 * This file contains routines to replace fpu087.asm.
 *
 * This file Copyright 1991 Ken Shirriff.  It may be used according to the
 * fractint license conditions, blah blah blah.
 */


#include "port.h"
#include "prototyp.h"

/*
 *----------------------------------------------------------------------
 *
 * xxx086 --
 *
 * Simulate integer math routines using floating point.
 * This will of course slow things down, so this should all be
 * changed at some point.
 *
 *----------------------------------------------------------------------
 */

int overflow = 0;

double _2_ = 2.0;
double _1_ = 1.0;
double PointFive = 0.5;
double infinity = 1.0E+300;

#ifndef NASM
void FPUaptan387(double *y, double *x, double *atan)
{
    if (isnan(*x) || isnan(*y) || isinf(*x) || isinf(*y))
        *atan = 1.0;
    else
        *atan = (double)atan2l(*y,*x);
}

void FPUcplxmul(_CMPLX *x, _CMPLX *y, _CMPLX *z)
{
    double tx, ty;

    if (x->y == 0.0 && y->y == 0.0)   /* x & y are real */
    {
        tx = (double)x->x * (double)y->x;
        ty = 0.0;
    }
    else if (y->y == 0.0)   /* y is real */
    {
        tx = (double)x->x * (double)y->x;
        ty = (double)x->y * (double)y->x;
    }
    else if (x->y == 0.0)   /* x is real */
    {
        tx = (double)x->x * (double)y->x;
        ty = (double)x->x * (double)y->y;
    }
    else
    {
        tx = (double)x->x * (double)y->x - (double)x->y * (double)y->y;
        ty = (double)x->x * (double)y->y + (double)x->y * (double)y->x;
    }

    if (isnan(ty) || isinf(ty))
        z->y = (double)infinity;
    else
        z->y = (double)ty;
    if (isnan(tx) || isinf(tx))
        z->x = (double)infinity;
    else
        z->x = (double)tx;
}

void FPUcplxdiv(_CMPLX *x, _CMPLX *y, _CMPLX *z)
{
    double mod,tx,ty,yxmod,yymod, yx, yy;
    yx = y->x;
    yy = y->y;
    mod = yx * yx + yy * yy;
    if (mod == 0.0 || fabsl(mod) <= DBL_MIN)
    {
        z->x = (double)infinity;
        z->y = (double)infinity;
        overflow = 1;
        return;
    }

    if (yy == 0.0)   /* if y is real */
    {
        z->x = (double)x->x / yx;
        z->y = (double)x->y / yx;
    }
    else
    {
        yxmod = yx/mod;
        yymod = - yy/mod;
        tx = x->x * yxmod - x->y * yymod;
        ty = x->x * yymod + x->y * yxmod;
        z->x = (double)tx;
        z->y = (double)ty;
    }
}

void FPUsincos(double *Angle, double *Sin, double *Cos)
{
    if (isnan(*Angle) || isinf(*Angle))
    {
        *Sin = 0.0;
        *Cos = 1.0;
    }
    else
    {
        *Sin = (double)sinl(*Angle);
        *Cos = (double)cosl(*Angle);
    }
}

void FPUsinhcosh(double *Angle, double *Sinh, double *Cosh)
{
    if (isnan(*Angle) || isinf(*Angle))
    {
        *Sinh = 1.0;
        *Cosh = 1.0;
    }
    else
    {
        *Sinh = (double)sinhl(*Angle);
        *Cosh = (double)coshl(*Angle);
    }
    if (isnan(*Sinh) || isinf(*Sinh))
        *Sinh = 1.0;
    if (isnan(*Cosh) || isinf(*Cosh))
        *Cosh = 1.0;
}

void FPUcplxlog(_CMPLX *x, _CMPLX *z)
{
    double mod, xx, xy;
    xx = x->x;
    xy = x->y;
    if (xx == 0.0 && xy == 0.0)
    {
        z->x = z->y = 0.0;
        overflow = 1;
        return;
    }
#if 0  /* Don't need this */
    else if(xy == 0.0)   /* x is real */
    {
        z->x = logl(xx);
        z->y = 0.0;
        return;
    }
#endif
    mod = xx*xx + xy*xy;   /* always positive */
    if (isnan(mod) || isinf(mod))
        z->x = 0.5;
    else
        z->x = (double)(0.5 * logl(mod));
    if (isnan(xx) || isnan(xy) || isinf(xx) || isinf(xy))
        z->y = 1.0;
    else
        z->y = (double)atan2l(xy,xx);
}
#endif // NASM

/* Integer Routines */
void SinCos086(long x, long *sinx, long *cosx)
{
    double a;
    a = (double)x/(double)(1<<16);
    *sinx = (long) (sin(a)*(double)(1<<16));
    *cosx = (long) (cos(a)*(double)(1<<16));
}

void SinhCosh086(long x, long *sinx, long *cosx)
{
    double a;
    a = (double)x/(double)(1<<16);
    *sinx = (long) (sinh(a)*(double)(1<<16));
    *cosx = (long) (cosh(a)*(double)(1<<16));
}

long Exp086(x)
long x;
{
    return (long) (exp((double)x/(double)(1<<16))*(double)(1<<16));
}

#define em2float(l) (*(float *)&(l))
#define float2em(f) (*(int *)&(f))

/*
 * Input is a 16 bit offset number.  Output is shifted by Fudge.
 */
unsigned int ExpFudged(x, Fudge)
int x;
int Fudge;
{
    return (int) (exp((double)x/(double)(1<<16))*(double)(1<<Fudge));
}

/* This multiplies two e/m numbers and returns an e/m number. */
int r16Mul(x,y)
int x,y;
{
    float f;
    f = em2float(x)*em2float(y);
    return float2em(f);
}

/* This takes an exp/mant number and returns a shift-16 number */
int LogFloat14(x)
unsigned int x;
{
    return log((double)em2float(x))*(1<<16);
}

/* This divides two e/m numbers and returns an e/m number. */
int RegDivFloat(x,y)
int x,y;
{
    float f;
    f = em2float(x)/em2float(y);
    return float2em(f);
}

/*
 * This routine on the IBM converts shifted integer x,FudgeFact to
 * the 4 byte number: exp,mant,mant,mant
 * Instead of using exp/mant format, we'll just use floats.
 * Note: If sizeof(float) != sizeof(long), we're hosed.
 *     Changed long to int because the above is the case for 64-bit Linux.
*/
int RegFg2Float(x,FudgeFact)
int x;
int FudgeFact;
{
    float f;
    int l;
    f = x/(float)(1<<FudgeFact);
    l = float2em(f);
    return l;
}

/*
 * This converts em to shifted integer format.
 */
int RegFloat2Fg(x,Fudge)
int x;
int Fudge;
{
    return em2float(x)*(float)(1<<Fudge);
}

int RegSftFloat(x, Shift)
int x;
int Shift;
{
    float f;
    f = em2float(x);
    if (Shift>0)
    {
        f *= (1 << Shift);
    }
    else
    {
        f /= (1 << -Shift);
    }
    return float2em(f);
}
