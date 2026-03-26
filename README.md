# CMatrix 64
I got bored and made [CMatrix](https://github.com/abishekvashok/cmatrix.git) on the N64. Sue me.
![Screenshot in Normal Mode](https://github.com/RosieSapphire/CMatrix64/blob/master/screenshot_normal.png?raw=true)
![Screenshot in Rainbow Mode](https://github.com/RosieSapphire/CMatrix64/blob/master/screenshot_rainbow.png?raw=true)
## Controller Bindings
|C-Left|C-Right|C-Up|C-Down|Start|L|
|---|---|---|---|---|---|
Last Color|Next Color|Scroll Speed +|Scroll Speed -|Reset Settings|Rainbow Mode|

## Configuration
Inside the `Makefile`, there are two primary things you can toggle. `DEBUG_MODE` and `HIGH_RES`.
> Note: The way Makefiles work, `<VALUE> ?= ` means disabled, while <VALUE> ?= <Anything, but preferably just 1>` is enabled, so set them to `?= ` for disabled and `?= 1` for enabled.

### DEBUG_MODE
Just adds in all the debug symbols and shit for if you're gonna use `debugf` or something like [UNFLoader](https://github.com/Buu342/UNFLoader.git) with the `-d` flag to enable debug printing. It also enabled `rdpq_debug_start()` so there's that too; just all the general debugging shit. Disabling this will put it in release mode, which is effectively the default.

### HIGH_RES
Pretty fucking obvious: it just ups the resolution from 320x240 to 640x480 when enabled.
> Note: There seems to be an issue if `DEBUG_MODE` is also enabled where since the debugging takes more time to process, it gives warnings about the vblank or whatever taking too long or being out of sync, so be weary of that. There's a chance this silently happens in release mode as well, but I don't know. I also haven't tested this mode extensively enough to know if it's properly stable, but it seems fine enough. I should probably optimize it, but it works good enough for now.
