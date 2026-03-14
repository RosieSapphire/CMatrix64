BUILD_DIR=build
include $(N64_INST)/include/n64.mk

src = cmatrix.c
assets_ttf = $(wildcard assets/*.ttf)
assets_png = $(wildcard assets/*.png)

assets_conv = $(addprefix filesystem/,$(notdir $(assets_ttf:%.ttf=%.font64))) \
              $(addprefix filesystem/,$(notdir $(assets_png:%.png=%.sprite)))

MKSPRITE_FLAGS ?=
MKFONT_FLAGS ?=

all: cmatrix.z64

filesystem/%.font64: assets/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o filesystem "$<"

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"

filesystem/Pacifico.font64: MKFONT_FLAGS+=--size 12

$(BUILD_DIR)/cmatrix.dfs: $(assets_conv)
$(BUILD_DIR)/cmatrix.elf: $(src:%.c=$(BUILD_DIR)/%.o)

cmatrix.z64: N64_ROM_TITLE="RDPQ Font Demo"
cmatrix.z64: $(BUILD_DIR)/cmatrix.dfs 

clean:
	rm -rf $(BUILD_DIR) filesystem cmatrix.z64 compile_commands.json .cache/

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
