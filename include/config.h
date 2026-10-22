// Build-time configuration: wiring, endpoints, timings.
#pragma once

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

// ---------------------------------------------------------------------------
// Wiring: ESP32-S3-DevKitC-1 -> Good Display DESPI-C579 adapter -> GDEY0579T93
#define PIN_EPD_SCK 12
#define PIN_EPD_MOSI 11
#define PIN_EPD_CS 10
#define PIN_EPD_DC 9
#define PIN_EPD_RST 8
#define PIN_EPD_BUSY 7
#define PIN_BUTTON 0  // the BOOT button; any momentary switch to GND works

// ---------------------------------------------------------------------------
// Data sources
#define ADSB_URL_FMT "https://api.adsb.lol/v2/lat/%.4f/lon/%.4f/dist/%d"  // dist in NM, max 250
#define ADSB_FALLBACK_URL_FMT "https://opendata.adsb.fi/api/v2/lat/%.4f/lon/%.4f/dist/%d"
#define ROUTE_URL_FMT "https://api.adsbdb.com/v0/callsign/%s"
#define ROUTE_FALLBACK_URL_FMT "https://hexdb.io/api/v1/route/icao/%s"
#define TLE_URL "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// Firmware updates come from the latest GitHub release of this repository ("owner/name").
// tools/release.py publishes signed releases there. Until it is set, update checks are skipped.
#define GITHUB_REPO "kevinmluke/planespotter"
#define OTA_MANIFEST_URL "https://github.com/" GITHUB_REPO "/releases/latest/download/manifest.json"

// ---------------------------------------------------------------------------
// Timings (milliseconds unless noted)
#define ADSB_POLL_MS 8000          // aircraft refresh; keep >= 5 s to be polite to free APIs
#define ROUTE_CACHE_SIZE 24
#define ROUTE_RETRY_MS 300000       // don't re-ask for an unknown callsign for 5 min
#define TLE_REFRESH_S (12 * 3600)   // orbital elements
#define TLE_MAX_AGE_DAYS 14         // older than this: ISS position is no longer shown as live
#define OTA_CHECK_FIRST_MS (3 * 60 * 1000)
#define OTA_CHECK_EVERY_MS (24UL * 3600 * 1000)
#define OTA_HEALTH_DEADLINE_MS (5 * 60 * 1000)  // new firmware must reach the API within this

// Freshness: when does data stop being "live"?
#define FRESH_LIVE_S 25      // newer than this: LIVE badge
#define FRESH_OFFLINE_S 120  // older than this: full OFFLINE screen

// Display
#define FULL_REFRESH_EVERY 30         // partial refreshes between full (anti-ghosting) refreshes
#define FULL_REFRESH_MAX_MS (20 * 60 * 1000)
#define MIN_REDRAW_MS 9000            // e-paper: don't redraw more often than this in normal use

// ISS
#define ISS_ALERT_LEAD_S (10 * 60)    // start the countdown this long before the pass
#define ISS_PASS_SEARCH_H 48

// Setup portal
#define AP_SSID_PREFIX "PlaneSpotter-"
#define MDNS_HOST "planespotter"
