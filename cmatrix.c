#include <libdragon.h>

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 240u

#define FONT_JETBRAINS_MONO 1

static const rdpq_textparms_t text_params = { .align  = ALIGN_CENTER,
					      .valign = VALIGN_CENTER,
					      .width  = SCREEN_WIDTH,
					      .height = SCREEN_HEIGHT,
					      .wrap   = WRAP_WORD };

static const char text[] =
		"Two households, both alike in dignity,\n"
		"In fair Verona, where we lay our scene,\n"
		"From ancient grudge break to new mutiny,\n"
		"Where civil blood makes civil hands unclean.\n"
		"From forth the fatal loins of these two foes\n"
		"A pair of star-cross'd lovers take their life;\n";
static int text_len = sizeof(text) - 1;

static rdpq_font_t	     *fnt	= NULL;
static const rdpq_fontstyle_t fnt_style = {
	.color = RGBA32(0xED, 0xAE, 0x49, 0xFF)
};

int main(void)
{
	bool start_pressed = false;

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
	fnt = rdpq_font_load("rom:/Pacifico.font64");
	rdpq_font_style(fnt, 0, &fnt_style);
	rdpq_text_register_font(FONT_JETBRAINS_MONO, fnt);

	/* Main loop */
	for (; !start_pressed;) {
		rdpq_paragraph_t *par;
		joypad_buttons_t  btn_down;

		/* Render */
		rdpq_attach_clear(display_get(), NULL);

		rdpq_set_mode_fill(RGBA16(0x3, 0x6, 0x9, 0x1F));
		rdpq_fill_rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

		par = rdpq_paragraph_build(&text_params,
					   FONT_JETBRAINS_MONO,
					   text,
					   &text_len);
		rdpq_paragraph_render(par, 0, 0);
		rdpq_paragraph_free(par);

		rdpq_set_mode_standard();
		rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
		rdpq_mode_combiner(RDPQ_COMBINER_FLAT);

		rdpq_detach_show();

		/* Update */
		joypad_poll();
		btn_down      = joypad_get_buttons_pressed(JOYPAD_PORT_1);
		start_pressed = btn_down.start;
	}

	/* Terminate */
	rdpq_text_unregister_font(FONT_JETBRAINS_MONO);
	rdpq_font_free(fnt);
	joypad_close();
	rdpq_close();
	display_close();
}
