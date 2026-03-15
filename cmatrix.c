#include <libdragon.h>

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 240u

#define FONT_JETBRAINS_MONO_BOLD 1

#define TICKRATE 10u

static const float tickrate_sec = 1.f / (float)TICKRATE;

static const unsigned int text_pad_x = SCREEN_WIDTH / 4;
static const unsigned int text_pad_y = SCREEN_WIDTH / 4;

static const rdpq_textparms_t text_params = {
	.align	= ALIGN_CENTER,
	.valign = VALIGN_CENTER,
	.width	= SCREEN_WIDTH - text_pad_x,
	.height = SCREEN_HEIGHT - text_pad_y,
	.wrap	= WRAP_WORD
};

static const char text[] =
		"Two households, both alike in dignity, in "
		"fair Verona, where we lay our scene, from "
		"ancient grudge break to new mutiny, where "
		"civil blood makes civil hands unclean. From "
		"forth the fatal loins of these two foes a "
		"pair of star-cross'd lovers take their life.";
static int text_len = sizeof(text) - 1;

static rdpq_font_t	     *fnt	= NULL;
static const rdpq_fontstyle_t fnt_style = {
	.color = RGBA32(0xED, 0xAE, 0x49, 0xFF)
};

static unsigned int text_progress = 0;
static float	    text_timer	  = 0.f;
static float	    time_accum	  = 0.f;

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
		unsigned long	 ticks_old, ticks_new;
		float		 time_diff;

		ticks_old = get_ticks();

		/* Render */
		rdpq_attach_clear(display_get(), NULL);

		rdpq_set_mode_fill(RGBA16(0x3, 0x6, 0x9, 0x1F));
		rdpq_fill_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

		rdpq_text_printn(&text_params,
				 FONT_JETBRAINS_MONO_BOLD,
				 text_pad_x / 2,
				 text_pad_y / 2,
				 text,
				 text_progress);

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

			/* Reset text progress with start button */
			if (btn_down.start) {
				text_progress = 0;
				continue;
			}

			/* Advance text */
			if (text_progress < text_len) {
				++text_progress;
				continue;
			}

			/* Cap text at maximum length */
			text_progress = text_len;
		}
	}

	/* Terminate */
	rdpq_text_unregister_font(FONT_JETBRAINS_MONO_BOLD);
	rdpq_font_free(fnt);
	joypad_close();
	rdpq_close();
	display_close();
}
