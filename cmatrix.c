#include <libdragon.h>

#define SCREEN_WIDTH  320u
#define SCREEN_HEIGHT 240u

enum { FONT_PACIFICO = 1, FONT_ZEROVELOCITY };

static const unsigned int box_width = (unsigned int)((float)SCREEN_WIDTH * .8f);
static const unsigned int box_height =
		(unsigned int)((float)SCREEN_HEIGHT * .8f);
static const unsigned int     par_x0	 = (SCREEN_WIDTH - box_width) / 2;
static const unsigned int     par_y0	 = (SCREEN_HEIGHT - box_height) / 2;
static const rdpq_textparms_t par_params = { .align  = ALIGN_CENTER,
					     .valign = VALIGN_CENTER,
					     .width  = box_width,
					     .height = box_height,
					     .wrap   = WRAP_WORD };

static const rdpq_fontstyle_t fnt1_style = {
	.color = RGBA32(0xED, 0xAE, 0x49, 0xFF)
};

static const rdpq_fontstyle_t fnt2_style = {
	.color = RGBA32(0x82, 0x30, 0x38, 0xFF)
};

static const unsigned int box_x0 = (SCREEN_WIDTH - box_width) / 2;
static const unsigned int box_y0 = (SCREEN_HEIGHT - box_height) / 2;
static const unsigned int box_x1 = (SCREEN_WIDTH + box_width) / 2;
static const unsigned int box_y1 = (SCREEN_HEIGHT + box_height) / 2;

static const char text[] =
		"Two households, both alike in dignity,\n"
		"In fair Verona, where we lay our scene,\n"
		"From ancient grudge break to new mutiny,\n"
		"Where civil blood makes civil hands unclean.\n"
		"From forth the fatal loins of these two foes\n"
		"A pair of star-cross'd lovers take their life;\n";
static int text_len = sizeof(text) - 1;

static rdpq_font_t *fnt1 = NULL;
static rdpq_font_t *fnt2 = NULL;

static unsigned long frame = 0ul;

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

	/* Load, configure and register fonts */
	fnt1 = rdpq_font_load("rom:/Pacifico.font64");
	fnt2 = rdpq_font_load("rom:/FerriteCoreDX.font64");
	rdpq_font_style(fnt1, 0, &fnt1_style);
	rdpq_font_style(fnt2, 0, &fnt2_style);
	rdpq_text_register_font(FONT_PACIFICO, fnt1);
	rdpq_text_register_font(FONT_ZEROVELOCITY, fnt2);

	/* Main loop */
	for (frame = 0ul; !start_pressed; ++frame) {
		rdpq_paragraph_t *par;

		/* Render */
		rdpq_attach_clear(display_get(), NULL);

		rdpq_set_mode_fill(RGBA32(0x30, 0x63, 0x8E, 0xFF));
		rdpq_fill_rectangle(box_x0, box_y0, box_x1, box_y1);

		par = rdpq_paragraph_build(&par_params,
					   FONT_PACIFICO,
					   text,
					   &text_len);
		rdpq_paragraph_render(par, par_x0, par_y0);
		rdpq_paragraph_free(par);

		rdpq_set_mode_standard();
		rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
		rdpq_mode_combiner(RDPQ_COMBINER_FLAT);

		rdpq_detach_show();
	}

	/* Terminate */
	rdpq_text_unregister_font(FONT_ZEROVELOCITY);
	rdpq_text_unregister_font(FONT_PACIFICO);

	rdpq_font_free(fnt2);
	rdpq_font_free(fnt1);

	rdpq_close();
	display_close();
}
