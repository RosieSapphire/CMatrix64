BUILD_DIR := build
FS_DIR    := filesystem
include $(N64_INST)/include/n64.mk

NAME_STR := "CMatrix 64"
NAME     := cmatrix
DFS_FILE := $(BUILD_DIR)/$(NAME).dfs
ELF_FILE := $(BUILD_DIR)/$(NAME).elf
Z64_FILE := $(NAME).z64

C_FILES := $(NAME).c
O_FILES := $(C_FILES:%.c=$(BUILD_DIR)/%.o)
D_FILES := $(wildcard $(BUILD_DIR)/*.d)

ASSETS_TTF := $(wildcard assets/*.ttf)

ASSETS_CONV := $(addprefix $(FS_DIR)/,$(notdir $(ASSETS_TTF:%.ttf=%.font64)))

MKSPRITE_FLAGS ?=
MKFONT_FLAGS ?=

all: $(Z64_FILE)

$(FS_DIR)/%.font64: assets/%.ttf
	@mkdir -p $(dir $@)
	@echo "    [FONT] $@"
	$(N64_MKFONT) $(MKFONT_FLAGS) -o $(dir $@) "$<"

$(FS_DIR)/Pacifico.font64: MKFONT_FLAGS+=--size 12

$(DFS_FILE): $(ASSETS_CONV)
$(ELF_FILE): $(O_FILES)

$(Z64_FILE): N64_ROM_TITLE=$(NAME_STR)
$(Z64_FILE): $(DFS_FILE)

clean:
	rm -rf $(BUILD_DIR) $(FS_DIR) $(Z64_FILE) compile_commands.json .cache/

-include $(D_FILES)

.PHONY: all clean
