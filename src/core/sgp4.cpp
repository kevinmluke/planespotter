#include "sgp4.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace sgp4 {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2 * kPi;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kX2o3 = 2.0 / 3.0;

// WGS-72 (the constants the TLEs are fitted with)
constexpr double kMu = 398600.8;
constexpr double kRe = 6378.135;
const double kXke = 60.0 / std::sqrt(kRe * kRe * kRe / kMu);
constexpr double kJ2 = 0.001082616;
constexpr double kJ3 = -0.00000253881;
constexpr double kJ4 = -0.00000165597;
constexpr double kJ3oJ2 = kJ3 / kJ2;

// WGS-84 for observer coordinates
constexpr double kWgs84A = 6378.137;
constexpr double kWgs84F = 1.0 / 298.257223563;

double field(const char* line, int start, int len) {
  char buf[24];
  if (len >= static_cast<int>(sizeof(buf))) return 0;
  std::memcpy(buf, line + start, len);
  buf[len] = 0;
  return std::atof(buf);
}

// TLE "assumed decimal point" exponent field, e.g. " 18188-3" -> 0.18188e-3
double expField(const char* line, int start) {
  char mant[6];
  std::memcpy(mant, line + start + 1, 5);
  mant[5] = 0;
  char expo[3] = {line[start + 6], line[start + 7], 0};
  const double m = std::atof(mant) * 1e-5;
  const double value = m * std::pow(10.0, std::atoi(expo));
  return line[start] == '-' ? -value : value;
}

bool checksumOk(const char* line) {
  int sum = 0;
  for (int i = 0; i < 68; i++) {
    const char c = line[i];
    if (c >= '0' && c <= '9') sum += c - '0';
    else if (c == '-') sum += 1;
  }
  return (sum % 10) == (line[68] - '0');
}

double jdFromYmd(int y, int m, int d) {
  return 367.0 * y - std::floor(7 * (y + std::floor((m + 9) / 12.0)) * 0.25) +
         std::floor(275 * m / 9.0) + d + 1721013.5;
}

}  // namespace

bool Satellite::init(const char* l1, const char* l2) {
  valid_ = false;
  if (!l1 || !l2 || std::strlen(l1) < 69 || std::strlen(l2) < 69) return false;
  if (l1[0] != '1' || l2[0] != '2') return false;
  if (!checksumOk(l1) || !checksumOk(l2)) return false;

  int yy = static_cast<int>(field(l1, 18, 2));
  const double days = field(l1, 20, 12);
  const int year = yy < 57 ? 2000 + yy : 1900 + yy;
  jdEpoch_ = jdFromYmd(year, 1, 1) + days - 1.0;

  bstar_ = expField(l1, 53);
  inclo_ = field(l2, 8, 8) * kDeg2Rad;
  nodeo_ = field(l2, 17, 8) * kDeg2Rad;
  char ecc[9] = "0.";
  std::memcpy(ecc + 2, l2 + 26, 7);
  ecc[8] = 0;
  ecco_ = std::atof(ecc);
  argpo_ = field(l2, 34, 8) * kDeg2Rad;
  mo_ = field(l2, 43, 8) * kDeg2Rad;
  noKozai_ = field(l2, 52, 11) * kTwoPi / 1440.0;  // rad/min
  if (noKozai_ <= 0) return false;

  // ---- initl ----
  const double eccsq = ecco_ * ecco_;
  const double omeosq = 1.0 - eccsq;
  const double rteosq = std::sqrt(omeosq);
  const double cosio = std::cos(inclo_);
  const double cosio2 = cosio * cosio;
  const double ak = std::pow(kXke / noKozai_, kX2o3);
  const double d1 = 0.75 * kJ2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
  double del = d1 / (ak * ak);
  const double adel = ak * (1.0 - del * del - del * (1.0 / 3.0 + 134.0 * del * del / 81.0));
  del = d1 / (adel * adel);
  no_ = noKozai_ / (1.0 + del);
  ao_ = std::pow(kXke / no_, kX2o3);
  const double sinio = std::sin(inclo_);
  const double po = ao_ * omeosq;
  const double con42 = 1.0 - 5.0 * cosio2;
  con41_ = -con42 - cosio2 - cosio2;
  const double posq = po * po;
  const double rp = ao_ * (1.0 - ecco_);

  if (kTwoPi / no_ >= 225.0) return false;  // deep space: not supported

  // ---- sgp4init (near earth) ----
  const double ss = 78.0 / kRe + 1.0;
  const double qzms2t = std::pow((120.0 - 78.0) / kRe, 4);
  isimp_ = (rp < (220.0 / kRe + 1.0)) ? 1 : 0;
  double sfour = ss, qzms24 = qzms2t;
  const double perige = (rp - 1.0) * kRe;
  if (perige < 156.0) {
    sfour = perige - 78.0;
    if (perige < 98.0) sfour = 20.0;
    qzms24 = std::pow((120.0 - sfour) / kRe, 4);
    sfour = sfour / kRe + 1.0;
  }
  const double pinvsq = 1.0 / posq;
  const double tsi = 1.0 / (ao_ - sfour);
  eta_ = ao_ * ecco_ * tsi;
  const double etasq = eta_ * eta_;
  const double eeta = ecco_ * eta_;
  const double psisq = std::fabs(1.0 - etasq);
  const double coef = qzms24 * std::pow(tsi, 4);
  const double coef1 = coef / std::pow(psisq, 3.5);
  const double cc2 = coef1 * no_ *
                     (ao_ * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) +
                      0.375 * kJ2 * tsi / psisq * con41_ * (8.0 + 3.0 * etasq * (8.0 + etasq)));
  cc1_ = bstar_ * cc2;
  double cc3 = 0.0;
  if (ecco_ > 1.0e-4) cc3 = -2.0 * coef * tsi * kJ3oJ2 * no_ * sinio / ecco_;
  x1mth2_ = 1.0 - cosio2;
  cc4_ = 2.0 * no_ * coef1 * ao_ * omeosq *
         (eta_ * (2.0 + 0.5 * etasq) + ecco_ * (0.5 + 2.0 * etasq) -
          kJ2 * tsi / (ao_ * psisq) *
              (-3.0 * con41_ * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) +
               0.75 * x1mth2_ * (2.0 * etasq - eeta * (1.0 + etasq)) * std::cos(2.0 * argpo_)));
  cc5_ = 2.0 * coef1 * ao_ * omeosq * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);
  const double cosio4 = cosio2 * cosio2;
  const double temp1 = 1.5 * kJ2 * pinvsq * no_;
  const double temp2 = 0.5 * temp1 * kJ2 * pinvsq;
  const double temp3 = -0.46875 * kJ4 * pinvsq * pinvsq * no_;
  mdot_ = no_ + 0.5 * temp1 * rteosq * con41_ +
          0.0625 * temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
  argpdot_ = -0.5 * temp1 * con42 + 0.0625 * temp2 * (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
             temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
  const double xhdot1 = -temp1 * cosio;
  nodedot_ = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) + 2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
  omgcof_ = bstar_ * cc3 * std::cos(argpo_);
  xmcof_ = 0.0;
  if (ecco_ > 1.0e-4) xmcof_ = -kX2o3 * coef * bstar_ / eeta;
  nodecf_ = 3.5 * omeosq * xhdot1 * cc1_;
  t2cof_ = 1.5 * cc1_;
  const double den = std::fabs(cosio + 1.0) > 1.5e-12 ? (1.0 + cosio) : 1.5e-12;
  xlcof_ = -0.25 * kJ3oJ2 * sinio * (3.0 + 5.0 * cosio) / den;
  aycof_ = -0.5 * kJ3oJ2 * sinio;
  delmo_ = std::pow(1.0 + eta_ * std::cos(mo_), 3);
  sinmao_ = std::sin(mo_);
  x7thm1_ = 7.0 * cosio2 - 1.0;
  d2_ = d3_ = d4_ = t3cof_ = t4cof_ = t5cof_ = 0;
  if (isimp_ != 1) {
    const double cc1sq = cc1_ * cc1_;
    d2_ = 4.0 * ao_ * tsi * cc1sq;
    const double temp = d2_ * tsi * cc1_ / 3.0;
    d3_ = (17.0 * ao_ + sfour) * temp;
    d4_ = 0.5 * temp * ao_ * tsi * (221.0 * ao_ + 31.0 * sfour) * cc1_;
    t3cof_ = d2_ + 2.0 * cc1sq;
    t4cof_ = 0.25 * (3.0 * d3_ + cc1_ * (12.0 * d2_ + 10.0 * cc1sq));
    t5cof_ = 0.2 * (3.0 * d4_ + 12.0 * cc1_ * d3_ + 6.0 * d2_ * d2_ + 15.0 * cc1sq * (2.0 * d2_ + cc1sq));
  }
  valid_ = true;
  return true;
}

bool Satellite::propagate(double t, Vec3& r, Vec3& v) const {
  if (!valid_) return false;
  const double xmdf = mo_ + mdot_ * t;
  const double argpdf = argpo_ + argpdot_ * t;
  const double nodedf = nodeo_ + nodedot_ * t;
  double argpm = argpdf;
  double mm = xmdf;
  const double t2 = t * t;
  double nodem = nodedf + nodecf_ * t2;
  double tempa = 1.0 - cc1_ * t;
  double tempe = bstar_ * cc4_ * t;
  double templ = t2cof_ * t2;

  if (isimp_ != 1) {
    const double delomg = omgcof_ * t;
    const double delmtemp = 1.0 + eta_ * std::cos(xmdf);
    const double delm = xmcof_ * (delmtemp * delmtemp * delmtemp - delmo_);
    const double temp = delomg + delm;
    mm = xmdf + temp;
    argpm = argpdf - temp;
    const double t3 = t2 * t, t4 = t3 * t;
    tempa = tempa - d2_ * t2 - d3_ * t3 - d4_ * t4;
    tempe = tempe + bstar_ * cc5_ * (std::sin(mm) - sinmao_);
    templ = templ + t3cof_ * t3 + t4 * (t4cof_ + t * t5cof_);
  }

  double nm = no_;
  double em = ecco_;
  const double inclm = inclo_;
  const double am = std::pow(kXke / nm, kX2o3) * tempa * tempa;
  nm = kXke / std::pow(am, 1.5);
  em = em - tempe;
  if (em >= 1.0 || em < -0.001 || am < 0.95) return false;
  if (em < 1.0e-6) em = 1.0e-6;
  mm = mm + no_ * templ;
  double xlm = mm + argpm + nodem;
  nodem = std::fmod(nodem, kTwoPi);
  argpm = std::fmod(argpm, kTwoPi);
  xlm = std::fmod(xlm, kTwoPi);
  mm = std::fmod(xlm - argpm - nodem, kTwoPi);

  const double sinip = std::sin(inclm), cosip = std::cos(inclm);

  // Long-period periodics
  const double axnl = em * std::cos(argpm);
  double temp = 1.0 / (am * (1.0 - em * em));
  const double aynl = em * std::sin(argpm) + temp * aycof_;
  const double xl = mm + argpm + nodem + temp * xlcof_ * axnl;

  // Kepler's equation
  const double u = std::fmod(xl - nodem, kTwoPi);
  double eo1 = u, tem5 = 9999.9, sineo1 = 0, coseo1 = 0;
  for (int ktr = 1; std::fabs(tem5) >= 1.0e-12 && ktr <= 10; ktr++) {
    sineo1 = std::sin(eo1);
    coseo1 = std::cos(eo1);
    tem5 = 1.0 - coseo1 * axnl - sineo1 * aynl;
    tem5 = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
    if (std::fabs(tem5) >= 0.95) tem5 = tem5 > 0.0 ? 0.95 : -0.95;
    eo1 += tem5;
  }

  // Short-period periodics
  const double ecose = axnl * coseo1 + aynl * sineo1;
  const double esine = axnl * sineo1 - aynl * coseo1;
  const double el2 = axnl * axnl + aynl * aynl;
  const double pl = am * (1.0 - el2);
  if (pl < 0.0) return false;
  const double rl = am * (1.0 - ecose);
  const double rdotl = std::sqrt(am) * esine / rl;
  const double rvdotl = std::sqrt(pl) / rl;
  const double betal = std::sqrt(1.0 - el2);
  temp = esine / (1.0 + betal);
  const double sinu = am / rl * (sineo1 - aynl - axnl * temp);
  const double cosu = am / rl * (coseo1 - axnl + aynl * temp);
  double su = std::atan2(sinu, cosu);
  const double sin2u = (cosu + cosu) * sinu;
  const double cos2u = 1.0 - 2.0 * sinu * sinu;
  temp = 1.0 / pl;
  const double temp1 = 0.5 * kJ2 * temp;
  const double temp2 = temp1 * temp;

  const double mrt = rl * (1.0 - 1.5 * temp2 * betal * con41_) + 0.5 * temp1 * x1mth2_ * cos2u;
  su = su - 0.25 * temp2 * x7thm1_ * sin2u;
  const double xnode = nodem + 1.5 * temp2 * cosip * sin2u;
  const double xinc = inclm + 1.5 * temp2 * cosip * sinip * cos2u;
  const double mvt = rdotl - nm * temp1 * x1mth2_ * sin2u / kXke;
  const double rvdot = rvdotl + nm * temp1 * (x1mth2_ * cos2u + 1.5 * con41_) / kXke;

  const double sinsu = std::sin(su), cossu = std::cos(su);
  const double snod = std::sin(xnode), cnod = std::cos(xnode);
  const double sini = std::sin(xinc), cosi = std::cos(xinc);
  const double xmx = -snod * cosi, xmy = cnod * cosi;
  const double ux = xmx * sinsu + cnod * cossu;
  const double uy = xmy * sinsu + snod * cossu;
  const double uz = sini * sinsu;
  const double vx = xmx * cossu - cnod * sinsu;
  const double vy = xmy * cossu - snod * sinsu;
  const double vz = sini * cossu;

  if (mrt < 1.0) return false;  // decayed
  const double vkmpersec = kRe * kXke / 60.0;
  r = {mrt * ux * kRe, mrt * uy * kRe, mrt * uz * kRe};
  v = {(mvt * ux + rvdot * vx) * vkmpersec, (mvt * uy + rvdot * vy) * vkmpersec,
       (mvt * uz + rvdot * vz) * vkmpersec};
  return true;
}

bool Satellite::propagateJd(double jd, Vec3& r, Vec3& v) const {
  return propagate((jd - jdEpoch_) * 1440.0, r, v);
}

// ---------------------------------------------------------------------------

double unixToJd(double s) { return s / 86400.0 + 2440587.5; }
double jdToUnix(double jd) { return (jd - 2440587.5) * 86400.0; }

double gmst(double jd) {
  const double tut1 = (jd - 2451545.0) / 36525.0;
  double temp = -6.2e-6 * tut1 * tut1 * tut1 + 0.093104 * tut1 * tut1 +
                (876600.0 * 3600 + 8640184.812866) * tut1 + 67310.54841;
  temp = std::fmod(temp * kDeg2Rad / 240.0, kTwoPi);
  if (temp < 0.0) temp += kTwoPi;
  return temp;
}

Vec3 temeToEcef(const Vec3& p, double jd) {
  const double g = gmst(jd);
  const double c = std::cos(g), s = std::sin(g);
  return {c * p.x + s * p.y, -s * p.x + c * p.y, p.z};
}

Vec3 geodeticToEcef(const Geodetic& g) {
  const double lat = g.latDeg * kDeg2Rad, lon = g.lonDeg * kDeg2Rad;
  const double e2 = kWgs84F * (2 - kWgs84F);
  const double sl = std::sin(lat);
  const double n = kWgs84A / std::sqrt(1 - e2 * sl * sl);
  return {(n + g.altKm) * std::cos(lat) * std::cos(lon), (n + g.altKm) * std::cos(lat) * std::sin(lon),
          (n * (1 - e2) + g.altKm) * sl};
}

Geodetic ecefToGeodetic(const Vec3& p) {
  const double e2 = kWgs84F * (2 - kWgs84F);
  const double lon = std::atan2(p.y, p.x);
  const double rxy = std::sqrt(p.x * p.x + p.y * p.y);
  double lat = std::atan2(p.z, rxy * (1 - e2));
  double n = kWgs84A, alt = 0;
  for (int i = 0; i < 5; i++) {
    const double sl = std::sin(lat);
    n = kWgs84A / std::sqrt(1 - e2 * sl * sl);
    alt = rxy / std::cos(lat) - n;
    lat = std::atan2(p.z, rxy * (1 - e2 * n / (n + alt)));
  }
  return {lat / kDeg2Rad, lon / kDeg2Rad, alt};
}

LookAngle lookAngle(const Geodetic& obs, const Vec3& tgt) {
  const Vec3 o = geodeticToEcef(obs);
  const double rx = tgt.x - o.x, ry = tgt.y - o.y, rz = tgt.z - o.z;
  const double lat = obs.latDeg * kDeg2Rad, lon = obs.lonDeg * kDeg2Rad;
  const double sla = std::sin(lat), cla = std::cos(lat), slo = std::sin(lon), clo = std::cos(lon);
  const double south = sla * clo * rx + sla * slo * ry - cla * rz;
  const double east = -slo * rx + clo * ry;
  const double zen = cla * clo * rx + cla * slo * ry + sla * rz;
  const double range = std::sqrt(rx * rx + ry * ry + rz * rz);
  double az = std::atan2(east, -south) / kDeg2Rad;
  if (az < 0) az += 360.0;
  return {az, std::asin(zen / range) / kDeg2Rad, range};
}

Vec3 sunEci(double jd) {
  // Low-precision solar coordinates (Astronomical Almanac), ~0.01 deg.
  const double n = jd - 2451545.0;
  const double L = std::fmod(280.460 + 0.9856474 * n, 360.0) * kDeg2Rad;
  const double g = std::fmod(357.528 + 0.9856003 * n, 360.0) * kDeg2Rad;
  const double lambda = L + (1.915 * std::sin(g) + 0.020 * std::sin(2 * g)) * kDeg2Rad;
  const double eps = (23.439 - 0.0000004 * n) * kDeg2Rad;
  const double rAu = 1.00014 - 0.01671 * std::cos(g) - 0.00014 * std::cos(2 * g);
  const double rKm = rAu * 149597870.7;
  return {rKm * std::cos(lambda), rKm * std::cos(eps) * std::sin(lambda),
          rKm * std::sin(eps) * std::sin(lambda)};
}

void subsolarPoint(double jd, double& latDeg, double& lonDeg) {
  const Vec3 s = temeToEcef(sunEci(jd), jd);
  const double r = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
  latDeg = std::asin(s.z / r) / kDeg2Rad;
  lonDeg = std::atan2(s.y, s.x) / kDeg2Rad;
}

double sunElevationDeg(const Geodetic& obs, double jd) {
  return lookAngle(obs, temeToEcef(sunEci(jd), jd)).elDeg;
}

bool isSunlit(const Vec3& sat, double jd) {
  const Vec3 s = sunEci(jd);
  const double sn = std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z);
  const double ux = s.x / sn, uy = s.y / sn, uz = s.z / sn;
  const double proj = sat.x * ux + sat.y * uy + sat.z * uz;
  if (proj > 0) return true;  // on the day side of the terminator plane
  const double px = sat.x - proj * ux, py = sat.y - proj * uy, pz = sat.z - proj * uz;
  return std::sqrt(px * px + py * py + pz * pz) > kRe;
}

}  // namespace sgp4
