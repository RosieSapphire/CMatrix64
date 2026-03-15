#include <libdragon.h>

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 240u

#define FONT_JETBRAINS_MONO_BOLD 1

#define TICKRATE 12u

static const float tickrate_sec = 1.f / (float)TICKRATE;

static const rdpq_textparms_t text_params = { .align  = ALIGN_LEFT,
					      .valign = VALIGN_TOP,
					      .width  = SCREEN_WIDTH,
					      .height = SCREEN_HEIGHT,
					      .wrap   = WRAP_WORD };

#define TEXT_DIM_X 41u
#define TEXT_DIM_Y 14u

static char text_buf[TEXT_DIM_X * TEXT_DIM_Y];

_Static_assert((sizeof(text_buf) % TEXT_DIM_X) == 0);
static const unsigned int text_buf_line_cnt = sizeof(text_buf) / TEXT_DIM_X;

enum { COLOR_RED = 0, COLOR_GRN, COLOR_BLU, COLOR_COUNT };

static rdpq_font_t	     *fnt		      = NULL;
static const rdpq_fontstyle_t fnt_styles[COLOR_COUNT] = {
	{ .color = RGBA32(0xFF, 0x00, 0x00, 0xFF) },
	{ .color = RGBA32(0x00, 0xFF, 0x00, 0xFF) },
	{ .color = RGBA32(0x00, 0x00, 0xFF, 0xFF) },
};
static signed int color_selected = COLOR_GRN;
static bool	  rainbow_mode	 = false;

static float time_accum = 0.f;

#if 0
static void text_buf_fill_overscan_test(char buf[TEXT_DIM_X * TEXT_DIM_Y])
{
	memcpy(buf,
	       (const char *)"+---------------------------------------+"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "|xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|"
			     "+---------------------------------------+",
	       TEXT_DIM_X * TEXT_DIM_Y);
}
#endif

struct stream {
	unsigned int length;
	signed int   progress;
	char	     chars[TEXT_DIM_Y];
};

static struct stream streams[TEXT_DIM_X];

static void buf_fill_random(char *buf, const size_t buf_sz)
{
	size_t i;

	/* Generate random array */
	getentropy(buf, buf_sz);

	/* Correct it */
	for (i = 0; i < buf_sz; ++i) {
		char c;

	buf_fill_random_retry:
		/* Generate a random number */
		c = getentropy32() & 0xFF;

		/* Make sure it's within the range of printable characters */
		while (c < 33)
			c += 16;
		while (c > 126)
			c -= 16;

		/*
		 * There's a few characters that fuck with the text
		 * printing in Libdragon, so we're excluding those.
		 */
		if (c == '$' || c == '^')
			goto buf_fill_random_retry;

		buf[i] = c;
	}
}

static void stream_init(struct stream *s)
{
	do {
		s->length = getentropy32() % TEXT_DIM_Y;
	} while (s->length < 3 || s->length > TEXT_DIM_Y - 2);
	s->progress = -(signed int)s->length;
	buf_fill_random(s->chars, TEXT_DIM_Y);
}

static __inline void streams_array_randomize(struct stream *arr,
					     const size_t   cnt)
{
	size_t i;

	for (i = 0; i < cnt; ++i) {
		stream_init(arr + i);
		arr[i].progress = (getentropy32() % TEXT_DIM_Y);
	}
}

static void stream_update(struct stream *s)
{
	/*
	 * Update progress, and if it's not over
	 * the limit, we're done for this frame.
	 */
	if (++s->progress < (signed int)TEXT_DIM_Y)
		return;

	/* Randomly generate a new stream, as it has reached its end */
	stream_init(s);
}

static void stream_to_text_buf(char buf[TEXT_DIM_X * TEXT_DIM_Y],
			       const struct stream *s,
			       const unsigned int   x)
{
	unsigned int l;
	signed int   i, p, pf, e;

	assertf(s->length <= TEXT_DIM_Y,
		"Stream length is too high (is %u; max is %u)\n",
		s->length,
		TEXT_DIM_Y);
	assertf(s->progress < (signed int)TEXT_DIM_Y,
		"Stream has already left screen (progress = %d)",
		s->progress);

	/* Get clamped length */
	p = s->progress;
	l = s->length;
	if (p + (signed int)l >= TEXT_DIM_Y)
		l = TEXT_DIM_Y - 1;

	/* Get clamped end */
	e = p + l;
	if (e > TEXT_DIM_Y)
		e = TEXT_DIM_Y;

	/* Padding */
	if (p - 1 >= 0)
		buf[(p - 1) * TEXT_DIM_X + x] = ' ';

	/* Line itself */
	pf = (p < 0) ? 0 : p;
	for (i = pf; i < e; ++i)
		buf[i * TEXT_DIM_X + x] = s->chars[i];

	/*
	 * Shitty retarded fucking hack to clear the last
	 * character of a row when a new one starts
	 */
	if (p < 0)
		buf[(TEXT_DIM_Y - 1) * TEXT_DIM_X + x] = ' ';
}

int main(void)
{
	/* N64 init */
	debug_init_isviewer();
	debug_init_usblog();

	dfs_init(DFS_DEFAULT_LOCATION);
	display_init(RESOLUTION_320x240,
		     DEPTH_16_BPP,
		     3,
		     GAMMA_NONE,
		     FILTERS_RESAMPLE);
	rdpq_init();
	joypad_init();

	/* Load, configure and register fonts */
	fnt = rdpq_font_load("rom:/jetbrains_mono_bold.font64");
	rdpq_font_style(fnt, 0, fnt_styles + color_selected);
	rdpq_text_register_font(FONT_JETBRAINS_MONO_BOLD, fnt);

	memset(text_buf, ' ', sizeof(text_buf));

	streams_array_randomize(streams, sizeof(streams) / sizeof(*streams));

	/* Main loop */
	for (;;) {
		unsigned long ticks_old, ticks_new;
		unsigned int  i;
		float	      time_diff;

		ticks_old = get_ticks();

		/* Render */
		rdpq_attach_clear(display_get(), NULL);

		/* Print the buffer out line by line */
		for (i = 0; i < text_buf_line_cnt; ++i) {
			rdpq_text_printn(&text_params,
					 FONT_JETBRAINS_MONO_BOLD,
					 16,
					 10 + (i * 16),
					 text_buf + (i * TEXT_DIM_X),
					 TEXT_DIM_X);
		}

		rdpq_set_mode_standard();
		rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
		rdpq_mode_combiner(RDPQ_COMBINER_FLAT);

		rdpq_detach_show();

		/* Calculate time */
		ticks_new = get_ticks();
		time_diff = (float)(ticks_new - ticks_old) /
			    (float)TICKS_PER_SECOND;
		time_accum += time_diff;

		/* Update */
		for (; time_accum >= tickrate_sec; time_accum -= tickrate_sec) {
			joypad_buttons_t    btn_down;
			unsigned int	    color_selected_old;

			joypad_poll();
			btn_down = joypad_get_buttons_pressed(JOYPAD_PORT_1);

			/* Toggle rainbow mode! */
			rainbow_mode ^= btn_down.l;

			for (i = 0; i < TEXT_DIM_X; ++i)
				stream_update(streams + i);

			color_selected_old = color_selected;
			if (rainbow_mode)
				++color_selected;
			else
				color_selected += (btn_down.c_right -
						   btn_down.c_left);

			if (color_selected == color_selected_old)
				continue;

			/* Clamp the color if it changed */
			if (color_selected < COLOR_RED)
				color_selected = COLOR_BLU;
			else if (color_selected > COLOR_BLU)
				color_selected = COLOR_RED;

			rdpq_font_style(fnt, 0, fnt_styles + color_selected);
		}

		for (i = 0; i < TEXT_DIM_X; ++i)
			stream_to_text_buf(text_buf, streams + i, i);
	}

	/* Terminate */
	rdpq_text_unregister_font(FONT_JETBRAINS_MONO_BOLD);
	rdpq_font_free(fnt);
	joypad_close();
	rdpq_close();
	display_close();
}
