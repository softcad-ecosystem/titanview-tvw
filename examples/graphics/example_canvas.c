/* Public C99 canvas illustration from SoftCAD's TVW graphics documentation.
 * REQUIRES included LGPL headers from graphics/libtitangfx/src/:
 *   ts_gfx_canvas.h, ts_engine_abi.h
 * This bundle now includes those approved headers in graphics/libtitangfx/src/.
 * This example does NOT use or validate the full TVW language/compiler runtime.
 */
#define TS_GFX_CANVAS_IMPLEMENTATION
#include "ts_gfx_canvas.h"
#include "ts_engine_abi.h"
#include <stdio.h>

int main(void) {
    TS_Canvas *c = ts_canvas_new(16, 16);
    if (!c) return 1;
    ts_canvas_clear(c, ts_rgb(22, 88, 99));
    printf("%u %u %08x\n", (unsigned)ts_canvas_width(c),
           (unsigned)ts_canvas_height(c), ts_pixel_get(c, 0, 0));
    ts_canvas_free(c);
    return 0;
}
