#include <limits.h>
#include <string.h>
#if !defined(__386BSD__)
#if !defined(_WIN32)
#include <malloc.h>
#endif
#endif

/* see Fractint.c for a description of the "include"  hierarchy */
#include "port.h"
#include "prototyp.h"
#include "helpdefs.h"
#include "fractype.h"

#if !defined(XFRACT)
#define MPCmod(m) (*pMPadd(*pMPmul((m).x, (m).x), *pMPmul((m).y, (m).y)))
#endif

long calculate_mandelbrot_fp_asm(void);

/* -------------------------------------------------------------------- */
/*              Setup (once per fractal image) routines                 */
/* -------------------------------------------------------------------- */

int mandelbrot_setup(void)           /* Mandelbrot Routine */
{
	if (g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL
		&& !g_invert && g_decomposition[0] == 0 && g_rq_limit == 4.0
		&& g_bit_shift == 29 && !g_potential_flag
		&& g_biomorph == -1 && g_inside > -59 && g_outside >= -1
		&& g_use_initial_orbit_z != 1 && g_using_jiim == 0 && g_bail_out_test == Mod
		&& (g_orbit_save & ORBITSAVE_SOUND) == 0)
	{
		g_calculate_type = calculate_mandelbrot; /* the normal case - use CALCMAND */
	}
	else
	{
		/* special case: use the main processing loop */
		g_calculate_type = standard_fractal;
		g_long_parameter = &g_initial_z_l;
	}
	return 1;
}

int julia_setup(void)            /* Julia Routine */
{
	if (g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL
		&& !g_invert && g_decomposition[0] == 0 && g_rq_limit == 4.0
		&& g_bit_shift == 29 && !g_potential_flag
		&& g_biomorph == -1 && g_inside > -59 && g_outside >= -1
		&& !g_finite_attractor && g_using_jiim == 0 && g_bail_out_test == Mod
		&& (g_orbit_save & ORBITSAVE_SOUND) == 0)
	{
		g_calculate_type = calculate_mandelbrot; /* the normal case - use CALCMAND */
	}
	else
	{
		/* special case: use the main processing loop */
		g_calculate_type = standard_fractal;
		g_long_parameter = &g_parameter_l;
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
	}
	return 1;
}

int stand_alone_setup(void)
{
	timer(TIMER_ENGINE, g_current_fractal_specific->calculate_type);
	return 0;           /* effectively disable solid-guessing */
}

int unity_setup(void)
{
	g_periodicity_check = 0;
	g_one_fudge = (1L << g_bit_shift);
	g_two_fudge = g_one_fudge + g_one_fudge;
	return 1;
}

int mandelbrot_setup_fp(void)
{
	g_bf_math = 0;
	g_c_exp = (int)g_parameters[2];
	g_power.x = g_parameters[2] - 1.0;
	g_power.y = g_parameters[3];
	g_float_parameter = &g_initial_z;
	switch (g_fractal_type)
	{
	case MARKSMANDELFP:
		if (g_c_exp < 1)
		{
			g_c_exp = 1;
			g_parameters[2] = 1;
		}
		if (!(g_c_exp & 1))
		{
			g_symmetry = XYAXIS_NOPARM;    /* odd exponents */
		}
		if (g_c_exp & 1)
		{
			g_symmetry = XAXIS_NOPARM;
		}
		break;
	case MANDELFP:
		/*
		floating point code could probably be altered to handle many of
		the situations that otherwise are using standard_fractal().
		calculate_mandelbrot_fp() can currently handle invert, any g_rq_limit, g_potential_flag
		zmag, epsilon cross, and all the current outside options
													Wes Loewer 11/03/91
		Took out support for inside= options, for speed. 7/13/97
		*/
		if (g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL
			&& !g_distance_test
			&& g_decomposition[0] == 0
			&& g_biomorph == -1
			&& (g_inside >= -1)
			/* uncomment this next line if more outside options are added */
			&& g_outside >= -6
			&& g_use_initial_orbit_z != 1
			&& (g_sound_flags & SOUNDFLAG_ORBITMASK) < SOUNDFLAG_X
			&& g_using_jiim == 0 && g_bail_out_test == Mod
			&& (g_orbit_save & ORBITSAVE_SOUND) == 0)
		{
			g_calculate_type = calculate_mandelbrot_fp; /* the normal case - use calculate_mandelbrot_fp */
#if !defined(XFRACT)
			if (g_cpu >= 386 && g_fpu >= 387)
			{
				calculate_mandelbrot_fp_p5_asm_start();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_p5_asm;
			}
			else if (g_cpu == 286 && g_fpu >= 287)
			{
				calculate_mandelbrot_start_fp_asm();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_287_asm;
			}
			else
			{
				calculate_mandelbrot_start_fp_asm();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_87_asm;
			}
#else
#ifdef NASM
			if (g_fpu == -1)
			{
				calculate_mandelbrot_fp_p5_asm_start();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_p5_asm;
			}
			else
#endif
			{
				calculate_mandelbrot_start_fp_asm();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_asm;
			}
#endif
		}
		else
		{
			/* special case: use the main processing loop */
			g_calculate_type = standard_fractal;
		}
		break;
	case FPMANDELZPOWER:
		if ((double)g_c_exp == g_parameters[2] && (g_c_exp & 1)) /* odd exponents */
		{
			g_symmetry = XYAXIS_NOPARM;
		}
		if (g_parameters[3] != 0)
		{
			g_symmetry = NOSYM;
		}
		g_fractal_specific[g_fractal_type].orbitcalc = 
			(g_parameters[3] == 0.0 && g_debug_flag != DEBUGFLAG_UNOPT_POWER && (double)g_c_exp == g_parameters[2]) ?
			z_power_orbit_fp : complex_z_power_orbit_fp;
		break;
	case MAGNET1M:
	case MAGNET2M:
		g_attractors[0].x = 1.0;      /* 1.0 + 0.0i always attracts */
		g_attractors[0].y = 0.0;      /* - both MAGNET1 and MAGNET2 */
		g_attractor_period[0] = 1;
		g_num_attractors = 1;
		break;
	case SPIDERFP:
		if (g_periodicity_check == 1) /* if not user set */
		{
			g_periodicity_check = 4;
		}
		break;
	case MANDELEXP:
		g_symmetry = XAXIS_NOPARM;
		break;
/* Added to account for symmetry in manfn + exp and manfn + zsqrd */
/*     JCO 2/29/92 */
	case FPMANTRIGPLUSEXP:
	case FPMANTRIGPLUSZSQRD:
		g_symmetry = (g_parameter.y == 0.0) ? XAXIS : NOSYM;
		if ((g_trig_index[0] == LOG) || (g_trig_index[0] == 14)) /* LOG or FLIP */
		{
			g_symmetry = NOSYM;
		}
		break;
	case QUATFP:
		g_float_parameter = &g_temp_z;
		g_num_attractors = 0;
		g_periodicity_check = 0;
		break;
	case HYPERCMPLXFP:
		g_float_parameter = &g_temp_z;
		g_num_attractors = 0;
		g_periodicity_check = 0;
		if (g_parameters[2] != 0)
		{
			g_symmetry = NOSYM;
		}
		if (g_trig_index[0] == 14) /* FLIP */
		{
			g_symmetry = NOSYM;
		}
		break;
	case TIMSERRORFP:
		if (g_trig_index[0] == 14) /* FLIP */
		{
			g_symmetry = NOSYM;
		}
		break;
	case MARKSMANDELPWRFP:
		if (g_trig_index[0] == 14) /* FLIP */
		{
			g_symmetry = NOSYM;
		}
		break;
	default:
		break;
	}
	return 1;
}

int julia_setup_fp(void)
{
	g_c_exp = (int)g_parameters[2];
	g_float_parameter = &g_parameter;
	if (g_fractal_type == COMPLEXMARKSJUL)
	{
		g_power.x = g_parameters[2] - 1.0;
		g_power.y = g_parameters[3];
		g_coefficient = ComplexPower(*g_float_parameter, g_power);
	}
	switch (g_fractal_type)
	{
	case JULIAFP:
		/*
		floating point code could probably be altered to handle many of
		the situations that otherwise are using standard_fractal().
		calculate_mandelbrot_fp() can currently handle invert, any g_rq_limit, g_potential_flag
		zmag, epsilon cross, and all the current outside options
													Wes Loewer 11/03/91
		Took out support for inside= options, for speed. 7/13/97
		*/
		if (g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL
				&& !g_distance_test
				&& g_decomposition[0] == 0
				&& g_biomorph == -1
				&& (g_inside >= -1)
				/* uncomment this next line if more outside options are added */
				&& g_outside >= -6
				&& g_use_initial_orbit_z != 1
				&& (g_sound_flags & SOUNDFLAG_ORBITMASK) < SOUNDFLAG_X
				&& !g_finite_attractor
				&& g_using_jiim == 0 && g_bail_out_test == Mod
				&& (g_orbit_save & ORBITSAVE_SOUND) == 0)
		{
			g_calculate_type = calculate_mandelbrot_fp; /* the normal case - use calculate_mandelbrot_fp */
#if !defined(XFRACT)
			if (g_cpu >= 386 && g_fpu >= 387)
			{
				calculate_mandelbrot_fp_p5_asm_start();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_p5_asm;
			}
			else if (g_cpu == 286 && g_fpu >= 287)
			{
				calculate_mandelbrot_start_fp_asm();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_287_asm;
			}
			else
			{
				calculate_mandelbrot_start_fp_asm();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_87_asm;
			}
#else
#ifdef NASM
			if (g_fpu == -1)
			{
				calculate_mandelbrot_fp_p5_asm_start();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_p5_asm;
			}
			else
#endif
			{
				calculate_mandelbrot_start_fp_asm();
				g_calculate_mandelbrot_asm_fp = (long (*)(void))calculate_mandelbrot_fp_asm;
			}
#endif
		}
		else
		{
			/* special case: use the main processing loop */
			g_calculate_type = standard_fractal;
			get_julia_attractor (0.0, 0.0);   /* another attractor? */
		}
		break;
	case FPJULIAZPOWER:
		if ((g_c_exp & 1) || g_parameters[3] != 0.0 || (double)g_c_exp != g_parameters[2])
		{
			g_symmetry = NOSYM;
		}
		g_fractal_specific[g_fractal_type].orbitcalc = 
			(g_parameters[3] == 0.0 && g_debug_flag != DEBUGFLAG_UNOPT_POWER && (double)g_c_exp == g_parameters[2])
			? z_power_orbit_fp : complex_z_power_orbit_fp;
		get_julia_attractor (g_parameters[0], g_parameters[1]); /* another attractor? */
		break;
	case MAGNET2J:
		magnet2_precalculate_fp();
	case MAGNET1J:
		g_attractors[0].x = 1.0;      /* 1.0 + 0.0i always attracts */
		g_attractors[0].y = 0.0;      /* - both MAGNET1 and MAGNET2 */
		g_attractor_period[0] = 1;
		g_num_attractors = 1;
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	case LAMBDAFP:
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		get_julia_attractor (0.5, 0.0);   /* another attractor? */
		break;
	case LAMBDAEXP:
		if (g_parameter.y == 0.0)
		{
			g_symmetry = XAXIS;
		}
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	/* Added to account for symmetry in julfn + exp and julfn + zsqrd */
	/*     JCO 2/29/92 */
	case FPJULTRIGPLUSEXP:
	case FPJULTRIGPLUSZSQRD:
		g_symmetry = (g_parameter.y == 0.0) ? XAXIS : NOSYM;
		if ((g_trig_index[0] == LOG) || (g_trig_index[0] == 14)) /* LOG or FLIP */
		{
			g_symmetry = NOSYM;
		}
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	case HYPERCMPLXJFP:
		if (g_parameters[2] != 0)
		{
			g_symmetry = NOSYM;
		}
		if (g_trig_index[0] != SQR)
		{
			g_symmetry = NOSYM;
		}
	case QUATJULFP:
		g_num_attractors = 0;   /* attractors broken since code checks r, i not j, k */
		g_periodicity_check = 0;
		if (g_parameters[4] != 0.0 || g_parameters[5] != 0)
		{
			g_symmetry = NOSYM;
		}
		break;
	case FPPOPCORN:
	case FPPOPCORNJUL:
		{
			int default_functions = 0;
			if (g_trig_index[0] == SIN &&
				g_trig_index[1] == TAN &&
				g_trig_index[2] == SIN &&
				g_trig_index[3] == TAN &&
				fabs(g_parameter2.x - 3.0) < .0001 &&
				g_parameter2.y == 0 &&
				g_parameter.y == 0)
			{
				default_functions = 1;
				if (g_fractal_type == FPPOPCORNJUL)
				{
					g_symmetry = ORIGIN;
				}
			}
			if (g_save_release <= 1960)
			{
				g_current_fractal_specific->orbitcalc = popcorn_old_orbit_fp;
			}
			else if (default_functions && DEBUGFLAG_REAL_POPCORN == g_debug_flag)
			{
				g_current_fractal_specific->orbitcalc = popcorn_orbit_fp;
			}
			else
			{
				g_current_fractal_specific->orbitcalc = popcorn_fn_orbit_fp;
			}
			get_julia_attractor (0.0, 0.0);   /* another attractor? */
		}
		break;
	case FPCIRCLE:
		if (g_inside == STARTRAIL) /* FPCIRCLE locks up when used with STARTRAIL */
		{
			g_inside = 0; /* arbitrarily set inside = NUMB */
		}
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	default:
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	}
	return 1;
}

int mandelbrot_setup_l(void)
{
	g_c_exp = (int)g_parameters[2];
	if (g_fractal_type == MARKSMANDEL && g_c_exp < 1)
	{
		g_c_exp = 1;
		g_parameters[2] = 1;
	}
	if ((g_fractal_type == MARKSMANDEL   && !(g_c_exp & 1)) ||
		(g_fractal_type == LMANDELZPOWER && (g_c_exp & 1)))
	{
		g_symmetry = XYAXIS_NOPARM;    /* odd exponents */
	}
	if ((g_fractal_type == MARKSMANDEL && (g_c_exp & 1)) || g_fractal_type == LMANDELEXP)
	{
		g_symmetry = XAXIS_NOPARM;
	}
	if (g_fractal_type == SPIDER && g_periodicity_check == 1)
	{
		g_periodicity_check = 4;
	}
	g_long_parameter = &g_initial_z_l;
	if (g_fractal_type == LMANDELZPOWER)
	{
		if (g_parameters[3] == 0.0 && g_debug_flag != DEBUGFLAG_UNOPT_POWER && (double)g_c_exp == g_parameters[2])
		{
			g_fractal_specific[g_fractal_type].orbitcalc = z_power_orbit;
		}
		else
		{
			g_fractal_specific[g_fractal_type].orbitcalc = complex_z_power_orbit;
		}
		if (g_parameters[3] != 0 || (double)g_c_exp != g_parameters[2])
		{
			g_symmetry = NOSYM;
		}
	}
	/* Added to account for symmetry in manfn + exp and manfn + zsqrd */
	/*     JCO 2/29/92 */
	if ((g_fractal_type == LMANTRIGPLUSEXP) || (g_fractal_type == LMANTRIGPLUSZSQRD))
	{
		g_symmetry = (g_parameter.y == 0.0) ? XAXIS : NOSYM;
		if ((g_trig_index[0] == LOG) || (g_trig_index[0] == 14)) /* LOG or FLIP */
		{
			g_symmetry = NOSYM;
		}
	}
	if (g_fractal_type == TIMSERROR)
	{
		if (g_trig_index[0] == 14) /* FLIP */
		{
			g_symmetry = NOSYM;
		}
	}
	if (g_fractal_type == MARKSMANDELPWR)
	{
		if (g_trig_index[0] == 14) /* FLIP */
		{
			g_symmetry = NOSYM;
		}
	}
	return 1;
}

int julia_setup_l(void)
{
	g_c_exp = (int)g_parameters[2];
	g_long_parameter = &g_parameter_l;
	switch (g_fractal_type)
	{
	case LJULIAZPOWER:
		if ((g_c_exp & 1) || g_parameters[3] != 0.0 || (double)g_c_exp != g_parameters[2])
		{
			g_symmetry = NOSYM;
		}
		g_fractal_specific[g_fractal_type].orbitcalc = 
			(g_parameters[3] == 0.0 && g_debug_flag != DEBUGFLAG_UNOPT_POWER && (double)g_c_exp == g_parameters[2])
			? z_power_orbit : complex_z_power_orbit;
		break;
	case LAMBDA:
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		get_julia_attractor (0.5, 0.0);   /* another attractor? */
		break;
	case LLAMBDAEXP:
		if (g_parameter_l.y == 0)
		{
			g_symmetry = XAXIS;
		}
		break;
	/* Added to account for symmetry in julfn + exp and julfn + zsqrd */
	/*     JCO 2/29/92 */
	case LJULTRIGPLUSEXP:
	case LJULTRIGPLUSZSQRD:
		g_symmetry = (g_parameter.y == 0.0) ? XAXIS : NOSYM;
		if ((g_trig_index[0] == LOG) || (g_trig_index[0] == 14)) /* LOG or FLIP */
		{
			g_symmetry = NOSYM;
		}
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	case LPOPCORN:
	case LPOPCORNJUL:
		{
			int default_functions = 0;
			if (g_trig_index[0] == SIN &&
				g_trig_index[1] == TAN &&
				g_trig_index[2] == SIN &&
				g_trig_index[3] == TAN &&
				fabs(g_parameter2.x - 3.0) < .0001 &&
				g_parameter2.y == 0 &&
				g_parameter.y == 0)
			{
				default_functions = 1;
				if (g_fractal_type == LPOPCORNJUL)
				{
					g_symmetry = ORIGIN;
				}
			}
			if (g_save_release <= 1960)
			{
				g_current_fractal_specific->orbitcalc = popcorn_old_orbit;
			}
			else if (default_functions && DEBUGFLAG_REAL_POPCORN == g_debug_flag)
			{
				g_current_fractal_specific->orbitcalc = popcorn_orbit;
			}
			else
			{
				g_current_fractal_specific->orbitcalc = popcorn_fn_orbit;
			}
			get_julia_attractor (0.0, 0.0);   /* another attractor? */
		}
		break;
	default:
		get_julia_attractor (0.0, 0.0);   /* another attractor? */
		break;
	}
	return 1;
}

int trig_plus_sqr_setup_l(void)
{
	g_current_fractal_specific->per_pixel =  julia_per_pixel;
	g_current_fractal_specific->orbitcalc =  trig_plus_sqr_orbit;
	if (g_parameter_l.x == g_fudge && g_parameter_l.y == 0L && g_parameter2_l.y == 0L
		&& g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL)
	{
		if (g_parameter2_l.x == g_fudge)        /* Scott variant */
		{
			g_current_fractal_specific->orbitcalc =  scott_trig_plus_sqr_orbit;
		}
		else if (g_parameter2_l.x == -g_fudge)  /* Skinner variant */
		{
			g_current_fractal_specific->orbitcalc =  skinner_trig_sub_sqr_orbit;
		}
	}
	return julia_setup_l();
}

int trig_plus_sqr_setup_fp(void)
{
	g_current_fractal_specific->per_pixel =  julia_per_pixel_fp;
	g_current_fractal_specific->orbitcalc =  trig_plus_sqr_orbit_fp;
	if (g_parameter.x == 1.0 && g_parameter.y == 0.0 && g_parameter2.y == 0.0
		&& g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL)
	{
		if (g_parameter2.x == 1.0)        /* Scott variant */
		{
			g_current_fractal_specific->orbitcalc =  scott_trig_plus_sqr_orbit_fp;
		}
		else if (g_parameter2.x == -1.0)  /* Skinner variant */
		{
			g_current_fractal_specific->orbitcalc =  skinner_trig_sub_sqr_orbit_fp;
		}
	}
	return julia_setup_fp();
}

static int fn_plus_fn_symmetry(void) /* set symmetry matrix for fn + fn type */
{
	static char fnplusfn[7][7] =
	{/* fn2 ->sin     cos    sinh    cosh   exp    log    sqr  */
	/* fn1 */
	/* sin */ {PI_SYM, XAXIS, XYAXIS, XAXIS, XAXIS, XAXIS, XAXIS},
	/* cos */ {XAXIS, PI_SYM, XAXIS,  XYAXIS, XAXIS, XAXIS, XAXIS},
	/* sinh*/ {XYAXIS, XAXIS, XYAXIS, XAXIS, XAXIS, XAXIS, XAXIS},
	/* cosh*/ {XAXIS, XYAXIS, XAXIS,  XYAXIS, XAXIS, XAXIS, XAXIS},
	/* exp */ {XAXIS, XYAXIS, XAXIS,  XAXIS, XYAXIS, XAXIS, XAXIS},
	/* log */ {XAXIS, XAXIS, XAXIS,  XAXIS, XAXIS, XAXIS, XAXIS},
	/* sqr */ {XAXIS, XAXIS, XAXIS,  XAXIS, XAXIS, XAXIS, XYAXIS}
	};
	if (g_parameter.y == 0.0 && g_parameter2.y == 0.0)
	{
		if (g_trig_index[0] < 7 && g_trig_index[1] < 7)  /* bounds of array JCO 5/6/92*/
		{
			g_symmetry = fnplusfn[g_trig_index[0]][g_trig_index[1]];  /* JCO 5/6/92 */
		}
		if (g_trig_index[0] == 14 || g_trig_index[1] == 14) /* FLIP */
		{
			g_symmetry = NOSYM;
		}
	}                 /* defaults to XAXIS symmetry JCO 5/6/92 */
	else
	{
		g_symmetry = NOSYM;
	}
	return 0;
}

int trig_plus_trig_setup_l(void)
{
	fn_plus_fn_symmetry();
	if (g_trig_index[1] == SQR)
	{
		return trig_plus_sqr_setup_l();
	}
	g_current_fractal_specific->per_pixel =  julia_per_pixel_l;
	g_current_fractal_specific->orbitcalc =  trig_plus_trig_orbit;
	if (g_parameter_l.x == g_fudge && g_parameter_l.y == 0L && g_parameter2_l.y == 0L
		&& g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL)
	{
		if (g_parameter2_l.x == g_fudge)        /* Scott variant */
		{
			g_current_fractal_specific->orbitcalc =  scott_trig_plus_trig_orbit;
		}
		else if (g_parameter2_l.x == -g_fudge)  /* Skinner variant */
		{
			g_current_fractal_specific->orbitcalc =  skinner_trig_sub_trig_orbit;
		}
	}
	return julia_setup_l();
}

int trig_plus_trig_setup_fp(void)
{
	fn_plus_fn_symmetry();
	if (g_trig_index[1] == SQR)
	{
		return trig_plus_sqr_setup_fp();
	}
	g_current_fractal_specific->per_pixel =  other_julia_per_pixel_fp;
	g_current_fractal_specific->orbitcalc =  trig_plus_trig_orbit_fp;
	if (g_parameter.x == 1.0 && g_parameter.y == 0.0 && g_parameter2.y == 0.0
		&& g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL)
	{
		if (g_parameter2.x == 1.0)        /* Scott variant */
		{
			g_current_fractal_specific->orbitcalc =  scott_trig_plus_trig_orbit_fp;
		}
		else if (g_parameter2.x == -1.0)  /* Skinner variant */
		{
			g_current_fractal_specific->orbitcalc =  skinner_trig_sub_trig_orbit_fp;
		}
	}
	return julia_setup_fp();
}

int lambda_trig_or_trig_setup(void)
{
	/* default symmetry is ORIGIN  JCO 2/29/92 (changed from PI_SYM) */
	g_long_parameter = &g_parameter_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_parameter;
	if ((g_trig_index[0] == EXP) || (g_trig_index[1] == EXP))
	{
		g_symmetry = NOSYM; /* JCO 1/9/93 */
	}
	if ((g_trig_index[0] == LOG) || (g_trig_index[1] == LOG))
	{
		g_symmetry = XAXIS;
	}
	get_julia_attractor (0.0, 0.0);      /* an attractor? */
	return 1;
}

int julia_trig_or_trig_setup(void)
{
	/* default symmetry is XAXIS */
	g_long_parameter = &g_parameter_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_parameter;
	if (g_parameter.y != 0.0)
	{
		g_symmetry = NOSYM;
	}
	if (g_trig_index[0] == 14 || g_trig_index[1] == 14) /* FLIP */
	{
		g_symmetry = NOSYM;
	}
	get_julia_attractor (0.0, 0.0);      /* an attractor? */
	return 1;
}

int mandelbrot_lambda_trig_or_trig_setup(void)
{ /* psuedo */
	/* default symmetry is XAXIS */
	g_long_parameter = &g_initial_z_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_initial_z;
	if (g_trig_index[0] == SQR)
	{
		g_symmetry = NOSYM;
	}
	if ((g_trig_index[0] == LOG) || (g_trig_index[1] == LOG))
	{
		g_symmetry = NOSYM;
	}
	return 1;
}

int mandelbrot_trig_or_trig_setup(void)
{
	/* default symmetry is XAXIS_NOPARM */
	g_long_parameter = &g_initial_z_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_initial_z;
	if ((g_trig_index[0] == 14) || (g_trig_index[1] == 14)) /* FLIP  JCO 5/28/92 */
	{
		g_symmetry = NOSYM;
	}
	return 1;
}


int z_trig_plus_z_setup(void)
{
	/*   static char ZXTrigPlusZSym1[] = */
	/* fn1 ->  sin   cos    sinh  cosh exp   log   sqr */
	/*           {XAXIS, XYAXIS, XAXIS, XYAXIS, XAXIS, NOSYM, XYAXIS}; */
	/*   static char ZXTrigPlusZSym2[] = */
	/* fn1 ->  sin   cos    sinh  cosh exp   log   sqr */
	/*           {NOSYM, ORIGIN, NOSYM, ORIGIN, NOSYM, NOSYM, ORIGIN}; */

	if (g_parameters[1] == 0.0 && g_parameters[3] == 0.0)
	{
		/*      symmetry = ZXTrigPlusZSym1[g_trig_index[0]]; */
		switch (g_trig_index[0])
		{
		case COS:   /* changed to two case statments and made any added */
		case COSH:  /* functions default to XAXIS symmetry. JCO 5/25/92 */
		case SQR:
		case 9:   /* 'real' cos */
			g_symmetry = XYAXIS;
			break;
		case 14:   /* FLIP  JCO 2/29/92 */
			g_symmetry = YAXIS;
			break;
		case LOG:
			g_symmetry = NOSYM;
			break;
		default:
			g_symmetry = XAXIS;
			break;
		}
	}
	else
	{
		/*      symmetry = ZXTrigPlusZSym2[g_trig_index[0]]; */
		switch (g_trig_index[0])
		{
		case COS:
		case COSH:
		case SQR:
		case 9:   /* 'real' cos */
			g_symmetry = ORIGIN;
			break;
		case 14:   /* FLIP  JCO 2/29/92 */
			g_symmetry = NOSYM;
			break;
		default:
			g_symmetry = NOSYM;
			break;
		}
	}
	if (g_current_fractal_specific->isinteger)
	{
		g_current_fractal_specific->orbitcalc =  z_trig_z_plus_z_orbit;
		if (g_parameter_l.x == g_fudge && g_parameter_l.y == 0L && g_parameter2_l.y == 0L
			&& g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL)
		{
			if (g_parameter2_l.x == g_fudge)     /* Scott variant */
			{
				g_current_fractal_specific->orbitcalc =  scott_z_trig_z_plus_z_orbit;
			}
			else if (g_parameter2_l.x == -g_fudge)  /* Skinner variant */
			{
				g_current_fractal_specific->orbitcalc =  skinner_z_trig_z_minus_z_orbit;
			}
		}
		return julia_setup_l();
	}
	else
	{
		g_current_fractal_specific->orbitcalc =  z_trig_z_plus_z_orbit_fp;
		if (g_parameter.x == 1.0 && g_parameter.y == 0.0 && g_parameter2.y == 0.0
			&& g_debug_flag != DEBUGFLAG_NO_ASM_MANDEL)
		{
			if (g_parameter2.x == 1.0)     /* Scott variant */
			{
				g_current_fractal_specific->orbitcalc =  scott_z_trig_z_plus_z_orbit_fp;
			}
			else if (g_parameter2.x == -1.0)       /* Skinner variant */
			{
				g_current_fractal_specific->orbitcalc =  skinner_z_trig_z_minus_z_orbit_fp;
			}
		}
	}
	return julia_setup_fp();
}

int lambda_trig_setup(void)
{
	int isinteger = g_current_fractal_specific->isinteger;
	g_current_fractal_specific->orbitcalc =  (isinteger != 0)
		? lambda_trig_orbit : lambda_trig_orbit_fp;
	switch (g_trig_index[0])
	{
	case SIN:
	case COS:
	case 9:   /* 'real' cos, added this and default for additional functions */
		g_symmetry = PI_SYM;
		g_current_fractal_specific->orbitcalc = 
			isinteger ? lambda_trig1_orbit : lambda_trig1_orbit_fp;
		break;
	case SINH:
	case COSH:
		g_symmetry = ORIGIN;
		g_current_fractal_specific->orbitcalc = 
			isinteger ? lambda_trig2_orbit : lambda_trig2_orbit_fp;
		break;
	case SQR:
		g_symmetry = ORIGIN;
		break;
	case EXP:
		g_current_fractal_specific->orbitcalc = 
			isinteger ? lambda_exponent_orbit : lambda_exponent_orbit_fp;
		g_symmetry = NOSYM; /* JCO 1/9/93 */
		break;
	case LOG:
		g_symmetry = NOSYM;
		break;
	default:   /* default for additional functions */
		g_symmetry = ORIGIN;  /* JCO 5/8/92 */
		break;
	}
	get_julia_attractor (0.0, 0.0);      /* an attractor? */
	return isinteger ? julia_setup_l() : julia_setup_fp();
}

int julia_fn_plus_z_squared_setup(void)
{
	/*   static char fnpluszsqrd[] = */
	/* fn1 ->  sin   cos    sinh  cosh   sqr    exp   log  */
	/* sin    {NOSYM, ORIGIN, NOSYM, ORIGIN, ORIGIN, NOSYM, NOSYM}; */

	/*   symmetry = fnpluszsqrd[g_trig_index[0]];   JCO  5/8/92 */
	switch (g_trig_index[0]) /* fix sqr symmetry & add additional functions */
	{
	case COS: /* cosxx */
	case COSH:
	case SQR:
	case 9:   /* 'real' cos */
	case 10:  /* tan */
	case 11:  /* tanh */
		g_symmetry = ORIGIN;
		/* default is for NOSYM symmetry */
	}
	return g_current_fractal_specific->isinteger ? julia_setup_l() : julia_setup_fp();
}

int sqr_trig_setup(void)
{
	/*   static char SqrTrigSym[] = */
	/* fn1 ->  sin    cos    sinh   cosh   sqr    exp   log  */
	/*           {PI_SYM, PI_SYM, XYAXIS, XYAXIS, XYAXIS, XAXIS, XAXIS}; */
	/*   symmetry = SqrTrigSym[g_trig_index[0]];      JCO  5/9/92 */
	switch (g_trig_index[0]) /* fix sqr symmetry & add additional functions */
	{
	case SIN:
	case COS: /* cosxx */
	case 9:   /* 'real' cos */
		g_symmetry = PI_SYM;
		/* default is for XAXIS symmetry */
	}
	return g_current_fractal_specific->isinteger ? julia_setup_l() : julia_setup_fp();
}

int fn_fn_setup(void)
{
	static char fnxfn[7][7] =
	{/* fn2 ->sin     cos    sinh    cosh  exp    log    sqr */
	/* fn1 */
	/* sin */ {PI_SYM, YAXIS, XYAXIS, XYAXIS, XAXIS, NOSYM, XYAXIS},
	/* cos */ {YAXIS, PI_SYM, XYAXIS, XYAXIS, XAXIS, NOSYM, XYAXIS},
	/* sinh*/ {XYAXIS, XYAXIS, XYAXIS, XYAXIS, XAXIS, NOSYM, XYAXIS},
	/* cosh*/ {XYAXIS, XYAXIS, XYAXIS, XYAXIS, XAXIS, NOSYM, XYAXIS},
	/* exp */ {XAXIS, XAXIS, XAXIS, XAXIS, XAXIS, NOSYM, XYAXIS},
	/* log */ {NOSYM, NOSYM, NOSYM, NOSYM, NOSYM, XAXIS, NOSYM},
	/* sqr */ {XYAXIS, XYAXIS, XYAXIS, XYAXIS, XYAXIS, NOSYM, XYAXIS},
	};
	/*
	if (g_trig_index[0] == EXP || g_trig_index[0] == LOG || g_trig_index[1] == EXP || g_trig_index[1] == LOG)
		g_symmetry = XAXIS;
	else if ((g_trig_index[0] == SIN && g_trig_index[1] == SIN) || (g_trig_index[0] == COS && g_trig_index[1] == COS))
		g_symmetry = PI_SYM;
	else if ((g_trig_index[0] == SIN && g_trig_index[1] == COS) || (g_trig_index[0] == COS && g_trig_index[1] == SIN))
		g_symmetry = YAXIS;
	else
		g_symmetry = XYAXIS;
	*/
	if (g_trig_index[0] < 7 && g_trig_index[1] < 7)  /* bounds of array JCO 5/22/92*/
	{
		g_symmetry = fnxfn[g_trig_index[0]][g_trig_index[1]];  /* JCO 5/22/92 */
	}
	/* defaults to XAXIS symmetry JCO 5/22/92 */
	else  /* added to complete the symmetry JCO 5/22/92 */
	{
		if (g_trig_index[0] == LOG || g_trig_index[1] == LOG)
		{
			g_symmetry = NOSYM;
		}
		if (g_trig_index[0] == 9 || g_trig_index[1] == 9)  /* 'real' cos */
		{
			if (g_trig_index[0] == SIN || g_trig_index[1] == SIN)
			{
				g_symmetry = PI_SYM;
			}
			if (g_trig_index[0] == COS || g_trig_index[1] == COS)
			{
				g_symmetry = PI_SYM;
			}
		}
		if (g_trig_index[0] == 9 && g_trig_index[1] == 9)
		{
			g_symmetry = PI_SYM;
		}
	}
	return g_current_fractal_specific->isinteger ? julia_setup_l() : julia_setup_fp();
}

int mandelbrot_trig_setup(void)
{
	int isinteger = g_current_fractal_specific->isinteger;
	g_current_fractal_specific->orbitcalc = 
		isinteger ? lambda_trig_orbit : lambda_trig_orbit_fp;
	g_symmetry = XYAXIS_NOPARM;
	switch (g_trig_index[0])
	{
	case SIN:
	case COS:
		g_current_fractal_specific->orbitcalc = 
			isinteger ? lambda_trig1_orbit : lambda_trig1_orbit_fp;
		break;
	case SINH:
	case COSH:
		g_current_fractal_specific->orbitcalc = 
			isinteger ? lambda_trig2_orbit : lambda_trig2_orbit_fp;
		break;
	case EXP:
		g_symmetry = XAXIS_NOPARM;
		g_current_fractal_specific->orbitcalc = 
			isinteger ? lambda_exponent_orbit : lambda_exponent_orbit_fp;
		break;
	case LOG:
		g_symmetry = XAXIS_NOPARM;
		break;
	default:   /* added for additional functions, JCO 5/25/92 */
		g_symmetry = XYAXIS_NOPARM;
		break;
	}
	return isinteger ? mandelbrot_setup_l() : mandelbrot_setup_fp();
}

int marks_julia_setup(void)
{
#if !defined(XFRACT)
	if (g_parameters[2] < 1)
	{
		g_parameters[2] = 1;
	}
	g_c_exp = (int)g_parameters[2];
	g_long_parameter = &g_parameter_l;
	g_old_z_l = *g_long_parameter;
	if (g_c_exp > 3)
	{
		complex_power_l(&g_old_z_l, g_c_exp-1, &g_coefficient_l, g_bit_shift);
	}
	else if (g_c_exp == 3)
	{
		g_coefficient_l.x = multiply(g_old_z_l.x, g_old_z_l.x, g_bit_shift) - multiply(g_old_z_l.y, g_old_z_l.y, g_bit_shift);
		g_coefficient_l.y = multiply(g_old_z_l.x, g_old_z_l.y, g_bit_shift_minus_1);
	}
	else if (g_c_exp == 2)
	{
		g_coefficient_l = g_old_z_l;
	}
	else if (g_c_exp < 2)
	{
		g_coefficient_l.x = 1L << g_bit_shift;
		g_coefficient_l.y = 0L;
	}
	get_julia_attractor (0.0, 0.0);      /* an attractor? */
#endif
	return 1;
}

int marks_julia_setup_fp(void)
{
	if (g_parameters[2] < 1)
	{
		g_parameters[2] = 1;
	}
	g_c_exp = (int)g_parameters[2];
	g_float_parameter = &g_parameter;
	g_old_z = *g_float_parameter;
	if (g_c_exp > 3)
	{
		complex_power(&g_old_z, g_c_exp-1, &g_coefficient);
	}
	else if (g_c_exp == 3)
	{
		g_coefficient.x = sqr(g_old_z.x) - sqr(g_old_z.y);
		g_coefficient.y = g_old_z.x*g_old_z.y*2;
	}
	else if (g_c_exp == 2)
	{
		g_coefficient = g_old_z;
	}
	else if (g_c_exp < 2)
	{
		g_coefficient.x = 1.0;
		g_coefficient.y = 0.0;
	}
	get_julia_attractor (0.0, 0.0);      /* an attractor? */
	return 1;
}

int sierpinski_setup(void)
{
	/* sierpinski */
	g_periodicity_check = 0;                /* disable periodicity checks */
	g_tmp_z_l.x = 1;
	g_tmp_z_l.x = g_tmp_z_l.x << g_bit_shift; /* g_tmp_z_l.x = 1 */
	g_tmp_z_l.y = g_tmp_z_l.x >> 1;                        /* g_tmp_z_l.y = .5 */
	return 1;
}

int sierpinski_setup_fp(void)
{
	/* sierpinski */
	g_periodicity_check = 0;                /* disable periodicity checks */
	g_temp_z.x = 1;
	g_temp_z.y = 0.5;
	return 1;
}

int phoenix_setup(void)
{
	g_long_parameter = &g_parameter_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_parameter;
	g_degree = (int)g_parameter2.x;
	if (g_degree < 2 && g_degree > -3)
	{
		g_degree = 0;
	}
	g_parameters[2] = (double)g_degree;
	if (g_degree == 0)
	{
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_orbit_fp : phoenix_orbit;
	}
	if (g_degree >= 2)
	{
		g_degree = g_degree - 1;
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_plus_orbit_fp : phoenix_plus_orbit;
	}
	if (g_degree <= -3)
	{
		g_degree = abs(g_degree) - 2;
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_minus_orbit_fp : phoenix_minus_orbit;
	}

	return 1;
}

int phoenix_complex_setup(void)
{
	g_long_parameter = &g_parameter_l;
	g_float_parameter = &g_parameter;
	g_degree = (int)g_parameters[4];
	if (g_degree < 2 && g_degree > -3)
	{
		g_degree = 0;
	}
	g_parameters[4] = (double)g_degree;
	if (g_degree == 0)
	{
		g_symmetry = (g_parameter2.x != 0 || g_parameter2.y != 0) ? NOSYM : ORIGIN;
		if (g_parameter.y == 0 && g_parameter2.y == 0)
		{
			g_symmetry = XAXIS;
		}
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_complex_orbit_fp : phoenix_complex_orbit;
	}
	if (g_degree >= 2)
	{
		g_degree = g_degree - 1;
		g_symmetry = (g_parameter.y == 0 && g_parameter2.y == 0) ? XAXIS : NOSYM;
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_complex_plus_orbit_fp : phoenix_complex_plus_orbit;
	}
	if (g_degree <= -3)
	{
		g_degree = abs(g_degree) - 2;
		g_symmetry = (g_parameter.y == 0 && g_parameter2.y == 0) ? XAXIS : NOSYM;
		g_current_fractal_specific->orbitcalc = g_user_float_flag ?
			phoenix_complex_minus_orbit_fp : phoenix_complex_minus_orbit;
	}

	return 1;
}

int mandelbrot_phoenix_setup(void)
{
	g_long_parameter = &g_initial_z_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_initial_z;
	g_degree = (int)g_parameter2.x;
	if (g_degree < 2 && g_degree > -3)
	{
		g_degree = 0;
	}
	g_parameters[2] = (double)g_degree;
	if (g_degree == 0)
	{
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_orbit_fp : phoenix_orbit;
	}
	if (g_degree >= 2)
	{
		g_degree = g_degree - 1;
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_plus_orbit_fp : phoenix_plus_orbit;
	}
	if (g_degree <= -3)
	{
		g_degree = abs(g_degree) - 2;
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_minus_orbit_fp : phoenix_minus_orbit;
	}

	return 1;
}

int mandelbrot_phoenix_complex_setup(void)
{
	g_long_parameter = &g_initial_z_l; /* added to consolidate code 10/1/92 JCO */
	g_float_parameter = &g_initial_z;
	g_degree = (int)g_parameters[4];
	if (g_degree < 2 && g_degree > -3)
	{
		g_degree = 0;
	}
	g_parameters[4] = (double)g_degree;
	if (g_parameter.y != 0 || g_parameter2.y != 0)
	{
		g_symmetry = NOSYM;
	}
	if (g_degree == 0)
	{
		g_current_fractal_specific->orbitcalc = 
			g_user_float_flag ? phoenix_complex_orbit_fp : phoenix_complex_orbit;
	}
	if (g_degree >= 2)
	{
		g_degree = g_degree - 1;
		g_current_fractal_specific->orbitcalc =
			g_user_float_flag ? phoenix_complex_plus_orbit_fp : phoenix_complex_plus_orbit;
	}
	if (g_degree <= -3)
	{
		g_degree = abs(g_degree) - 2;
		g_current_fractal_specific->orbitcalc =
			g_user_float_flag ? phoenix_complex_minus_orbit_fp : phoenix_complex_minus_orbit;
	}

	return 1;
}

int standard_setup(void)
{
	if (g_fractal_type == UNITYFP)
	{
		g_periodicity_check = 0;
	}
	return 1;
}

int volterra_lotka_setup(void)
{
	if (g_parameters[0] < 0.0)
	{
		g_parameters[0] = 0.0;
	}
	if (g_parameters[1] < 0.0)
	{
		g_parameters[1] = 0.0;
	}
	if (g_parameters[0] > 1.0)
	{
		g_parameters[0] = 1.0;
	}
	if (g_parameters[1] > 1.0)
	{
		g_parameters[1] = 1.0;
	}
	g_float_parameter = &g_parameter;
	return 1;
}