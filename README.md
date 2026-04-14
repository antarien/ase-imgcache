# ase-imgcache

[![Layer](https://img.shields.io/badge/Shared-Image%20Cache-yellow.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()

> LRU image cache for native Cairo/Pixbuf rendering. Single source of truth
> for image loading across all native ASE clients.

Part of [ASE - Antares Simulation Engine](../../..)

## Overview

`ase-imgcache` is the shared SSOT module for image loading in native GTK-based
ASE clients (`ase-client-viewer`, `ase-client-explorer`, future native tools).
Every native client that renders PNG, JPG, or SVG images into a Cairo context
goes through this module — nowhere else in the C++ codebase is
`Gdk::Pixbuf::create_from_file` or `cairo_image_surface_create_from_png`
permitted.

Before this module existed, each client re-decoded and re-scaled every image
on every render pass. `ase-imgcache` caches pre-scaled `Cairo::ImageSurface`
objects keyed by `(path, max_width)` with LRU eviction, so repeated frames
are effectively free once an image has been loaded once.

## Features

- **Pre-scaled caching** — Images decoded and scaled once, reused every frame
- **LRU eviction** — Configurable max entry count (default 64)
- **Placeholder fallback** — Missing/corrupt files render a visible labelled box
- **Format support** — PNG, JPG, SVG via GdkPixbuf loaders
- **Single-threaded** — Intended for Cairo render thread; zero mutex overhead

## API

```cpp
#include <ase/imgcache/image_cache.hpp>

// Convenience: draw image at (x, y), return consumed vertical space.
const double h = ase::imgcache::render(cr, "/path/to/image.png",
                                       x, y, max_width);

// Lower level: obtain the cached Entry (surface + dimensions).
const auto entry = ase::imgcache::get("/path/to/image.png", max_width);
if (entry.surface) {
    cr->set_source(entry.surface, x, y);
    cr->paint();
}

// Cache controls.
ase::imgcache::set_max_entries(128);
ase::imgcache::clear();
const std::size_t n = ase::imgcache::size();
```

## Consumers

| Client                | Usage                                              |
|-----------------------|----------------------------------------------------|
| `ase-client-viewer`   | Markdown image blocks, `:::figure` / `:::gallery`  |
| `ase-client-explorer` | Thumbnail previews                                 |

## Integration

### From root ASE build (normal case)

Already wired in via `shared/ase-imgcache` in the root `CMakeLists.txt`.
Consumers just link the target:

```cmake
target_link_libraries(my-client PRIVATE ase::imgcache)
```

### Standalone subproject

```cmake
if(NOT TARGET ase::imgcache)
    add_subdirectory(${ASE_ROOT}/shared/ase-imgcache
                     ${CMAKE_BINARY_DIR}/ase-imgcache)
endif()
target_link_libraries(my-client PRIVATE ase::imgcache)
```

## Dependencies

- C++20
- `gtkmm-4.0` (transitively through `ase::gtk` adapter when available,
  otherwise via `pkg_check_modules`)
- `libcairomm-1.16` (bundled with gtkmm-4)
- `libgdk-pixbuf-2.0` (bundled with gtkmm-4)

## Scope Boundaries

**In scope:** Local filesystem image loading, pre-scaling, LRU caching,
placeholder fallback for missing files.

**Out of scope:**

- HTTP/HTTPS fetching — would require libcurl. If a client ever needs this,
  a separate fetch-and-stash layer should sit in front of `ase-imgcache`
  and feed it local paths, keeping this module dependency-free.
- Async/background decoding — the current API is synchronous to match
  Cairo's main-thread execution model. Adding async would introduce a
  mutex and a worker thread; that is a separate module, not a TODO inside
  this one.
- Server-side rendering — `shared/ase-imgcache` is for native clients only.
  TypeScript web clients use `<img>` tags with browser-native caching.

## Versioning

`VERSION` file at the repository root is the SSOT. Format: `MM.mm.pp.BBBBB`.

## License

Proprietary — Antares Simulation Engine
