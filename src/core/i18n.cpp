#include "i18n.h"

namespace i18n {
namespace {

struct Pair {
  S id;
  const char* en;
  const char* fr;
};

// Strings may contain printf placeholders; callers pass the matching arguments.
const Pair kStrings[] = {
    {S::Live, "LIVE", "EN DIRECT"},
    {S::Delayed, "DELAYED %ds", "RETARD %ds"},
    {S::Offline, "OFFLINE", "HORS LIGNE"},
    {S::AircraftCount, "%d aircraft", "%d avions"},

    {S::Altitude, "ALTITUDE", "ALTITUDE"},
    {S::Speed, "SPEED", "VITESSE"},
    {S::Heading, "HEADING", "CAP"},
    {S::Distance, "DISTANCE", "DISTANCE"},
    {S::RouteUnknown, "Route unknown", "Trajet inconnu"},
    {S::OnGround, "On the ground", "Au sol"},
    {S::Reg, "Reg.", "Immat."},
    {S::QuietSkies, "Quiet skies", "Ciel calme"},
    {S::NoAircraftWithin, "No aircraft within %s", "Aucun avion à moins de %s"},

    {S::RareAircraft, "RARE AIRCRAFT", "AVION RARE"},
    {S::Military, "MILITARY", "MILITAIRE"},
    {S::Emergency, "EMERGENCY", "URGENCE"},
    {S::Sq7500, "Squawk 7500 - unlawful interference", "Transpondeur 7500 - intervention illicite"},
    {S::Sq7600, "Squawk 7600 - radio failure", "Transpondeur 7600 - panne radio"},
    {S::Sq7700, "Squawk 7700 - general emergency", "Transpondeur 7700 - urgence générale"},
    {S::DeclaredEmergency, "Emergency declared by the crew", "Urgence déclarée par l'équipage"},

    {S::Iss, "ISS", "ISS"},
    {S::IssAltitude, "ALTITUDE", "ALTITUDE"},
    {S::IssSpeed, "SPEED", "VITESSE"},
    {S::InSunlight, "In sunlight", "Au soleil"},
    {S::InShadow, "In Earth's shadow", "Dans l'ombre de la Terre"},
    {S::NextVisiblePass, "NEXT VISIBLE PASS", "PROCHAIN PASSAGE VISIBLE"},
    {S::NoVisiblePass, "None in the next 48 h", "Aucun dans les 48 h"},
    {S::Today, "Today", "Aujourd'hui"},
    {S::Tomorrow, "Tomorrow", "Demain"},
    {S::MaxElevation, "max %d°", "max %d°"},
    {S::OrbitAge, "Orbit data: %d d old", "Orbite : il y a %d j"},
    {S::ComputedOnDevice, "Position computed on device", "Position calculée localement"},
    {S::Over, "Over", "Au-dessus"},
    {S::Duration, "%d min", "%d min"},

    {S::IssVisibleIn, "ISS VISIBLE IN", "ISS VISIBLE DANS"},
    {S::IssNow, "ISS OVERHEAD NOW", "L'ISS PASSE MAINTENANT"},
    {S::LookDir, "Look %s%s", "Regardez %s%s"},
    {S::AppearsIn, "Appears %s%s, %d° up", "Apparaît %s%s, à %d°"},
    {S::RisesTo, "Rises to %d° %s%s", "Monte à %d° %s%s"},
    {S::DisappearsIn, "Disappears %s%s", "Disparaît %s%s"},
    {S::VisibleFor, "Visible for %d min", "Visible pendant %d min"},
    {S::HighNow, "%d° above the horizon", "%d° au-dessus de l'horizon"},
    {S::WaitingForDark, "Look up at %s", "Levez les yeux à %s"},

    {S::SetupTitle, "SETUP", "CONFIGURATION"},
    {S::SettingsTitle, "SETTINGS", "RÉGLAGES"},
    {S::ScanWithCamera, "Scan with your phone's camera", "Scannez avec l'appareil photo"},
    {S::SetupThen, "to choose your Wi-Fi and your town.", "pour choisir votre Wi-Fi et votre ville."},
    {S::SettingsThen, "to change Wi-Fi, town or display options.", "pour changer le Wi-Fi, la ville ou l'affichage."},
    {S::NoCamera, "Or join Wi-Fi %s  ·  password %s", "Ou Wi-Fi %s  ·  mot de passe %s"},
    {S::CloseHint, "Press the button to close.", "Appuyez sur le bouton pour fermer."},
    {S::Connecting, "Connecting to %s...", "Connexion à %s..."},
    {S::ConnectedTo, "Connected to %s", "Connecté à %s"},
    {S::FailPassword, "Wrong password for %s", "Mot de passe incorrect pour %s"},
    {S::FailNotFound, "%s not found. Is it a 2.4 GHz network?", "%s introuvable. Est-ce un réseau 2,4 GHz ?"},
    {S::FailCaptive, "%s needs a sign-in page. Try another network.", "%s exige une page de connexion. Essayez un autre réseau."},
    {S::FailNoInternet, "%s has no internet access.", "%s n'a pas accès à Internet."},
    {S::FailGeneric, "Could not join %s", "Impossible de rejoindre %s"},
    {S::HoldForWifi, "To change Wi-Fi, hold the button for 3 seconds.", "Pour changer de Wi-Fi, maintenez le bouton 3 secondes."},

    {S::OfflineTitle, "OFFLINE", "HORS LIGNE"},
    {S::ReasonWifi, "Wi-Fi disconnected", "Wi-Fi déconnecté"},
    {S::ReasonApi, "Flight data unavailable", "Données de vol indisponibles"},
    {S::ReasonTime, "Waiting for the clock to sync", "En attente de l'heure"},
    {S::ReasonNoLocation, "Location not set", "Position non définie"},
    {S::LastUpdate, "Last data received %s", "Dernières données reçues %s"},
    {S::Never, "never", "jamais"},
    {S::Retrying, "Retrying automatically", "Nouvelle tentative automatique"},
    {S::NotLive, "Nothing on this screen is live.", "Rien sur cet écran n'est en direct."},

    {S::Updating, "Updating to %s", "Mise à jour vers %s"},
    {S::DoNotUnplug, "Please keep the display plugged in.", "Merci de ne pas débrancher l'écran."},
    {S::UpdatedTo, "Updated to %s", "Mis à jour vers %s"},
    {S::UpdateRolledBack, "Update failed, back on %s", "Échec de la mise à jour, retour à %s"},

    {S::ModeAuto, "AUTO", "AUTO"},
    {S::ModeAircraft, "AIRCRAFT", "AVIONS"},
    {S::ModeIss, "ISS", "ISS"},
    {S::HoldToReset, "Keep holding to erase all settings", "Maintenez pour tout réinitialiser"},
    {S::ResetDone, "Settings erased. Restarting.", "Réglages effacés. Redémarrage."},
};

const char* const kCompassEn[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
const char* const kCompassFr[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                    "S", "SSO", "SO", "OSO", "O", "ONO", "NO", "NNO"};
const char* const kDirEn[8] = {"north", "north-east", "east", "south-east",
                               "south", "south-west", "west", "north-west"};
const char* const kDirFr[8] = {"nord", "nord-est", "est", "sud-est", "sud", "sud-ouest", "ouest", "nord-ouest"};
// "au nord", "à l'est", "au sud-est", "à l'ouest"
const char* const kPrefixFr[8] = {"au ", "au ", "à l'", "au ", "au ", "au ", "à l'", "au "};

}  // namespace

const char* tr(Lang lang, S id) {
  static_assert(sizeof(kStrings) / sizeof(kStrings[0]) == static_cast<int>(S::Count_),
                "every string id needs a translation");
  const Pair& p = kStrings[static_cast<int>(id)];
  return lang == Lang::FR ? p.fr : p.en;
}

const char* compass16(Lang lang, int i) { return (lang == Lang::FR ? kCompassFr : kCompassEn)[((i % 16) + 16) % 16]; }
const char* direction8(Lang lang, int i) { return (lang == Lang::FR ? kDirFr : kDirEn)[((i % 8) + 8) % 8]; }
const char* directionPrefix(Lang lang, int i) {
  return lang == Lang::FR ? kPrefixFr[((i % 8) + 8) % 8] : "in the ";
}

}  // namespace i18n
