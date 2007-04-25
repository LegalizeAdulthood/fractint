/* d_win32.c
 *
 * Routines for a Win32 GDI driver for fractint.
 */
#include <assert.h>
#include <stdio.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <windows.h>

#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "helpdefs.h"
#include "drivers.h"
#include "fihelp.h"

#include "WinText.h"
#include "frame.h"
#include "plot.h"
#include "d_win32.h"
#include "ods.h"

extern HINSTANCE g_instance;

#define DRAW_INTERVAL 6
#define TIMER_ID 1

class GDIDriver : public Win32BaseDriver
{
public:
	GDIDriver(const char *name, const char *description)
		: Win32BaseDriver(name, description),
		m_plot(),
		m_text_not_graphics(true)
	{
	}

	virtual int initialize(int *argc, char **argv);
	virtual void terminate();
	virtual void get_max_screen(int &x_max, int &y_max) const;
	virtual int resize();
	virtual int read_palette();
	virtual int write_palette();
	virtual void schedule_alarm(int soon);
	virtual void write_pixel(int x, int y, int color);
	virtual int read_pixel(int x, int y);
	virtual void read_span(int y, int x, int lastx, BYTE *pixels);
	virtual void write_span(int y, int x, int lastx, const BYTE *pixels);
	virtual void set_line_mode(int mode);
	virtual void draw_line(int x1, int y1, int x2, int y2, int color);
	virtual void redraw();
	virtual void window();
	virtual void set_for_text();
	virtual void set_for_graphics();
	virtual void set_clear();
	virtual void set_video_mode(const VIDEOINFO &mode);
	virtual int validate_mode(const VIDEOINFO &mode);
	virtual void pause();
	virtual void resume();
	virtual void display_string(int x, int y, int fg, int bg, const char *text);
	virtual void save_graphics();
	virtual void restore_graphics();
	virtual void flush();
	virtual void get_truecolor(int x, int y, int &r, int &g, int &b, int &a)	{}
	virtual void put_truecolor(int x, int y, int r, int g, int b, int a)		{}

private:
	int check_arg(char *arg);
	void show_hide_windows(HWND show, HWND hide);
	void max_size(int &width, int &height, bool &center_x, bool &center_y);
	void center_windows(bool center_x, bool center_y);

	Plot m_plot;
	bool m_text_not_graphics;

	static VIDEOINFO s_modes[];
};

/* VIDEOINFO:															*/
/*         char    name[26];       Adapter name (IBM EGA, etc)          */
/*         char    comment[26];    Comments (UNTESTED, etc)             */
/*         int     keynum;         key number used to invoked this mode */
/*                                 2-10 = F2-10, 11-40 = S, C, A{F1-F10}  */
/*         int     videomodeax;    begin with INT 10H, AX=(this)        */
/*         int     videomodebx;                 ...and BX=(this)        */
/*         int     videomodecx;                 ...and CX=(this)        */
/*         int     videomodedx;                 ...and DX=(this)        */
/*                                 NOTE:  IF AX==BX==CX==0, SEE BELOW   */
/*         int     dotmode;        video access method used by asm code */
/*                                      1 == BIOS 10H, AH=12, 13 (SLOW)  */
/*                                      2 == access like EGA/VGA        */
/*                                      3 == access like MCGA           */
/*                                      4 == Tseng-like  SuperVGA*256   */
/*                                      5 == P'dise-like SuperVGA*256   */
/*                                      6 == Vega-like   SuperVGA*256   */
/*                                      7 == "Tweaked" IBM-VGA ...*256  */
/*                                      8 == "Tweaked" SuperVGA ...*256 */
/*                                      9 == Targa Format               */
/*                                      10 = Hercules                   */
/*                                      11 = "disk video" (no screen)   */
/*                                      12 = 8514/A                     */
/*                                      13 = CGA 320x200x4, 640x200x2   */
/*                                      14 = Tandy 1000                 */
/*                                      15 = TRIDENT  SuperVGA*256      */
/*                                      16 = Chips&Tech SuperVGA*256    */
/*         int     g_x_dots;          number of dots across the screen     */
/*         int     g_y_dots;          number of dots down the screen       */
/*         int     g_colors;         number of g_colors available           */

#define DRIVER_MODE(name_, comment_, key_, width_, height_, mode_) \
	{ name_, comment_, key_, 0, 0, 0, 0, mode_, width_, height_, 256 }
#define MODE19(n_, c_, k_, w_, h_) DRIVER_MODE(n_, c_, k_, w_, h_, 19)
VIDEOINFO GDIDriver::s_modes[] =
{
	MODE19("Win32 GDI Video          ", "                        ", 0,  320,  240),
	MODE19("Win32 GDI Video          ", "                        ", 0,  400,  300),
	MODE19("Win32 GDI Video          ", "                        ", 0,  480,  360),
	MODE19("Win32 GDI Video          ", "                        ", 0,  600,  450),
	MODE19("Win32 GDI Video          ", "                        ", 0,  640,  480),
	MODE19("Win32 GDI Video          ", "                        ", 0,  800,  600),
	MODE19("Win32 GDI Video          ", "                        ", 0, 1024,  768),
	MODE19("Win32 GDI Video          ", "                        ", 0, 1200,  900),
	MODE19("Win32 GDI Video          ", "                        ", 0, 1280,  960),
	MODE19("Win32 GDI Video          ", "                        ", 0, 1400, 1050),
	MODE19("Win32 GDI Video          ", "                        ", 0, 1500, 1125),
	MODE19("Win32 GDI Video          ", "                        ", 0, 1600, 1200)
};
#undef MODE19
#undef DRIVER_MODE

/* check_arg
 *
 *	See if we want to do something with the argument.
 *
 * Results:
 *	Returns 1 if we parsed the argument.
 *
 * Side effects:
 *	Increments i if we use more than 1 argument.
 */
int GDIDriver::check_arg(char *arg)
{
	return 0;
}

/* handle_special_keys
 *
 * First, do some slideshow processing.  Then handle F1 and TAB display.
 *
 * Because we want context sensitive help to work everywhere, with the
 * help to display indicated by a non-zero value, we need
 * to trap the F1 key at a very low level.  The same is true of the
 * TAB display.
 *
 * What we do here is check for these keys and invoke their displays.
 * To avoid a recursive invoke of help(), a static is used to avoid
 * recursing on ourselves as help will invoke get key!
 */
static int handle_special_keys(int ch)
{
	static int inside_help = 0;

	if (SLIDES_PLAY == g_slides)
	{
		if (ch == FIK_ESC)
		{
			stop_slide_show();
			ch = 0;
		}
		else if (!ch)
		{
			ch = slide_show();
		}
	}
	else if ((SLIDES_RECORD == g_slides) && ch)
	{
		record_show(ch);
	}

	if (FIK_F1 == ch && get_help_mode() && !inside_help)
	{
		inside_help = 1;
		help(0);
		inside_help = 0;
		ch = 0;
	}
	else if (FIK_TAB == ch && g_tab_mode)
	{
		int old_tab = g_tab_mode;
		g_tab_mode = 0;
		tab_display();
		g_tab_mode = old_tab;
		ch = 0;
	}

	return ch;
}

static void parse_geometry(const char *spec, int *x, int *y, int *width, int *height)
{
	/* do something like XParseGeometry() */
	if (2 == sscanf(spec, "%dx%d", width, height))
	{
		/* all we care about is width and height for disk output */
		*x = 0;
		*y = 0;
	}
}

void GDIDriver::show_hide_windows(HWND show, HWND hide)
{
	ShowWindow(show, SW_NORMAL);
	ShowWindow(hide, SW_HIDE);
}

void GDIDriver::max_size(int &width, int &height, bool &center_x, bool &center_y)
{
	width = m_wintext.max_width;
	height = m_wintext.max_height;
	if (g_video_table[g_adapter].x_dots > width)
	{
		width = g_video_table[g_adapter].x_dots;
		center_x = FALSE;
	}
	if (g_video_table[g_adapter].y_dots > height)
	{
		height = g_video_table[g_adapter].y_dots;
		center_y = FALSE;
	}
}

void GDIDriver::center_windows(bool center_x, bool center_y)
{
	POINT text_pos = { 0 }, plot_pos = { 0 };

	if (center_x)
	{
		plot_pos.x = (g_frame.width - m_plot.width)/2;
	}
	else
	{
		text_pos.x = (g_frame.width - m_wintext.max_width)/2;
	}

	if (center_y)
	{
		plot_pos.y = (g_frame.height - m_plot.height)/2;
	}
	else
	{
		text_pos.y = (g_frame.height - m_wintext.max_height)/2;
	}

	BOOL status = SetWindowPos(m_plot.window, NULL,
		plot_pos.x, plot_pos.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	_ASSERTE(status);
	status = SetWindowPos(m_wintext.hWndCopy, NULL,
		text_pos.x, text_pos.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	_ASSERTE(status);
}

/***********************************************************************
////////////////////////////////////////////////////////////////////////
\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
***********************************************************************/

/*----------------------------------------------------------------------
*
* terminate --
*
*	Cleanup windows and stuff.
*
* Results:
*	None.
*
* Side effects:
*	Cleans up.
*
*----------------------------------------------------------------------
*/
void GDIDriver::terminate()
{
	ODS("gdi_terminate");

	plot_terminate(&m_plot);
	Win32BaseDriver::terminate();
}

void GDIDriver::get_max_screen(int &x_max, int &y_max) const
{
	RECT desktop;
	::GetClientRect(::GetDesktopWindow(), &desktop);
	desktop.right -= ::GetSystemMetrics(SM_CXFRAME)*2;
	desktop.bottom -= ::GetSystemMetrics(SM_CYFRAME)*2
		+ ::GetSystemMetrics(SM_CYCAPTION) - 1;

	x_max = desktop.right;
	y_max = desktop.bottom;
}

/*----------------------------------------------------------------------
*
* init --
*
*	Initialize the windows and stuff.
*
* Results:
*	None.
*
* Side effects:
*	Initializes windows.
*
*----------------------------------------------------------------------
*/
int GDIDriver::initialize(int *argc, char **argv)
{
	LPCSTR title = "FractInt for Windows";

	ODS("gdi_init");
	if (!Win32BaseDriver::initialize(argc, argv))
	{
		return FALSE;
	}

	plot_init(&m_plot, g_instance, "Plot");

	/* filter out driver arguments */
	for (int i = 0; i < *argc; i++)
	{
		if (check_arg(argv[i]))
		{
			int j;
			for (j = i; j < *argc-1; j++)
			{
				argv[j] = argv[j + 1];
			}
			argv[j] = NULL;
			--*argc;
		}
	}

	/* add default list of video modes */
	int width, height;
	get_max_screen(width, height);

	for (int m = 0; m < NUM_OF(s_modes); m++)
	{
		if ((s_modes[m].x_dots <= width) &&
			(s_modes[m].y_dots <= height))
		{
			add_video_mode(this, s_modes[m]);
		}
	}

	return TRUE;
}

/* resize
 *
 * Check if we need resizing.  If no, return 0.
 * If yes, resize internal buffers and return 1.
 */
int GDIDriver::resize()
{
	int width, height;
	bool center_graphics_x, center_graphics_y;

	max_size(width, height, center_graphics_x, center_graphics_y);
	if ((g_video_table[g_adapter].x_dots == m_plot.width)
		&& (g_video_table[g_adapter].y_dots == m_plot.height)
		&& (width == g_frame.width)
		&& (height == g_frame.height))
	{
		return 0;
	}

	frame_resize(width, height);
	plot_resize(&m_plot);
	center_windows(center_graphics_x, center_graphics_y);
	return 1;
}


/*----------------------------------------------------------------------
* read_palette
*
*	Reads the current video palette into g_dac_box.
*
*
* Results:
*	None.
*
* Side effects:
*	Fills in g_dac_box.
*
*----------------------------------------------------------------------
*/
int GDIDriver::read_palette()
{
	return plot_read_palette(&m_plot);
}

/*
*----------------------------------------------------------------------
*
* write_palette --
*	Writes g_dac_box into the video palette.
*
*
* Results:
*	None.
*
* Side effects:
*	Changes the displayed g_colors.
*
*----------------------------------------------------------------------
*/
int GDIDriver::write_palette()
{
	return plot_write_palette(&m_plot);
}

/*
*----------------------------------------------------------------------
*
* schedule_alarm --
*
*	Start the refresh alarm
*
* Results:
*	None.
*
* Side effects:
*	Starts the alarm.
*
*----------------------------------------------------------------------
*/
void GDIDriver::schedule_alarm(int soon)
{
	soon = (soon ? 1 : DRAW_INTERVAL)*1000;
	if (m_text_not_graphics)
	{
		wintext_schedule_alarm(&m_wintext, soon);
	}
	else
	{
		plot_schedule_alarm(&m_plot, soon);
	}
}

/*
*----------------------------------------------------------------------
*
* write_pixel --
*
*	Write a point to the screen
*
* Results:
*	None.
*
* Side effects:
*	Draws point.
*
*----------------------------------------------------------------------
*/
void GDIDriver::write_pixel(int x, int y, int color)
{
	plot_write_pixel(&m_plot, x, y, color);
}

/*
*----------------------------------------------------------------------
*
* read_pixel --
*
*	Read a point from the screen
*
* Results:
*	Value of point.
*
* Side effects:
*	None.
*
*----------------------------------------------------------------------
*/
int GDIDriver::read_pixel(int x, int y)
{
	return plot_read_pixel(&m_plot, x, y);
}

/*
*----------------------------------------------------------------------
*
* write_span --
*
*	Write a line of pixels to the screen.
*
* Results:
*	None.
*
* Side effects:
*	Draws pixels.
*
*----------------------------------------------------------------------
*/
void GDIDriver::write_span(int y, int x, int lastx, const BYTE *pixels)
{
	plot_write_span(&m_plot, x, y, lastx, pixels);
}

/*
*----------------------------------------------------------------------
*
* read_span --
*
*	Reads a line of pixels from the screen.
*
* Results:
*	None.
*
* Side effects:
*	Gets pixels
*
*----------------------------------------------------------------------
*/
void GDIDriver::read_span(int y, int x, int lastx, BYTE *pixels)
{
	plot_read_span(&m_plot, y, x, lastx, pixels);
}

void GDIDriver::set_line_mode(int mode)
{
	plot_set_line_mode(&m_plot, mode);
}

void GDIDriver::draw_line(int x1, int y1, int x2, int y2, int color)
{
	plot_draw_line(&m_plot, x1, y1, x2, y2, color);
}

/*
*----------------------------------------------------------------------
*
* redraw --
*
*	Refresh the screen.
*
* Results:
*	None.
*
* Side effects:
*	Redraws the screen.
*
*----------------------------------------------------------------------
*/
void GDIDriver::redraw()
{
	ODS("gdi_redraw");
	if (m_text_not_graphics)
	{
		wintext_paintscreen(&m_wintext, 0, 80, 0, 25);
	}
	else
	{
		plot_redraw(&m_plot);
	}
	frame_pump_messages(FALSE);
}

void GDIDriver::window()
{
	int width;
	int height;
	bool center_x, center_y;

	max_size(width, height, center_x, center_y);
	frame_window(width, height);
	m_wintext.hWndParent = g_frame.window;
	wintext_texton(&m_wintext);
	plot_window(&m_plot, g_frame.window);
	center_windows(center_x, center_y);
}

void GDIDriver::set_for_text()
{
	m_text_not_graphics = true;
	show_hide_windows(m_wintext.hWndCopy, m_plot.window);
}

void GDIDriver::set_for_graphics()
{
	m_text_not_graphics = false;
	show_hide_windows(m_plot.window, m_wintext.hWndCopy);
	Win32BaseDriver::hide_text_cursor();
}

/* set_clear
*/
void GDIDriver::set_clear()
{
	if (m_text_not_graphics)
	{
		wintext_clear(&m_wintext);
	}
	else
	{
		plot_clear(&m_plot);
	}
}

/* set_video_mode
*/
void GDIDriver::set_video_mode(const VIDEOINFO &mode)
{
	extern void set_normal_dot(void);
	extern void set_normal_line(void);

	/* initially, set the virtual line to be the scan line length */
	g_vx_dots = g_screen_width;
	g_is_true_color = 0;				/* assume not truecolor */
	g_ok_to_print = FALSE;
	g_good_mode = 1;
	if (g_dot_mode != 0)
	{
		g_and_color = g_colors-1;
		g_box_count = 0;
		g_dac_learn = 1;
		g_dac_count = g_cycle_limit;
		g_got_real_dac = TRUE;			/* we are "VGA" */

		driver_read_palette();
	}

	resize();
	plot_clear(&m_plot);

	if (g_disk_flag)
	{
		disk_end();
	}

	set_normal_dot();
	set_normal_line();

	set_for_graphics();
	set_clear();
}

int GDIDriver::validate_mode(const VIDEOINFO &mode)
{
	int width, height;
	get_max_screen(width, height);

	/* allow modes <= size of screen with 256 g_colors and g_dot_mode = 19
	   ax/bx/cx/dx must be zero. */
	return (mode.x_dots <= width) &&
		(mode.y_dots <= height) &&
		(mode.colors == 256) &&
		(mode.videomodeax == 0) &&
		(mode.videomodebx == 0) &&
		(mode.videomodecx == 0) &&
		(mode.videomodedx == 0) &&
		(mode.dotmode == 19);
}

void GDIDriver::pause()
{
	if (m_wintext.hWndCopy)
	{
		ShowWindow(m_wintext.hWndCopy, SW_HIDE);
	}
	if (m_plot.window)
	{
		ShowWindow(m_plot.window, SW_HIDE);
	}
}

void GDIDriver::resume()
{
	if (!m_wintext.hWndCopy)
	{
		window();
	}

	ShowWindow(m_wintext.hWndCopy, SW_NORMAL);
	wintext_resume(&m_wintext);
}

void GDIDriver::display_string(int x, int y, int fg, int bg, const char *text)
{
	_ASSERTE(!m_text_not_graphics);
	plot_display_string(&m_plot, x, y, fg, bg, text);
}

void GDIDriver::save_graphics()
{
	plot_save_graphics(&m_plot);
}

void GDIDriver::restore_graphics()
{
	plot_restore_graphics(&m_plot);
}

void GDIDriver::flush()
{
	plot_flush(&m_plot);
}

static GDIDriver gdi_driver_info("gdi", "A GDI driver for 32-bit Windows.");
AbstractDriver *gdi_driver = &gdi_driver_info;
