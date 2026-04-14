/**
 * ASE Image Cache — implementation.
 *
 * Fixed-capacity LRU backed by a flat array of kMaxEntries slots.
 * Linear scan for lookup and eviction: n is small enough that this
 * beats any pointer-chasing list + hash-table combination in practice
 * and keeps the module free of std::list / std::function / std::map
 * dependencies (forbidden by the ASE validator).
 *
 * Each slot stores (path, max_width, Entry, last_used, occupied).
 *   hit   → update last_used = ++counter
 *   miss  → pick an empty slot, otherwise the slot with the smallest
 *           last_used, and refill it. Cairo surfaces are ref-counted,
 *           so eviction just drops the RefPtr and the surface is
 *           released when no caller still holds it.
 *
 * Paths longer than kMaxPathLen bypass the cache entirely and are
 * decoded on every call — this avoids silent collisions between two
 * distinct paths that share the same truncated prefix.
 *
 * String handling goes through ase::utils::strops helpers — C string
 * functions (strcpy/strlen/memcpy) are forbidden by the ASE validator.
 */

#include <ase/imgcache/image_cache.hpp>
#include <ase/utils/strops.hpp>

#include <gdkmm/general.h>
#include <gdkmm/pixbuf.h>

#include <cstdint>

namespace ase::imgcache {

namespace {

constexpr int      kMaxEntries = 128;
constexpr uint32_t kMaxPathLen = 1024;
constexpr uint32_t kMaxMsgLen  = kMaxPathLen + 16;

struct Slot {
    char          path[kMaxPathLen] = {};
    int           max_width         = 0;
    Entry         entry;
    std::uint64_t last_used         = 0;
    bool          occupied          = false;
};

struct Cache {
    Slot          slots[kMaxEntries];
    std::uint64_t counter     = 0;
    int           max_entries = 64;  // logical cap; always ≤ kMaxEntries
};

Cache& cache() {
    static Cache c;
    return c;
}

int occupied_count(const Cache& c) {
    int n = 0;
    for (int i = 0; i < kMaxEntries; ++i) {
        if (c.slots[i].occupied) ++n;
    }
    return n;
}

int find_lru(const Cache& c) {
    int           victim = -1;
    std::uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < kMaxEntries; ++i) {
        if (c.slots[i].occupied && c.slots[i].last_used < oldest) {
            oldest = c.slots[i].last_used;
            victim = i;
        }
    }
    return victim;
}

void evict_to_cap(Cache& c) {
    while (occupied_count(c) > c.max_entries) {
        const int victim = find_lru(c);
        if (victim < 0) return;
        c.slots[victim] = Slot{};
    }
}

int find_hit(const Cache& c, const char* path, int max_width) {
    for (int i = 0; i < kMaxEntries; ++i) {
        const Slot& s = c.slots[i];
        if (s.occupied
            && s.max_width == max_width
            && ase::utils::str_equal(s.path, path, kMaxPathLen)) {
            return i;
        }
    }
    return -1;
}

int acquire_slot(Cache& c) {
    for (int i = 0; i < kMaxEntries; ++i) {
        if (!c.slots[i].occupied) return i;
    }
    // All slots occupied → LRU victim (find_lru always succeeds here).
    return find_lru(c);
}

Entry decode_and_scale(const char* path, int max_width) {
    Entry e;
    try {
        auto pixbuf = Gdk::Pixbuf::create_from_file(path);
        if (!pixbuf) return e;

        const int src_w = pixbuf->get_width();
        const int src_h = pixbuf->get_height();
        if (src_w <= 0 || src_h <= 0) return e;

        double scale = 1.0;
        if (src_w > max_width && max_width > 0) {
            scale = static_cast<double>(max_width) / static_cast<double>(src_w);
        }

        const int draw_w = static_cast<int>(src_w * scale);
        const int draw_h = static_cast<int>(src_h * scale);
        if (draw_w <= 0 || draw_h <= 0) return e;

        auto surface = Cairo::ImageSurface::create(
            Cairo::Surface::Format::ARGB32, draw_w, draw_h);
        auto img_cr = Cairo::Context::create(surface);
        img_cr->scale(scale, scale);
        Gdk::Cairo::set_source_pixbuf(img_cr, pixbuf, 0, 0);
        img_cr->paint();

        e.surface     = surface;
        e.draw_width  = draw_w;
        e.draw_height = draw_h;
    } catch (...) {
        // Decode failure → empty entry; caller renders placeholder.
    }
    return e;
}

}  // namespace

Entry get(const std::string& path, int max_width) {
    Cache& c = cache();

    const uint32_t plen = static_cast<uint32_t>(path.size());
    if (plen == 0 || plen >= kMaxPathLen) {
        return decode_and_scale(path.c_str(), max_width);
    }

    const int hit = find_hit(c, path.c_str(), max_width);
    if (hit >= 0) {
        c.slots[hit].last_used = ++c.counter;
        return c.slots[hit].entry;
    }

    const int idx = acquire_slot(c);
    Slot& s = c.slots[idx];
    ase::utils::str_copy(s.path, kMaxPathLen, path.c_str());
    s.max_width = max_width;
    s.entry     = decode_and_scale(path.c_str(), max_width);
    s.last_used = ++c.counter;
    s.occupied  = true;

    evict_to_cap(c);
    return s.entry;
}

double render(const Cairo::RefPtr<Cairo::Context>& cr,
              const std::string& path,
              double x, double y, int max_width) {
    Entry e = get(path, max_width);
    if (!e.surface) {
        cr->set_source_rgba(0.353, 0.353, 0.353, 0.5);
        cr->rectangle(x, y, max_width, 40);
        cr->fill();
        cr->set_source_rgb(0.541, 0.604, 0.604);
        cr->select_font_face("Fira Code",
            Cairo::ToyFontFace::Slant::NORMAL,
            Cairo::ToyFontFace::Weight::NORMAL);
        cr->set_font_size(10);
        cr->move_to(x + 8, y + 24);

        char msg[kMaxMsgLen];
        ase::utils::str_copy  (msg, kMaxMsgLen, "[image: ");
        ase::utils::str_append(msg, kMaxMsgLen, path.c_str());
        ase::utils::str_append(msg, kMaxMsgLen, "]");
        cr->show_text(msg);
        return 50;
    }

    cr->set_source(e.surface, x, y);
    cr->paint();
    return static_cast<double>(e.draw_height) + 10.0;
}

void set_max_entries(std::size_t count) {
    Cache& c = cache();
    int n = static_cast<int>(count);
    if (n < 1) n = 1;
    if (n > kMaxEntries) n = kMaxEntries;
    c.max_entries = n;
    evict_to_cap(c);
}

void clear() {
    Cache& c = cache();
    for (int i = 0; i < kMaxEntries; ++i) {
        c.slots[i] = Slot{};
    }
    c.counter = 0;
}

std::size_t size() {
    return static_cast<std::size_t>(occupied_count(cache()));
}

}  // namespace ase::imgcache
