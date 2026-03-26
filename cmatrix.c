#include <libdragon.h>
#include <rp_assert.h>
#include <rp_types.h>

#ifdef HIGH_RES

#define SCREEN_WIDTH  640u
#define SCREEN_HEIGHT 480u

#define TEXT_DIM_X 82u
#define TEXT_DIM_Y 29u

#else /* #ifdef HIGH_RES */

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 240u

#define TEXT_DIM_X 41u
#define TEXT_DIM_Y 14u

#endif /* #ifdef HIGH_RES #else */

#define FONT_MAIN 1

#define TICKRATE	   64u
#define UPDATERATE_DEFAULT 12u
#define COLOR_CHANGE_RATE  120.f

enum { COLOR_RED = 0, COLOR_GRN, COLOR_BLU, COLOR_COUNT };

struct stream {
	u32  length;
	s32  progress;
	char chars[TEXT_DIM_Y];
};

static rdpq_font_t	     *fnt	  = NULL;
static const rdpq_textparms_t text_params = { .align  = ALIGN_LEFT,
					      .valign = VALIGN_TOP,
					      .width  = SCREEN_WIDTH,
					      .height = SCREEN_HEIGHT,
					      .wrap   = WRAP_WORD };

static char text_buf[TEXT_DIM_X * TEXT_DIM_Y];
RP_STATIC_ASSERT((sizeof(text_buf) % TEXT_DIM_X) == 0,
		 _text_buf_must_be_divisible_by_width);

static rspq_block_t *cmatrix_render_mode_base_dl = NULL;
static rspq_block_t *cmatrix_partial_clear_dl = NULL;

static u32 updaterate	  = UPDATERATE_DEFAULT;
static f32 updaterate_sec = 0.f;

#ifdef _DEBUG
static rspq_block_t *debug_mode_dl	    = NULL;
static bool_t	     debug_mode_blink_state = TRUE;
static f32	     debug_mode_blink_timer = 0.f;
#endif /* #ifdef _DEBUG */

static const f32 tickrate_sec	    = 1.f / (f32)TICKRATE;
static f32	 rainbow_timer_prev = 0.f;
static f32	 rainbow_timer_curr = 0.f;

static const rdpq_fontstyle_t fnt_styles[COLOR_COUNT] = {
	{ .color = RGBA32(0xFF, 0x00, 0x00, 0xFF) },
	{ .color = RGBA32(0x00, 0xFF, 0x00, 0xFF) },
	{ .color = RGBA32(0x00, 0x00, 0xFF, 0xFF) },
};

static s32    color_selected = COLOR_GRN;
static bool_t rainbow_mode   = FALSE;

static f32 time_accum = 0.f;

static struct stream streams[TEXT_DIM_X];

#ifdef _DEBUG
static rspq_block_t *debug_mode_dl_gen(void)
{
	static const color_t c = RGBA32(0xFF, 0x00, 0x00, 0xFF);

	static const struct {
		u16 x;
		u16 y;
		u16 w;
		u16 h;
	} r = { 248, 204, 46, 22 };

	static const rdpq_textparms_t tp = { .align  = ALIGN_CENTER,
					     .valign = VALIGN_CENTER,
					     .width  = r.w,
					     .height = r.h,
					     .wrap   = WRAP_WORD };

	static const rdpq_fontstyle_t fs = {
		.color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF)
	};

	rspq_block_begin();
	{
		rdpq_set_mode_fill(c);
		rdpq_fill_rectangle(r.x, r.y, r.x + r.w, r.y + r.h);
		rdpq_font_style(fnt, 0, &fs);
		rdpq_text_print(&tp, FONT_MAIN, r.x + 1, r.y, "DEBUG");
	}
	return rspq_block_end();
}
#endif /* #ifdef _DEBUG */

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
	} while (!s->length);
	s->progress = -(s32)s->length;
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
	if ((size_t)(s - streams) == (TEXT_DIM_X - 1)) {
		s->length   = TEXT_DIM_Y;
		s->progress = 0;
		memcpy(s->chars, "abcdefghijklmn", TEXT_DIM_Y);
		return;
	}

	/*
	 * Update progress, and if it's not over
	 * the limit, we're done for this frame.
	 */
	if (++s->progress < (s32)TEXT_DIM_Y)
		return;

	/* Randomly generate a new stream, as it has reached its end */
	stream_init(s);
}

static void stream_to_text_buf(char buf[TEXT_DIM_X * TEXT_DIM_Y],
			       const struct stream *s,
			       const u32	    x)
{
	u32 l;
	s32 i, p, pf, e;

	assertf(s->length <= TEXT_DIM_Y,
		"Stream length is too high (is %lu; max is %u)\n",
		s->length,
		TEXT_DIM_Y);
	assertf(s->progress < (s32)TEXT_DIM_Y,
		"Stream has already left screen (progress = %ld)",
		s->progress);

	/* Get clamped length */
	p = s->progress;
	l = s->length;
	if (p + l >= (s32)TEXT_DIM_Y)
		l = TEXT_DIM_Y - 1;

	/* Get clamped end */
	e = p + l;
	if (e > (s32)TEXT_DIM_Y)
		e = (s32)TEXT_DIM_Y;

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

static __inline rspq_block_t *cmatrix_render_mode_base_dl_gen(void)
{
	rspq_block_begin();
	{
		rdpq_mode_alphacompare(0);
		rdpq_mode_antialias(AA_NONE);
		rdpq_mode_dithering(DITHER_NOISE_NOISE);
		rdpq_mode_filter(FILTER_POINT);
		rdpq_mode_fog(0);
		rdpq_mode_mipmap(MIPMAP_NONE, 0);
		rdpq_mode_persp(false);
		rdpq_mode_zbuf(false, false);
	}
	return rspq_block_end();
}

static __inline rspq_block_t *cmatrix_partial_clear_dl_gen(void)
{
	rspq_block_begin();
	{
		rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
		rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY_CONST);
		rdpq_set_prim_color(color_from_packed32(0x0));
		rdpq_set_fog_color(color_from_packed32(0x60));
		rdpq_fill_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
	return rspq_block_end();
}

struct color_hsv {
	f32 h;
	f32 s;
	f32 v;
};

u32 rgba32_from_hsv(const struct color_hsv *hsv)
{
	f32 r = 0, g = 0, b = 0, hf, f, pv, qv, tv, h;
	u32 i;

	/* Ensure hue is within 0-360 degrees */
	h = hsv->h;
	while (h < 0)
		h += 360;
	while (h >= 360)
		h -= 360;

	if (hsv->v <= 0)
		return 0x0;

	/* Gray */
	if (hsv->s <= 0) {
		const u8 c = (u8)(hsv->v * 255.0f);

		return (c << 24) | (c << 16) | (c << 8) | 0xFF;
	}

	hf = h / 60.f;
	i  = (u32)floorf(hf);
	f  = hf - (f32)i;
	pv = hsv->v * (1 - hsv->s);
	qv = hsv->v * (1 - hsv->s * f);
	tv = hsv->v * (1 - hsv->s * (1 - f));

	switch (i) {
	case 0: /* Red is dominant */
		r = hsv->v;
		g = tv;
		b = pv;
		break;
	case 1: /* Green is dominant */
		r = qv;
		g = hsv->v;
		b = pv;
		break;
	case 2:
		r = pv;
		g = hsv->v;
		b = tv;
		break;
	case 3: /* Blue is dominant */
		r = pv;
		g = qv;
		b = hsv->v;
		break;
	case 4:
		r = tv;
		g = pv;
		b = hsv->v;
		break;
	case 5: /* Red is dominant again */
		r = hsv->v;
		g = pv;
		b = qv;
		break;
	default:
		assertf(0, "Hue is out of range!");
	}

	/* Convert to a u32 and return */
	return ((u8)(r * 255.0) << 24) | ((u8)(g * 255.0) << 16) |
	       ((u8)(b * 255.0) << 8) | 0xFF;
}

static void
cmatrix_buffer_render_to_screen(const char   buf[TEXT_DIM_X * TEXT_DIM_Y],
				const bool_t do_rainbow,
				const f32    timer_prev,
				const f32    timer_curr,
				const f32    subtick)
{
	f32    t;
	size_t i;

	t = timer_prev + (timer_curr - timer_prev) * subtick;

	rdpq_mode_combiner(RDPQ_COMBINER_TEX_FLAT);
	rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

	for (i = 0; i < TEXT_DIM_Y; ++i) {
		f32 fi;

		fi = (f32)i / TEXT_DIM_Y;

		if (do_rainbow) {
			struct color_hsv hsv;
			color_t		 col;
			rdpq_fontstyle_t s = { 0 };

			hsv.h	= fi * 360.f + t;
			hsv.s	= 1.f;
			hsv.v	= 1.f;
			col	= color_from_packed32(rgba32_from_hsv(&hsv));
			s.color = col;

			rdpq_font_style(fnt, 0, &s);
		} else {
			rdpq_font_style(fnt, 0, fnt_styles + color_selected);
		}

		rdpq_text_printn(&text_params,
				 FONT_MAIN,
				 16,            /* 38 */
				 10 + (i * 16), /*  4 */
				 buf + (i * TEXT_DIM_X),
				 TEXT_DIM_X);
	}
}

int main(void)
{
	/* N64 init */
#ifdef _DEBUG
	debug_init_isviewer();
	debug_init_usblog();
#endif /* #ifdef _DEBUG */

	dfs_init(DFS_DEFAULT_LOCATION);
#ifdef HIGH_RES
	display_init(RESOLUTION_640x480,
#else  /* #ifdef HIGH_RES */
	display_init(RESOLUTION_320x240,
#endif /* #ifdef HIGH_RES #else */
		     DEPTH_16_BPP,
		     3,
		     GAMMA_NONE,
		     FILTERS_RESAMPLE);
	rdpq_init();
#ifdef _DEBUG
	rdpq_debug_start();
#endif /* #ifdef _DEBUG */
	joypad_init();

	/* Load, configure and register fonts */
	fnt = rdpq_font_load("rom:/jetbrains_mono_bold.font64");
	rdpq_text_register_font(FONT_MAIN, fnt);

	memset(text_buf, ' ', sizeof(text_buf));

	streams_array_randomize(streams, sizeof(streams) / sizeof(*streams));
	updaterate_sec = 1.f / (f32)updaterate;

	/* Generate and verify displaylist */
	cmatrix_render_mode_base_dl = cmatrix_render_mode_base_dl_gen();
	cmatrix_partial_clear_dl = cmatrix_partial_clear_dl_gen();

#ifdef _DEBUG
	debug_mode_dl = debug_mode_dl_gen();
#endif /* #ifndef _DEBUG */

	assertf(cmatrix_render_mode_base_dl,
		"Failed to generate displaylist for the base render mode");
	assertf(cmatrix_partial_clear_dl,
		"Failed to generate displaylist for partially clearing screen");
#ifdef _DEBUG
	assertf(debug_mode_dl,
		"Failed to generate debug mode label displaylist");
#endif /* #ifdef _DEBUG */

	/* Main loop */
	for (;;) {
		static f32 subtick = 0.f;
		u64	   ticks_old, ticks_new;
		u32	   i;
		f32	   time_diff;

		ticks_old = get_ticks();

		/*************
		 * RENDERING *
		 *************/
		rdpq_attach(display_get(), NULL);

		rspq_block_run(cmatrix_render_mode_base_dl);
		rspq_block_run(cmatrix_partial_clear_dl);

		/*
		 * TODO:
		 * I would love to put this into a
		 * DL, but it doesn't seem to work.
		 */
		cmatrix_buffer_render_to_screen(text_buf,
						rainbow_mode,
						rainbow_timer_prev,
						rainbow_timer_curr,
						subtick);
#ifdef _DEBUG
		if (debug_mode_blink_state)
			rspq_block_run(debug_mode_dl);
#endif /* #ifdef _DEBUG */

		rdpq_detach_show();

		/* Calculate time */
		ticks_new = get_ticks();
		time_diff = (f32)(ticks_new - ticks_old) /
			    (f32)TICKS_PER_SECOND;
		time_accum += time_diff;

		/************
		 * UPDATING *
		 ************/
		for (; time_accum >= tickrate_sec; time_accum -= tickrate_sec) {
			static f32	 stream_timer = 0.f;
			f32		 dt;
			u32		 updaterate_old;
			s32		 color_selected_old;
			joypad_buttons_t btn_down;

			dt = tickrate_sec;

#ifdef _DEBUG
			debug_mode_blink_timer += dt;
			while ((u32)debug_mode_blink_timer) {
				debug_mode_blink_timer -=
						(u32)debug_mode_blink_timer;
				debug_mode_blink_state ^= TRUE;
			}
#endif /* #ifdef _DEBUG */

			joypad_poll();
			btn_down = joypad_get_buttons_pressed(JOYPAD_PORT_1);

			/* Reset to defaults with the start button */
			color_selected_old = color_selected;
			updaterate_old	   = updaterate;
			if (btn_down.start) {
				color_selected = COLOR_GRN;
				updaterate     = UPDATERATE_DEFAULT;
				rainbow_mode   = false;
			}

			/*
			 * TODO:
			 * Bubby wants me to make it go in reverse. qwq
			 */

			/* Change the speed of the streams */
			if (btn_down.c_up && updaterate < 64)
				updaterate += 4;

			if (btn_down.c_down && updaterate > 4)
				updaterate -= 4;

			if (updaterate_old != updaterate) {
				updaterate_sec = 1.f / (f32)updaterate;
				stream_timer   = 0.f;
			}

			/*
			 * TODO: When holding L, make it so that you can
			 * use the c-buttons to adjust the flashing speed
			 * of the rainbow mode. FUCKING EPILEPSY WARNING!!!
			 */

			/* Update the streams at a fixed rate */
			stream_timer += tickrate_sec;
			while (stream_timer > updaterate_sec) {
				stream_timer -= updaterate_sec;
				for (i = 0; i < TEXT_DIM_X; ++i)
					stream_update(streams + i);
			}

			/* Toggle rainbow mode! */
			rainbow_mode ^= btn_down.l;

			/* Update the rainbow mode timer if it's active */
			if (rainbow_mode) {
				rainbow_timer_prev = rainbow_timer_curr;
				rainbow_timer_curr -= dt * COLOR_CHANGE_RATE;
				continue;
			}

			/* Change color ONLY IF rainbow mode is not active */
			color_selected += (btn_down.c_right - btn_down.c_left);
			if (color_selected == color_selected_old)
				continue;

			/* Clamp the color if it changed */
			if (color_selected < COLOR_RED)
				color_selected = COLOR_BLU;
			else if (color_selected > COLOR_BLU)
				color_selected = COLOR_RED;
		}

		subtick = time_accum / tickrate_sec;

		for (i = 0; i < TEXT_DIM_X; ++i)
			stream_to_text_buf(text_buf, streams + i, i);
	}

	/* Terminate */
	rspq_block_free(cmatrix_partial_clear_dl);
	rspq_block_free(cmatrix_render_mode_base_dl);
	rdpq_text_unregister_font(FONT_MAIN);
	rdpq_font_free(fnt);
	joypad_close();
#ifdef _DEBUG
	rdpq_debug_stop();
#endif /* #ifdef _DEBUG */
	rdpq_close();
	display_close();
}
