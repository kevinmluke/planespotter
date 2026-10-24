// Renders every screen with sample data to PBM files (converted to PNG by
// render_previews.py). Uses the exact same drawing code as the firmware.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/core/geo.h"
#include "../../src/core/iss.h"
#include "../../src/generated/aircraft_db.h"
#include "../../src/ui/screens.h"

using namespace ui;

static const char* kTle1 = "1 25544U 98067A   26294.04791053  .00009575  00000+0  18188-3 0  9996";
static const char* kTle2 = "2 25544  51.6327  37.2623 0006759 289.5321  70.4938 15.48967427591263";
static const double kHomeLat = 48.8566, kHomeLon = 2.3522;

static void save(GFXcanvas1& c, const std::string& name) {
  const std::string path = "build/previews/" + name + ".pbm";
  FILE* f = std::fopen(path.c_str(), "wb");
  std::fprintf(f, "P4\n%d %d\n", c.width(), c.height());
  // GFXcanvas1 stores 1 = set (we draw black as 0), PBM wants 1 = black.
  const uint8_t* buf = c.getBuffer();
  const int n = ((c.width() + 7) / 8) * c.height();
  for (int i = 0; i < n; i++) std::fputc(static_cast<uint8_t>(~buf[i]), f);
  std::fclose(f);
}

static bool gDark = false;

static void setCommon(Common& c, Lang lang, Freshness fr = Freshness::Live) {
  c.dark = gDark;
  c.lang = lang;
  c.units = lang == Lang::FR ? Units::Metric : Units::Imperial;
  c.clock24 = lang == Lang::FR;
  c.status.freshness = fr;
  c.status.delaySec = 47;
  std::strcpy(c.status.clock, lang == Lang::FR ? "21:04" : "9:04 PM");
  c.status.aircraftCount = 14;
  c.status.mode = Mode::Auto;
}

static void addBlips(Blip* b, int& n, bool withSelected) {
  const Blip demo[] = {{12.4f, 312, 268, withSelected, false}, {21, 40, 95, false, false}, {7.5f, 150, 10, false, false},
                       {26, 220, 180, false, true},           {17, 95, 300, false, false}, {28, 330, 45, false, false}};
  n = 0;
  for (const auto& d : demo) b[n++] = d;
}

static void aircraftSample(AircraftView& a, Lang lang) {
  const bool fr = lang == Lang::FR;
  std::strcpy(a.flight, fr ? "AF1234" : "UA1234");
  std::strcpy(a.callsign, fr ? "AFR1234" : "UAL1234");
  std::strcpy(a.airline, fr ? "Air France" : "United");
  std::strcpy(a.model, "Airbus A320neo");
  std::strcpy(a.reg, fr ? "F-HBNA" : "N401UA");
  std::strcpy(a.typeCode, "A20N");
  a.hasRoute = true;
  std::strcpy(a.originCity, fr ? "Paris" : "San Francisco");
  std::strcpy(a.originCode, fr ? "CDG" : "SFO");
  std::strcpy(a.destCity, fr ? "Montréal" : "New York");
  std::strcpy(a.destCode, fr ? "YUL" : "JFK");
  a.routeProgress = 0.08f;
  a.altFt = 23450;
  a.vrateFpm = 1850;
  a.gsKt = 412;
  a.trackDeg = 288;
  a.distKm = 12.4f;
  a.bearingDeg = 312;
  a.family = db::familyIndex("a320");
}

int main() {
  GFXcanvas1 canvas(kW, kH);
  U8G2_FOR_ADAFRUIT_GFX u8;
  u8.begin(canvas);
  Canvas cv{canvas, u8};

  iss::Tracker tr;
  tr.setTle(kTle1, kTle2);
  tr.setObserver(kHomeLat, kHomeLon, 50);
  const int64_t base = static_cast<int64_t>(sgp4::jdToUnix(2461336.5 + 0.875));  // 2026-10-21 21:00 UTC

  struct Variant {
    Lang lang;
    bool dark;
  };
  for (Variant v : {Variant{Lang::EN, false}, Variant{Lang::EN, true}}) {  // the UI is English-only
    const Lang lang = v.lang;
    gDark = v.dark;
    const std::string sfx = std::string(lang == Lang::FR ? "_fr" : "_en") + (v.dark ? "_dark" : "");

    static AircraftScreen as;
    as = AircraftScreen();
    setCommon(as.c, lang);
    aircraftSample(as.ac, lang);
    addBlips(as.blips, as.blipCount, true);
    as.rangeKm = 30;
    drawAircraft(cv, as);
    save(canvas, "01_aircraft" + sfx);

    // Rare takeover: BelugaXL
    as = AircraftScreen();
    setCommon(as.c, lang);
    std::strcpy(as.ac.flight, "BGA114F");
    std::strcpy(as.ac.airline, "Airbus Transport International");
    std::strcpy(as.ac.model, "Airbus BelugaXL");
    std::strcpy(as.ac.reg, "F-GXLI");
    std::strcpy(as.ac.typeCode, "A337");
    as.ac.hasRoute = true;
    std::strcpy(as.ac.originCity, "Toulouse");
    std::strcpy(as.ac.originCode, "TLS");
    std::strcpy(as.ac.destCity, "Hamburg");
    std::strcpy(as.ac.destCode, "XFW");
    as.ac.routeProgress = 0.55f;
    as.ac.altFt = 27000;
    as.ac.gsKt = 398;
    as.ac.trackDeg = 18;
    as.ac.distKm = 41;
    as.ac.bearingDeg = 200;
    as.ac.family = db::familyIndex("belugaxl");
    as.ac.takeover = true;
    as.ac.reason = ac::RareReason::RareType;
    drawAircraft(cv, as);
    save(canvas, "02_rare" + sfx);

    // Military takeover
    std::strcpy(as.ac.flight, "CTM2034");
    std::strcpy(as.ac.airline, lang == Lang::FR ? "Armée de l'Air (Cotam)" : "French Air Force (Cotam)");
    std::strcpy(as.ac.model, "Airbus A400M Atlas");
    std::strcpy(as.ac.reg, "F-RBAJ");
    std::strcpy(as.ac.typeCode, "A400");
    as.ac.hasRoute = false;
    as.ac.altFt = 8500;
    as.ac.vrateFpm = -900;
    as.ac.gsKt = 240;
    as.ac.trackDeg = 135;
    as.ac.distKm = 9.2f;
    as.ac.family = db::familyIndex("a400m");
    as.ac.reason = ac::RareReason::Military;
    drawAircraft(cv, as);
    save(canvas, "03_military" + sfx);

    // Emergency
    aircraftSample(as.ac, lang);
    as.ac.takeover = true;
    as.ac.reason = ac::RareReason::Emergency;
    as.ac.squawk = 7700;
    as.ac.altFt = 9800;
    as.ac.vrateFpm = -2200;
    as.c.status.freshness = Freshness::Delayed;
    drawAircraft(cv, as);
    save(canvas, "04_emergency" + sfx);

    // ISS map
    static IssMapScreen im;
    im = IssMapScreen();
    setCommon(im.c, lang);
    im.c.status.mode = Mode::Iss;
    tr.position(base, im.pos);
    im.homeLat = kHomeLat;
    im.homeLon = kHomeLon;
    im.distanceKm = geo::distanceKm(kHomeLat, kHomeLon, im.pos.latDeg, im.pos.lonDeg);
    im.trackCount = tr.groundTrack(base - 45 * 60, base + 95 * 60, 70, im.trackLat, im.trackLon, IssMapScreen::kTrackPts);
    im.trackNowIndex = (45 * 60) / 70;
    double sl, so;
    sgp4::subsolarPoint(sgp4::unixToJd(static_cast<double>(base)), sl, so);
    im.sunLat = sl;
    im.sunLon = so;
    im.now = base;
    im.tzOffsetS = 7200;
    im.hasPass = tr.nextVisiblePass(base, 48, im.nextPass);
    im.tleAgeDays = static_cast<int>(tr.tleAgeDays(base));
    drawIssMap(cv, im);
    save(canvas, "05_iss_map" + sfx);

    // ISS alert (10 minutes before) and in progress
    static IssAlertScreen ia;
    ia = IssAlertScreen();
    setCommon(ia.c, lang);
    if (im.hasPass) {
      ia.pass = im.nextPass;
      ia.now = ia.pass.visStart.t - 7 * 60 - 20;
      ia.tzOffsetS = 7200;
      drawIssAlert(cv, ia);
      save(canvas, "06_iss_alert" + sfx);
      ia.inProgress = true;
      ia.now = ia.pass.visMax.t;
      tr.skyPosition(ia.now, ia.current);
      drawIssAlert(cv, ia);
      save(canvas, "07_iss_now" + sfx);
    }

    // Setup (first time: bilingual) and settings (long press), plus join outcomes
    static SetupScreen ss;
    ss = SetupScreen();
    setCommon(ss.c, lang);
    std::strcpy(ss.apSsid, "PlaneSpotter-3F2A");
    std::strcpy(ss.apPassword, "skies-2481");
    std::strcpy(ss.qrPayload, "WIFI:T:WPA;S:PlaneSpotter-3F2A;P:skies-2481;;");
    if (lang == Lang::EN) {
      drawSetup(cv, ss);
      save(canvas, v.dark ? "08_setup_dark" : "08_setup");
      ss.join = JoinResult::WrongPassword;
      std::strcpy(ss.joinSsid, "Maison-5G");
      drawSetup(cv, ss);
      save(canvas, v.dark ? "08_setup_wrongpass_dark" : "08_setup_wrongpass");
      ss.join = JoinResult::Captive;
      std::strcpy(ss.joinSsid, "Guest-Office");
      drawSetup(cv, ss);
      save(canvas, v.dark ? "08_setup_captive_dark" : "08_setup_captive");
      ss.join = JoinResult::Ok;
      drawSetup(cv, ss);
      save(canvas, v.dark ? "08_setup_ok_dark" : "08_setup_ok");
    }
    ss.join = JoinResult::None;
    ss.settingsMode = true;
    drawSetup(cv, ss);
    save(canvas, "09_settings" + sfx);

    // Offline
    static OfflineScreen os;
    os = OfflineScreen();
    setCommon(os.c, lang, Freshness::Offline);
    os.reason = OfflineReason::Wifi;
    std::strcpy(os.lastUpdate, lang == Lang::FR ? "à 20:51" : "at 8:51 PM");
    std::strcpy(os.detail, "Maison-5G");
    drawOffline(cv, os);
    save(canvas, "10_offline" + sfx);

    // Empty sky
    static EmptySkyScreen es;
    es = EmptySkyScreen();
    setCommon(es.c, lang);
    es.c.status.aircraftCount = 0;
    es.c.status.mode = Mode::Aircraft;
    es.hasPass = im.hasPass;
    es.nextPass = im.nextPass;
    es.now = base;
    es.tzOffsetS = 7200;
    drawEmptySky(cv, es);
    save(canvas, "11_quiet" + sfx);


    // OTA message
    static MessageScreen ms;
    ms = MessageScreen();
    setCommon(ms.c, lang);
    std::snprintf(ms.title, sizeof(ms.title), i18n::tr(lang, i18n::S::Updating), "1.1.0");
    std::strcpy(ms.line1, i18n::tr(lang, i18n::S::DoNotUnplug));
    ms.progress = 62;
    drawMessage(cv, ms);
    save(canvas, "12_update" + sfx);
  }
  std::printf("previews written\n");
  return 0;
}
