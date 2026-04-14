#pragma once

/**
 * ASE Image Cache — shared image loading for native clients.
 *
 * Single source of truth for all PNG/JPG/SVG loading in Cairo-based native
 * ASE renderers. Caches pre-scaled image surfaces keyed by (path, max_width)
 * so repeated frames never re-decode or re-scale the same image.
 *
 * Single-threaded: intended for the Cairo render thread (main/UI thread).
 * Eviction policy: LRU with a configurable entry cap (default 64).
 *
 * Supported formats: PNG, JPG, SVG (whatever GdkPixbuf loaders are installed).
 * Unsupported paths or decode failures yield an empty Entry whose surface
 * is a null RefPtr; the render() helper draws a visible placeholder box in
 * that case so missing images never silently disappear.
 *
 * Out of scope for this module (see README):
 *   - HTTP/HTTPS fetching (no libcurl dependency)
 *   - Async/background decoding
 */

#include <cairomm/cairomm.h>
#include <cstddef>
#include <string>

namespace ase::imgcache {

struct Entry {
    Cairo::RefPtr<Cairo::ImageSurface> surface;
    int draw_width  = 0;
    int draw_height = 0;
};

// Returns the pre-scaled cached entry for (path, max_width). On the first
// call for a given key the image is decoded via Gdk::Pixbuf, scaled down so
// that width ≤ max_width preserving aspect ratio, and stored. On decode
// failure the returned Entry has a null surface (check with `if (!e.surface)`).
Entry get(const std::string& path, int max_width);

// Convenience wrapper: draws the cached surface at (x, y) on `cr`. Returns
// the consumed vertical space (draw_height + 10 px padding). On decode
// failure draws a placeholder rectangle with "[image: path]" text and
// returns 50.
double render(const Cairo::RefPtr<Cairo::Context>& cr,
              const std::string& path,
              double x, double y, int max_width);

// Cache-wide controls.
void        set_max_entries(std::size_t count);
void        clear();
std::size_t size();

}  // namespace ase::imgcache
