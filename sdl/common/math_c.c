
/* see Fractint.c for a description of the "include"  hierarchy */
#include "port.h"
#include "prototyp.h"

#ifndef sqr
#define sqr(x) ((x)*(x))
#endif

#ifndef NASM
void FPUcplxexp(_CMPLX *x, _CMPLX *z)
{
    LDBL e2x, y;

    y = x->y;
    e2x = expl(x->x);
    if (isnan(e2x) || isinf(e2x))
        e2x = 1.0;
    z->x = (LDBL)(e2x * cosl(y));
    z->y = (LDBL)(e2x * sinl(y));
}
#endif // NASM

_CMPLX ComplexPower(_CMPLX xx, _CMPLX yy)
{
    _CMPLX z, cLog, t;

    /* fixes power bug - if any complaints, backwards compatibility hook
       goes here TIW 3/95 */
    if (ldcheck == 0)
        if (xx.x == 0.0 && xx.y == 0.0)
        {
            z.x = z.y = 0.0;
            return(z);
        }

    if(yy.y == 0.0)   /* yy is real */
    {
        if(yy.x == 0.0)   /* x^0 = (1.0, 0.0) */
        {
            z.x = 1.0;
            z.y = 0.0;
            return(z);
        }
        if(yy.x == 1.0)   /* x^1 = x */
        {
            z.x = xx.x;
            z.y = xx.y;
            return(z);
        }
    }

    FPUcplxlog(&xx, &cLog);
    FPUcplxmul(&cLog, &yy, &t);

    FPUcplxexp(&t, &z);

    return(z);
}

/* Raise complex number (base) to the (exp) power, storing the result
** in complex (result).
*/
static LDBL xt, yt, t2;

void cpower(_CMPLX *base, int exp, _CMPLX *result)
{
    if (exp<0)
    {
        cpower(base,-exp,result);
        CMPLXrecip(*result,*result);
        return;
    }

    xt = base->x;
    yt = base->y;

    if (exp & 1)
    {
        result->x = xt;
        result->y = yt;
    }
    else
    {
        result->x = 1.0;
        result->y = 0.0;
    }

    exp >>= 1;
    while (exp)
    {
        t2 = xt * xt - yt * yt;
        yt = 2 * xt * yt;
        xt = t2;

        if (exp & 1)
        {
            t2 = xt * result->x - yt * result->y;
            result->y = result->y * xt + yt * result->x;
            result->x = t2;
        }
        exp >>= 1;
    }
}

/* long version */
static long lxt, lyt, lt2;
int
lcpower(_LCMPLX *base, int exp, _LCMPLX *result, int bitshift)
{
    /*
    static long maxarg;
    maxarg = 64L<<bitshift;
    */

    if (exp<0)
    {
        overflow = lcpower(base,-exp,result,bitshift);
        LCMPLXrecip(*result,*result);
        return(overflow);
    }

    overflow = 0;
    lxt = base->x;
    lyt = base->y;

    if (exp & 1)
    {
        result->x = lxt;
        result->y = lyt;
    }
    else
    {
        result->x = 1L<<bitshift;
        result->y = 0L;
    }

    exp >>= 1;
    while (exp)
    {
        /*
        if(labs(lxt) >= maxarg || labs(lyt) >= maxarg)
           return(-1);
        */
        lt2 = multiply(lxt, lxt, bitshift) - multiply(lyt,lyt,bitshift);
        lyt = multiply(lxt,lyt,bitshiftless1);
        if (overflow)
            return(overflow);
        lxt = lt2;

        if (exp & 1)
        {
            lt2 = multiply(lxt,result->x, bitshift) - multiply(lyt,result->y,bitshift);
            result->y = multiply(result->y,lxt,bitshift) + multiply(lyt,result->x,bitshift);
            result->x = lt2;
        }
        exp >>= 1;
    }
    if (result->x == 0 && result->y == 0)
        overflow = 1;
    return(overflow);
}

#if 0
int
complex_mult(_CMPLX arg1,_CMPLX arg2,_CMPLX *pz)
{
    pz->x = arg1.x*arg2.x - arg1.y*arg2.y;
    pz->y = arg1.x*arg2.y+arg1.y*arg2.x;
    return(0);
}

int
complex_div(_CMPLX numerator,_CMPLX denominator,_CMPLX *pout)
{
    LDBL mod;
    if ((mod = modulus(denominator)) < LDBL_MIN)
        return(1);
    conjugate(&denominator);
    complex_mult(numerator,denominator,pout);
    pout->x = pout->x/mod;
    pout->y = pout->y/mod;
    return(0);
}


int
z_to_the_z(_CMPLX *z, _CMPLX *out)
{
    static _CMPLX tmp1,tmp2;
    /* raises complex z to the z power */
    int errno_xxx;
    errno_xxx = 0;

    if (fabs(z->x) < DBL_EPSILON) return(-1);

    /* log(x + iy) = 1/2(log(x*x + y*y) + i(arc_tan(y/x)) */
    tmp1.x = .5*log(sqr(z->x)+sqr(z->y));

    /* the fabs in next line added to prevent discontinuity in image */
    tmp1.y = atan(fabs(z->y/z->x));

    /* log(z)*z */
    tmp2.x = tmp1.x * z->x - tmp1.y * z->y;
    tmp2.y = tmp1.x * z->y + tmp1.y * z->x;

    /* z*z = e**(log(z)*z) */
    /* e**(x + iy) =  e**x * (cos(y) + isin(y)) */

    tmpexp = exp(tmp2.x);

    FPUsincos(&tmp2.y,&siny,&cosy);
    out->x = tmpexp*cosy;
    out->y = tmpexp*siny;
    return(errno_xxx);
}
#endif

/*

  The following Complex function routines added by Tim Wegner November 1994.

*/

#define Sqrtz(z,rz) (*(rz) = ComplexSqrtFloat((z).x, (z).y))

/* rz=Arcsin(z)=-i*Log{i*z+sqrt(1-z*z)} */
void Arcsinz(_CMPLX z,_CMPLX *rz)
{
    _CMPLX tempz1,tempz2;

    FPUcplxmul( &z, &z, &tempz1);
    tempz1.x = 1.0 - tempz1.x;
    tempz1.y = -tempz1.y;  /* tempz1 = 1 - tempz1 */
    Sqrtz( tempz1, &tempz1);

    tempz2.x = -z.y;
    tempz2.y = z.x;                /* tempz2 = i*z  */
    tempz1.x += tempz2.x;
    tempz1.y += tempz2.y;    /* tempz1 += tempz2 */
    FPUcplxlog( &tempz1, &tempz1);
    rz->x = tempz1.y;
    rz->y = -tempz1.x;           /* rz = (-i)*tempz1 */
}   /* end. Arcsinz */


/* rz=Arccos(z)=-i*Log{z+sqrt(z*z-1)} */
void Arccosz(_CMPLX z,_CMPLX *rz)
{
    _CMPLX temp;

    FPUcplxmul( &z, &z, &temp);
    temp.x -= 1.0;                                 /* temp = temp - 1 */
    Sqrtz( temp, &temp);

    temp.x += z.x;
    temp.y += z.y;                /* temp = z + temp */

    FPUcplxlog( &temp, &temp);
    rz->x = temp.y;
    rz->y = -temp.x;              /* rz = (-i)*tempz1 */
}   /* end. Arccosz */

void Arcsinhz(_CMPLX z,_CMPLX *rz)
{
    _CMPLX temp;

    FPUcplxmul( &z, &z, &temp);
    temp.x += 1.0;                                 /* temp = temp + 1 */
    Sqrtz( temp, &temp);
    temp.x += z.x;
    temp.y += z.y;                /* temp = z + temp */
    FPUcplxlog( &temp, rz);
}  /* end. Arcsinhz */

/* rz=Arccosh(z)=Log(z+sqrt(z*z-1)} */
void Arccoshz(_CMPLX z,_CMPLX *rz)
{
    _CMPLX tempz;
    FPUcplxmul( &z, &z, &tempz);
    tempz.x -= 1.0;                                /* tempz = tempz - 1 */
    Sqrtz( tempz, &tempz);
    tempz.x += z.x;
    tempz.y += z.y;             /* tempz = z + tempz */
    FPUcplxlog( &tempz, rz);
}   /* end. Arccoshz */

/* rz=Arctanh(z)=1/2*Log{(1+z)/(1-z)} */
void Arctanhz(_CMPLX z,_CMPLX *rz)
{
    _CMPLX temp0,temp1,temp2;

    if ( z.x == 0.0)
    {
        rz->x = 0.0;
        rz->y = atanl( z.y);
        return;
    }
    else
    {
        if ( fabsl(z.x) == 1.0 && z.y == 0.0)
        {
            return;
        }
        else if ( fabsl( z.x) < 1.0 && z.y == 0.0)
        {
            rz->x = logl((1+z.x)/(1-z.x))/2.0;
            rz->y = 0.0;
            return;
        }
        else
        {
            temp0.x = 1.0 + z.x;
            temp0.y = z.y;             /* temp0 = 1 + z */
            temp1.x = 1.0 - z.x;
            temp1.y = -z.y;            /* temp1 = 1 - z */
            FPUcplxdiv( &temp0, &temp1, &temp2);
            FPUcplxlog( &temp2, &temp2);
            rz->x = .5*temp2.x;
            rz->y = .5*temp2.y;       /* rz = .5*temp2 */
            return;
        }
    }
}   /* end. Arctanhz */

/* rz=Arctan(z)=i/2*Log{(1-i*z)/(1+i*z)} */
void Arctanz(_CMPLX z,_CMPLX *rz)
{
    _CMPLX temp0,temp1,temp2,temp3;
    if ( z.x == 0.0 && z.y == 0.0)
        rz->x = rz->y = 0.0;
    else if ( z.x != 0.0 && z.y == 0.0)
    {
        rz->x = atanl( z.x);
        rz->y = 0.0;
    }
    else if ( z.x == 0.0 && z.y != 0.0)
    {
        temp0.x = z.y;
        temp0.y = 0.0;
        Arctanhz( temp0, &temp0);
        rz->x = -temp0.y;
        rz->y = temp0.x;              /* i*temp0 */
    }
    else if ( z.x != 0.0 && z.y != 0.0)
    {

        temp0.x = -z.y;
        temp0.y = z.x;                  /* i*z */
        temp1.x = 1.0 - temp0.x;
        temp1.y = -temp0.y;      /* temp1 = 1 - temp0 */
        temp2.x = 1 + temp0.x;
        temp2.y = temp0.y;       /* temp2 = 1 + temp0 */

        FPUcplxdiv( &temp1, &temp2, &temp3);
        FPUcplxlog( &temp3, &temp3);
        rz->x = -temp3.y*.5;
        rz->y = .5*temp3.x;           /* .5*i*temp0 */
    }
}   /* end. Arctanz */

#define SinCosFudge 0x10000L
#ifdef LONGSQRT
long lsqrt(long f)
{
    int N;
    unsigned long y0, z;
    static long a=0, b=0, c=0;                  /* constant factors */

    if (f == 0)
        return f;
    if (f <  0)
        return 0;

    if (a==0)                                   /* one-time compute consts */
    {
        a = (long)(fudge * .41731);
        b = (long)(fudge * .59016);
        c = (long)(fudge * .7071067811);
    }

    N  = 0;
    while (f & 0xff000000L)                     /* shift arg f into the */
    {
        /* range: 0.5 <= f < 1  */
        N++;
        f /= 2;
    }
    while (!(f & 0xff800000L))
    {
        N--;
        f *= 2;
    }

    y0 = a + multiply(b, f,  bitshift);         /* Newton's approximation */

    z  = y0 + divide (f, y0, bitshift);
    y0 = (z>>2) + divide(f, z,  bitshift);

    if (N % 2)
    {
        N++;
        y0 = multiply(c,y0, bitshift);
    }
    N /= 2;
    if (N >= 0)
        return y0 <<  N;                        /* correct for shift above */
    else
        return y0 >> -N;
}
#endif
LCMPLX ComplexSqrtLong(long x, long y)
{
    double    mag, theta;
    long      maglong, thetalong;
    LCMPLX    result;

#ifndef LONGSQRT
    mag       = sqrt(sqrt(((double) multiply(x,x,bitshift))/fudge +
                          ((double) multiply(y,y,bitshift))/ fudge));
    maglong   = (long)(mag * fudge);
#else
    maglong   = lsqrt(lsqrt(multiply(x,x,bitshift)+multiply(y,y,bitshift)));
#endif
    theta     = atan2((double) y/fudge, (double) x/fudge)/2;
    thetalong = (long)(theta * SinCosFudge);
    SinCos086(thetalong, &result.y, &result.x);
    result.x  = multiply(result.x << (bitshift - 16), maglong, bitshift);
    result.y  = multiply(result.y << (bitshift - 16), maglong, bitshift);
    return result;
}

_CMPLX ComplexSqrtFloat(LDBL x, LDBL y)
{
    LDBL mag;
    double theta;
    _CMPLX  result;

    if (x == 0.0 && y == 0.0)
        result.x = result.y = 0.0;
    else if (y == 0.0)   /* number is real */
    {
        if (x >= 0.0)
        {
           result.x = sqrtl(x);
           result.y = 0.0;
        }
        else
        {
            result.x = 0.0;
            result.y = sqrtl(-x);
        }
    }
    else
    {
        mag   = sqrtl(sqrtl(x*x + y*y));
        theta = atan2l(y, x) / 2.0;
        FPUsincos(&theta, &result.y, &result.x);
        result.x *= mag;
        result.y *= mag;
    }
    return result;
}


/***** FRACTINT specific routines and variables *****/

BYTE *LogTable = (BYTE *)0;
int MaxLTSize;
int  Log_Calc = 0;
static double mlf;
static unsigned int lf;

/* int LogFlag;
   LogFlag == 1  -- standard log palettes
   LogFlag == -1 -- 'old' log palettes
   LogFlag >  1  -- compress counts < LogFlag into color #1
   LogFlag < -1  -- use quadratic palettes based on square roots && compress
*/

void SetupLogTable(void)
{
    float l, f, c, m;
    unsigned int prev, limit, sptop;
    unsigned n;

    if (save_release > 1920 || Log_Fly_Calc == 1)   /* set up on-the-fly variables */
    {
        if (LogFlag > 0)   /* new log function */
        {
            lf = (LogFlag > 1) ? LogFlag : 0;
            if (lf >= (unsigned int)MaxLTSize)
                lf = MaxLTSize - 1;
            mlf = (colors - (lf?2:1)) / log(MaxLTSize - lf);
        }
        else if (LogFlag == -1)   /* old log function */
        {
            mlf = (colors - 1) / log(MaxLTSize);
        }
        else if (LogFlag <= -2)   /* sqrt function */
        {
            if ((lf = 0 - LogFlag) >= (unsigned int)MaxLTSize)
                lf = MaxLTSize - 1;
            mlf = (colors - 2) / sqrt(MaxLTSize - lf);
        }
    }

    if (Log_Calc)
        return; /* LogTable not defined, bail out now */

    if (save_release > 1920 && !Log_Calc)
    {
        Log_Calc = 1;   /* turn it on */
        for (prev = 0; prev <= (unsigned int)MaxLTSize; prev++)
            LogTable[prev] = (BYTE)logtablecalc((int)prev);
        Log_Calc = 0;   /* turn it off, again */
        return;
    }

    if (LogFlag > -2)
    {
        lf = (LogFlag > 1) ? LogFlag : 0;
        if (lf >= (unsigned int)MaxLTSize)
            lf = MaxLTSize - 1;
        Fg2Float((int)(MaxLTSize-lf), 0, m);
        fLog14(m, m);
        Fg2Float((int)(colors-(lf?2:1)), 0, c);
        fDiv(m, c, m);
        for (prev = 1; prev <= lf; prev++)
            LogTable[prev] = 1;
        for (n = (lf?2:1); n < (unsigned int)colors; n++)
        {
            Fg2Float((int)n, 0, f);
            fMul16(f, m, f);
            fExp14(f, l);
            limit = (unsigned int)Float2Fg(l, 0) + lf;
            if (limit > (unsigned int)MaxLTSize || n == (unsigned int)(colors-1))
                limit = MaxLTSize;
            while (prev <= limit)
                LogTable[prev++] = (BYTE)n;
        }
    }
    else
    {
        if ((lf = 0 - LogFlag) >= (unsigned int)MaxLTSize)
            lf = MaxLTSize - 1;
        Fg2Float((int)(MaxLTSize-lf), 0, m);
        fSqrt14(m, m);
        Fg2Float((int)(colors-2), 0, c);
        fDiv(m, c, m);
        for (prev = 1; prev <= lf; prev++)
            LogTable[prev] = 1;
        for (n = 2; n < (unsigned int)colors; n++)
        {
            Fg2Float((int)n, 0, f);
            fMul16(f, m, f);
            fMul16(f, f, l);
            limit = (unsigned int)(Float2Fg(l, 0) + lf);
            if (limit > (unsigned int)MaxLTSize || n == (unsigned int)(colors-1))
                limit = MaxLTSize;
            while (prev <= limit)
                LogTable[prev++] = (BYTE)n;
        }
    }
    LogTable[0] = 0;
    if (LogFlag != -1)
        for (sptop = 1; sptop < (unsigned int)MaxLTSize; sptop++) /* spread top to incl unused colors */
            if (LogTable[sptop] > LogTable[sptop-1])
                LogTable[sptop] = (BYTE)(LogTable[sptop-1]+1);
}

long logtablecalc(long citer)
{
    long ret = 0;

    if (LogFlag == 0 && !rangeslen) /* Oops, how did we get here? */
        return(citer);
    if (LogTable && !Log_Calc)
        return(LogTable[(long)min(citer, MaxLTSize)]);

    if (LogFlag > 0)   /* new log function */
    {
        if ((unsigned long)citer <= lf + 1)
            ret = 1;
        else if ((citer - lf) / log(citer - lf) <= mlf)
        {
            if (save_release < 2002)
                ret = (long)(citer - lf + (lf?1:0));
            else
                ret = (long)(citer - lf);
        }
        else
            ret = (long)(mlf * log(citer - lf)) + 1;
    }
    else if (LogFlag == -1)   /* old log function */
    {
        if (citer == 0)
            ret = 1;
        else
            ret = (long)(mlf * log(citer)) + 1;
    }
    else if (LogFlag <= -2)   /* sqrt function */
    {
        if ((unsigned long)citer <= lf)
            ret = 1;
        else if ((unsigned long)(citer - lf) <= (unsigned long)(mlf * mlf))
            ret = (long)(citer - lf + 1);
        else
            ret = (long)(mlf * sqrt(citer - lf)) + 1;
    }
    return (ret);
}

int ExpFloat14(int xx)
{
    static float fLogTwo = (float)0.6931472;
    int f;
    int Ans;

    f = 23 - (int)RegFloat2Fg(RegDivFloat(xx, *(int*)&fLogTwo), 0);
    Ans = ExpFudged(RegFloat2Fg(xx, 16), f);
    return(RegFg2Float(Ans, (char)f));
}

/*
 * The following functions calculate the real and imaginary complex
 * coordinates of the point in the complex plane corresponding to
 * the screen coordinates (col,row) at the current zoom corners
 * settings. The functions come in two flavors. One looks up the pixel
 * values using the precalculated grid arrays dx0, dx1, dy0, and dy1,
 * which has a speed advantage but is limited to MAXPIXELS image
 * dimensions. The other calculates the complex coordinates at a
 * cost of two additions and two multiplications for each component,
 * but works at any resolution.
 *
 * It appears that the speed advantage
 * of the lookup vs the calculation is negligible on machines with
 * coprocessors. Bert Tyler's original implementation was designed for
 * machines with no coprocessor; on those machines the saving was
 * significant. For the time being, the table lookup capability will
 * be maintained.
 */

/* Real component, grid lookup version - requires dx0/dx1 arrays */
static double dxpixel_grid(void)
{
    return(dx0[col]+dx1[row]);
}

/* Real component, calculation version - does not require arrays */
static double dxpixel_calc(void)
{
    return((double)(xxmin + col*delxx + row*delxx2));
}

/* Imaginary component, grid lookup version - requires dy0/dy1 arrays */
static double dypixel_grid(void)
{
    return(dy0[row]+dy1[col]);
}

/* Imaginary component, calculation version - does not require arrays */
static double dypixel_calc(void)
{
    return((double)(yymax - row*delyy - col*delyy2));
}

/* Real component, grid lookup version - requires lx0/lx1 arrays */
static long lxpixel_grid(void)
{
    return(lx0[col]+lx1[row]);
}

/* Real component, calculation version - does not require arrays */
static long lxpixel_calc(void)
{
    return(xmin + col*delx + row*delx2);
}

/* Imaginary component, grid lookup version - requires ly0/ly1 arrays */
static long lypixel_grid(void)
{
    return(ly0[row]+ly1[col]);
}

/* Imaginary component, calculation version - does not require arrays */
static long lypixel_calc(void)
{
    return(ymax - row*dely - col*dely2);
}

double (*dxpixel)(void) = dxpixel_calc;
double (*dypixel)(void) = dypixel_calc;
long   (*lxpixel)(void) = lxpixel_calc;
long   (*lypixel)(void) = lypixel_calc;

void set_pixel_calc_functions(void)
{
    if (use_grid)
    {
        dxpixel = dxpixel_grid;
        dypixel = dypixel_grid;
        lxpixel = lxpixel_grid;
        lypixel = lypixel_grid;
    }
    else
    {
        dxpixel = dxpixel_calc;
        dypixel = dypixel_calc;
        lxpixel = lxpixel_calc;
        lypixel = lypixel_calc;
    }
}

/*
;
;       32-bit integer multiply routine with an 'n'-bit shift.
;       Overflow condition returns 0x7fffh with overflow = 1;
;
;       long x, y, z, multiply();
;       int n;
;
;       z = multiply(x,y,n)
;
*/

/*
 * 32 bit integer multiply with n bit shift.
 * Note that we fake integer multiplication with floating point
 * multiplication.  Note that this causes a loss of precision and
 * the integer math images won't necessarily match those that use
 * the assembly routines.
 */
#ifndef NASM
long multiply(long x, long y, int n)
{
    register long l;
    l = ((float)x)* ((float)y)/(float)(1<<n);
    if (l==0x7fffffff)
    {
        overflow = 1;
    }
    return l;
}

/*
;
;       32-bit integer divide routine with an 'n'-bit shift.
;       Overflow condition returns 0x7fffh with overflow = 1;
;
;       z = divide(x,y,n);       z = x / y;
*/
long divide(long x, long y, int n)
{
    return (long) ( ((float)x)/ ((float)y)*(float)(1<<n));
}
#endif  /* NASM  */

/*
 * Generate a gaussian distributed number.
 * The right half of the distribution is folded onto the lower half.
 * That is, the curve slopes up to the peak and then drops to 0.
 * The larger slope is, the smaller the standard deviation.
 * The values vary from 0+offset to range+offset, with the peak
 * at range+offset.
 * To make this more complicated, you only have a
 * 1 in Distribution*(1-Probability/Range*con)+1 chance of getting a
 * Gaussian; otherwise you just get offset.
 */
int GausianNumber(int Probability, int Range)
{
    int n, r;
    long Accum = 0, p;

    p = divide((long)Probability << 16, (long)Range << 16, 16);
    p = multiply(p, con, 16);
    p = multiply((long)Distribution << 16, p, 16);
    if (!(rand15() % (Distribution - (int)(p >> 16) + 1)))
    {
        for (n = 0; n < Slope; n++)
            Accum += rand15();
        Accum /= Slope;
        r = (int)(multiply((long)Range << 15, Accum, 15) >> 14);
        r = r - Range;
        if (r < 0)
            r = -r;
        return(Range - r + Offset);
    }
    return(Offset);
}
