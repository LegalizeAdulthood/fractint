#ifndef CMPLX_H
#define CMPLX_H

/* various complex number defs */

#ifndef _CMPLX_DEFINED
#define _CMPLX_DEFINED

struct DHyperComplex {
    double x,y;
    double z,t;
};

struct LHyperComplex {
    long x,y;
    long z,t;
};

struct DComplex {
    double x,y;
};

struct LDComplex {
    LDBL x,y;
};

struct LComplex {
    long x,y;
};

typedef struct  DComplex         _CMPLX;
typedef struct  LDComplex        _LDCMPLX;
typedef struct  LComplex         _LCMPLX;
typedef struct  DHyperComplex    _HCMPLX;
typedef struct  LHyperComplex    _LHCMPLX;

#endif

#ifndef sqr
#define sqr(x) ((x)*(x))
#endif

#ifndef lsqr
#define lsqr(x) (multiply((x),(x),bitshift))
#endif

#define modulus(z)       (sqr((z).x)+sqr((z).y))
#define conjugate(pz)   ((pz)->y = 0.0 - (pz)->y)
#define distance(z1,z2)  (sqr((z1).x-(z2).x)+sqr((z1).y-(z2).y))

/*** Formula Declarations ***/
enum MATH_TYPE { D_MATH, L_MATH };
extern enum MATH_TYPE MathType;

#define fDiv(x, y, z) (void)((*(int*)&z) = RegDivFloat(*(int*)&x, *(int*)&y))
#define fMul16(x, y, z) (void)((*(int*)&z) = r16Mul(*(int*)&x, *(int*)&y))
#define fShift(x, Shift, z) (void)((*(int*)&z) = \
   RegSftFloat(*(int*)&x, Shift))
#define Fg2Float(x, f, z) (void)((*(int*)&z) = RegFg2Float(x, f))
#define Float2Fg(x, f) RegFloat2Fg(*(int*)&x, f)
#define fLog14(x, z) (void)((*(int*)&z) = \
        RegFg2Float(LogFloat14(*(int*)&x), 16))
#define fExp14(x, z) (void)((*(int*)&z) = ExpFloat14(*(int*)&x));
#define fSqrt14(x, z) fLog14(x, z); fShift(z, -1, z); fExp14(z, z)

/* the following are declared 4 dimensional as an experiment */
/* changing declarations to _CMPLX and _LCMPLX restores the code */
/* to 2D */
union Arg {
   _CMPLX     d;
   _LCMPLX    l;
/*
   _DHCMPLX   dh;
   _LHCMPLX   lh; */
};

struct ConstArg {
   char *s;
   int len;
   union Arg a;
};

extern union Arg *Arg1,*Arg2;

extern void lStkSin(void),lStkCos(void),lStkSinh(void),lStkCosh(void),lStkLog(void),lStkExp(void),lStkSqr(void);
extern void dStkSin(void),dStkCos(void),dStkSinh(void),dStkCosh(void),dStkLog(void),dStkExp(void),dStkSqr(void);

extern void (*ltrig0)(void);
extern void (*ltrig1)(void);
extern void (*ltrig2)(void);
extern void (*ltrig3)(void);
extern void (*dtrig0)(void);
extern void (*dtrig1)(void);
extern void (*dtrig2)(void);
extern void (*dtrig3)(void);

struct fls { /* function, load, store pointers  CAE fp */
   void (*function)(void);
   union Arg *operand;
};

/* -------------------------------------------------------------------- */
/*   The following #defines allow the complex transcendental functions  */
/*   in parser.c to be used here thus avoiding duplicated code.         */
/* -------------------------------------------------------------------- */

#define CMPLXmod(z)       (sqr((z).x)+sqr((z).y))
#define CMPLXconj(z)    ((z).y =  -((z).y))
#define LCMPLXmod(z)       (lsqr((z).x)+lsqr((z).y))
#define LCMPLXconj(z)   ((z).y =  -((z).y))

#define LCMPLXtrig0(arg,out) Arg1->l = (arg); ltrig0(); (out)=Arg1->l
#define LCMPLXtrig1(arg,out) Arg1->l = (arg); ltrig1(); (out)=Arg1->l
#define LCMPLXtrig2(arg,out) Arg1->l = (arg); ltrig2(); (out)=Arg1->l
#define LCMPLXtrig3(arg,out) Arg1->l = (arg); ltrig3(); (out)=Arg1->l

#define  CMPLXtrig0(arg,out) Arg1->d = (arg); dtrig0(); (out)=Arg1->d
#define  CMPLXtrig1(arg,out) Arg1->d = (arg); dtrig1(); (out)=Arg1->d
#define  CMPLXtrig2(arg,out) Arg1->d = (arg); dtrig2(); (out)=Arg1->d
#define  CMPLXtrig3(arg,out) Arg1->d = (arg); dtrig3(); (out)=Arg1->d

#define LCMPLXsin(arg,out)   Arg1->l = (arg); lStkSin();  (out) = Arg1->l
#define LCMPLXcos(arg,out)   Arg1->l = (arg); lStkCos();  (out) = Arg1->l
#define LCMPLXsinh(arg,out)  Arg1->l = (arg); lStkSinh(); (out) = Arg1->l
#define LCMPLXcosh(arg,out)  Arg1->l = (arg); lStkCosh(); (out) = Arg1->l
#define LCMPLXlog(arg,out)   Arg1->l = (arg); lStkLog();  (out) = Arg1->l
#define LCMPLXexp(arg,out)   Arg1->l = (arg); lStkExp();  (out) = Arg1->l
/*
#define LCMPLXsqr(arg,out)   Arg1->l = (arg); lStkSqr();  (out) = Arg1->l
*/
#define LCMPLXsqr(arg,out)   \
   (out).x = lsqr((arg).x) - lsqr((arg).y);\
   (out).y = multiply((arg).x, (arg).y, bitshiftless1)
#define LCMPLXsqr_old(out)       \
   (out).y = multiply(lold.x, lold.y, bitshiftless1);\
   (out).x = ltempsqrx - ltempsqry

#define LCMPLXpwr(arg1,arg2,out)    Arg2->l = (arg1); Arg1->l = (arg2);\
         lStkPwr(); Arg1++; Arg2++; (out) = Arg2->l
#define LCMPLXmult(arg1,arg2,out)    Arg2->l = (arg1); Arg1->l = (arg2);\
         lStkMul(); Arg1++; Arg2++; (out) = Arg2->l
#define LCMPLXadd(arg1,arg2,out)    \
    (out).x = (arg1).x + (arg2).x; (out).y = (arg1).y + (arg2).y
#define LCMPLXsub(arg1,arg2,out)    \
    (out).x = (arg1).x - (arg2).x; (out).y = (arg1).y - (arg2).y

#define LCMPLXtimesreal(arg,real,out)   \
    (out).x = multiply((arg).x,(real),bitshift);\
    (out).y = multiply((arg).y,(real),bitshift)

#define LCMPLXrecip(arg,out)    \
{ long denom; denom = lsqr((arg).x) + lsqr((arg).y);\
if(denom==0L) overflow=1; else {(out).x = divide((arg).x,denom,bitshift);\
(out).y = -divide((arg).y,denom,bitshift);}}

#define CMPLXsin(arg,out)    Arg1->d = (arg); dStkSin();  (out) = Arg1->d
#define CMPLXcos(arg,out)    Arg1->d = (arg); dStkCos();  (out) = Arg1->d
#define CMPLXsinh(arg,out)   Arg1->d = (arg); dStkSinh(); (out) = Arg1->d
#define CMPLXcosh(arg,out)   Arg1->d = (arg); dStkCosh(); (out) = Arg1->d
#define CMPLXlog(arg,out)    Arg1->d = (arg); dStkLog();  (out) = Arg1->d
#define CMPLXexp(arg,out)    FPUcplxexp(&(arg), &(out))
/*
#define CMPLXsqr(arg,out)    Arg1->d = (arg); dStkSqr();  (out) = Arg1->d
*/
#define CMPLXsqr(arg,out)    \
   (out).x = sqr((arg).x) - sqr((arg).y);\
   (out).y = ((arg).x+(arg).x) * (arg).y
#define CMPLXsqr_old(out)       \
   (out).y = (old.x+old.x) * old.y;\
   (out).x = tempsqrx - tempsqry

#define CMPLXpwr(arg1,arg2,out)   (out)= ComplexPower((arg1), (arg2))
#define CMPLXmult1(arg1,arg2,out)    Arg2->d = (arg1); Arg1->d = (arg2);\
         dStkMul(); Arg1++; Arg2++; (out) = Arg2->d
#define CMPLXmult(arg1,arg2,out)  \
        {\
           _CMPLX TmP;\
           TmP.x = (arg1).x*(arg2).x - (arg1).y*(arg2).y;\
           TmP.y = (arg1).x*(arg2).y + (arg1).y*(arg2).x;\
           (out) = TmP;\
         }
#define CMPLXadd(arg1,arg2,out)    \
    (out).x = (arg1).x + (arg2).x; (out).y = (arg1).y + (arg2).y
#define CMPLXsub(arg1,arg2,out)    \
    (out).x = (arg1).x - (arg2).x; (out).y = (arg1).y - (arg2).y
#define CMPLXtimesreal(arg,real,out)   \
    (out).x = (arg).x*(real);\
    (out).y = (arg).y*(real)

#define CMPLXrecip(arg,out)    \
   { LDBL denom; denom = sqr((arg).x) + sqr((arg).y);\
     if(denom==0.0) {(out).x = 1.0e10;(out).y = 1.0e10;}else\
    { (out).x =  (arg).x/denom;\
     (out).y = -(arg).y/denom;}}

#define CMPLXneg(arg,out)  (out).x = -(arg).x; (out).y = -(arg).y

#endif
