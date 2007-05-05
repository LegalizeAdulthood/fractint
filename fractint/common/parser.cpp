/* Parser.c (C) 1990, Mark C. Peterson, CompuServe [70441, 3353]
	All rights reserved.

	Code may be used in any program provided the author is credited
	either during program execution or in the documentation.  Source
	code may be distributed only in combination with public domain or
	shareware source code.  Source code may be modified provided the
	copyright notice and this message is left unchanged and all
	modifications are clearly documented.

	I would appreciate a copy of any work which incorporates this code,
	however this is optional.

	Mark C. Peterson
	405-C Queen St. Suite #181
	Southington, CT 06489
	(203) 276-9721
*/

/*      Chuck Ebbert (CompuServe [76306, 1226]) changed code marked 'CAE fp'    */
/*   for fast 387 floating-point math.  See PARSERA.ASM and PARSERFP.C */
/*   (13 Dec 1992.)  */
/* */
/*   Modified 12 July 1993 by CAE to fix crash when formula not found.  */

#include <string.h>
#include <ctype.h>
#include <time.h>

/* see Fractint.c for a description of the "include"  hierarchy */
#include "port.h"
#include "prototyp.h"
#include "drivers.h"

#ifdef WATCH_MP
double x1, y1, x2, y2;
#endif

#define MAX_OPS 250
#define MAX_ARGS 100
#define MAX_BOXX 8192  /* max size of g_box_x array */

unsigned g_formula_max_ops  = MAX_OPS;
unsigned g_formula_max_args = MAX_ARGS;

enum MATH_TYPE MathType = D_MATH;
/* moved _LCMPLX and union ARg to mpmath.h -6-20-90 TIW */

static unsigned long number_of_ops;
static unsigned long number_of_loads;
static unsigned long number_of_stores;
static unsigned long number_of_jumps;

struct PEND_OP
{
	void (*f)();
	int p;
};

#if !defined(XFRACT) && !defined(_WIN32)
/* reuse an array in the decoder */
JUMP_CONTROL_ST *jump_control = (JUMP_CONTROL_ST *) g_size_of_string;
#else
JUMP_CONTROL_ST jump_control[MAX_JUMPS];
#endif

int jump_index, InitJumpIndex;

static int frm_prescan(FILE *open_file);

#define CASE_TERMINATOR case',':\
						case '\n':\
						case '(':\
						case ')':\
						case '!':\
						case '=':\
						case '<':\
						case '>':\
						case '|':\
						case '&':\
						case '}':\
						case ':':\
						case '+':\
						case '-':\
						case '*':\
						case '/':\
						case '^'

#define CASE_ALPHA      case 'a':\
						case 'b':\
						case 'c':\
						case 'd':\
						case 'e':\
						case 'f':\
						case 'g':\
						case 'h':\
						case 'i':\
						case 'j':\
						case 'k':\
						case 'l':\
						case 'm':\
						case 'n':\
						case 'o':\
						case 'p':\
						case 'q':\
						case 'r':\
						case 's':\
						case 't':\
						case 'u':\
						case 'v':\
						case 'w':\
						case 'x':\
						case 'y':\
						case 'z'

#define CASE_NUM        case '0':\
						case '1':\
						case '2':\
						case '3':\
						case '4':\
						case '5':\
						case '6':\
						case '7':\
						case '8':\
						case '9'

	/* token_type definitions */
#define NOT_A_TOKEN           0
#define PARENS                1
#define PARAM_VARIABLE        2
#define USER_NAMED_VARIABLE   3
#define PREDEFINED_VARIABLE   4
#define REAL_CONSTANT         5
#define COMPLEX_CONSTANT      6
#define FUNCTION              7
#define PARAM_FUNCTION        8
#define FLOW_CONTROL          9
#define OPERATOR             10
#define END_OF_FORMULA       11

	/* token IDs */
#define END_OF_FILE            1
#define ILLEGAL_CHARACTER      2
#define ILLEGAL_VARIABLE_NAME  3
#define TOKEN_TOO_LONG         4
#define FUNC_USED_AS_VAR       5
#define JUMP_MISSING_BOOLEAN   6
#define JUMP_WITH_ILLEGAL_CHAR 7
#define UNDEFINED_FUNCTION     8
#define ILLEGAL_OPERATOR       9
#define ILL_FORMED_CONSTANT   10
#define OPEN_PARENS            1
#define CLOSE_PARENS          -1

struct token_st
{
	char token_str[80];
	int token_type;
	int token_id;
	_CMPLX token_const;
};


/* CAE fp added MAX_STORES and LOADS */
/* MAX_STORES must be even to make Unix alignment work */
/* TW made dependent on g_formula_max_ops */

#define MAX_STORES ((g_formula_max_ops/4)*2)  /* at most only half the ops can be stores */
#define MAX_LOADS ((unsigned)(g_formula_max_ops*.8))  /* and 80% can be loads */
/* PB 901103 made some of the following static for safety */

static struct PEND_OP o[2300];

#if 0
static void ops_allocate();
static void vars_allocate();
#endif

struct var_list_st
{
	char name[34];
	struct var_list_st *next_item;
} *var_list;

struct const_list_st
{
	_CMPLX complex_const;
	struct const_list_st *next_item;
} *complx_list, *real_list;

static void parser_allocate();

union Arg *Arg1, *Arg2;
/* PB 910417 removed unused "a" array */

/* CAE fp  made some of the following non-static for PARSERA.ASM */
/* Some of these variables should be renamed for safety */
union Arg s[20], **Store, **Load;     /* static CAE fp */
int g_store_ptr, g_lod_ptr, OpPtr;      /* static CAE fp */
int var_count;
int complx_count;
int real_count;


void (**f)() = (void (**)())0; /* static CAE fp */

int g_is_mand = 1;

unsigned int g_posp, g_parser_vsp, g_last_op;     /* CAE fp made non-static */
static unsigned int n, NextOp, InitN;
static int paren, ExpectingArg;
struct ConstArg *v = (struct ConstArg *)0;      /* was static CAE fp */
int InitLodPtr, InitStoPtr, InitOpPtr, g_last_init_op;      /* was static CAE fp */
static int Delta16;
double g_fudge_limit;           /* TIW 05-04-91 */
static double fg;
static int ShiftBack;     /* TIW 06-18-90 */
static int SetRandom;     /* MCP 11-21-91 */
static int Randomized;
static unsigned long RandNum;
short g_uses_p1, g_uses_p2, g_uses_p3, g_uses_p4, g_uses_p5;
int uses_jump;
short g_uses_is_mand;
unsigned int chars_in_formula;

#if !defined(XFRACT)
#define ChkLongDenom(denom)\
	do \
	{ \
		if ((denom == 0 || g_overflow) && g_save_release > 1920) \
		{\
			g_overflow = 1; \
			return; \
		}\
		else if (denom == 0) \
		{ \
			return; \
		} \
	} \
	while (0)

#endif

#define ChkFloatDenom(denom)\
	do \
	{ \
		if (fabs(denom) <= DBL_MIN)		\
		{								\
			if (g_save_release > 1920)	\
			{							\
				g_overflow = 1;			\
			}							\
			return;						\
		} \
	} \
	while (0)

#define LastSqr v[4].a

/* ParseErrs() defines; all calls to ParseErrs(), or any variable which will
	be used as the argument in a call to ParseErrs(), should use one of these
	defines.
*/

#define PE_NO_ERRORS_FOUND                           -1
#define PE_SHOULD_BE_ARGUMENT                         0
#define PE_SHOULD_BE_OPERATOR                         1
#define PE_NEED_A_MATCHING_OPEN_PARENS                2
#define PE_NEED_MORE_CLOSE_PARENS                     3
#define PE_UNDEFINED_OPERATOR                         4
#define PE_UNDEFINED_FUNCTION                         5
#define PE_TABLE_OVERFLOW                             6
#define PE_NO_MATCH_RIGHT_PAREN                       7
#define PE_NO_LEFT_BRACKET_FIRST_LINE                 8
#define PE_UNEXPECTED_EOF                             9
#define PE_INVALID_SYM_USING_NOSYM                   10
#define PE_FORMULA_TOO_LARGE                         11
#define PE_INSUFFICIENT_MEM_FOR_TYPE_FORMULA         12
#define PE_COULD_NOT_OPEN_FILE_WHERE_FORMULA_LOCATED 13
#define PE_JUMP_NOT_FIRST                            14
#define PE_NO_CHAR_AFTER_THIS_JUMP                   15
#define PE_JUMP_NEEDS_BOOLEAN                        16
#define PE_ENDIF_REQUIRED_AFTER_ELSE                 17
#define PE_ENDIF_WITH_NO_IF                          18
#define PE_MISPLACED_ELSE_OR_ELSEIF                  19
#define PE_UNMATCHED_IF_IN_INIT_SECTION              20
#define PE_IF_WITH_NO_ENDIF                          21
#define PE_ERROR_IN_PARSING_JUMP_STATEMENTS          22
#define PE_TOO_MANY_JUMPS                            23
#define PE_FORMULA_NAME_TOO_LARGE                    24
#define PE_ILLEGAL_ASSIGNMENT                        25
#define PE_ILLEGAL_VAR_NAME                          26
#define PE_INVALID_CONST                             27
#define PE_ILLEGAL_CHAR                              28
#define PE_NESTING_TO_DEEP                           29
#define PE_UNMATCHED_MODULUS                         30
#define PE_FUNC_USED_AS_VAR                          31
#define PE_NO_NEG_AFTER_EXPONENT                     32
#define PE_TOKEN_TOO_LONG                            33
#define PE_SECOND_COLON                              34
#define PE_INVALID_CALL_TO_PARSEERRS                 35

static char *ParseErrs(int which)
{
	int lasterr;
	static char e0[] = {"Should be an Argument"};
	static char e1[] = {"Should be an Operator"};
	static char e2[] = {"')' needs a matching '('"};
	static char e3[] = {"Need more ')'"};
	static char e4[] = {"Undefined Operator"};
	static char e5[] = {"Undefined Function"};
	static char e6[] = {"Table overflow"};
	static char e7[] = {"Didn't find matching ')' in symmetry declaration"};
	static char e8[] = {"No '{' found on first line"};
	static char e9[] = {"Unexpected EOF!"};
	static char e10[] = {"Symmetry below is invalid, will use NOSYM"};
	static char e11[] = {"Formula is too large"};
	static char e12[] = {"Insufficient memory to run fractal type 'formula'"};
	static char e13[] = {"Could not open file where formula located"};
	static char e14[] = {"No characters may precede jump instruction"};
	static char e15[] = {"No characters may follow this jump instruction"};
	static char e16[] = {"Jump instruction missing required (boolean argument)"};
	static char e17[] = {"Next jump after \"else\" must be \"endif\""};
	static char e18[] = {"\"endif\" has no matching \"if\""};
	static char e19[] = {"Misplaced \"else\" or \"elseif()\""};
	static char e20[] = {"\"if ()\" in initialization has no matching \"endif\""};
	static char e21[] = {"\"if ()\" has no matching \"endif\""};
	static char e22[] = {"Error in parsing jump statements"};
	static char e23[] = {"Formula has too many jump commands"};
	static char e24[] = {"Formula name has too many characters"};
	static char e25[] = {"Only variables are allowed to left of assignment"};
	static char e26[] = {"Illegal variable name"};
	static char e27[] = {"Invalid constant expression"};
	static char e28[] = {"This character not supported by parser"};
	static char e29[] = {"Nesting of parentheses exceeds maximum depth"};
	static char e30[] = {"Unmatched modulus operator \"|\" in this expression"}; /*last one */
	static char e31[] = {"Can't use function name as variable"};
	static char e32[] = {"Negative exponent must be enclosed in parens"};
	static char e33[] = {"Variable or constant exceeds 32 character limit"};
	static char e34[] = {"Only one \":\" permitted in a formula"};
	static char e35[] = {"Invalid ParseErrs code"};
	static char *ErrStrings[] =
	{
		e0,  e1,  e2,  e3,  e4,  e5,
		e6,  e7,  e8,  e9,  e10,
		e11, e12, e13, e14, e15,
		e16, e17, e18, e19, e20,
		e21, e22, e23, e24, e25,
		e26, e27, e28, e29, e30,
		e31, e32, e33, e34, e35
	};
	lasterr = sizeof(ErrStrings)/sizeof(ErrStrings[0]) - 1;
	if (which > lasterr)
	{
		which = lasterr;
	}
	return (char *)ErrStrings[which];
}

/* use the following when only float functions are implemented to
	get MP math and Integer math */

#if !defined(XFRACT)
#define FUNCT
#ifdef FUNCT /* use function form save space - isn't really slower */

static void mStkFunct(void (*fct)())   /* call lStk via dStk */
{
	Arg1->d = MPC2cmplx(Arg1->m);
	(*fct)();
	Arg1->m = cmplx2MPC(Arg1->d);
}

static void lStkFunct(void (*fct)())   /* call lStk via dStk */
{
	double y;
	/*
		intermediate variable needed for safety because of
		different size of double and long in Arg union
	*/
	y = (double)Arg1->l.y / fg;
	Arg1->d.x = (double)Arg1->l.x / fg;
	Arg1->d.y = y;
	(*fct)();
	if (fabs(Arg1->d.x) < g_fudge_limit && fabs(Arg1->d.y) < g_fudge_limit)
	{
		Arg1->l.x = (long)(Arg1->d.x*fg);
		Arg1->l.y = (long)(Arg1->d.y*fg);
	}
	else
	{
		g_overflow = 1;
	}
}
#else  /* use Macro form for (?) greater speed */
  /* call lStk via dStk */
#define mStkFunct(fct)  \
	Arg1->d = MPC2cmplx(Arg1->m); \
	(*fct)(); \
	Arg1->m = cmplx2MPC(Arg1->d);


/* call lStk via dStk */
#define lStkFunct(fct) {\
	double y; \
	y = (double)Arg1->l.y / fg; \
	Arg1->d.x = (double)Arg1->l.x / fg; \
	Arg1->d.y = y; \
	(*fct)(); \
	if (fabs(Arg1->d.x) < g_fudge_limit && fabs(Arg1->d.y) < g_fudge_limit) {\
		Arg1->l.x = (long)(Arg1->d.x*fg); \
		Arg1->l.y = (long)(Arg1->d.y*fg); \
	}\
	else\
		g_overflow = 1; \
}


#endif

#endif

/* Random number code, MCP 11-21-91 */

unsigned long new_random_number()
{
	return RandNum = ((RandNum << 15) + rand15()) ^ RandNum;
}

void lRandom()
{
	v[7].a.l.x = new_random_number() >> (32 - g_bit_shift);
	v[7].a.l.y = new_random_number() >> (32 - g_bit_shift);
}

void dRandom()
{
	long x, y;

	/* Use the same algorithm as for fixed math so that they will generate
          the same fractals when the srand() function is used. */
	x = new_random_number() >> (32 - g_bit_shift);
	y = new_random_number() >> (32 - g_bit_shift);
	v[7].a.d.x = ((double)x / (1L << g_bit_shift));
	v[7].a.d.y = ((double)y / (1L << g_bit_shift));

}

#if !defined(XFRACT)
void mRandom()
{
	long x, y;

	/* Use the same algorithm as for fixed math so that they will generate
		the same fractals when the srand() function is used. */
	x = new_random_number() >> (32 - g_bit_shift);
	y = new_random_number() >> (32 - g_bit_shift);
	v[7].a.m.x = *fg2MP(x, g_bit_shift);
	v[7].a.m.y = *fg2MP(y, g_bit_shift);
}
#endif

void SetRandFnct()
{
	unsigned Seed;

	if (!SetRandom)
	{
		RandNum = Arg1->l.x ^ Arg1->l.y;
	}

	Seed = (unsigned)RandNum ^ (unsigned)(RandNum >> 16);
	srand(Seed);
	SetRandom = 1;

	/* Clear out the seed */
	new_random_number();
	new_random_number();
	new_random_number();
}

void RandomSeed()
{
	time_t ltime;

	/* Use the current time to randomize the random number sequence. */
	time(&ltime);
	srand((unsigned int)ltime);

	new_random_number();
	new_random_number();
	new_random_number();
	Randomized = 1;
}

#if !defined(XFRACT)
void lStkSRand()
{
	SetRandFnct();
	lRandom();
	Arg1->l = v[7].a.l;
}
#endif

#if !defined(XFRACT)
void mStkSRand()
{
	Arg1->l.x = Arg1->m.x.Mant ^ (long)Arg1->m.x.Exp;
	Arg1->l.y = Arg1->m.y.Mant ^ (long)Arg1->m.y.Exp;
	SetRandFnct();
	mRandom();
	Arg1->m = v[7].a.m;
}
#endif

void dStkSRand()
{
	Arg1->l.x = (long)(Arg1->d.x*(1L << g_bit_shift));
	Arg1->l.y = (long)(Arg1->d.y*(1L << g_bit_shift));
	SetRandFnct();
	dRandom();
	Arg1->d = v[7].a.d;
}

void (*StkSRand)() = dStkSRand;


void dStkLodDup()
{
	Arg1 += 2;
	Arg2 += 2;
	*Arg2 = *Arg1 = *Load[g_lod_ptr];
	g_lod_ptr += 2;
}

void dStkLodSqr()
{
	Arg1++;
	Arg2++;
	Arg1->d.y = Load[g_lod_ptr]->d.x*Load[g_lod_ptr]->d.y*2.0;
	Arg1->d.x = (Load[g_lod_ptr]->d.x*Load[g_lod_ptr]->d.x) - (Load[g_lod_ptr]->d.y*Load[g_lod_ptr]->d.y);
	g_lod_ptr++;
}

void dStkLodSqr2()
{
	Arg1++;
	Arg2++;
	LastSqr.d.x = Load[g_lod_ptr]->d.x*Load[g_lod_ptr]->d.x;
	LastSqr.d.y = Load[g_lod_ptr]->d.y*Load[g_lod_ptr]->d.y;
	Arg1->d.y = Load[g_lod_ptr]->d.x*Load[g_lod_ptr]->d.y*2.0;
	Arg1->d.x = LastSqr.d.x - LastSqr.d.y;
	LastSqr.d.x += LastSqr.d.y;
	LastSqr.d.y = 0;
	g_lod_ptr++;
}

void dStkStoDup()
{}
void dStkStoSqr()
{}
void dStkStoSqr0()
{}

void dStkLodDbl()
{
	Arg1++;
	Arg2++;
	Arg1->d.x = Load[g_lod_ptr]->d.x*2.0;
	Arg1->d.y = Load[g_lod_ptr]->d.y*2.0;
	g_lod_ptr++;
}

void dStkStoDbl()
{}
void dStkReal2()
{}

void dStkSqr0()
{
	LastSqr.d.y = Arg1->d.y*Arg1->d.y; /* use LastSqr as temp storage */
	Arg1->d.y = Arg1->d.x*Arg1->d.y*2.0;
	Arg1->d.x = Arg1->d.x*Arg1->d.x - LastSqr.d.y;
}


void dStkSqr3()
{
	Arg1->d.x = Arg1->d.x*Arg1->d.x;
}



void dStkAbs()
{
	Arg1->d.x = fabs(Arg1->d.x);
	Arg1->d.y = fabs(Arg1->d.y);
}

#if !defined(XFRACT)
void mStkAbs()
{
	if (Arg1->m.x.Exp < 0)
	{
		Arg1->m.x.Exp = -Arg1->m.x.Exp;
	}
	if (Arg1->m.y.Exp < 0)
	{
		Arg1->m.y.Exp = -Arg1->m.y.Exp;
	}
}

void lStkAbs()
{
	Arg1->l.x = labs(Arg1->l.x);
	Arg1->l.y = labs(Arg1->l.y);
}
#endif

void (*StkAbs)() = dStkAbs;

void dStkSqr()
{
	LastSqr.d.x = Arg1->d.x*Arg1->d.x;
	LastSqr.d.y = Arg1->d.y*Arg1->d.y;
	Arg1->d.y = Arg1->d.x*Arg1->d.y*2.0;
	Arg1->d.x = LastSqr.d.x - LastSqr.d.y;
	LastSqr.d.x += LastSqr.d.y;
	LastSqr.d.y = 0;
}

#if !defined(XFRACT)
void mStkSqr()
{
	LastSqr.m.x = *MPmul(Arg1->m.x, Arg1->m.x);
	LastSqr.m.y = *MPmul(Arg1->m.y, Arg1->m.y);
	Arg1->m.y = *MPmul(Arg1->m.x, Arg1->m.y);
	Arg1->m.y.Exp++;
	Arg1->m.x = *MPsub(LastSqr.m.x, LastSqr.m.y);
	LastSqr.m.x = *MPadd(LastSqr.m.x, LastSqr.m.y);
	LastSqr.m.y.Exp = 0;
	LastSqr.m.y.Mant = 0L;
}

void lStkSqr()
{
	LastSqr.l.x = multiply(Arg1->l.x, Arg1->l.x, g_bit_shift);
	LastSqr.l.y = multiply(Arg1->l.y, Arg1->l.y, g_bit_shift);
	Arg1->l.y = multiply(Arg1->l.x, Arg1->l.y, g_bit_shift) << 1;
	Arg1->l.x = LastSqr.l.x - LastSqr.l.y;
	LastSqr.l.x += LastSqr.l.y;
	LastSqr.l.y = 0L;
}
#endif

void (*StkSqr)() = dStkSqr;

void dStkAdd()
{
	Arg2->d.x += Arg1->d.x;
	Arg2->d.y += Arg1->d.y;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)

void mStkAdd()
{
	Arg2->m = MPCadd(Arg2->m, Arg1->m);
	Arg1--;
	Arg2--;
}

void lStkAdd()
{
	Arg2->l.x += Arg1->l.x;
	Arg2->l.y += Arg1->l.y;
	Arg1--;
	Arg2--;
}
#endif

void (*StkAdd)() = dStkAdd;

void dStkSub()
{
	Arg2->d.x -= Arg1->d.x;
	Arg2->d.y -= Arg1->d.y;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkSub()
{
	Arg2->m = MPCsub(Arg2->m, Arg1->m);
	Arg1--;
	Arg2--;
}

void lStkSub()
{
	Arg2->l.x -= Arg1->l.x;
	Arg2->l.y -= Arg1->l.y;
	Arg1--;
	Arg2--;
}
#endif

void (*StkSub)() = dStkSub;

void dStkConj()
{
	Arg1->d.y = -Arg1->d.y;
}

#if !defined(XFRACT)
void mStkConj()
{
	Arg1->m.y.Exp ^= 0x8000;
}

void lStkConj()
{
	Arg1->l.y = -Arg1->l.y;
}
#endif

void (*StkConj)() = dStkConj;

void dStkFloor()
{
	Arg1->d.x = floor(Arg1->d.x);
	Arg1->d.y = floor(Arg1->d.y);
}

#if !defined(XFRACT)
void mStkFloor()
{
	mStkFunct(dStkFloor);   /* call lStk via dStk */
}

void lStkFloor()
{
	/*
	* Kill fractional part. This operation truncates negative numbers
	* toward negative infinity as desired.
	*/
	Arg1->l.x = (Arg1->l.x) >> g_bit_shift;
	Arg1->l.y = (Arg1->l.y) >> g_bit_shift;
	Arg1->l.x = (Arg1->l.x) << g_bit_shift;
	Arg1->l.y = (Arg1->l.y) << g_bit_shift;
}
#endif

void (*StkFloor)() = dStkFloor;

void dStkCeil()
{
	Arg1->d.x = ceil(Arg1->d.x);
	Arg1->d.y = ceil(Arg1->d.y);
}

#if !defined(XFRACT)
void mStkCeil()
{
	mStkFunct(dStkCeil);   /* call lStk via dStk */
}

void lStkCeil()
{
	/* the shift operation does the "floor" operation, so we
		negate everything before the operation */
	Arg1->l.x = (-Arg1->l.x) >> g_bit_shift;
	Arg1->l.y = (-Arg1->l.y) >> g_bit_shift;
	Arg1->l.x = -((Arg1->l.x) << g_bit_shift);
	Arg1->l.y = -((Arg1->l.y) << g_bit_shift);
}
#endif

void (*StkCeil)() = dStkCeil;

void dStkTrunc()
{
	Arg1->d.x = (int)(Arg1->d.x);
	Arg1->d.y = (int)(Arg1->d.y);
}

#if !defined(XFRACT)
void mStkTrunc()
{
	mStkFunct(dStkTrunc);   /* call lStk via dStk */
}

void lStkTrunc()
{
	/* shifting and shifting back truncates positive numbers,
		so we make the numbers positive */
	int signx, signy;
	signx = sign(Arg1->l.x);
	signy = sign(Arg1->l.y);
	Arg1->l.x = labs(Arg1->l.x);
	Arg1->l.y = labs(Arg1->l.y);
	Arg1->l.x = (Arg1->l.x) >> g_bit_shift;
	Arg1->l.y = (Arg1->l.y) >> g_bit_shift;
	Arg1->l.x = (Arg1->l.x) << g_bit_shift;
	Arg1->l.y = (Arg1->l.y) << g_bit_shift;
	Arg1->l.x = signx*Arg1->l.x;
	Arg1->l.y = signy*Arg1->l.y;
}
#endif

void (*StkTrunc)() = dStkTrunc;

void dStkRound()
{
	Arg1->d.x = floor(Arg1->d.x + .5);
	Arg1->d.y = floor(Arg1->d.y + .5);
}

#if !defined(XFRACT)
void mStkRound()
{
	mStkFunct(dStkRound);   /* call lStk via dStk */
}

void lStkRound()
{
	/* Add .5 then truncate */
	Arg1->l.x += (1L << g_bit_shift_minus_1);
	Arg1->l.y += (1L << g_bit_shift_minus_1);
	lStkFloor();
}
#endif

void (*StkRound)() = dStkRound;

void dStkZero()
{
	Arg1->d.y = Arg1->d.x = 0.0;
}

#if !defined(XFRACT)
void mStkZero()
{
	Arg1->m.x.Mant = Arg1->m.x.Exp = 0;
	Arg1->m.y.Mant = Arg1->m.y.Exp = 0;
}

void lStkZero()
{
	Arg1->l.y = Arg1->l.x = 0;
}
#endif

void (*StkZero)() = dStkZero;

void dStkOne()
{
	Arg1->d.x = 1.0;
	Arg1->d.y = 0.0;
}

#if !defined(XFRACT)
void mStkOne()
{
	Arg1->m = g_one_mpc;
}

void lStkOne()
{
	Arg1->l.x = (long) fg;
	Arg1->l.y = 0L;
}
#endif

void (*StkOne)() = dStkOne;


void dStkReal()
{
	Arg1->d.y = 0.0;
}

#if !defined(XFRACT)
void mStkReal()
{
	Arg1->m.y.Exp = 0;
	Arg1->m.y.Mant = 0L;
}

void lStkReal()
{
	Arg1->l.y = 0l;
}
#endif

void (*StkReal)() = dStkReal;

void dStkImag()
{
	Arg1->d.x = Arg1->d.y;
	Arg1->d.y = 0.0;
}

#if !defined(XFRACT)
void mStkImag()
{
	Arg1->m.x = Arg1->m.y;
	Arg1->m.y.Exp = 0;
	Arg1->m.y.Mant = 0L;
}

void lStkImag()
{
	Arg1->l.x = Arg1->l.y;
	Arg1->l.y = 0l;
}
#endif

void (*StkImag)() = dStkImag;

void dStkNeg()
{
	Arg1->d.x = -Arg1->d.x;
	Arg1->d.y = -Arg1->d.y;
}

#if !defined(XFRACT)
void mStkNeg()
{
	Arg1->m.x.Exp ^= 0x8000;
	Arg1->m.y.Exp ^= 0x8000;
}

void lStkNeg()
{
	Arg1->l.x = -Arg1->l.x;
	Arg1->l.y = -Arg1->l.y;
}
#endif

void (*StkNeg)() = dStkNeg;

void dStkMul()
{
	FPUcplxmul(&Arg2->d, &Arg1->d, &Arg2->d);
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkMul()
{
	Arg2->m = MPCmul(Arg2->m, Arg1->m);
	Arg1--;
	Arg2--;
}

void lStkMul()
{
	long x, y;

	x = multiply(Arg2->l.x, Arg1->l.x, g_bit_shift) -
	multiply(Arg2->l.y, Arg1->l.y, g_bit_shift);
	y = multiply(Arg2->l.y, Arg1->l.x, g_bit_shift) +
	multiply(Arg2->l.x, Arg1->l.y, g_bit_shift);
	Arg2->l.x = x;
	Arg2->l.y = y;
	Arg1--;
	Arg2--;
}
#endif

void (*StkMul)() = dStkMul;

void dStkDiv()
{
	FPUcplxdiv(&Arg2->d, &Arg1->d, &Arg2->d);
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkDiv()
{
	Arg2->m = MPCdiv(Arg2->m, Arg1->m);
	Arg1--;
	Arg2--;
}

void lStkDiv()
{
	long x, y, mod, x2, y2;

	mod = multiply(Arg1->l.x, Arg1->l.x, g_bit_shift) +
	multiply(Arg1->l.y, Arg1->l.y, g_bit_shift);
	x = divide(Arg1->l.x, mod, g_bit_shift);
	y = -divide(Arg1->l.y, mod, g_bit_shift);
	/* pb 900617 changed next 4 lines to use x2, y2 instead of x, y */
	x2 = multiply(Arg2->l.x, x, g_bit_shift) - multiply(Arg2->l.y, y, g_bit_shift);
	y2 = multiply(Arg2->l.y, x, g_bit_shift) + multiply(Arg2->l.x, y, g_bit_shift);
	Arg2->l.x = x2;
	Arg2->l.y = y2;
	Arg1--;
	Arg2--;
}
#endif

void (*StkDiv)() = dStkDiv;

void dStkMod()
{
	Arg1->d.x = (Arg1->d.x*Arg1->d.x) + (Arg1->d.y*Arg1->d.y);
	Arg1->d.y = 0.0;
}

#if !defined(XFRACT)
void mStkMod()
{
	Arg1->m.x = MPCmod(Arg1->m);
	Arg1->m.y.Exp = 0;
	Arg1->m.y.Mant = 0L;
}

void lStkMod()
{
/*   Arg1->l.x = multiply(Arg2->l.x, Arg1->l.x, g_bit_shift) + */
/*   multiply(Arg2->l.y, Arg1->l.y, g_bit_shift); */
/*** I don't understand how this ever worked correctly! JCO 12/31/94 ***/
	Arg1->l.x = multiply(Arg1->l.x, Arg1->l.x, g_bit_shift) +
	multiply(Arg1->l.y, Arg1->l.y, g_bit_shift);
	if (Arg1->l.x < 0)
	{
		g_overflow = 1;
	}
	Arg1->l.y = 0L;
}

void lStkModOld()
{
	Arg1->l.x = multiply(Arg2->l.x, Arg1->l.x, g_bit_shift) +
	multiply(Arg2->l.y, Arg1->l.y, g_bit_shift);
	if (Arg1->l.x < 0)
	{
		g_overflow = 1;
	}
	Arg1->l.y = 0L;
}
#endif

void (*StkMod)() = dStkMod;

void StkSto()
{
	*Store[g_store_ptr++] = *Arg1;
}

void (*PtrStkSto)() = StkSto;

void StkLod()
{
	Arg1++;
	Arg2++;
	*Arg1 = *Load[g_lod_ptr++];
}

void StkClr()
{
	s[0] = *Arg1;
	Arg1 = &s[0];
	Arg2 = Arg1;
	Arg2--;
}

void (*PtrStkClr)() = StkClr;

/* MCP 4-9-91, Added Flip() */

void dStkFlip()
{
	double t;

	t = Arg1->d.x;
	Arg1->d.x = Arg1->d.y;
	Arg1->d.y = t;
}

#if !defined(XFRACT)
void mStkFlip()
{
	struct MP t;

	t = Arg1->m.x;
	Arg1->m.x = Arg1->m.y;
	Arg1->m.y = t;
}

void lStkFlip()
{
	long t;

	t = Arg1->l.x;
	Arg1->l.x = Arg1->l.y;
	Arg1->l.y = t;
}
#endif

void (*StkFlip)() = dStkFlip;

void dStkSin()
{
	double sinx, cosx, sinhy, coshy;

	FPUsincos(&Arg1->d.x, &sinx, &cosx);
	FPUsinhcosh(&Arg1->d.y, &sinhy, &coshy);
	Arg1->d.x = sinx*coshy;
	Arg1->d.y = cosx*sinhy;
}

#if !defined(XFRACT)
void mStkSin()
{
	mStkFunct(dStkSin);   /* call lStk via dStk */
}

void lStkSin()
{
	long x, y, sinx, cosx, sinhy, coshy;
	x = Arg1->l.x >> Delta16;
	y = Arg1->l.y >> Delta16;
	SinCos086(x, &sinx, &cosx);
	SinhCosh086(y, &sinhy, &coshy);
	Arg1->l.x = multiply(sinx, coshy, ShiftBack); /* TIW 06-18-90 */
	Arg1->l.y = multiply(cosx, sinhy, ShiftBack); /* TIW 06-18-90 */
}
#endif

void (*StkSin)() = dStkSin;

/* The following functions are supported by both the parser and for fn
	variable replacement. TIW 04-22-91 */

void dStkTan()
{
	double sinx, cosx, sinhy, coshy, denom;
	Arg1->d.x *= 2;
	Arg1->d.y *= 2;
	FPUsincos(&Arg1->d.x, &sinx, &cosx);
	FPUsinhcosh(&Arg1->d.y, &sinhy, &coshy);
	denom = cosx + coshy;
	ChkFloatDenom(denom);
	Arg1->d.x = sinx/denom;
	Arg1->d.y = sinhy/denom;
}

#if !defined(XFRACT)
void mStkTan()
{
	mStkFunct(dStkTan);   /* call lStk via dStk */
}

void lStkTan()
{
	long x, y, sinx, cosx, sinhy, coshy, denom;
	x = Arg1->l.x >> Delta16;
	x = x << 1;
	y = Arg1->l.y >> Delta16;
	y = y << 1;
	SinCos086(x, &sinx, &cosx);
	SinhCosh086(y, &sinhy, &coshy);
	denom = cosx + coshy;
	ChkLongDenom(denom);
	Arg1->l.x = divide(sinx, denom, g_bit_shift);
	Arg1->l.y = divide(sinhy, denom, g_bit_shift);
}
#endif

void (*StkTan)() = dStkTan;

void dStkTanh()
{
	double siny, cosy, sinhx, coshx, denom;
	Arg1->d.x *= 2;
	Arg1->d.y *= 2;
	FPUsincos(&Arg1->d.y, &siny, &cosy);
	FPUsinhcosh(&Arg1->d.x, &sinhx, &coshx);
	denom = coshx + cosy;
	ChkFloatDenom(denom);
	Arg1->d.x = sinhx/denom;
	Arg1->d.y = siny/denom;
}

#if !defined(XFRACT)
void mStkTanh()
{
	mStkFunct(dStkTanh);   /* call lStk via dStk */
}

void lStkTanh()
{
	long x, y, siny, cosy, sinhx, coshx, denom;
	x = Arg1->l.x >> Delta16;
	x = x << 1;
	y = Arg1->l.y >> Delta16;
	y = y << 1;
	SinCos086(y, &siny, &cosy);
	SinhCosh086(x, &sinhx, &coshx);
	denom = coshx + cosy;
	ChkLongDenom(denom);
	Arg1->l.x = divide(sinhx, denom, g_bit_shift);
	Arg1->l.y = divide(siny, denom, g_bit_shift);
}
#endif

void (*StkTanh)() = dStkTanh;

void dStkCoTan()
{
	double sinx, cosx, sinhy, coshy, denom;
	Arg1->d.x *= 2;
	Arg1->d.y *= 2;
	FPUsincos(&Arg1->d.x, &sinx, &cosx);
	FPUsinhcosh(&Arg1->d.y, &sinhy, &coshy);
	denom = coshy - cosx;
	ChkFloatDenom(denom);
	Arg1->d.x = sinx/denom;
	Arg1->d.y = -sinhy/denom;
}

#if !defined(XFRACT)
void mStkCoTan()
{
	mStkFunct(dStkCoTan);   /* call lStk via dStk */
}

void lStkCoTan()
{
	long x, y, sinx, cosx, sinhy, coshy, denom;
	x = Arg1->l.x >> Delta16;
	x = x << 1;
	y = Arg1->l.y >> Delta16;
	y = y << 1;
	SinCos086(x, &sinx, &cosx);
	SinhCosh086(y, &sinhy, &coshy);
	denom = coshy - cosx;
	ChkLongDenom(denom);
	Arg1->l.x = divide(sinx, denom, g_bit_shift);
	Arg1->l.y = -divide(sinhy, denom, g_bit_shift);
}
#endif

void (*StkCoTan)() = dStkCoTan;

void dStkCoTanh()
{
	double siny, cosy, sinhx, coshx, denom;
	Arg1->d.x *= 2;
	Arg1->d.y *= 2;
	FPUsincos(&Arg1->d.y, &siny, &cosy);
	FPUsinhcosh(&Arg1->d.x, &sinhx, &coshx);
	denom = coshx - cosy;
	ChkFloatDenom(denom);
	Arg1->d.x = sinhx/denom;
	Arg1->d.y = -siny/denom;
}

#if !defined(XFRACT)
void mStkCoTanh()
{
	mStkFunct(dStkCoTanh);   /* call lStk via dStk */
}

void lStkCoTanh()
{
	long x, y, siny, cosy, sinhx, coshx, denom;
	x = Arg1->l.x >> Delta16;
	x = x << 1;
	y = Arg1->l.y >> Delta16;
	y = y << 1;
	SinCos086(y, &siny, &cosy);
	SinhCosh086(x, &sinhx, &coshx);
	denom = coshx - cosy;
	ChkLongDenom(denom);
	Arg1->l.x = divide(sinhx, denom, g_bit_shift);
	Arg1->l.y = -divide(siny, denom, g_bit_shift);
}
#endif

void (*StkCoTanh)() = dStkCoTanh;

/* The following functions are not directly used by the parser - support
	for the parser was not provided because the existing parser language
	represents these quite easily. They are used for fn variable support
	in miscres.c but are placed here because they follow the pattern of
	the other parser functions. TIW 04-22-91 */

void dStkRecip()
{
	double mod;
	mod = Arg1->d.x*Arg1->d.x + Arg1->d.y*Arg1->d.y;
	ChkFloatDenom(mod);
	Arg1->d.x =  Arg1->d.x/mod;
	Arg1->d.y = -Arg1->d.y/mod;
}

#if !defined(XFRACT)
void mStkRecip()
{
	struct MP mod;
	mod = *MPadd(*MPmul(Arg1->m.x, Arg1->m.x), *MPmul(Arg1->m.y, Arg1->m.y));
	if (mod.Mant == 0L)
	{
		g_overflow = 1;
		return;
	}
	Arg1->m.x = *MPdiv(Arg1->m.x, mod);
	Arg1->m.y = *MPdiv(Arg1->m.y, mod);
	Arg1->m.y.Exp ^= 0x8000;
}

void lStkRecip()
{
	long mod;
	mod = multiply(Arg1->l.x, Arg1->l.x, g_bit_shift)
		+ multiply(Arg1->l.y, Arg1->l.y, g_bit_shift);
	if (g_save_release > 1920)
	{
		ChkLongDenom(mod);
	}
	else if (mod <= 0L)
	{
		return;
	}
	Arg1->l.x =  divide(Arg1->l.x, mod, g_bit_shift);
	Arg1->l.y = -divide(Arg1->l.y, mod, g_bit_shift);
}
#endif

void StkIdent()  /* do nothing - the function Z */
{
}
/* End TIW 04-22-91 */

void dStkSinh()
{
	double siny, cosy, sinhx, coshx;

	FPUsincos(&Arg1->d.y, &siny, &cosy);
	FPUsinhcosh(&Arg1->d.x, &sinhx, &coshx);
	Arg1->d.x = sinhx*cosy;
	Arg1->d.y = coshx*siny;
}

#if !defined(XFRACT)
void mStkSinh()
{
	mStkFunct(dStkSinh);   /* call lStk via dStk */
}

void lStkSinh()
{
	long x, y, sinhx, coshx, siny, cosy;

	x = Arg1->l.x >> Delta16;
	y = Arg1->l.y >> Delta16;
	SinCos086(y, &siny, &cosy);
	SinhCosh086(x, &sinhx, &coshx);
	Arg1->l.x = multiply(cosy, sinhx, ShiftBack); /* TIW 06-18-90 */
	Arg1->l.y = multiply(siny, coshx, ShiftBack); /* TIW 06-18-90 */
}
#endif

void (*StkSinh)() = dStkSinh;

void dStkCos()
{
	double sinx, cosx, sinhy, coshy;

	FPUsincos(&Arg1->d.x, &sinx, &cosx);
	FPUsinhcosh(&Arg1->d.y, &sinhy, &coshy);
	Arg1->d.x = cosx*coshy;
	Arg1->d.y = -sinx*sinhy; /* TIW 04-25-91 sign */
}

#if !defined(XFRACT)
void mStkCos()
{
	mStkFunct(dStkCos);   /* call lStk via dStk */
}

void lStkCos()
{
	long x, y, sinx, cosx, sinhy, coshy;

	x = Arg1->l.x >> Delta16;
	y = Arg1->l.y >> Delta16;
	SinCos086(x, &sinx, &cosx);
	SinhCosh086(y, &sinhy, &coshy);
	Arg1->l.x = multiply(cosx, coshy, ShiftBack); /* TIW 06-18-90 */
	Arg1->l.y = -multiply(sinx, sinhy, ShiftBack); /* TIW 04-25-91 sign */
}
#endif

void (*StkCos)() = dStkCos;

/* Bogus version of cos, to replicate bug which was in regular cos till v16: */

void dStkCosXX()
{
	dStkCos();
	Arg1->d.y = -Arg1->d.y;
}

#if !defined(XFRACT)
void mStkCosXX()
{
	mStkFunct(dStkCosXX);   /* call lStk via dStk */
}

void lStkCosXX()
{
	lStkCos();
	Arg1->l.y = -Arg1->l.y;
}
#endif

void (*StkCosXX)() = dStkCosXX;

void dStkCosh()
{
	double siny, cosy, sinhx, coshx;

	FPUsincos(&Arg1->d.y, &siny, &cosy);
	FPUsinhcosh(&Arg1->d.x, &sinhx, &coshx);
	Arg1->d.x = coshx*cosy;
	Arg1->d.y = sinhx*siny;
}

#if !defined(XFRACT)
void mStkCosh()
{
	mStkFunct(dStkCosh);   /* call lStk via dStk */
}

void lStkCosh()
{
	long x, y, sinhx, coshx, siny, cosy;

	x = Arg1->l.x >> Delta16;
	y = Arg1->l.y >> Delta16;
	SinCos086(y, &siny, &cosy);
	SinhCosh086(x, &sinhx, &coshx);
	Arg1->l.x = multiply(cosy, coshx, ShiftBack); /* TIW 06-18-90 */
	Arg1->l.y = multiply(siny, sinhx, ShiftBack); /* TIW 06-18-90 */
}
#endif

void (*StkCosh)() = dStkCosh;

/* TIW added arc functions here 11-25-94 */

void dStkASin()
{
	Arcsinz(Arg1->d, &(Arg1->d));
}

#if !defined(XFRACT)
void mStkASin()
{
	mStkFunct(dStkASin);
}

void lStkASin()
{
	lStkFunct(dStkASin);
}
#endif

void (*StkASin)() = dStkASin;

void dStkASinh()
{
	Arcsinhz(Arg1->d, &(Arg1->d));
}

#if !defined(XFRACT)
void mStkASinh()
{
	mStkFunct(dStkASinh);
}

void lStkASinh()
{
	lStkFunct(dStkASinh);
}
#endif

void (*StkASinh)() = dStkASinh;

void dStkACos()
{
	Arccosz(Arg1->d, &(Arg1->d));
}

#if !defined(XFRACT)
void mStkACos()
{
	mStkFunct(dStkACos);
}

void lStkACos()
{
	lStkFunct(dStkACos);
}
#endif

void (*StkACos)() = dStkACos;

void dStkACosh()
{
	Arccoshz(Arg1->d, &(Arg1->d));
}

#if !defined(XFRACT)
void mStkACosh()
{
	mStkFunct(dStkACosh);
}

void lStkACosh()
{
	lStkFunct(dStkACosh);
}
#endif

void (*StkACosh)() = dStkACosh;

void dStkATan()
{
	Arctanz(Arg1->d, &(Arg1->d));
}

#if !defined(XFRACT)
void mStkATan()
{
	mStkFunct(dStkATan);
}

void lStkATan()
{
	lStkFunct(dStkATan);
}
#endif

void (*StkATan)() = dStkATan;

void dStkATanh()
{
	Arctanhz(Arg1->d, &(Arg1->d));
}

#if !defined(XFRACT)
void mStkATanh()
{
	mStkFunct(dStkATanh);
}

void lStkATanh()
{
	lStkFunct(dStkATanh);
}
#endif

void (*StkATanh)() = dStkATanh;

void dStkSqrt()
{
	Arg1->d = ComplexSqrtFloat(Arg1->d.x, Arg1->d.y);
}

#if !defined(XFRACT)
void mStkSqrt()
{
	mStkFunct(dStkSqrt);
}

void lStkSqrt()
{
	/* lStkFunct(dStkSqrt); */
	Arg1->l = ComplexSqrtLong(Arg1->l.x, Arg1->l.y);
}
#endif

void (*StkSqrt)() = dStkSqrt;

void dStkCAbs()
{
	Arg1->d.x = sqrt(sqr(Arg1->d.x) + sqr(Arg1->d.y));
	Arg1->d.y = 0.0;
}

#if !defined(XFRACT)
void mStkCAbs()
{
	mStkFunct(dStkCAbs);
}

void lStkCAbs()
{
	lStkFunct(dStkCAbs);
}
#endif

void (*StkCAbs)() = dStkCAbs;

/* TIW end arc functions 11-25-94 */

void dStkLT()
{
	Arg2->d.x = (double)(Arg2->d.x < Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkLT()
{
	Arg2->m.x = *fg2MP((long)(MPcmp(Arg2->m.x, Arg1->m.x) == -1), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkLT()
{
	Arg2->l.x = (long)(Arg2->l.x < Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkLT)() = dStkLT;

void dStkGT()
{
	Arg2->d.x = (double)(Arg2->d.x > Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkGT()
{
	Arg2->m.x = *fg2MP((long)(MPcmp(Arg2->m.x, Arg1->m.x) == 1), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkGT()
{
	Arg2->l.x = (long)(Arg2->l.x > Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkGT)() = dStkGT;

void dStkLTE()
{
	Arg2->d.x = (double)(Arg2->d.x <= Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkLTE()
{
	int comp;

	comp = MPcmp(Arg2->m.x, Arg1->m.x);
	Arg2->m.x = *fg2MP((long)(comp == -1 || comp == 0), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkLTE()
{
	Arg2->l.x = (long)(Arg2->l.x <= Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkLTE)() = dStkLTE;

void dStkGTE()
{
	Arg2->d.x = (double)(Arg2->d.x >= Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkGTE()
{
	int comp;

	comp = MPcmp(Arg2->m.x, Arg1->m.x);
	Arg2->m.x = *fg2MP((long)(comp == 1 || comp == 0), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkGTE()
{
	Arg2->l.x = (long)(Arg2->l.x >= Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkGTE)() = dStkGTE;

void dStkEQ()
{
	Arg2->d.x = (double)(Arg2->d.x == Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkEQ()
{
	int comp;

	comp = MPcmp(Arg2->m.x, Arg1->m.x);
	Arg2->m.x = *fg2MP((long)(comp == 0), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkEQ()
{
	Arg2->l.x = (long)(Arg2->l.x == Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkEQ)() = dStkEQ;

void dStkNE()
{
	Arg2->d.x = (double)(Arg2->d.x != Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkNE()
{
	int comp;

	comp = MPcmp(Arg2->m.x, Arg1->m.x);
	Arg2->m.x = *fg2MP((long)(comp != 0), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkNE()
{
	Arg2->l.x = (long)(Arg2->l.x != Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkNE)() = dStkNE;

void dStkOR()
{
	Arg2->d.x = (double)(Arg2->d.x || Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkOR()
{
	Arg2->m.x = *fg2MP((long)(Arg2->m.x.Mant || Arg1->m.x.Mant), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkOR()
{
	Arg2->l.x = (long)(Arg2->l.x || Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkOR)() = dStkOR;

void dStkAND()
{
	Arg2->d.x = (double)(Arg2->d.x && Arg1->d.x);
	Arg2->d.y = 0.0;
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkAND()
{
	Arg2->m.x = *fg2MP((long)(Arg2->m.x.Mant && Arg1->m.x.Mant), 0);
	Arg2->m.y.Exp = 0;
	Arg2->m.y.Mant = 0L;
	Arg1--;
	Arg2--;
}

void lStkAND()
{
	Arg2->l.x = (long)(Arg2->l.x && Arg1->l.x) << g_bit_shift; /* JCO 12/26/94 */
	Arg2->l.y = 0l;
	Arg1--;
	Arg2--;
}
#endif

void (*StkAND)() = dStkAND;
void dStkLog()
{
	FPUcplxlog(&Arg1->d, &Arg1->d);
}

#if !defined(XFRACT)
void mStkLog()
{
	mStkFunct(dStkLog);   /* call lStk via dStk */
}

void lStkLog()
{
	lStkFunct(dStkLog);
}
#endif

void (*StkLog)() = dStkLog;

void FPUcplxexp(_CMPLX *x, _CMPLX *z)
{
	FPUcplxexp387(x, z);
}

void dStkExp()
{
	FPUcplxexp(&Arg1->d, &Arg1->d);
}

#if !defined(XFRACT)
void mStkExp()
{
	mStkFunct(dStkExp);   /* call lStk via dStk */
}

void lStkExp()
{
	lStkFunct(dStkExp);
}
#endif

void (*StkExp)() = dStkExp;

void dStkPwr()
{
	Arg2->d = ComplexPower(Arg2->d, Arg1->d);
	Arg1--;
	Arg2--;
}

#if !defined(XFRACT)
void mStkPwr()
{
	_CMPLX x, y;

	x = MPC2cmplx(Arg2->m);
	y = MPC2cmplx(Arg1->m);
	x = ComplexPower(x, y);
	Arg2->m = cmplx2MPC(x);
	Arg1--;
	Arg2--;
}

void lStkPwr()
{
	_CMPLX x, y;

	x.x = (double)Arg2->l.x / fg;
	x.y = (double)Arg2->l.y / fg;
	y.x = (double)Arg1->l.x / fg;
	y.y = (double)Arg1->l.y / fg;
	x = ComplexPower(x, y);
	if (fabs(x.x) < g_fudge_limit && fabs(x.y) < g_fudge_limit)
	{
		Arg2->l.x = (long)(x.x*fg);
		Arg2->l.y = (long)(x.y*fg);
	}
	else
	{
		g_overflow = 1;
	}
	Arg1--;
	Arg2--;
}
#endif

void (*StkPwr)() = dStkPwr;

void EndInit()
{
	g_last_init_op = OpPtr;
	InitJumpIndex = jump_index;
}

void (*PtrEndInit)() = EndInit;

void StkJump()
{
	OpPtr =  jump_control[jump_index].ptrs.JumpOpPtr;
	g_lod_ptr = jump_control[jump_index].ptrs.JumpLodPtr;
	g_store_ptr = jump_control[jump_index].ptrs.JumpStoPtr;
	jump_index = jump_control[jump_index].DestJumpIndex;
}

void dStkJumpOnFalse()
{
	if (Arg1->d.x == 0)
	{
		StkJump();
	}
	else
	{
		jump_index++;
	}
}

void mStkJumpOnFalse()
{
#if !defined(XFRACT)
	if (Arg1->m.x.Mant == 0)
	{
		StkJump();
	}
	else
	{
		jump_index++;
	}
#endif
}

void lStkJumpOnFalse()
{
	if (Arg1->l.x == 0)
	{
		StkJump();
	}
	else
	{
		jump_index++;
	}
}

void (*StkJumpOnFalse)() = dStkJumpOnFalse;

void dStkJumpOnTrue()
{
	if (Arg1->d.x)
	{
		StkJump();
	}
	else
	{
		jump_index++;
	}
}

void mStkJumpOnTrue()
{
#if !defined(XFRACT)
	if (Arg1->m.x.Mant)
	{
		StkJump();
	}
	else
	{
		jump_index++;
	}
#endif
}

void lStkJumpOnTrue()
{
	if (Arg1->l.x)
	{
		StkJump();
	}
	else
	{
		jump_index++;
	}
}

void (*StkJumpOnTrue)() = dStkJumpOnTrue;

void StkJumpLabel()
{
	jump_index++;
}


static unsigned count_white_space(char *str)
{
	unsigned n, done;

	for (done = n = 0; !done; n++)
	{
		switch (str[n])
		{
		case ' ':
		case '\t':
		case '\n':
		case '\r':
			break;
		default:
			done = 1;
		}
	}
	return n - 1;
}

/* detect if constant is part of a (a, b) construct */
static int isconst_pair(char *Str)
{
	int n, j;
	int answer = 0;
	/* skip past first number */
	for (n = 0; isdigit(Str[n]) || Str[n] == '.'; n++)
	{
	}
	if (Str[n] == ',')
	{
		j = n + count_white_space(&Str[n + 1]) + 1;
		if (isdigit(Str[j])
			|| (Str[j] == '-' && (isdigit(Str[j + 1]) || Str[j + 1] == '.'))
			|| Str[j] == '.')
		{
			answer = 1;
		}
	}
	return answer;
}

struct ConstArg *is_constant(char *Str, int Len)
{
	_CMPLX z;
	unsigned n, j;
	/* next line enforces variable vs constant naming convention */
	for (n = 0; n < g_parser_vsp; n++)
	{
		if (v[n].len == Len)
		{
			if (!strnicmp(v[n].s, Str, Len))
			{
				if (n == 1)        /* The formula uses 'p1'. */
				{
					g_uses_p1 = 1;
				}
				if (n == 2)        /* The formula uses 'p2'. */
				{
					g_uses_p2 = 1;
				}
				if (n == 7)        /* The formula uses 'rand'. */
				{
					RandomSeed();
				}
				if (n == 8)        /* The formula uses 'p3'. */
				{
					g_uses_p3 = 1;
				}
				if (n == 13)        /* The formula uses 'ismand'. */
				{
					g_uses_is_mand = 1;
				}
				if (n == 17)        /* The formula uses 'p4'. */
				{
					g_uses_p4 = 1;
				}
				if (n == 18)        /* The formula uses 'p5'. */
				{
					g_uses_p5 = 1;
				}
#if !defined(XFRACT)
				if (n == 10 || n == 11 || n == 12)
				{
					if (MathType == L_MATH)
					{
						driver_unget_key('f');
					}
				}
#endif
				if (!isconst_pair(Str))
				{
					return &v[n];
				}
			}
		}
	}
	v[g_parser_vsp].s = Str;
	v[g_parser_vsp].len = Len;
	v[g_parser_vsp].a.d.x = v[g_parser_vsp].a.d.y = 0.0;

#if !defined(XFRACT)
	/* v[g_parser_vsp].a should already be zeroed out */
	switch (MathType)
	{
	case M_MATH:
		v[g_parser_vsp].a.m.x.Mant = v[g_parser_vsp].a.m.x.Exp = 0;
		v[g_parser_vsp].a.m.y.Mant = v[g_parser_vsp].a.m.y.Exp = 0;
		break;
	case L_MATH:
		v[g_parser_vsp].a.l.x = v[g_parser_vsp].a.l.y = 0;
		break;
	}
#endif

	if (isdigit(Str[0])
		|| (Str[0] == '-' && (isdigit(Str[1]) || Str[1] == '.'))
		|| Str[0] == '.')
	{
		if (o[g_posp-1].f == StkNeg)
		{
			g_posp--;
			Str = Str - 1;
			InitN--;
			v[g_parser_vsp].len++;
		}
		for (n = 1; isdigit(Str[n]) || Str[n] == '.'; n++)
		{
		}
		if (Str[n] == ',')
		{
			j = n + count_white_space(&Str[n + 1]) + 1;
			if (isdigit(Str[j])
				|| (Str[j] == '-' && (isdigit(Str[j + 1]) || Str[j + 1] == '.'))
				|| Str[j] == '.')
			{
				z.y = atof(&Str[j]);
				for (; isdigit(Str[j]) || Str[j] == '.' || Str[j] == '-'; j++)
				{
				}
				v[g_parser_vsp].len = j;
			}
			else
			{
				z.y = 0.0;
			}
		}
		else
		{
			z.y = 0.0;
		}
		z.x = atof(Str);
		switch (MathType)
		{
		case D_MATH:
			v[g_parser_vsp].a.d = z;
			break;
#if !defined(XFRACT)
		case M_MATH:
			v[g_parser_vsp].a.m = cmplx2MPC(z);
			break;
		case L_MATH:
			v[g_parser_vsp].a.l.x = (long)(z.x*fg);
			v[g_parser_vsp].a.l.y = (long)(z.y*fg);
			break;
#endif
		}
		v[g_parser_vsp].s = Str;
	}
	return &v[g_parser_vsp++];
}


struct FNCT_LIST
{
	char *s;              /* TIW 03-31-91 added far */
	void (**ptr)();
};

/* TIW 03-30-91 START */
void (*StkTrig0)() = dStkSin;
void (*StkTrig1)() = dStkSqr;
void (*StkTrig2)() = dStkSinh;
void (*StkTrig3)() = dStkCosh;

char * JumpList[] =
{
	"if",
	"elseif",
	"else",
	"endif",
	""
};



int isjump(char *Str, int Len)
{
	/* return values
       0 - Not a jump
       1 - if
       2 - elseif
       3 - else
       4 - endif
	*/

	int i;

	for (i = 0; *JumpList[i]; i++)
	{
		if ((int) strlen(JumpList[i]) == Len)
		{
			if (!strnicmp(JumpList[i], Str, Len))
			{
				return i + 1;
			}
		}
	}
	return 0;
}


char g_max_fn = 0;
/* TIW 03-30-91 STOP */

struct FNCT_LIST FnctList[] =  /* TIW 03-31-91 added far */
{
	{"sin",   &StkSin},
	{"sinh",  &StkSinh},
	{"cos",   &StkCos},
	{"cosh",  &StkCosh},
	{"sqr",   &StkSqr},
	{"log",   &StkLog},
	{"exp",   &StkExp},
	{"abs",   &StkAbs},
	{"conj",  &StkConj},
	{"real",  &StkReal},
	{"imag",  &StkImag},
	{"fn1",   &StkTrig0},   /* TIW 03-30-91 */
	{"fn2",   &StkTrig1},   /* TIW 03-30-91 */
	{"fn3",   &StkTrig2},   /* TIW 03-30-91 */
	{"fn4",   &StkTrig3},   /* TIW 03-30-91 */
	{"flip",  &StkFlip},    /* MCP 4-9-91 */
	{"tan",   &StkTan},     /* TIW 04-22-91 */
	{"tanh",  &StkTanh},    /* TIW 04-22-91 */
	{"cotan", &StkCoTan},   /* TIW 04-24-91 */
	{"cotanh", &StkCoTanh},  /* TIW 04-24-91 */
	{"cosxx", &StkCosXX},   /* PB  04-28-91 */
	{"srand", &StkSRand},   /* MCP 11-21-91 */
	{"asin",  &StkASin},    /* TIW 11-26-94 */
	{"asinh", &StkASinh},   /* TIW 11-26-94 */
	{"acos",  &StkACos},    /* TIW 11-26-94 */
	{"acosh", &StkACosh},   /* TIW 11-26-94 */
	{"atan",  &StkATan},    /* TIW 11-26-94 */
	{"atanh", &StkATanh},   /* TIW 11-26-94 */
	{"sqrt",  &StkSqrt},    /* TIW 11-26-94 */
	{"cabs",  &StkCAbs},    /* TIW 11-26-94 */
	{"floor", &StkFloor},   /* TIW 06-30-96 */
	{"ceil",  &StkCeil},    /* TIW 06-30-96 */
	{"trunc", &StkTrunc},   /* TIW 06-30-96 */
	{"round", &StkRound},   /* TIW 06-30-96 */
};

char *OPList[] =
{
	",",	/*  0 */
	"!=",	/*  1 */
	"=",	/*  2 */
	"==",	/*  3 */
	"<",	/*  4 */
	"<=",	/*  5 */
	">",	/*  6 */
	">=",	/*  7 */
	"|",	/*  8 */
	"||",	/*  9 */
	"&&",	/* 10 */
	":",	/* 11 */
	"+",	/* 12 */
	"-",	/* 13 */
	"*",	/* 14 */
	"/",	/* 15 */
	"^"		/* 16 */
};


void not_a_function()
{
}
void function_not_found()
{
}

/* determine if s names a function and if so which one */
/* TIW 04-22-91 */
int whichfn(char *s, int len)
{
	int out;
	if (len != 3)
	{
		out = 0;
	}
	else if (strnicmp(s, "fn", 2))
	{
		out = 0;
	}
	else
	{
		out = atoi(s + 2);
	}
	if (out < 1 || out > 4)
	{
		out = 0;
	}
	return out;
}

#if !defined(XFRACT)
t_function *is_function(char *Str, int Len)
#else
void (*is_function(char *Str, int Len))()
#endif
{
	unsigned n;
	int functnum;    /* TIW 04-22-91 */

	n = count_white_space(&Str[Len]);
	if (Str[Len + n] == '(')
	{
		for (n = 0; n < sizeof(FnctList) / sizeof(struct FNCT_LIST); n++)
		{
			if ((int) strlen(FnctList[n].s) == Len)  /* TIW 03-31-91 added far */
			{
				if (!strnicmp(FnctList[n].s, Str, Len))  /* TIW 03-31-91 added far */
				{
					/* count function variables */
					functnum = whichfn(Str, Len);
					if (functnum != 0)    /* TIW 04-22-91 */
					{
						if (functnum > g_max_fn)                  /* TIW 04-22-91 */
						{
							g_max_fn = (char)functnum;                  /* TIW 04-22-91 */
						}
					}
					return *FnctList[n].ptr;
				}
			}
		}
		return function_not_found;
	}
	return not_a_function;
}

void RecSortPrec()
{
	int ThisOp = NextOp++;
	while (o[ThisOp].p > o[NextOp].p && NextOp < g_posp)
	{
		RecSortPrec();
	}
	f[OpPtr++] = o[ThisOp].f;
}

static char *Constants[] =
{
	"pixel",        /* v[0] */
	"p1",           /* v[1] */
	"p2",           /* v[2] */
	"z",            /* v[3] */
	"LastSqr",      /* v[4] */
	"pi",           /* v[5] */
	"e",            /* v[6] */
	"rand",         /* v[7] */
	"p3",           /* v[8] */
	"whitesq",      /* v[9] */
	"scrnpix",      /* v[10] */
	"scrnmax",      /* v[11] */
	"maxit",        /* v[12] */
	"ismand",       /* v[13] */
	"center",       /* v[14] */
	"magxmag",      /* v[15] */
	"rotskew",      /* v[16] */
	"p4",           /* v[17] */
	"p5"            /* v[18] */
};

struct SYMETRY
{
	char *s;
	int n;
}
SymStr[] =
{
	{"NOSYM",         0},
	{"XAXIS_NOPARM", -1},
	{"XAXIS",         1},
	{"YAXIS_NOPARM", -2},
	{"YAXIS",         2},
	{"XYAXIS_NOPARM", -3},
	{"XYAXIS",        3},
	{"ORIGIN_NOPARM", -4},
	{"ORIGIN",        4},
	{"PI_SYM_NOPARM", -5},
	{"PI_SYM",        5},
	{"XAXIS_NOIMAG", -6},
	{"XAXIS_NOREAL",  6},
	{"NOPLOT",       99},
	{"",              0}
};

static int ParseStr(char *Str, int pass)
{
	struct ConstArg *c;
	int ModFlag = 999, Len, Equals = 0, Mod[20], mdstk = 0;
	int jumptype;
	double const_pi, const_e;
	double Xctr, Yctr, Xmagfactor, Rotation, Skew;
	LDBL Magnification;
	SetRandom = Randomized = 0;
	uses_jump = 0;
	jump_index = 0;
	if (!g_type_specific_work_area)
	{
		stop_message(0, ParseErrs(PE_INSUFFICIENT_MEM_FOR_TYPE_FORMULA));
		return 1;
	}
	switch (MathType)
	{
	case D_MATH:
		StkAdd = dStkAdd;
		StkSub = dStkSub;
		StkNeg = dStkNeg;
		StkMul = dStkMul;
		StkSin = dStkSin;
		StkSinh = dStkSinh;
		StkLT = dStkLT;
		StkLTE = dStkLTE;
		StkMod = dStkMod;
		StkSqr = dStkSqr;
		StkCos = dStkCos;
		StkCosh = dStkCosh;
		StkLog = dStkLog;
		StkExp = dStkExp;
		StkPwr = dStkPwr;
		StkDiv = dStkDiv;
		StkAbs = dStkAbs;
		StkReal = dStkReal;
		StkImag = dStkImag;
		StkConj = dStkConj;
		StkTrig0 = g_trig0_d;   /* TIW 03-30-91 */
		StkTrig1 = g_trig1_d;   /* TIW 03-30-91 */
		StkTrig2 = g_trig2_d;   /* TIW 03-30-91 */
		StkTrig3 = g_trig3_d;   /* TIW 03-30-91 */
		StkFlip = dStkFlip;
		StkTan = dStkTan;    /* TIW 04-22-91 */
		StkTanh = dStkTanh;  /* TIW 04-22-91 */
		StkCoTan = dStkCoTan;    /* TIW 04-24-91 */
		StkCoTanh = dStkCoTanh;  /* TIW 04-24-91 */
		StkCosXX = dStkCosXX;    /* PB  04-28-91 */
		StkGT  = dStkGT;         /* MCP 11-3-91 */
		StkGTE = dStkGTE;        /* MCP 11-3-91 */
		StkEQ  = dStkEQ;         /* MCP 11-3-91 */
		StkNE  = dStkNE;         /* MCP 11-3-91 */
		StkAND = dStkAND;        /* MCP 11-3-91 */
		StkOR  = dStkOR ;        /* MCP 11-3-91 */
		StkSRand = dStkSRand;    /* MCP 11-21-91 */
		StkASin = dStkASin;      /* TIW 11-25-94 */
		StkASinh = dStkASinh;    /* TIW 11-25-94 */
		StkACos = dStkACos;      /* TIW 11-25-94 */
		StkACosh = dStkACosh;    /* TIW 11-25-94 */
		StkATan = dStkATan;      /* TIW 11-25-94 */
		StkATanh = dStkATanh;    /* TIW 11-25-94 */
		StkCAbs = dStkCAbs;      /* TIW 11-25-94 */
		StkSqrt = dStkSqrt;      /* TIW 11-25-94 */
		StkZero = dStkZero;      /* JCO 12-31-94 */
		StkFloor = dStkFloor;    /* TIW 06-30-96 */
		StkCeil = dStkCeil;      /* TIW 06-30-96 */
		StkTrunc = dStkTrunc;    /* TIW 06-30-96 */
		StkRound = dStkRound;    /* TIW 06-30-96 */
		StkJumpOnTrue  = dStkJumpOnTrue;     /* GGM 02-10-97 */
		StkJumpOnFalse = dStkJumpOnFalse;    /* GGM 02-10-97 */
		StkOne = dStkOne;        /* GGM 10-08-97 */
		break;
#if !defined(XFRACT)
	case M_MATH:
		StkAdd = mStkAdd;
		StkSub = mStkSub;
		StkNeg = mStkNeg;
		StkMul = mStkMul;
		StkSin = mStkSin;
		StkSinh = mStkSinh;
		StkLT = mStkLT;
		StkLTE = mStkLTE;
		StkMod = mStkMod;
		StkSqr = mStkSqr;
		StkCos = mStkCos;
		StkCosh = mStkCosh;
		StkLog = mStkLog;
		StkExp = mStkExp;
		StkPwr = mStkPwr;
		StkDiv = mStkDiv;
		StkAbs = mStkAbs;
		StkReal = mStkReal;
		StkImag = mStkImag;
		StkConj = mStkConj;
		StkTrig0 = g_trig0_m;  /* TIW 03-30-91 */
		StkTrig1 = g_trig1_m;  /* TIW 03-30-91 */
		StkTrig2 = g_trig2_m;  /* TIW 03-30-91 */
		StkTrig3 = g_trig3_m;  /* TIW 03-30-91 */
		StkFlip = mStkFlip;
		StkTan  = mStkTan;  /* TIW 04-22-91 */
		StkTanh  = mStkTanh; /* TIW 04-22-91 */
		StkCoTan  = mStkCoTan;  /* TIW 04-24-91 */
		StkCoTanh  = mStkCoTanh; /* TIW 04-24-91 */
		StkCosXX = mStkCosXX;   /* PB  04-28-91 */
		StkGT  = mStkGT;         /* MCP 11-3-91 */
		StkGTE = mStkGTE;        /* MCP 11-3-91 */
		StkEQ  = mStkEQ;         /* MCP 11-3-91 */
		StkNE  = mStkNE;         /* MCP 11-3-91 */
		StkAND = mStkAND;        /* MCP 11-3-91 */
		StkOR  = mStkOR ;        /* MCP 11-3-91 */
		StkSRand = mStkSRand;    /* MCP 11-21-91 */
		StkASin = mStkASin;      /* TIW 11-25-94 */
		StkACos = mStkACos;      /* TIW 11-25-94 */
		StkACosh = mStkACosh;    /* TIW 11-25-94 */
		StkATan = mStkATan;      /* TIW 11-25-94 */
		StkATanh = mStkATanh;    /* TIW 11-25-94 */
		StkCAbs = mStkCAbs;      /* TIW 11-25-94 */
		StkSqrt = mStkSqrt;      /* TIW 11-25-94 */
		StkZero = mStkZero;      /* JCO 12-31-94 */
		StkFloor = mStkFloor;    /* TIW 06-30-96 */
		StkCeil = mStkCeil;      /* TIW 06-30-96 */
		StkTrunc = mStkTrunc;    /* TIW 06-30-96 */
		StkRound = mStkRound;    /* TIW 06-30-96 */
		StkJumpOnTrue  = mStkJumpOnTrue;     /* GGM 02-10-97 */
		StkJumpOnFalse = mStkJumpOnFalse;    /* GGM 02-10-97 */
		StkOne = mStkOne;        /* GGM 10-08-97 */
		break;
	case L_MATH:
		Delta16 = g_bit_shift - 16;
		ShiftBack = 32 - g_bit_shift; /* TW 06-18-90 */
		StkAdd = lStkAdd;
		StkSub = lStkSub;
		StkNeg = lStkNeg;
		StkMul = lStkMul;
		StkSin = lStkSin;
		StkSinh = lStkSinh;
		StkLT = lStkLT;
		StkLTE = lStkLTE;
		StkMod = (g_save_release > 1826) ? lStkMod : lStkModOld;
		StkSqr = lStkSqr;
		StkCos = lStkCos;
		StkCosh = lStkCosh;
		StkLog = lStkLog;
		StkExp = lStkExp;
		StkPwr = lStkPwr;
		StkDiv = lStkDiv;
		StkAbs = lStkAbs;
		StkReal = lStkReal;
		StkImag = lStkImag;
		StkConj = lStkConj;
		StkTrig0 = g_trig0_l;   /* TIW 03-30-91 */
		StkTrig1 = g_trig1_l;   /* TIW 03-30-91 */
		StkTrig2 = g_trig2_l;   /* TIW 03-30-91 */
		StkTrig3 = g_trig3_l;   /* TIW 03-30-91 */
		StkFlip = lStkFlip;
		StkTan  = lStkTan;   /* TIW 04-22-91 */
		StkTanh  = lStkTanh; /* TIW 04-22-91 */
		StkCoTan  = lStkCoTan;   /* TIW 04-24-91 */
		StkCoTanh  = lStkCoTanh; /* TIW 04-24-91 */
		StkCosXX = lStkCosXX;    /* PB  04-28-91 */
		StkGT  = lStkGT;         /* MCP 11-3-91 */
		StkGTE = lStkGTE;        /* MCP 11-3-91 */
		StkEQ  = lStkEQ;         /* MCP 11-3-91 */
		StkNE  = lStkNE;         /* MCP 11-3-91 */
		StkAND = lStkAND;        /* MCP 11-3-91 */
		StkOR  = lStkOR ;        /* MCP 11-3-91 */
		StkSRand = lStkSRand;    /* MCP 11-21-91 */
		StkASin = lStkASin;      /* TIW 11-25-94 */
		StkACos = lStkACos;      /* TIW 11-25-94 */
		StkACosh = lStkACosh;    /* TIW 11-25-94 */
		StkATan = lStkATan;      /* TIW 11-25-94 */
		StkATanh = lStkATanh;    /* TIW 11-25-94 */
		StkCAbs = lStkCAbs;      /* TIW 11-25-94 */
		StkSqrt = lStkSqrt;      /* TIW 11-25-94 */
		StkZero = lStkZero;      /* JCO 12-31-94 */
		StkFloor = lStkFloor;    /* TIW 06-30-96 */
		StkCeil = lStkCeil;      /* TIW 06-30-96 */
		StkTrunc = lStkTrunc;    /* TIW 06-30-96 */
		StkRound = lStkRound;    /* TIW 06-30-96 */
		StkJumpOnTrue  = lStkJumpOnTrue;     /* GGM 02-10-97 */
		StkJumpOnFalse = lStkJumpOnFalse;    /* GGM 02-10-97 */
		StkOne = lStkOne;        /* GGM 10-08-97 */
		break;
#endif
	}
	g_max_fn = 0;   /* TIW 03-30-91 */
	for (g_parser_vsp = 0; g_parser_vsp < sizeof(Constants) / sizeof(char*); g_parser_vsp++)
	{
		v[g_parser_vsp].s = Constants[g_parser_vsp];
		v[g_parser_vsp].len = (int) strlen(Constants[g_parser_vsp]);
	}
	convert_center_mag(&Xctr, &Yctr, &Magnification, &Xmagfactor, &Rotation, &Skew);
	const_pi = atan(1.0)*4;
	const_e  = exp(1.0);
	v[7].a.d.x = v[7].a.d.y = 0.0;
	v[11].a.d.x = (double)g_x_dots;
	v[11].a.d.y = (double)g_y_dots;
	v[12].a.d.x = (double)g_max_iteration;
	v[12].a.d.y = 0;
	v[13].a.d.x = (double)g_is_mand;
	v[13].a.d.y = 0;
	v[14].a.d.x = Xctr;
	v[14].a.d.y = Yctr;
	v[15].a.d.x = (double)Magnification;
	v[15].a.d.y = Xmagfactor;
	v[16].a.d.x = Rotation;
	v[16].a.d.y = Skew;

	switch (MathType)
	{
	case D_MATH:
		v[1].a.d.x = g_parameters[0];
		v[1].a.d.y = g_parameters[1];
		v[2].a.d.x = g_parameters[2];
		v[2].a.d.y = g_parameters[3];
		v[5].a.d.x = const_pi;
		v[5].a.d.y = 0.0;
		v[6].a.d.x = const_e;
		v[6].a.d.y = 0.0;
		v[8].a.d.x = g_parameters[4];
		v[8].a.d.y = g_parameters[5];
		v[17].a.d.x = g_parameters[6];
		v[17].a.d.y = g_parameters[7];
		v[18].a.d.x = g_parameters[8];
		v[18].a.d.y = g_parameters[9];
		break;
#if !defined(XFRACT)
	case M_MATH:
		v[1].a.m.x = *d2MP(g_parameters[0]);
		v[1].a.m.y = *d2MP(g_parameters[1]);
		v[2].a.m.x = *d2MP(g_parameters[2]);
		v[2].a.m.y = *d2MP(g_parameters[3]);
		v[5].a.m.x = *d2MP(const_pi);
		v[5].a.m.y = *d2MP(0.0);
		v[6].a.m.x = *d2MP(const_e);
		v[6].a.m.y = *d2MP(0.0);
		v[8].a.m.x = *d2MP(g_parameters[4]);
		v[8].a.m.y = *d2MP(g_parameters[5]);
		v[11].a.m  = cmplx2MPC(v[11].a.d);
		v[12].a.m  = cmplx2MPC(v[12].a.d);
		v[13].a.m  = cmplx2MPC(v[13].a.d);
		v[14].a.m  = cmplx2MPC(v[14].a.d);
		v[15].a.m  = cmplx2MPC(v[15].a.d);
		v[16].a.m  = cmplx2MPC(v[16].a.d);
		v[17].a.m.x = *d2MP(g_parameters[6]);
		v[17].a.m.y = *d2MP(g_parameters[7]);
		v[18].a.m.x = *d2MP(g_parameters[8]);
		v[18].a.m.y = *d2MP(g_parameters[9]);
		break;
	case L_MATH:
		v[1].a.l.x = (long)(g_parameters[0]*fg);
		v[1].a.l.y = (long)(g_parameters[1]*fg);
		v[2].a.l.x = (long)(g_parameters[2]*fg);
		v[2].a.l.y = (long)(g_parameters[3]*fg);
		v[5].a.l.x = (long)(const_pi*fg);
		v[5].a.l.y = 0L;
		v[6].a.l.x = (long)(const_e*fg);
		v[6].a.l.y = 0L;
		v[8].a.l.x = (long)(g_parameters[4]*fg);
		v[8].a.l.y = (long)(g_parameters[5]*fg);
		v[11].a.l.x = g_x_dots;
		v[11].a.l.x <<= g_bit_shift;
		v[11].a.l.y = g_y_dots;
		v[11].a.l.y <<= g_bit_shift;
		v[12].a.l.x = g_max_iteration;
		v[12].a.l.x <<= g_bit_shift;
		v[12].a.l.y = 0L;
		v[13].a.l.x = g_is_mand;
		v[13].a.l.x <<= g_bit_shift;
		v[13].a.l.y = 0L;
		v[14].a.l.x = (long)(v[14].a.d.x*fg);
		v[14].a.l.y = (long)(v[14].a.d.y*fg);
		v[15].a.l.x = (long)(v[15].a.d.x*fg);
		v[15].a.l.y = (long)(v[15].a.d.y*fg);
		v[16].a.l.x = (long)(v[16].a.d.x*fg);
		v[16].a.l.y = (long)(v[16].a.d.y*fg);
		v[17].a.l.x = (long)(g_parameters[6]*fg);
		v[17].a.l.y = (long)(g_parameters[7]*fg);
		v[18].a.l.x = (long)(g_parameters[8]*fg);
		v[18].a.l.y = (long)(g_parameters[9]*fg);
		break;
#endif
	}

	g_last_init_op = paren = OpPtr = g_lod_ptr = g_store_ptr = g_posp = 0;
	ExpectingArg = 1;
	for (n = 0; Str[n]; n++)
	{
		if (!Str[n])
		{
			break;
		}
		InitN = n;
		switch (Str[n])
		{
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			break;
		case '(':
			paren++;
			break;
		case ')':
			paren--;
			break;
		case '|':
			if (Str[n + 1] == '|')
			{
				ExpectingArg = 1;
				n++;
				o[g_posp].f = StkOR;
				o[g_posp++].p = 7 - (paren + Equals)*15;
			}
			else if (ModFlag == paren-1)
			{
				paren--;
				ModFlag = Mod[--mdstk];
			}
			else
			{
				Mod[mdstk++] = ModFlag;
				o[g_posp].f = StkMod;
				o[g_posp++].p = 2 - (paren + Equals)*15;
				ModFlag = paren++;
			}
			break;
		case ',':
		case ';':
			if (!ExpectingArg)
			{
				ExpectingArg = 1;
				o[g_posp].f = (void(*)())0;
				o[g_posp++].p = 15;
				o[g_posp].f = StkClr;
				o[g_posp++].p = -30000;
				Equals = paren = 0;
			}
			break;
		case ':':
			ExpectingArg = 1;
			o[g_posp].f = (void(*)())0;
			o[g_posp++].p = 15;
			o[g_posp].f = EndInit;
			o[g_posp++].p = -30000;
			Equals = paren = 0;
			g_last_init_op = 10000;
			break;
		case '+':
			ExpectingArg = 1;
			o[g_posp].f = StkAdd;
			o[g_posp++].p = 4 - (paren + Equals)*15;
			break;
		case '-':
			if (ExpectingArg)
			{
				o[g_posp].f = StkNeg;
				o[g_posp++].p = 2 - (paren + Equals)*15;
			}
			else
			{
				o[g_posp].f = StkSub;
				o[g_posp++].p = 4 - (paren + Equals)*15;
				ExpectingArg = 1;
			}
			break;
		case '&':
			ExpectingArg = 1;
			n++;
			o[g_posp].f = StkAND;
			o[g_posp++].p = 7 - (paren + Equals)*15;
			break;
		case '!':
			ExpectingArg = 1;
			n++;
			o[g_posp].f = StkNE;
			o[g_posp++].p = 6 - (paren + Equals)*15;
			break;
		case '<':
			ExpectingArg = 1;
			if (Str[n + 1] == '=')
			{
				n++;
				o[g_posp].f = StkLTE;
			}
			else
			{
				o[g_posp].f = StkLT;
			}
			o[g_posp++].p = 6 - (paren + Equals)*15;
			break;
		case '>':
			ExpectingArg = 1;
			if (Str[n + 1] == '=')
			{
				n++;
				o[g_posp].f = StkGTE;
			}
			else
			{
				o[g_posp].f = StkGT;
			}
			o[g_posp++].p = 6 - (paren + Equals)*15;
			break;
		case '*':
			ExpectingArg = 1;
			o[g_posp].f = StkMul;
			o[g_posp++].p = 3 - (paren + Equals)*15;
			break;
		case '/':
			ExpectingArg = 1;
			o[g_posp].f = StkDiv;
			o[g_posp++].p = 3 - (paren + Equals)*15;
			break;
		case '^':
			ExpectingArg = 1;
			o[g_posp].f = StkPwr;
			o[g_posp++].p = 2 - (paren + Equals)*15;
			break;
		case '=':
			ExpectingArg = 1;
			if (Str[n + 1] == '=')
			{
				n++;
				o[g_posp].f = StkEQ;
				o[g_posp++].p = 6 - (paren + Equals)*15;
			}
			else
			{
				o[g_posp-1].f = StkSto;
				o[g_posp-1].p = 5 - (paren + Equals)*15;
				Store[g_store_ptr++] = Load[--g_lod_ptr];
				Equals++;
			}
			break;
		default:
			while (isalnum(Str[n + 1]) || Str[n + 1] == '.' || Str[n + 1] == '_')
			{
				n++;
			}
			Len = (n + 1)-InitN;
			ExpectingArg = 0;
			jumptype = isjump(&Str[InitN], Len);
			if (jumptype != 0)
			{
				uses_jump = 1;
				switch (jumptype)
				{
				case 1:                      /* if */
					ExpectingArg = 1;
					jump_control[jump_index++].type = 1;
					o[g_posp].f = StkJumpOnFalse;
					o[g_posp++].p = 1;
					break;
				case 2:                     /* elseif */
					ExpectingArg = 1;
					jump_control[jump_index++].type = 2;
					jump_control[jump_index++].type = 2;
					o[g_posp].f = StkJump;
					o[g_posp++].p = 1;
					o[g_posp].f = (void(*)())0;
					o[g_posp++].p = 15;
					o[g_posp].f = StkClr;
					o[g_posp++].p = -30000;
					o[g_posp].f = StkJumpOnFalse;
					o[g_posp++].p = 1;
					break;
				case 3:                     /* else */
					jump_control[jump_index++].type = 3;
					o[g_posp].f = StkJump;
					o[g_posp++].p = 1;
					break;
				case 4: /* endif */
					jump_control[jump_index++].type = 4;
					o[g_posp].f = StkJumpLabel;
					o[g_posp++].p = 1;
					break;
				default:
					break;
				}
			}
			else
			{
				o[g_posp].f = is_function(&Str[InitN], Len);
				if (o[g_posp].f != not_a_function)
				{
					o[g_posp++].p = 1 - (paren + Equals)*15;
					ExpectingArg = 1;
				}
				else
				{
					c = is_constant(&Str[InitN], Len);
					Load[g_lod_ptr++] = &(c->a);
					o[g_posp].f = StkLod;
					o[g_posp++].p = 1 - (paren + Equals)*15;
					n = InitN + c->len - 1;
				}
			}
			break;
		}
	}
	o[g_posp].f = (void(*)())0;
	o[g_posp++].p = 16;
	NextOp = 0;
	g_last_op = g_posp;
	while (NextOp < g_posp)
	{
		if (o[NextOp].f)
		{
			RecSortPrec();
		}
		else
		{
			NextOp++;
			g_last_op--;
		}
	}
	return 0;
}


int Formula()
{
	if (g_formula_name[0] == 0 || g_overflow)
	{
		return 1;
	}

	g_lod_ptr = InitLodPtr;
	g_store_ptr = InitStoPtr;
	OpPtr = InitOpPtr;
	jump_index = InitJumpIndex;
	/* Set the random number, MCP 11-21-91 */
	if (SetRandom || Randomized)
	{
		switch (MathType)
		{
		case D_MATH:
			dRandom();
			break;
#if !defined(XFRACT)
		case L_MATH:
			lRandom();
			break;
		case M_MATH:
			mRandom();
#endif
		}
	}

	Arg1 = &s[0];
	Arg2 = Arg1-1;
	while (OpPtr < (int)g_last_op)
	{
		f[OpPtr]();
		OpPtr++;
#ifdef WATCH_MP
		x1 = *MP2d(Arg1->m.x);
		y1 = *MP2d(Arg1->m.y);
		x2 = *MP2d(Arg2->m.x);
		y2 = *MP2d(Arg2->m.y);
#endif
	}

	switch (MathType)
	{
	case D_MATH:
		g_old_z = g_new_z = v[3].a.d;
		return Arg1->d.x == 0.0;
#if !defined(XFRACT)
	case M_MATH:
		g_old_z = g_new_z = MPC2cmplx(v[3].a.m);
		return Arg1->m.x.Exp == 0 && Arg1->m.x.Mant == 0;
	case L_MATH:
		g_old_z_l = g_new_z_l = v[3].a.l;
		if (g_overflow)
		{
			return 1;
		}
		return Arg1->l.x == 0L;
#endif
	}
	return 1;
}

int form_per_pixel()
{
	if (g_formula_name[0] == 0)
	{
		return 1;
	}
	g_overflow = g_lod_ptr = g_store_ptr = OpPtr = jump_index = 0;
	Arg1 = &s[0];
	Arg2 = Arg1;
	Arg2--;


	v[10].a.d.x = (double)g_col;
	v[10].a.d.y = (double)g_row;

	switch (MathType)
	{
	case D_MATH:
		v[9].a.d.x = ((g_row + g_col) & 1) ? 1.0 : 0.0;
		v[9].a.d.y = 0.0;
		break;


#if !defined(XFRACT)
	case M_MATH:
		if ((g_row + g_col) & 1)
		{
			v[9].a.m = g_one_mpc;
		}
		else
		{
			v[9].a.m.x.Mant = v[9].a.m.x.Exp = 0;
			v[9].a.m.y.Mant = v[9].a.m.y.Exp = 0;
		}
		v[10].a.m = cmplx2MPC(v[10].a.d);
		break;
	case L_MATH:
		v[9].a.l.x = (long) (((g_row + g_col) & 1)*fg);
		v[9].a.l.y = 0L;
		v[10].a.l.x = g_col;
		v[10].a.l.x <<= g_bit_shift;
		v[10].a.l.y = g_row;
		v[10].a.l.y <<= g_bit_shift;
		break;
#endif
	}

	/* TW started additions for inversion support here 4/17/94 */
	if (g_invert)
	{
		invert_z(&g_old_z);
		switch (MathType)
		{
		case D_MATH:
			v[0].a.d.x = g_old_z.x;
			v[0].a.d.y = g_old_z.y;
			break;
#if !defined(XFRACT)
		case M_MATH:
			v[0].a.m.x = *d2MP(g_old_z.x);
			v[0].a.m.y = *d2MP(g_old_z.y);
			break;
		case L_MATH:
			/* watch out for overflow */
			if (sqr(g_old_z.x) + sqr(g_old_z.y) >= 127)
			{
				g_old_z.x = 8;  /* value to bail out in one iteration */
				g_old_z.y = 8;
			}
			/* convert to fudged longs */
			v[0].a.l.x = (long)(g_old_z.x*fg);
			v[0].a.l.y = (long)(g_old_z.y*fg);
			break;
#endif
		}
	}
	else
	{
		/* TW end of inversion support changes here 4/17/94 */
		switch (MathType)
		{
		case D_MATH:
			v[0].a.d.x = g_dx_pixel();
			v[0].a.d.y = g_dy_pixel();
			break;
#if !defined(XFRACT)
		case M_MATH:
			v[0].a.m.x = *d2MP(g_dx_pixel());
			v[0].a.m.y = *d2MP(g_dy_pixel());
			break;
		case L_MATH:
			v[0].a.l.x = g_lx_pixel();
			v[0].a.l.y = g_ly_pixel();
			break;
#endif
		}
	}

	if (g_last_init_op)
	{
		g_last_init_op = g_last_op;
	}
	while (OpPtr < g_last_init_op)
	{
		f[OpPtr]();
		OpPtr++;
	}
	InitLodPtr = g_lod_ptr;
	InitStoPtr = g_store_ptr;
	InitOpPtr = OpPtr;
	/* Set old variable for orbits TIW 12-18-93 */
	switch (MathType)
	{
	case D_MATH:
		g_old_z = v[3].a.d;
		break;
#if !defined(XFRACT)
	case M_MATH:
		g_old_z = MPC2cmplx(v[3].a.m);
		break;
	case L_MATH:
		g_old_z_l = v[3].a.l;
		break;
#endif
	}

	return g_overflow ? 0 : 1;
}

int fill_if_group(int endif_index, JUMP_PTRS_ST* jump_data)
{
	int i   = endif_index;
	int ljp = endif_index; /* ljp means "last jump processed" */
	while (i > 0)
	{
		i--;
		switch (jump_control[i].type)
		{
		case 1:    /*if (); this concludes processing of this group*/
			jump_control[i].ptrs = jump_data[ljp];
			jump_control[i].DestJumpIndex = ljp + 1;
			return i;
		case 2:    /*elseif* (2 jumps, the else and the if*/
				/* first, the "if" part */
			jump_control[i].ptrs = jump_data[ljp];
			jump_control[i].DestJumpIndex = ljp + 1;

				/* then, the else part */
			i--; /*fall through to "else" is intentional*/
		case 3:
			jump_control[i].ptrs = jump_data[endif_index];
			jump_control[i].DestJumpIndex = endif_index + 1;
			ljp = i;
			break;
		case 4:    /*endif*/
				i = fill_if_group(i, jump_data);
			break;
		default:
			break;
		}
	}
	return -1; /* should never get here */
}

int fill_jump_struct()
{ /* Completes all entries in jump structure. Returns 1 on error) */
  /* On entry, jump_index is the number of jump functions in the formula*/
	int i = 0;
	int loadcount = 0;
	int storecount = 0;
	int checkforelse = 0;
	void (*JumpFunc)() = NULL;
	int find_new_func = 1;

	JUMP_PTRS_ST jump_data[MAX_JUMPS];

	for (OpPtr = 0; OpPtr < (int) g_last_op; OpPtr++)
	{
		if (find_new_func)
		{
			switch (jump_control[i].type)
			{
			case 1:
				JumpFunc = StkJumpOnFalse;
				break;
			case 2:
				checkforelse = !checkforelse;
				JumpFunc = checkforelse ? StkJump : StkJumpOnFalse;
				break;
			case 3:
				JumpFunc = StkJump;
				break;
			case 4:
				JumpFunc = StkJumpLabel;
				break;
			default:
				break;
			}
			find_new_func = 0;
		}
		if (*(f[OpPtr]) == StkLod)
		{
			loadcount++;
		}
		else if (*(f[OpPtr]) == StkSto)
		{
			storecount++;
		}
		else if (*(f[OpPtr]) == JumpFunc)
		{
			jump_data[i].JumpOpPtr = OpPtr;
			jump_data[i].JumpLodPtr = loadcount;
			jump_data[i].JumpStoPtr = storecount;
			i++;
			find_new_func = 1;
		}
	}

	/* Following for safety only; all should always be false */
	if (i != jump_index || jump_control[i - 1].type != 4
		|| jump_control[0].type != 1)
	{
		return 1;
	}

	while (i > 0)
	{
		i = fill_if_group(i-1, jump_data);
	}
	return i < 0 ? 1 : 0;
}

static char *FormStr;

int frmgetchar (FILE *openfile)
{
	int c;
	int done = 0;
	int linewrap = 0;
	while (!done)
	{
		c = getc(openfile);
		switch (c)
		{
		case '\r': case ' ' : case '\t' :
			break;
		case '\\':
			linewrap = 1;
			break;
		case ';' :
			do
			{
				c = getc(openfile);
			}
			while (c != '\n' && c != EOF && c != '\032');
			if (c == EOF || c == '\032')
			{
				done = 1;
			}
		case '\n' :
			if (!linewrap)
			{
				done = 1;
			}
			linewrap = 0;
			break;
		default:
			done = 1;
			break;
		}
	}
	return tolower(c);
}

/* This function also gets flow control info */

void getfuncinfo(struct token_st *tok)
{
	int i;
	for (i = 0; i < sizeof(FnctList)/ sizeof(struct FNCT_LIST); i++)
	{
		if (!strcmp(FnctList[i].s, tok->token_str))
		{
			tok->token_id = i;
			tok->token_type = (i >= 11 && i <= 14) ? PARAM_FUNCTION : FUNCTION;
			return;
		}
	}

	for (i = 0; i < 4; i++)  /*pick up flow control*/
	{
		if (!strcmp(JumpList[i], tok->token_str))
		{
			tok->token_type = FLOW_CONTROL;
			tok->token_id   = i + 1;
			return;
		}
	}
	tok->token_type = NOT_A_TOKEN;
	tok->token_id   = UNDEFINED_FUNCTION;
	return;
}

void getvarinfo(struct token_st *tok)
{
	int i;

	for (i = 0; i < sizeof(Constants) / sizeof(char*); i++)
	{
		if (!strcmp(Constants[i], tok->token_str))
		{
			tok->token_id = i;
			switch (i)
			{
			case 1: case 2: case 8: case 13: case 17: case 18:
				tok->token_type = PARAM_VARIABLE;
				break;
			default:
				tok->token_type = PREDEFINED_VARIABLE;
				break;
			}
			return;
		}
	}
	tok->token_type = USER_NAMED_VARIABLE;
	tok->token_id   = 0;
}

/* fills in token structure where numeric constant is indicated */
/* Note - this function will be called twice to fill in the components
		of a complex constant. See is_complex_constant() below. */

/* returns 1 on success, 0 on NOT_A_TOKEN */

int frmgetconstant(FILE *openfile, struct token_st *tok)
{
	int c;
	int i = 1;
	int getting_base = 1;
	long filepos = ftell(openfile);
	int got_decimal_already = 0;
	int done = 0;
	tok->token_const.x = 0.0;          /*initialize values to 0*/
	tok->token_const.y = 0.0;
	if (tok->token_str[0] == '.')
	{
		got_decimal_already = 1;
	}
	while (!done)
	{
		c = frmgetchar(openfile);
		switch (c)
		{
		case EOF: case '\032':
			tok->token_str[i] = (char) 0;
			tok->token_type = NOT_A_TOKEN;
			tok->token_id   = END_OF_FILE;
			return 0;
		CASE_NUM:
			tok->token_str[i++] = (char) c;
			filepos = ftell(openfile);
			break;
		case '.':
			if (got_decimal_already || !getting_base)
			{
				tok->token_str[i++] = (char) c;
				tok->token_str[i++] = (char) 0;
				tok->token_type = NOT_A_TOKEN;
				tok->token_id = ILL_FORMED_CONSTANT;
				return 0;
			}
			else
			{
				tok->token_str[i++] = (char) c;
				got_decimal_already = 1;
				filepos = ftell(openfile);
			}
			break;
		default :
			if (c == 'e' && getting_base && (isdigit(tok->token_str[i-1]) || (tok->token_str[i-1] == '.' && i > 1)))
			{
				tok->token_str[i++] = (char) c;
				getting_base = 0;
				got_decimal_already = 0;
				filepos = ftell(openfile);
				c = frmgetchar(openfile);
				if (c == '-' || c == '+')
				{
					tok->token_str[i++] = (char) c;
					filepos = ftell(openfile);
				}
				else
				{
					fseek(openfile, filepos, SEEK_SET);
				}
			}
			else if (isalpha(c) || c == '_')
			{
				tok->token_str[i++] = (char) c;
				tok->token_str[i++] = (char) 0;
				tok->token_type = NOT_A_TOKEN;
				tok->token_id = ILL_FORMED_CONSTANT;
				return 0;
			}
			else if (tok->token_str[i-1] == 'e' || (tok->token_str[i-1] == '.' && i == 1))
			{
				tok->token_str[i++] = (char) c;
				tok->token_str[i++] = (char) 0;
				tok->token_type = NOT_A_TOKEN;
				tok->token_id = ILL_FORMED_CONSTANT;
				return 0;
			}
			else
			{
				fseek(openfile, filepos, SEEK_SET);
				tok->token_str[i++] = (char) 0;
				done = 1;
			}
			break;
		}
		if (i == 33 && tok->token_str[32])
		{
			tok->token_str[33] = (char) 0;
			tok->token_type = NOT_A_TOKEN;
			tok->token_id = TOKEN_TOO_LONG;
			return 0;
		}
	}    /* end of while loop. Now fill in the value */
	tok->token_const.x = atof(tok->token_str);
	tok->token_type = REAL_CONSTANT;
	tok->token_id   = 0;
	return 1;
}

void is_complex_constant(FILE *openfile, struct token_st *tok)
{
	/* should test to make sure tok->token_str[0] == '(' */
	struct token_st temp_tok;
	long filepos;
	int c;
	int sign_value = 1;
	int done = 0;
	int getting_real = 1;
	FILE *debug_token = NULL;
	tok->token_str[1] = (char) 0;  /* so we can concatenate later */

	filepos = ftell(openfile);
	if (DEBUGFLAG_DISK_MESSAGES == g_debug_flag)
	{
		debug_token = fopen("frmconst.txt", "at");
	}

	while (!done)
	{
		c = frmgetchar(openfile);
		switch (c)
		{
		CASE_NUM : case '.':
			if (debug_token != NULL)
			{
				fprintf(debug_token,  "Set temp_tok.token_str[0] to %c\n", c);
			}
			temp_tok.token_str[0] = (char) c;
			break;
		case '-' :
			if (debug_token != NULL)
			{
				fprintf(debug_token,  "First char is a minus\n");
			}
			sign_value = -1;
			c = frmgetchar(openfile);
			if (c == '.' || isdigit(c))
			{
				if (debug_token != NULL)
				{
					fprintf(debug_token,  "Set temp_tok.token_str[0] to %c\n", c);
				}
				temp_tok.token_str[0] = (char) c;
			}
			else
			{
				if (debug_token != NULL)
				{
					fprintf(debug_token,  "First char not a . or NUM\n");
				}
				done = 1;
			}
			break;
		default:
			if (debug_token != NULL)
			{
				fprintf(debug_token,  "First char not a . or NUM\n");
			}
			done = 1;
			break;
		}
		if (debug_token != NULL)
		{
				fprintf(debug_token,  "Calling frmgetconstant unless done is 1; done is %d\n", done);
		}
		if (!done && frmgetconstant (openfile, &temp_tok))
		{
			c = frmgetchar(openfile);
			if (debug_token != NULL)
			{
				fprintf(debug_token, "frmgetconstant returned 1; next token is %c\n", c);
			}
			if (getting_real && c == ',')  /*we have the real part now*/
			{
				if (sign_value == -1)
				{
					strcat(tok->token_str, "-");
				}
				strcat(tok->token_str, temp_tok.token_str);
				strcat(tok->token_str, ",");
				tok->token_const.x = temp_tok.token_const.x*sign_value;
				getting_real = 0;
				sign_value = 1;
			}
			else if (!getting_real && c == ')')  /* we have the complex part */
			{
				if (sign_value == -1)
				{
					strcat(tok->token_str, "-");
				}
				strcat(tok->token_str, temp_tok.token_str);
				strcat(tok->token_str, ")");
				tok->token_const.y = temp_tok.token_const.x*sign_value;
				tok->token_type = tok->token_const.y ? COMPLEX_CONSTANT : REAL_CONSTANT;
				tok->token_id   = 0;
				if (debug_token != NULL)
				{
					fprintf(debug_token,  "Exiting with type set to %d\n", tok->token_const.y ? COMPLEX_CONSTANT : REAL_CONSTANT);
					fclose (debug_token);
				}
				return;
			}
			else
			{
				done = 1;
			}
		}
		else
		{
			done = 1;
		}
	}
	fseek (openfile, filepos, SEEK_SET);
	tok->token_str[1] = (char) 0;
	tok->token_const.y = tok->token_const.x = 0.0;
	tok->token_type = PARENS;
	tok->token_id = OPEN_PARENS;
	if (debug_token != NULL)
	{
		fprintf(debug_token,  "Exiting with ID set to OPEN_PARENS\n");
		fclose (debug_token);
	}
	return;
}

int frmgetalpha(FILE *openfile, struct token_st *tok)
{
	int c;
	int i = 1;
	int var_name_too_long = 0;
	long filepos;
	long last_filepos = ftell(openfile);
	while ((c = frmgetchar(openfile)) != EOF && c != '\032')
	{
		filepos = ftell(openfile);
		switch (c)
		{
		CASE_ALPHA: CASE_NUM: case '_':
			if (i < 79)
			{
				tok->token_str[i++] = (char) c;
			}
			else
			{
				tok->token_str[i] = (char) 0;
			}
			if (i == 33)
			{
				var_name_too_long = 1;
			}
			last_filepos = filepos;
			break;
		default:
			if (c == '.')  /*illegal character in variable or func name*/
			{
				tok->token_type = NOT_A_TOKEN;
				tok->token_id   = ILLEGAL_VARIABLE_NAME;
				tok->token_str[i++] = '.';
				tok->token_str[i] = (char) 0;
				return 0;
			}
			else if (var_name_too_long)
			{
				tok->token_type = NOT_A_TOKEN;
				tok->token_id   = TOKEN_TOO_LONG;
				tok->token_str[i] = (char) 0;
				fseek(openfile, last_filepos, SEEK_SET);
				return 0;
			}
			tok->token_str[i] = (char) 0;
			fseek(openfile, last_filepos, SEEK_SET);
			getfuncinfo(tok);
			if (c == '(')  /*getfuncinfo() correctly filled structure*/
			{
				if (tok->token_type == NOT_A_TOKEN)
				{
					return 0;
				}
				else if (tok->token_type == FLOW_CONTROL && (tok->token_id == 3 || tok->token_id == 4))
				{
					tok->token_type = NOT_A_TOKEN;
					tok->token_id   = JUMP_WITH_ILLEGAL_CHAR;
					return 0;
				}
				else
				{
					return 1;
				}
			}
			/*can't use function names as variables*/
			else if (tok->token_type == FUNCTION || tok->token_type == PARAM_FUNCTION)
			{
				tok->token_type = NOT_A_TOKEN;
				tok->token_id   = FUNC_USED_AS_VAR;
				return 0;
			}
			else if (tok->token_type == FLOW_CONTROL && (tok->token_id == 1 || tok->token_id == 2))
			{
				tok->token_type = NOT_A_TOKEN;
				tok->token_id   = JUMP_MISSING_BOOLEAN;
				return 0;
			}
			else if (tok->token_type == FLOW_CONTROL && (tok->token_id == 3 || tok->token_id == 4))
			{
				if (c == ',' || c == '\n' || c == ':')
				{
					return 1;
				}
				else
				{
					tok->token_type = NOT_A_TOKEN;
					tok->token_id   = JUMP_WITH_ILLEGAL_CHAR;
					return 0;
				}
			}
			else
			{
				getvarinfo(tok);
				return 1;
			}
		}
	}
	tok->token_str[0] = (char) 0;
	tok->token_type = NOT_A_TOKEN;
	tok->token_id   = END_OF_FILE;
	return 0;
}

void frm_get_eos(FILE *openfile, struct token_st *this_token)
{
	long last_filepos = ftell(openfile);
	int c;

	for (c = frmgetchar(openfile); (c == '\n' || c == ',' || c == ':'); c = frmgetchar(openfile))
	{
		if (c == ':')
		{
			this_token->token_str[0] = ':';
		}
		last_filepos = ftell(openfile);
	}
	if (c == '}')
	{
		this_token->token_str[0] = '}';
		this_token->token_type = END_OF_FORMULA;
		this_token->token_id   = 0;
	}
	else
	{
		fseek(openfile, last_filepos, SEEK_SET);
		if (this_token->token_str[0] == '\n')
		{
			this_token->token_str[0] = ',';
		}
	}
}

/*frmgettoken fills token structure; returns 1 on success and 0 on
  NOT_A_TOKEN and END_OF_FORMULA
*/

int frmgettoken(FILE *openfile, struct token_st *this_token)
{
	int c;
	int i = 1;
	long filepos;

	c = frmgetchar(openfile);
	switch (c)
	{
	CASE_NUM: case '.':
		this_token->token_str[0] = (char) c;
		return frmgetconstant(openfile, this_token);
	CASE_ALPHA: case '_':
		this_token->token_str[0] = (char) c;
		return frmgetalpha(openfile, this_token);
	CASE_TERMINATOR:
		this_token->token_type = OPERATOR; /* this may be changed below */
		this_token->token_str[0] = (char) c;
		filepos = ftell(openfile);
		if (c == '<' || c == '>' || c == '=')
		{
			c = frmgetchar(openfile);
			if (c == '=')
			{
				this_token->token_str[i++] = (char) c;
			}
			else
			{
				fseek(openfile, filepos, SEEK_SET);
			}
		}
		else if (c == '!')
		{
			c = frmgetchar(openfile);
			if (c == '=')
			{
				this_token->token_str[i++] = (char) c;
			}
			else
			{
				fseek(openfile, filepos, SEEK_SET);
				this_token->token_str[1] = (char) 0;
				this_token->token_type = NOT_A_TOKEN;
				this_token->token_id = ILLEGAL_OPERATOR;
				return 0;
			}
		}
		else if (c == '|')
		{
			c = frmgetchar(openfile);
			if (c == '|')
			{
				this_token->token_str[i++] = (char) c;
			}
			else
			{
				fseek(openfile, filepos, SEEK_SET);
			}
		}
		else if (c == '&')
		{
			c = frmgetchar(openfile);
			if (c == '&')
			{
				this_token->token_str[i++] = (char) c;
			}
			else
			{
				fseek(openfile, filepos, SEEK_SET);
				this_token->token_str[1] = (char) 0;
				this_token->token_type = NOT_A_TOKEN;
				this_token->token_id = ILLEGAL_OPERATOR;
				return 0;
			}
		}
		else if (this_token->token_str[0] == '}')
		{
			this_token->token_type = END_OF_FORMULA;
			this_token->token_id   = 0;
		}
		else if (this_token->token_str[0] == '\n'
			|| this_token->token_str[0] == ','
			|| this_token->token_str[0] == ':')
		{
			frm_get_eos(openfile, this_token);
		}
		else if (this_token->token_str[0] == ')')
		{
			this_token->token_type = PARENS;
			this_token->token_id = CLOSE_PARENS;
		}
		else if (this_token->token_str[0] == '(')
		{
			/* the following function will set token_type to PARENS and
				token_id to OPEN_PARENS if this is not the start of a
				complex constant */
			is_complex_constant(openfile, this_token);
				return 1;
		}
		this_token->token_str[i] = (char) 0;
		if (this_token->token_type == OPERATOR)
		{
			for (i = 0; i < sizeof(OPList)/sizeof(OPList[0]); i++)
			{
				if (!strcmp(OPList[i], this_token->token_str))
				{
					this_token->token_id = i;
				}
			}
		}
		return this_token->token_str[0] == '}' ? 0 : 1;
	case EOF: case '\032':
		this_token->token_str[0] = (char) 0;
		this_token->token_type = NOT_A_TOKEN;
		this_token->token_id = END_OF_FILE;
		return 0;
	default:
		this_token->token_str[0] = (char) c;
		this_token->token_str[1] = (char) 0;
		this_token->token_type = NOT_A_TOKEN;
		this_token->token_id = ILLEGAL_CHARACTER;
		return 0;
	}
}

int frm_get_param_stuff(char *Name)
{
	FILE *debug_token = NULL;
	int c;
	struct token_st current_token;
	FILE *entry_file = NULL;
	g_uses_p1 = g_uses_p2 = g_uses_p3 = g_uses_is_mand = g_max_fn = 0;
	g_uses_p4 = g_uses_p5 = 0;

	if (g_formula_name[0] == 0)
	{
		return 0;  /*  and don't reset the pointers  */
	}
	if (find_file_item(g_formula_filename, Name, &entry_file, ITEMTYPE_FORMULA))
	{
		stop_message(0, ParseErrs(PE_COULD_NOT_OPEN_FILE_WHERE_FORMULA_LOCATED));
		return 0;
	}
	do
	{
		c = frmgetchar(entry_file);
	}
	while (c != '{' && c != EOF && c != '\032');

	if (c != '{')
	{
		stop_message(0, ParseErrs(PE_UNEXPECTED_EOF));
		fclose(entry_file);
		return 0;
	}

	if (DEBUGFLAG_DISK_MESSAGES == g_debug_flag)
	{
		debug_token = fopen("frmtokens.txt", "at");
		if (debug_token != NULL)
		{
			fprintf(debug_token, "%s\n", Name);
		}
	}
	while (frmgettoken(entry_file, &current_token))
	{
		if (debug_token != NULL)
		{
			fprintf(debug_token, "%s\n", current_token.token_str);
			fprintf(debug_token, "token_type is %d\n", current_token.token_type);
			fprintf(debug_token, "token_id is %d\n", current_token.token_id);
			if (current_token.token_type == REAL_CONSTANT || current_token.token_type == COMPLEX_CONSTANT)
			{
				fprintf(debug_token, "Real value is %f\n", current_token.token_const.x);
				fprintf(debug_token, "Imag value is %f\n", current_token.token_const.y);
			}
			fprintf(debug_token, "\n");
		}
		switch (current_token.token_type)
		{
		case PARAM_VARIABLE:
			if (current_token.token_id == 1)
			{
				g_uses_p1 = 1;
			}
			else if (current_token.token_id == 2)
			{
				g_uses_p2 = 1;
			}
			else if (current_token.token_id == 8)
			{
				g_uses_p3 = 1;
			}
			else if (current_token.token_id == 13)
			{
				g_uses_is_mand = 1;
			}
			else if (current_token.token_id == 17)
			{
				g_uses_p4 = 1;
			}
			else if (current_token.token_id == 18)
			{
				g_uses_p5 = 1;
			}
			break;
		case PARAM_FUNCTION:
			if ((current_token.token_id - 10) > g_max_fn)
			{
				g_max_fn = (char) (current_token.token_id - 10);
			}
			break;
		}
	}
	fclose(entry_file);
	if (debug_token)
	{
		fclose(debug_token);
	}
	if (current_token.token_type != END_OF_FORMULA)
	{
		g_uses_p1 = g_uses_p2 = g_uses_p3 = g_uses_is_mand = g_max_fn = 0;
		g_uses_p4 = g_uses_p5 = 0;
		return 0;
	}
	return 1;
}

/* frm_check_name_and_sym():
	error checking to the open brace on the first line; return 1
	on success, 2 if an invalid symmetry is found, and 0 if errors
	are found which should cause the formula not to be executed
*/

int frm_check_name_and_sym(FILE *open_file, int report_bad_sym)
{
	long filepos = ftell(open_file);
	int c, i, done, at_end_of_name;

	/* first, test name */
	done = at_end_of_name = i = 0;
	while (!done)
	{
		c = getc(open_file);
		switch (c)
		{
		case EOF: case '\032':
			stop_message(0, ParseErrs(PE_UNEXPECTED_EOF));
			return 0;
		case '\r': case '\n':
			stop_message(0, ParseErrs(PE_NO_LEFT_BRACKET_FIRST_LINE));
			return 0;
		case ' ': case '\t':
			at_end_of_name = 1;
			break;
		case '(': case '{':
			done = 1;
			break;
		default :
			if (!at_end_of_name)
			{
				i++;
			}
			break;
		}
	}

	if (i > ITEMNAMELEN)
	{
		int j;
		int k = (int) strlen(ParseErrs(PE_FORMULA_NAME_TOO_LARGE));
		char msgbuf[100];
		strcpy(msgbuf, ParseErrs(PE_FORMULA_NAME_TOO_LARGE));
		strcat(msgbuf, ":\n   ");
		fseek(open_file, filepos, SEEK_SET);
		for (j = 0; j < i && j < 25; j++)
		{
			msgbuf[j + k + 2] = (char) getc(open_file);
		}
		msgbuf[j + k + 2] = (char) 0;
		stop_message(STOPMSG_FIXED_FONT, msgbuf);
		return 0;
	}
		/* get symmetry */
	g_symmetry = 0;
	if (c == '(')
	{
		char sym_buf[20];
		done = i = 0;
		while (!done)
		{
			c = getc(open_file);
			switch (c)
			{
			case EOF: case '\032':
				stop_message(0, ParseErrs(PE_UNEXPECTED_EOF));
				return 0;
			case '\r': case '\n':
				stop_message(STOPMSG_FIXED_FONT, ParseErrs(PE_NO_LEFT_BRACKET_FIRST_LINE));
				return 0;
			case '{':
				stop_message(STOPMSG_FIXED_FONT, ParseErrs(PE_NO_MATCH_RIGHT_PAREN));
				return 0;
			case ' ': case '\t':
				break;
			case ')':
				done = 1;
				break;
			default :
				if (i < 19)
				{
					sym_buf[i++] = (char) toupper(c);
				}
				break;
			}
		}
		sym_buf[i] = (char) 0;
		for (i = 0; SymStr[i].s[0]; i++)
		{
			if (!stricmp(SymStr[i].s, sym_buf))
			{
				g_symmetry = SymStr[i].n;
				break;
			}
		}
		if (SymStr[i].s[0] == (char) 0 && report_bad_sym)
		{
			char *msgbuf = (char *) malloc((int) strlen(ParseErrs(PE_INVALID_SYM_USING_NOSYM))
							+ (int) strlen(sym_buf) + 6);
			strcpy(msgbuf, ParseErrs(PE_INVALID_SYM_USING_NOSYM));
			strcat(msgbuf, ":\n   ");
			strcat(msgbuf, sym_buf);
			stop_message(STOPMSG_FIXED_FONT, msgbuf);
			free(msgbuf);
		}
	}
	if (c != '{')
	{
		done = 0;
		while (!done)
		{
			c = getc(open_file);
			switch (c)
			{
			case EOF: case '\032':
				stop_message(STOPMSG_FIXED_FONT, ParseErrs(PE_UNEXPECTED_EOF));
				return 0;
			case '\r': case '\n':
				stop_message(STOPMSG_FIXED_FONT, ParseErrs(PE_NO_LEFT_BRACKET_FIRST_LINE));
				return 0;
			case '{':
				done = 1;
				break;
			default :
				break;
			}
		}
	}
	return 1;
}


static char *PrepareFormula(FILE *File, int from_prompts1c)
{

	/* GGM 5-23-96: replaces FindFormula(). This function sets the
	symmetry and converts a formula into a string  with no spaces,
	and one comma after each expression except where the ':' is placed
	and except the final expression in the formula. The open file passed
	as an argument is open in "rb" mode and is positioned at the first
	letter of the name of the formula to be prepared. This function
	is called from RunForm() below.
	*/

	FILE *debug_fp = NULL;
	char *FormulaStr;
	struct token_st temp_tok;
	int Done;
	long filepos = ftell(File);

/* char debugmsg[500];
*/

	/*Test for a repeat*/

	if (frm_check_name_and_sym(File, from_prompts1c) == 0)
	{
		fseek(File, filepos, SEEK_SET);
		return NULL;
	}
	if (!frm_prescan(File))
	{
		fseek(File, filepos, SEEK_SET);
		return NULL;
	}

	if (chars_in_formula > 8190)
	{
		fseek(File, filepos, SEEK_SET);
		return NULL;
	}

	if (DEBUGFLAG_DISK_MESSAGES == g_debug_flag)
	{
		debug_fp = fopen("debugfrm.txt", "at");
		if (debug_fp != NULL)
		{
			fprintf(debug_fp, "%s\n", g_formula_name);
			if (g_symmetry != 0)
			{
				fprintf(debug_fp, "%s\n", SymStr[g_symmetry].s);
			}
		}
	}

	FormulaStr = (char *)g_box_x;
	FormulaStr[0] = (char) 0; /* To permit concantenation later */

	Done = 0;

	/*skip opening end-of-lines */
	while (!Done)
	{
		frmgettoken(File, &temp_tok);
		if (temp_tok.token_type == NOT_A_TOKEN)
		{
			stop_message(STOPMSG_FIXED_FONT, "Unexpected token error in PrepareFormula\n");
			fseek(File, filepos, SEEK_SET);
			return NULL;
		}
		else if (temp_tok.token_type == END_OF_FORMULA)
		{
			stop_message(STOPMSG_FIXED_FONT, "Formula has no executable instructions\n");
			fseek(File, filepos, SEEK_SET);
			return NULL;
		}
		if (temp_tok.token_str[0] == ',')
		{
			;
		}
		else
		{
			strcat(FormulaStr, temp_tok.token_str);
			Done = 1;
		}
	}

	Done = 0;
	while (!Done)
	{
		frmgettoken(File, &temp_tok);
		switch (temp_tok.token_type)
		{
		case NOT_A_TOKEN:
			stop_message(STOPMSG_FIXED_FONT, "Unexpected token error in PrepareFormula\n");
			fseek(File, filepos, SEEK_SET);
			return NULL;
		case END_OF_FORMULA:
			Done = 1;
			fseek(File, filepos, SEEK_SET);
			break;
		default:
			strcat(FormulaStr, temp_tok.token_str);
			break;
		}
	}

	if (debug_fp != NULL && FormulaStr != NULL)
	{
		fprintf(debug_fp, "   %s\n", FormulaStr);
	}
	if (debug_fp != NULL)
	{
		fclose(debug_fp);
	}


/* sprintf(debugmsg, "Chars in formula per g_box_x is %u.\n", strlen(FormulaStr));
	stop_message(0, debugmsg);
*/
	return FormulaStr;
}

int BadFormula()
{
	/*  moved from Parsera.Asm by CAE  12 July 1993  */

	/*  this is called when a formula is bad, instead of calling  */
	/*     the normal functions which will produce undefined results  */
	return 1;
}

int RunForm(char *Name, int from_prompts1c)  /*  returns 1 if an error occurred  */
{

	FILE *entry_file = NULL;

	/*  CAE changed fn 12 July 1993 to fix problem when formula not found  */

	/*  first set the pointers so they point to a fn which always returns 1  */
	g_current_fractal_specific->per_pixel = BadFormula;
	g_current_fractal_specific->orbitcalc = BadFormula;

	if (g_formula_name[0] == 0)
	{
		return 1;  /*  and don't reset the pointers  */
	}

	/* TW 5-31-94 add search for FRM files in directory */
	if (find_file_item(g_formula_filename, Name, &entry_file, ITEMTYPE_FORMULA))
	{
		stop_message(0, ParseErrs(PE_COULD_NOT_OPEN_FILE_WHERE_FORMULA_LOCATED));
		return 1;
	}

	FormStr = PrepareFormula(entry_file, from_prompts1c);
	fclose(entry_file);

	if (FormStr)  /*  No errors while making string */
	{
		parser_allocate();  /*  ParseStr() will test if this alloc worked  */
		if (ParseStr(FormStr, 1))
		{
			return 1;   /*  parse failed, don't change fn pointers  */
		}
		else
		{
			if (uses_jump == 1 && fill_jump_struct() == 1)
			{
				stop_message(0, ParseErrs(PE_ERROR_IN_PARSING_JUMP_STATEMENTS));
				return 1;
			}

			/* all parses succeeded so set the pointers back to good functions*/
			g_current_fractal_specific->per_pixel = form_per_pixel;
			g_current_fractal_specific->orbitcalc = Formula;
			return 0;
		}
	}
	else
	{
		return 1;   /* error in making string*/
	}
}


int formula_setup_fp()
{

	int RunFormRes;              /* CAE fp */
	/* TODO: when parsera.c contains assembly equivalents, remove !defined(_WIN32) */
#if !defined(XFRACT) && !defined(_WIN32)
	if (g_fpu > 0)
	{
		MathType = D_MATH;
		/* CAE changed below for fp */
		RunFormRes = !RunForm(g_formula_name, 0); /* RunForm() returns 1 for failure */
		if (RunFormRes && !(g_orbit_save & ORBITSAVE_SOUND) && !Randomized
			&& (g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL))
		{
			return CvtStk(); /* run fast assembler code in parsera.asm */
		}
		return RunFormRes;
	}
	else
	{
		MathType = M_MATH;
		return !RunForm(g_formula_name, 0);
	}
#else
	MathType = D_MATH;
	RunFormRes = !RunForm(g_formula_name, 0); /* RunForm() returns 1 for failure */
#if 0
	if (RunFormRes && (g_fpu == -1) && !(g_orbit_save & ORBITSAVE_SOUND) && !Randomized
		&& (g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL))
	{
		return CvtStk(); /* run fast assembler code in parsera.asm */
	}
#endif
	return RunFormRes;
#endif
}

int formula_setup_int()
{
#if defined(XFRACT) || defined(_WIN32)
	return integer_unsupported();
#else
	MathType = L_MATH;
	fg = (double)(1L << g_bit_shift);
	g_fudge_limit = (double)0x7fffffffL / fg;
	ShiftBack = 32 - g_bit_shift;
	return !RunForm(g_formula_name, 0);
#endif
}


/* TIW added 06-20-90 so functions can be called from fractals.c */
void init_misc()
{
	static struct ConstArg vv[5];
	static union Arg argfirst, argsecond;
	if (!v)
	{
		v = vv;
	}
	Arg1 = &argfirst;
	Arg2 = &argsecond; /* needed by all the ?Stk* functions */
	fg = (double)(1L << g_bit_shift);
	g_fudge_limit = (double)0x7fffffffL / fg;
	ShiftBack = 32 - g_bit_shift;
	Delta16 = g_bit_shift - 16;
	g_bit_shift_minus_1 = g_bit_shift-1;
	g_uses_p1 = g_uses_p2 = g_uses_p3 = uses_jump = g_uses_is_mand = 0;
	g_uses_p4 = g_uses_p5 = 0;
}


/* PB 910417 here to end changed.
		Allocate sub-arrays from one main malloc, using global variable
		g_type_specific_work_area; calcfrac.c releases this area when calculation
		ends or is terminated.
		Moved the "f" array to be allocated as part of this.
		*/

long g_total_formula_mem;
static void parser_allocate()
{
	/* CAE fp changed below for v18 */
	/* Note that XFRACT will waste about 6k here for g_function_load_store_pointers */
	/* Somewhat more memory is now allocated than in v17 here */
	/* however Store and Load were reduced in size to help make up for it */
	long f_size, Store_size, Load_size, v_size, p_size;
	int pass, is_bad_form = 0;
	/* TW Jan 1 1996 Made two passes to determine actual values of
		g_formula_max_ops and g_formula_max_args. */
	for (pass = 0; pass < 2; pass++)
	{
		free_work_area();
		if (pass == 0)
		{
			g_formula_max_ops = 2300; /* this value uses up about 64K memory */
			g_formula_max_args = (unsigned) (g_formula_max_ops/2.5);
		}
		f_size = sizeof(void (**)())*g_formula_max_ops;
		Store_size = sizeof(union Arg *)*MAX_STORES;
		Load_size = sizeof(union Arg *)*MAX_LOADS;
		v_size = sizeof(struct ConstArg)*g_formula_max_args;
		p_size = sizeof(struct fls *)*g_formula_max_ops;
		g_total_formula_mem = f_size + Load_size + Store_size + v_size + p_size /*+ jump_size*/
			+ sizeof(struct PEND_OP)*g_formula_max_ops;

		g_type_specific_work_area = malloc(f_size + Load_size + Store_size + v_size + p_size);
		f = (void (**)()) g_type_specific_work_area;
		Store = (union Arg **) (f + g_formula_max_ops);
		Load = (union Arg **) (Store + MAX_STORES);
		v = (struct ConstArg *) (Load + MAX_LOADS);
		g_function_load_store_pointers = (struct fls *) (v + g_formula_max_args);

		if (pass == 0)
		{
			is_bad_form = ParseStr(FormStr, pass);
			if (is_bad_form == 0)
			{
				/* per Chuck Ebbert, g_fudge these up a little */
				g_formula_max_ops = g_posp + 4;
				g_formula_max_args = g_parser_vsp + 4;
			}
		}
	}
	g_uses_p1 = g_uses_p2 = g_uses_p3 = g_uses_p4 = g_uses_p5 = 0;
}

void free_work_area()
{
	if (g_type_specific_work_area)
	{
		free(g_type_specific_work_area);
	}
	g_type_specific_work_area = NULL;
	Store = (union Arg **) NULL;
	Load = (union Arg **) NULL;
	v = (struct ConstArg *) NULL;
	f = (void (**)()) NULL;      /* CAE fp */
	g_function_load_store_pointers = (struct fls *) NULL;   /* CAE fp */
	g_total_formula_mem = 0;
}


struct error_data_st
{
	long start_pos;
	long error_pos;
	int error_number;
} errors[3];


void frm_error(FILE *open_file, long begin_frm)
{
	struct token_st tok;
	/* char debugmsg[500]; */
	int i, chars_to_error = 0, chars_in_error = 0, token_count;
	int statement_len, line_number;
	int done;
	char msgbuf[900];
	long filepos;
	int j;
	int initialization_error;
	strcpy (msgbuf, "\n");

	for (j = 0; j < 3 && errors[j].start_pos; j++)
	{
		initialization_error = errors[j].error_number == PE_SECOND_COLON ? 1 : 0;
		fseek(open_file, begin_frm, SEEK_SET);
		line_number = 1;
		while (ftell(open_file) != errors[j].error_pos)
		{
			i = fgetc(open_file);
			if (i == '\n')
			{
				line_number++;
			}
			else if (i == EOF || i == '}')
			{
				stop_message(0, "Unexpected EOF or end-of-formula in error function.\n");
				fseek (open_file, errors[j].error_pos, SEEK_SET);
				frmgettoken(open_file, &tok); /*reset file to end of error token */
				return;
			}
		}
		sprintf(&msgbuf[(int) strlen(msgbuf)], "Error(%d) at line %d:  %s\n  ", errors[j].error_number, line_number, ParseErrs(errors[j].error_number));
		i = (int) strlen(msgbuf);
		/* sprintf(debugmsg, "msgbuf is: %s\n and i is %d\n", msgbuf, i);
		stop_message (0, debugmsg);
		*/
		fseek(open_file, errors[j].start_pos, SEEK_SET);
		statement_len = token_count = 0;
		done = 0;
		while (!done)
		{
			filepos = ftell (open_file);
			if (filepos == errors[j].error_pos)
			{
				/* stop_message(0, "About to get error token\n"); */
				chars_to_error = statement_len;
				frmgettoken(open_file, &tok);
				chars_in_error = (int) strlen(tok.token_str);
				statement_len += chars_in_error;
				token_count++;
				/* sprintf(debugmsg, "Error is %s\nChars in error is %d\nChars to error is %d\n", tok.token_str, chars_in_error, chars_to_error);
				stop_message (0, debugmsg);
				*/
			}
			else
			{
				frmgettoken(open_file, &tok);
				/* sprintf(debugmsg, "Just got %s\n", tok.token_str);
				stop_message (0, debugmsg);
				*/
				statement_len += (int) strlen(tok.token_str);
				token_count++;
			}
			if ((tok.token_type == END_OF_FORMULA)
				|| (tok.token_type == OPERATOR
					&& (tok.token_id == 0 || tok.token_id == 11))
				|| (tok.token_type == NOT_A_TOKEN && tok.token_id == END_OF_FILE))
			{
				done = 1;
				if (token_count > 1 && !initialization_error)
				{
					token_count--;
				}
			}
		}
		fseek(open_file, errors[j].start_pos, SEEK_SET);
		if (chars_in_error < 74)
		{
			while (chars_to_error + chars_in_error > 74)
			{
				/* stop_message(0, "chars in error less than 74, but late in line"); */
				frmgettoken(open_file, &tok);
				chars_to_error -= (int) strlen(tok.token_str);
				token_count--;
			}
		}
		else
		{
			fseek(open_file, errors[j].error_pos, SEEK_SET);
			chars_to_error = 0;
			token_count = 1;
		}
		/* stop_message(0, "Back to beginning of statement to build msgbuf"); */
		while ((int) strlen(&msgbuf[i]) <= 74 && token_count--)
		{
			frmgettoken (open_file, &tok);
			strcat (msgbuf, tok.token_str);
			/* stop_message(0, &msgbuf[i]); */
		}
		fseek (open_file, errors[j].error_pos, SEEK_SET);
		frmgettoken (open_file, &tok);
		if ((int) strlen(&msgbuf[i]) > 74)
		{
			msgbuf[i + 74] = (char) 0;
		}
		strcat(msgbuf, "\n");
		i = (int) strlen(msgbuf);
		while (chars_to_error-- > -2)
		{
			strcat (msgbuf, " ");
		}
		/* sprintf(debugmsg, "Going into final line, chars in error is %d", chars_in_error);
		stop_message(0, debugmsg);
		*/
		if (errors[j].error_number == PE_TOKEN_TOO_LONG)
		{
			chars_in_error = 33;
		}
		while (chars_in_error-- && (int) strlen(&msgbuf[i]) <= 74)
		{
			strcat (msgbuf, "^");
		}
		strcat (msgbuf, "\n");
	}
	stop_message (8, msgbuf);
	return;
}

void display_var_list()
{
	struct var_list_st *p;
	stop_message(0, "List of user defined variables:\n");
	for (p = var_list; p; p = p->next_item)
	{
		stop_message(0, p->name);
	}

}

void display_const_lists()
{
	struct const_list_st *p;
	char msgbuf[800];
	stop_message (0, "Complex constants are:");
	for (p = complx_list; p; p = p->next_item)
	{
		sprintf(msgbuf, "%f, %f\n", p->complex_const.x, p->complex_const.y);
		stop_message(0, msgbuf);
	}
	stop_message (0, "Real constants are:");
	for (p = real_list; p; p = p->next_item)
	{
		sprintf(msgbuf, "%f, %f\n", p->complex_const.x, p->complex_const.y);
		stop_message(0, msgbuf);
	}
}


struct var_list_st *var_list_alloc()
{
	return (struct var_list_st *) malloc(sizeof(struct var_list_st));
}


struct const_list_st  *const_list_alloc()
{
	return (struct const_list_st *) malloc(sizeof(struct const_list_st));
}

void init_var_list()
{
	struct var_list_st *temp, *p;
	for (p = var_list; p; p = temp)
	{
		temp = p->next_item;
		free(p);
	}
	var_list = NULL;
}


void init_const_lists()
{
	struct const_list_st *temp, *p;
	for (p = complx_list; p; p = temp)
	{
		temp = p->next_item;
		free(p);
	}
	complx_list = NULL;
	for (p = real_list; p; p = temp)
	{
		temp = p->next_item;
		free(p);
	}
	real_list = NULL;
}

struct var_list_st *add_var_to_list(struct var_list_st *p, struct token_st tok)
{
	if (p == NULL)
	{
		p = var_list_alloc();
		if (p == NULL)
		{
			return NULL;
		}
		strcpy(p->name, tok.token_str);
		p->next_item = NULL;
	}
	else if (strcmp(p->name, tok.token_str) == 0)
	{
	}
	else
	{
		p->next_item = add_var_to_list(p->next_item, tok);
		if (p->next_item == NULL)
		{
			return NULL;
		}
	}
	return p;
}

struct const_list_st *add_const_to_list(struct const_list_st *p, struct token_st tok)
{
	if (p == NULL)
	{
		p = const_list_alloc();
		if (p == NULL)
		{
			return NULL;
		}
		p->complex_const.x = tok.token_const.x;
		p->complex_const.y = tok.token_const.y;
		p->next_item = NULL;
	}
	else if (p->complex_const.x == tok.token_const.x && p->complex_const.y == tok.token_const.y)
	{
	}
	else
	{
		p->next_item = add_const_to_list(p->next_item, tok);
		if (p->next_item == NULL)
		{
			return NULL;
		}
	}
	return p;
}

void count_lists()
{
/* char msgbuf[800];
*/ struct var_list_st *p;
	struct const_list_st *q;

	var_count = 0;
	complx_count = 0;
	real_count = 0;

	for (p = var_list; p; p = p->next_item)
	{
		var_count++;
	}
	for (q = complx_list; q; q = q->next_item)
	{
		complx_count++;
	}
	for (q = real_list; q; q = q->next_item)
	{
		real_count++;
	}
/*   sprintf(msgbuf, "Number of vars is %d\nNumber of complx is %d\nNumber of real is %d\n", var_count, complx_count, real_count);
	stop_message(0, msgbuf);
*/
}



/*frm_prescan() takes an open file with the file pointer positioned at
the beginning of the relevant formula, and parses the formula, token
by token, for syntax errors. The function also accumulates data for
memory allocation to be done later.

The function returns 1 if success, and 0 if errors are found.
*/

int disable_fastparser;
int must_use_float;


int frm_prescan(FILE *open_file)
{
	long filepos;
	int i;
	long statement_pos, orig_pos;
	int done = 0;
	struct token_st this_token;
	int errors_found = 0;
	int ExpectingArg = 1;
	int NewStatement = 1;
	int assignment_ok = 1;
	int already_got_colon = 0;
	unsigned long else_has_been_used = 0;
	unsigned long waiting_for_mod = 0;
	int waiting_for_endif = 0;
	int max_parens = sizeof(long)*8;
/* char debugmsg[800];
	stop_message (0, "Entering prescan");
*/

	disable_fastparser = 0;
	must_use_float     = 0;

	number_of_ops = number_of_loads = number_of_stores = number_of_jumps = (unsigned) 0L;
	chars_in_formula = (unsigned) 0;
	uses_jump = (short) 0;
	paren = 0;

	init_var_list();
	init_const_lists();

	orig_pos = statement_pos = ftell(open_file);
	for (i = 0; i < 3; i++)
	{
		errors[i].start_pos    = 0L;
		errors[i].error_pos    = 0L;
		errors[i].error_number = 0;
	}

	while (!done)
	{
		filepos = ftell (open_file);
		frmgettoken (open_file, &this_token);
		chars_in_formula += (int) strlen(this_token.token_str);
		switch (this_token.token_type)
		{
		case NOT_A_TOKEN:
			assignment_ok = 0;
			switch (this_token.token_id)
			{
			case END_OF_FILE:
				stop_message(0, ParseErrs(PE_UNEXPECTED_EOF));
				fseek(open_file, orig_pos, SEEK_SET);
				return 0;
			case ILLEGAL_CHARACTER:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_ILLEGAL_CHAR;
				}
				break;
			case ILLEGAL_VARIABLE_NAME:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_ILLEGAL_VAR_NAME;
				}
				break;
			case TOKEN_TOO_LONG:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_TOKEN_TOO_LONG;
				}
				break;
			case FUNC_USED_AS_VAR:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_FUNC_USED_AS_VAR;
				}
				break;
			case JUMP_MISSING_BOOLEAN:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_JUMP_NEEDS_BOOLEAN;
				}
				break;
			case JUMP_WITH_ILLEGAL_CHAR:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_NO_CHAR_AFTER_THIS_JUMP;
				}
				break;
			case UNDEFINED_FUNCTION:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_UNDEFINED_FUNCTION;
				}
				break;
			case ILLEGAL_OPERATOR:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_UNDEFINED_OPERATOR;
				}
				break;
			case ILL_FORMED_CONSTANT:
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_INVALID_CONST;
				}
				break;
			default:
				stop_message(0, "Unexpected arrival at default case in prescan()");
				fseek(open_file, orig_pos, SEEK_SET);
				return 0;
			}
			break;
		case PARENS:
			assignment_ok = 0;
			NewStatement = 0;
			switch (this_token.token_id)
			{
			case OPEN_PARENS:
				if (++paren > max_parens)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_NESTING_TO_DEEP;
					}
				}
				else if (!ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
					}
				}
				waiting_for_mod = waiting_for_mod << 1;
				break;
			case CLOSE_PARENS:
				if (paren)
				{
					paren--;
				}
				else
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_NEED_A_MATCHING_OPEN_PARENS;
					}
					paren = 0;
				}
				if (waiting_for_mod & 1L)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_UNMATCHED_MODULUS;
					}
				}
				else
				{
					waiting_for_mod = waiting_for_mod >> 1;
				}
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				break;
			default:
				break;
			}
			break;
		case PARAM_VARIABLE: /*i.e. p1, p2, p3, p4 or p5*/
			number_of_ops++;
			number_of_loads++;
			NewStatement = 0;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			ExpectingArg = 0;
			break;
		case USER_NAMED_VARIABLE: /* i.e. c, iter, etc. */
			number_of_ops++;
			number_of_loads++;
			NewStatement = 0;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			ExpectingArg = 0;
			/*
			var_list = add_var_to_list (var_list, this_token);
			if (var_list == NULL)
			{
				stop_message(0, ParseErrs(PE_INSUFFICIENT_MEM_FOR_TYPE_FORMULA));
				fseek(open_file, orig_pos, SEEK_SET);
				init_var_list();
				init_const_lists();
				return 0;
			}
			*/
			break;
		case PREDEFINED_VARIABLE: /* i.e. z, pixel, whitesq, etc. */
			number_of_ops++;
			number_of_loads++;
			NewStatement = 0;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			ExpectingArg = 0;
			break;
		case REAL_CONSTANT: /* i.e. 4, (4,0), etc.) */
			assignment_ok = 0;
			number_of_ops++;
			number_of_loads++;
			NewStatement = 0;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			ExpectingArg = 0;
			/*
			real_list = add_const_to_list (real_list, this_token);
			if (real_list == NULL)
			{
				stop_message(0, ParseErrs(PE_INSUFFICIENT_MEM_FOR_TYPE_FORMULA));
				fseek(open_file, orig_pos, SEEK_SET);
				init_var_list();
				init_const_lists();
				return 0;
			}
			*/
			break;
		case COMPLEX_CONSTANT: /* i.e. (1,2) etc. */
			assignment_ok = 0;
			number_of_ops++;
			number_of_loads++;
			NewStatement = 0;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			ExpectingArg = 0;
			/*
			complx_list = add_const_to_list (complx_list, this_token);
			if (complx_list == NULL)
			{
				stop_message(0, ParseErrs(PE_INSUFFICIENT_MEM_FOR_TYPE_FORMULA));
				fseek(open_file, orig_pos, SEEK_SET);
				init_var_list();
				init_const_lists();
				return 0;
			}
			*/
			break;
		case FUNCTION:
			assignment_ok = 0;
			NewStatement = 0;
			number_of_ops++;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			break;
		case PARAM_FUNCTION:
			assignment_ok = 0;
			NewStatement = 0;
			number_of_ops++;
			if (!ExpectingArg)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
				}
			}
			NewStatement = 0;
			break;
		case FLOW_CONTROL:
			assignment_ok = 0;
			number_of_ops++;
			number_of_jumps++;
			if (!NewStatement)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_JUMP_NOT_FIRST;
				}
			}
			else
			{
				uses_jump = 1;
				switch (this_token.token_id)
				{
				case 1:  /* if */
					else_has_been_used <<= 1;
					waiting_for_endif++;
					break;
				case 2: /*ELSEIF*/
					number_of_ops += 3; /*else + two clear statements*/
					number_of_jumps++;  /* this involves two jumps */
					if (else_has_been_used % 2)
					{
						if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
						{
							errors[errors_found].start_pos      = statement_pos;
							errors[errors_found].error_pos      = filepos;
							errors[errors_found++].error_number = PE_ENDIF_REQUIRED_AFTER_ELSE;
						}
					}
					else if (!waiting_for_endif)
					{
						if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
						{
							errors[errors_found].start_pos      = statement_pos;
							errors[errors_found].error_pos      = filepos;
							errors[errors_found++].error_number = PE_MISPLACED_ELSE_OR_ELSEIF;
						}
					}
					break;
				case 3: /*ELSE*/
					if (else_has_been_used % 2)
					{
						if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
						{
							errors[errors_found].start_pos      = statement_pos;
							errors[errors_found].error_pos      = filepos;
							errors[errors_found++].error_number = PE_ENDIF_REQUIRED_AFTER_ELSE;
						}
					}
					else if (!waiting_for_endif)
					{
						if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
						{
							errors[errors_found].start_pos      = statement_pos;
							errors[errors_found].error_pos      = filepos;
							errors[errors_found++].error_number = PE_MISPLACED_ELSE_OR_ELSEIF;
						}
					}
					else_has_been_used |= 1;
					break;
				case 4: /*ENDIF*/
					else_has_been_used >>= 1;
					waiting_for_endif--;
					if (waiting_for_endif < 0)
					{
						if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
						{
							errors[errors_found].start_pos      = statement_pos;
							errors[errors_found].error_pos      = filepos;
							errors[errors_found++].error_number = PE_ENDIF_WITH_NO_IF;
						}
						waiting_for_endif = 0;
					}
					break;
				default:
					break;
				}
			}
			break;
		case OPERATOR:
			number_of_ops++; /*This will be corrected below in certain cases*/
			switch (this_token.token_id)
			{
			case 0: case 11:    /* end of statement and : */
				number_of_ops++; /* ParseStr inserts a dummy op*/
				if (paren)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_NEED_MORE_CLOSE_PARENS;
					}
					paren = 0;
				}
				if (waiting_for_mod)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_UNMATCHED_MODULUS;
					}
					waiting_for_mod = 0;
				}
				if (!ExpectingArg)
				{
					if (this_token.token_id == 11)
					{
						number_of_ops += 2;
					}
					else
					{
						number_of_ops++;
					}
				}
				else if (!NewStatement)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				if (this_token.token_id == 11 && waiting_for_endif)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_UNMATCHED_IF_IN_INIT_SECTION;
					}
					waiting_for_endif = 0;
				}
				if (this_token.token_id == 11 && already_got_colon)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SECOND_COLON;
					}
				}
				if (this_token.token_id == 11)
				{
					already_got_colon = 1;
				}
				NewStatement = ExpectingArg = assignment_ok = 1;
				statement_pos = ftell(open_file);
				break;
			case 1:     /* != */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 2:     /* = */
				number_of_ops--; /*this just converts a load to a store*/
				number_of_loads--;
				number_of_stores++;
				if (!assignment_ok)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_ILLEGAL_ASSIGNMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 3:     /* == */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 4:     /* < */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 5:     /* <= */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 6:     /* > */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 7:     /* >= */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 8:     /* | */ /* (half of the modulus operator */
				assignment_ok = 0;
				if (!waiting_for_mod & 1L)
				{
					number_of_ops--;
				}
				if (!(waiting_for_mod & 1L) && !ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_OPERATOR;
					}
				}
				else if ((waiting_for_mod & 1L) && ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				waiting_for_mod = waiting_for_mod ^ 1L; /*switch right bit*/
				break;
			case 9:     /* || */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 10:    /* && */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 12:    /* + */ /* case 11 (":") is up with case 0 */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 13:    /* - */
				assignment_ok = 0;
				ExpectingArg = 1;
				break;
			case 14:    /* * */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 15:    /* / */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				ExpectingArg = 1;
				break;
			case 16:    /* ^ */
				assignment_ok = 0;
				if (ExpectingArg)
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
					}
				}
				filepos = ftell(open_file);
				frmgettoken (open_file, &this_token);
				if (this_token.token_str[0] == '-')
				{
					if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
					{
						errors[errors_found].start_pos      = statement_pos;
						errors[errors_found].error_pos      = filepos;
						errors[errors_found++].error_number = PE_NO_NEG_AFTER_EXPONENT;
					}
				}
				else
				{
					fseek(open_file, filepos, SEEK_SET);
				}
				ExpectingArg = 1;
				break;
			default:
				break;
			}
			break;
		case END_OF_FORMULA:
			number_of_ops += 3; /* Just need one, but a couple of extra just for the heck of it */
			if (paren)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_NEED_MORE_CLOSE_PARENS;
				}
				paren = 0;
			}
			if (waiting_for_mod)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_UNMATCHED_MODULUS;
				}
				waiting_for_mod = 0;
			}
			if (waiting_for_endif)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_IF_WITH_NO_ENDIF;
				}
				waiting_for_endif = 0;
			}
			if (ExpectingArg && !NewStatement)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_SHOULD_BE_ARGUMENT;
				}
				statement_pos = ftell(open_file);
			}

			if (number_of_jumps >= MAX_JUMPS)
			{
				if (!errors_found || errors[errors_found-1].start_pos != statement_pos)
				{
					errors[errors_found].start_pos      = statement_pos;
					errors[errors_found].error_pos      = filepos;
					errors[errors_found++].error_number = PE_TOO_MANY_JUMPS;
				}
			}
			done = 1;
			break;

		default:
			break;
		}
		if (errors_found == 3)
		{
			done = 1;
		}
	}
	if (errors[0].start_pos)
	{
/*    sprintf (debugmsg, "Errors structure on entering frm_error\n 0: %ld, %ld, %d\n1: %ld, %ld, %d\n2: %ld, %ld, %d\n\n",
			errors[0].start_pos, errors[0].error_pos, errors[0].error_number,
			errors[1].start_pos, errors[1].error_pos, errors[1].error_number,
			errors[2].start_pos, errors[2].error_pos, errors[2].error_number);
		stop_message (0, debugmsg);
*/    frm_error(open_file, orig_pos);
		fseek(open_file, orig_pos, SEEK_SET);
		return 0;
	}
	fseek(open_file, orig_pos, SEEK_SET);

/*   display_var_list();
	display_const_lists();
*/   count_lists();

/* sprintf(debugmsg, "Chars in formula per prescan() is %u.\n", chars_in_formula);
	stop_message(0, debugmsg);
*/ return 1;
}
