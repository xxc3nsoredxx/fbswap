# fbswap

Switching from one TTY to another kept resetting my scrollback.
That is annoying.
That is also hindering my "How long can I go without installing X11" challenge.
I figured that if I was able to keep copies of the frame buffer contents and switch between them I might be able to keep the scrollback.
So the logical conclusion is to try and write a kernel module to do just that.

## Compilation

Just run `make` in the `src/` directory.
Included are also scripts to load and unload the module (MUST BE RUN AS ROOT).
You also need a copy of the Linux kernel headers.
The makefile assumes the module Kbuild scripts are installed in `/lib/modules/[uname -r]/build` so adjust if needed.
