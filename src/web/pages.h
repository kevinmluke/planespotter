// The phone page, served over the display's own Wi-Fi network for both the
// first-time setup and later settings. It needs no internet: the town list is
// served by the device (see /cities.tsv). Plain HTML/JS, English and French.
#pragma once

static const char kSetupPage[] = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PlaneSpotter</title>
<style>
:root{--fg:#111;--bg:#f4f3ef;--card:#fff;--mut:#666;--acc:#111;--ok:#1d6b3a;--bad:#a12020;--line:#8883}
@media (prefers-color-scheme:dark){:root{--fg:#eee;--bg:#161616;--card:#222;--mut:#9a9a9a;--acc:#eee;--ok:#6fcf8f;--bad:#ff8a8a}}
*{box-sizing:border-box}body{margin:0;font:16px/1.4 -apple-system,system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:480px;margin:0 auto;padding:16px 16px 48px}h1{font-size:24px;margin:4px 0 2px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);margin:26px 0 8px;display:flex;gap:8px;align-items:center}
h2 b{display:inline-grid;place-items:center;width:20px;height:20px;border-radius:50%;background:var(--acc);color:var(--bg);font-size:12px}
.top{display:flex;justify-content:space-between;align-items:center}.lang a{color:var(--mut);text-decoration:none;padding:4px 8px;border-radius:8px}.lang a.on{background:var(--acc);color:var(--bg)}
.sub{color:var(--mut);margin:0}.card{background:var(--card);border-radius:14px;box-shadow:0 1px 3px #0002;overflow:hidden}
.net{display:flex;align-items:center;gap:10px;padding:13px 16px;border-bottom:1px solid var(--line);cursor:pointer}
.net:last-child{border-bottom:0}.net span{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.net.sel{background:var(--line)}
.net .bars{flex:none;font-size:12px;color:var(--mut);white-space:nowrap}
label{display:block;font-size:13px;color:var(--mut);margin:12px 0 4px}
input[type=text],input[type=password],input[type=search],input[type=number],select{width:100%;font-size:16px;padding:11px;border-radius:10px;border:1px solid #8886;background:var(--card);color:var(--fg)}
.row{display:flex;align-items:center;gap:12px;padding:11px 16px;border-bottom:1px solid var(--line)}.row:last-child{border-bottom:0}
.row>span{flex:1}.row.st{flex-direction:column;align-items:stretch;gap:6px}.row input[type=range]{flex:1}.val{min-width:58px;text-align:right;font-variant-numeric:tabular-nums}
button{font-size:17px;padding:14px;border:0;border-radius:12px;background:var(--acc);color:var(--bg);font-weight:600;width:100%;margin-top:20px}
button:disabled{opacity:.4}button.sec{background:transparent;color:var(--fg);border:1px solid #8886;font-size:15px;padding:10px;margin:0;flex:1}
.link{color:var(--fg);text-decoration:underline;cursor:pointer;font-size:14px}
#results div{padding:12px 16px;border-bottom:1px solid var(--line);cursor:pointer}#results div:last-child{border-bottom:0}
#results small,.chosen small{color:var(--mut)}.chosen{padding:13px 16px;display:flex;justify-content:space-between;gap:10px;align-items:center}
.msg{margin-top:16px;padding:14px;border-radius:12px;display:none}.msg.ok{display:block;background:#1d6b3a22;color:var(--ok)}
.msg.bad{display:block;background:#a1202022;color:var(--bad)}.msg.wait{display:block;background:var(--line)}
details summary{cursor:pointer;color:var(--mut);font-size:13px;text-transform:uppercase;letter-spacing:.06em;margin:26px 0 8px}
.pair{display:flex;gap:8px}.foot{font-size:12px;color:var(--mut);margin-top:28px;line-height:1.6}.hide{display:none!important}
</style></head><body><main>
<div class="top"><h1>PlaneSpotter</h1><div class="lang"><a href="#" data-l="en">EN</a><a href="#" data-l="fr">FR</a></div></div>
<p class="sub" id="intro"></p>

<h2><b>1</b><span data-t="wifi"></span></h2>
<div id="wifiCurrent" class="card hide"><div class="chosen"><span id="wifiCurName"></span><a class="link" id="wifiChange" data-t="change"></a></div></div>
<div id="wifiPick">
<div class="card" id="list"><div class="net"><span data-t="scanning"></span></div></div>
<p class="sub" style="font-size:13px;margin-top:6px"><a class="link" id="otherNet" data-t="other"></a> · <span data-t="only24"></span></p>
<div id="ssidBox" class="hide"><label data-t="netName"></label><input type="text" id="ssid" autocapitalize="off" autocorrect="off"></div>
<label class="hide" id="entToggleL"><input type="checkbox" id="entToggle"> <span data-t="entAsk"></span></label>
<div id="userBox" class="hide"><label data-t="user"></label><input type="text" id="user" autocapitalize="off" autocorrect="off" autocomplete="username"></div>
<div id="passBox"><label data-t="pass"></label><input type="password" id="pass" autocomplete="current-password"></div>
</div>

<h2><b>2</b><span data-t="town"></span></h2>
<div id="townChosen" class="card hide"><div class="chosen"><span id="townName"></span><a class="link" id="townChange" data-t="change"></a></div></div>
<div id="townPick"><input type="search" id="q" autocomplete="off"><div class="card hide" id="results" style="margin-top:8px"></div>
<p class="sub" style="font-size:13px;margin-top:6px"><a class="link" id="coordsLink" data-t="coords"></a></p>
<div id="coordsBox" class="pair hide"><input type="number" step="any" id="lat" placeholder="48.8566"><input type="number" step="any" id="lon" placeholder="2.3522"></div></div>

<details id="more"><summary data-t="more"></summary><div class="card">
<div class="row st"><span data-t="theme"></span><select id="theme"><option value="0" data-t="t0"></option><option value="1" data-t="t1"></option></select></div>
<div class="row st"><span data-t="units"></span><select id="units"><option value="0" data-t="u0"></option><option value="1" data-t="u1"></option><option value="2" data-t="u2"></option></select></div>
<div class="row st"><span data-t="mode"></span><select id="mode"><option value="0" data-t="m0"></option><option value="1" data-t="m1"></option><option value="2" data-t="m2"></option></select></div>
<div class="row"><span data-t="range"></span><input type="range" id="range" min="5" max="100" step="5"><b class="val" id="rangeV"></b></div>
<div class="row st"><span data-t="rareLevel"></span><select id="rareLevel"><option value="0" data-t="r0"></option><option value="1" data-t="r1"></option><option value="2" data-t="r2"></option><option value="3" data-t="r3"></option></select></div>
<div class="row"><span data-t="rareRange"></span><input type="range" id="rareRange" min="10" max="250" step="10"><b class="val" id="rareRangeV"></b></div>
<div class="row"><span data-t="iss"></span><input type="checkbox" id="issAlerts"></div>
<div class="row"><span data-t="auto"></span><input type="checkbox" id="autoUpdate"></div>
</div></details>

<button id="save" data-t="save"></button>
<div id="msg" class="msg"></div>
<div id="extra" class="pair hide" style="margin-top:14px"><button class="sec" id="upd" data-t="check"></button><button class="sec" id="close" data-t="closeBtn"></button></div>
<p class="foot"><span id="ver"></span><br><span data-t="mac"></span> <span id="mac"></span><br><span data-t="credit"></span></p>
</main><script>
const T={
en:{introSetup:"Two quick steps and the display starts showing the aircraft around you.",introSettings:"Change anything below, then press Save.",wifi:"Wi-Fi network",scanning:"Looking for networks…",none:"No networks found. Move closer to the router?",other:"Other network",only24:"2.4 GHz networks only",netName:"Network name",user:"Username",pass:"Password",entAsk:"This network asks for a username (work network)",town:"Where is the display?",search:"Type your town…",coords:"Enter exact coordinates instead",change:"Change",connected:"Connected to ",more:"More options",theme:"Theme",t0:"Light (black on white)",t1:"Dark (white on black)",units:"Units",u0:"Aviation (ft, kt)",u1:"Metric (m, km/h)",u2:"Imperial (ft, mph)",mode:"Show",m0:"Aircraft, ISS when quiet",m1:"Aircraft only",m2:"ISS only",range:"Closest aircraft within",rareLevel:"Take over the screen for",r0:"Never",r1:"Very rare aircraft only",r2:"Rare & military aircraft",r3:"Anything unusual",rareRange:"…when within",iss:"ISS alerts 10 min before a visible pass",auto:"Install updates automatically",save:"Save",check:"Check for updates",closeBtn:"Close settings",mac:"Device address (for IT):",credit:"Town list © GeoNames (CC BY 4.0)",needWifi:"Choose a Wi-Fi network first.",needTown:"Choose your town first.",connecting:"Connecting to the Wi-Fi… If this page stops responding, look at the display: it shows the result.",ok:"All set! Nearby aircraft will appear on the display within a minute. You can close this page.",saved:"Saved.",wrong_password:"The password was not accepted. Check it and try again.",not_found:"The display can't see this network. It only works with 2.4 GHz Wi-Fi.",captive:"This network needs a sign-in page (like hotel or guest Wi-Fi), which the display can't use. Choose another network.",no_internet:"Connected, but this network has no internet access.",failed:"Could not connect. Check the details and try again.",updating:"Checking for updates. If there is one, the display will install it and restart."},
fr:{introSetup:"Deux étapes rapides et l'écran affiche les avions autour de vous.",introSettings:"Modifiez ce que vous voulez, puis appuyez sur Enregistrer.",wifi:"Réseau Wi-Fi",scanning:"Recherche des réseaux…",none:"Aucun réseau trouvé. Rapprochez-vous de la box ?",other:"Autre réseau",only24:"réseaux 2,4 GHz uniquement",netName:"Nom du réseau",user:"Identifiant",pass:"Mot de passe",entAsk:"Ce réseau demande un identifiant (réseau d'entreprise)",town:"Où se trouve l'écran ?",search:"Tapez votre ville…",coords:"Saisir des coordonnées exactes",change:"Changer",connected:"Connecté à ",more:"Plus d'options",theme:"Thème",t0:"Clair (noir sur blanc)",t1:"Sombre (blanc sur noir)",units:"Unités",u0:"Aviation (ft, kt)",u1:"Métrique (m, km/h)",u2:"Impérial (ft, mph)",mode:"Afficher",m0:"Avions, l'ISS si ciel calme",m1:"Avions uniquement",m2:"ISS uniquement",range:"Avion le plus proche à moins de",rareLevel:"Prendre tout l'écran pour",r0:"Jamais",r1:"Les avions très rares",r2:"Avions rares et militaires",r3:"Tout ce qui sort de l'ordinaire",rareRange:"…à moins de",iss:"Alerte ISS 10 min avant un passage visible",auto:"Installer les mises à jour automatiquement",save:"Enregistrer",check:"Rechercher une mise à jour",closeBtn:"Fermer les réglages",mac:"Adresse de l'appareil (pour le service informatique) :",credit:"Liste des villes © GeoNames (CC BY 4.0)",needWifi:"Choisissez d'abord un réseau Wi-Fi.",needTown:"Choisissez d'abord votre ville.",connecting:"Connexion au Wi-Fi… Si cette page ne répond plus, regardez l'écran : il affiche le résultat.",ok:"C'est prêt ! Les avions proches apparaîtront sur l'écran d'ici une minute. Vous pouvez fermer cette page.",saved:"Enregistré.",wrong_password:"Le mot de passe a été refusé. Vérifiez-le et réessayez.",not_found:"L'écran ne voit pas ce réseau. Il ne fonctionne qu'en Wi-Fi 2,4 GHz.",captive:"Ce réseau exige une page de connexion (comme un Wi-Fi d'hôtel ou invité), que l'écran ne peut pas utiliser. Choisissez un autre réseau.",no_internet:"Connecté, mais ce réseau n'a pas accès à Internet.",failed:"Connexion impossible. Vérifiez les informations et réessayez.",updating:"Recherche de mise à jour. S'il y en a une, l'écran l'installera puis redémarrera."}};
const $=s=>document.querySelector(s);let LG=(navigator.language||"en").startsWith("fr")?"fr":"en",ST={},sel={net:null,town:null},cities=null,TZ=[];
function tr(){document.querySelectorAll("[data-t]").forEach(e=>e.textContent=T[LG][e.dataset.t]);$("#q").placeholder=T[LG].search;
document.querySelectorAll(".lang a").forEach(a=>a.className=a.dataset.l==LG?"on":"");$("#intro").textContent=T[LG][ST.mode=="settings"?"introSettings":"introSetup"];
if(ST.connectedSsid)$("#wifiCurName").textContent=T[LG].connected+ST.connectedSsid;km()}
document.querySelectorAll(".lang a").forEach(a=>a.onclick=e=>{e.preventDefault();LG=a.dataset.l;tr()});
function km(){$("#rangeV").textContent=$("#range").value+" km";$("#rareRangeV").textContent=$("#rareRange").value+" km"}
$("#range").oninput=()=>{if(+$("#rareRange").value<+$("#range").value)$("#rareRange").value=$("#range").value;km()};$("#rareRange").oninput=km;
function show(el,on){$(el).classList.toggle("hide",!on)}
function msg(kind,key){const m=$("#msg");m.className="msg "+kind;m.textContent=T[LG][key]||key}
// ---- Wi-Fi
function bars(r){return r>-55?"▂▄▆█":r>-67?"▂▄▆":r>-78?"▂▄":"▂"}
function pickNet(n,el){document.querySelectorAll(".net").forEach(e=>e.classList.remove("sel"));if(el)el.classList.add("sel");sel.net=n;
show("#ssidBox",!n);show("#entToggleL",!n);show("#userBox",n?n.ent:$("#entToggle").checked);show("#passBox",!n||n.secure);
if(n&&n.ent)$("#user").focus();else if(n&&n.secure)$("#pass").focus()}
$("#otherNet").onclick=()=>pickNet(null,null);$("#entToggle").onchange=()=>show("#userBox",$("#entToggle").checked);
async function scan(){try{const n=await fetch("/scan").then(r=>r.json());if(n.scanning){setTimeout(scan,1500);return}const l=$("#list");l.innerHTML="";
if(!n.length)l.innerHTML='<div class="net"><span>'+T[LG].none+"</span></div>";
n.forEach(x=>{const d=document.createElement("div");d.className="net";d.innerHTML="<span></span><span class=bars>"+(x.ent?"👤 ":x.secure?"🔒 ":"")+bars(x.rssi)+"</span>";
d.firstChild.textContent=x.ssid;d.onclick=()=>pickNet(x,d);l.appendChild(d)})}catch(e){setTimeout(scan,2000)}}
$("#wifiChange").onclick=()=>{show("#wifiCurrent",false);show("#wifiPick",true);scan()};
// ---- Towns (offline list served by the display)
const norm=s=>s.normalize("NFD").replace(/[̀-ͯ]/g,"").toLowerCase().replace(/[-'’.]/g," ");
let loading=null;function loadCities(){if(!loading)loading=fetch("/cities.tsv").then(r=>r.text()).then(txt=>{const lines=txt.split("\n");TZ=lines[0].split("\t").slice(1);
cities=[];for(let i=1;i<lines.length;i++){const f=lines[i].split("\t");if(f.length<6)continue;cities.push({n:f[0],r:f[1],c:f[2],lat:+f[3],lon:+f[4],tz:+f[5],k:norm(f[0])})}});return loading}
function label(c){return c.n+(c.r&&c.r!=c.n?", "+c.r:"")+", "+c.c}
let qt;$("#q").addEventListener("focus",loadCities,{once:true});
$("#q").oninput=()=>{clearTimeout(qt);qt=setTimeout(async()=>{await loadCities();const q=norm($("#q").value.trim());const box=$("#results");box.innerHTML="";
if(q.length<2){show("#results",false);return}const a=[],b=[];for(const c of cities){if(c.k.startsWith(q))a.push(c);else if(b.length<8&&c.k.includes(" "+q))b.push(c);if(a.length>=8)break}
const r=a.concat(b).slice(0,8);show("#results",r.length>0);r.forEach(c=>{const d=document.createElement("div");d.innerHTML="<span></span> <small></small>";
d.firstChild.textContent=c.n;d.lastChild.textContent=(c.r&&c.r!=c.n?c.r+", ":"")+c.c;d.onclick=()=>chooseTown({lat:c.lat,lon:c.lon,place:c.n,label:label(c),tz:TZ[c.tz]});box.appendChild(d)})},150)};
function chooseTown(t){sel.town=t;$("#townName").textContent=t.label;show("#townChosen",true);show("#townPick",false)}
$("#townChange").onclick=()=>{show("#townChosen",false);show("#townPick",true);$("#q").value="";$("#q").focus()};
$("#coordsLink").onclick=()=>show("#coordsBox",true);
// ---- Save & status
async function state(){ST=await fetch("/api/state").then(r=>r.json());return ST}
async function poll(){let s;try{s=await state()}catch(e){setTimeout(poll,1500);return}const j=s.join||{};
if(j.state=="connecting"){setTimeout(poll,1000);return}$("#save").disabled=false;
if(j.state=="ok"){msg("ok","ok");show("#extra",s.mode=="settings")}else if(j.state&&j.state!="idle"){msg("bad",j.state)}}
$("#save").onclick=async()=>{const body={lang:LG=="fr"?1:0,theme:+$("#theme").value,units:+$("#units").value,mode:+$("#mode").value,range:+$("#range").value,rareRange:+$("#rareRange").value,rareLevel:+$("#rareLevel").value,issAlerts:$("#issAlerts").checked,autoUpdate:$("#autoUpdate").checked};
const wifiOpen=!$("#wifiPick").classList.contains("hide");
if(wifiOpen){const ssid=sel.net?sel.net.ssid:$("#ssid").value.trim();if(!ssid){msg("bad","needWifi");return}
const ent=sel.net?sel.net.ent:$("#entToggle").checked;body.wifi={ssid,pass:$("#pass").value,user:ent?$("#user").value.trim():""}}
const la=parseFloat($("#lat").value),lo=parseFloat($("#lon").value);
if(!$("#coordsBox").classList.contains("hide")&&!isNaN(la)&&!isNaN(lo)){body.location={lat:la,lon:lo,place:"",tz:sel.town?sel.town.tz:(ST.tz||"UTC0")}}
else if(sel.town)body.location={lat:sel.town.lat,lon:sel.town.lon,place:sel.town.place,tz:sel.town.tz};
if(!body.location&&!ST.hasLocation){msg("bad","needTown");return}
$("#save").disabled=true;await fetch("/api/save",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)}).catch(()=>{});
if(body.wifi){msg("wait","connecting");setTimeout(poll,1500)}else{msg("ok","saved");$("#save").disabled=false;show("#extra",true)}};
$("#upd").onclick=async()=>{await fetch("/api/update",{method:"POST"}).catch(()=>{});msg("wait","updating")};
$("#close").onclick=async()=>{await fetch("/api/close",{method:"POST"}).catch(()=>{})};
(async()=>{try{await state()}catch(e){}const s=ST.settings||{};if(ST.mode=="settings")LG=s.lang==1?"fr":"en";
$("#theme").value=s.theme|0;$("#units").value=s.units|0;$("#mode").value=s.mode|0;$("#range").value=s.range||30;$("#rareRange").value=s.rareRange||80;
$("#rareLevel").value=s.rareLevel??2;$("#issAlerts").checked=s.issAlerts!==false;$("#autoUpdate").checked=s.autoUpdate!==false;
$("#ver").textContent="PlaneSpotter "+(ST.version||"");$("#mac").textContent=ST.mac||"";
if(ST.connectedSsid){show("#wifiCurrent",true);show("#wifiPick",false)}else scan();
if(ST.hasLocation){$("#townName").textContent=s.place||(s.lat.toFixed(3)+", "+s.lon.toFixed(3));show("#townChosen",true);show("#townPick",false)}
if(ST.mode=="settings")show("#extra",true);tr()})();
</script></body></html>)HTML";
