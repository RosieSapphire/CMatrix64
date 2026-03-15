#include <libdragon.h>

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 240u

#define FONT_JETBRAINS_MONO_BOLD 1

#define TICKRATE 64u

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

static rdpq_font_t	     *fnt	= NULL;
static const rdpq_fontstyle_t fnt_style = {
	.color = RGBA32(0xED, 0xAE, 0x49, 0xFF)
};

static float time_accum = 0.f;

static void _text_buf_fill_overscan_test(char buf[TEXT_DIM_X * TEXT_DIM_Y])
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
	rdpq_font_style(fnt, 0, &fnt_style);
	rdpq_text_register_font(FONT_JETBRAINS_MONO_BOLD, fnt);

	/* Main loop */
	for (;;) {
		unsigned long ticks_old, ticks_new;
		unsigned int  i;
		float	      time_diff;

		ticks_old = get_ticks();

		/* Render */
		rdpq_attach_clear(display_get(), NULL);

		rdpq_set_mode_fill(RGBA16(0x3, 0x6, 0x9, 0x1F));
		rdpq_fill_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

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
			joypad_buttons_t btn_down;

			joypad_poll();
			btn_down = joypad_get_buttons_pressed(JOYPAD_PORT_1);

			_text_buf_fill_overscan_test(text_buf);
		}
	}

	/* Terminate */
	rdpq_text_unregister_font(FONT_JETBRAINS_MONO_BOLD);
	rdpq_font_free(fnt);
	joypad_close();
	rdpq_close();
	display_close();
}
