// E-paper output with a refresh policy suited to a desk display:
// partial refreshes for updates, a full refresh every so often (and whenever
// the kind of screen changes) to clear ghosting.
#pragma once

#include <Arduino.h>

#include "../ui/screens.h"

namespace display {

enum class Kind : uint8_t { None, Setup, Aircraft, Takeover, EmptySky, IssMap, IssAlert, Offline, Message };

void begin();

// Renders with `draw` and pushes to the panel. `contentHash` lets callers skip
// identical frames: returns false (nothing done) if the hash matches the last frame.
bool show(Kind kind, uint32_t contentHash, void (*draw)(ui::Canvas&, const void*), const void* model, bool forceFull = false);

// Simple FNV-1a over a view model; callers quantise values first so tiny changes don't redraw.
uint32_t hash(const void* data, size_t len, uint32_t seed = 2166136261u);

uint32_t lastRefreshMs();
Kind lastKind();

}  // namespace display
