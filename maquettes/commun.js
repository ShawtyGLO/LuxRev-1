/* ------------------------------------------------------------------
   LuxRev-1 — briques communes aux maquettes de façade.

   Partagé pour que les variantes ne diffèrent QUE par la mise en page :
   si un bouton se comporte différemment d'une maquette à l'autre, on
   compare deux comportements au lieu de comparer deux formes.
   ------------------------------------------------------------------ */

const PROGRAMMES = ["Small 1","Small 2","Medium 1","Medium 2","Large 1","Large 2",
                    "Hall B","Hall D","Plate B","Plate D","Gate","Inverse"];

/* Réglages d'usine : T60 et pré-délai de la Program table du manuel LXP-1
   (_proto/specs_lxp1.json). Le pré-délai monte à 246 ms sur la machine. */
const USINE = {
  "Small 1":[0.8,16], "Small 2":[1.2,16], "Medium 1":[1.2,33], "Medium 2":[1.6,33],
  "Large 1":[2.2,33], "Large 2":[2.4,33], "Hall B":[2.6,33],   "Hall D":[1.6,33],
  "Plate B":[1.7,0],  "Plate D":[1.8,33], "Gate":[0.6,0],      "Inverse":[0.9,0]
};

const COMMANDES = [
  {id:"decay",  nom:"Decay",     min:0.08, max:9.0, def:2.6, pas:.01, fmt:v=>v.toFixed(2)+" s"},
  {id:"delay",  nom:"Pre-Delay", min:0,    max:246, def:33,  pas:1,   fmt:v=>Math.round(v)+" ms"},
  {id:"color",  nom:"Color",     min:0,    max:100, def:50,  pas:1,   fmt:v=>Math.round(v)+" %",  centre:50},
  {id:"stereo", nom:"Stéréo",    min:0,    max:160, def:100, pas:1,   fmt:v=>Math.round(v)+" %",  centre:100},
  {id:"tilt",   nom:"Tilt",      min:-9,   max:9,   def:0,   pas:.1,  fmt:v=>(v>0?"+":"")+v.toFixed(1)+" dB", centre:0},
  {id:"mix",    nom:"Mix",       min:0,    max:100, def:100, pas:1,   fmt:v=>Math.round(v)+" %"}
];

const A0 = -140, A1 = 140, L_ARC = 215;
const SVG = "http://www.w3.org/2000/svg";

/* ---------------- le bouton ---------------- */

function piste(c){
  const s = document.createElementNS(SVG,"svg");
  s.setAttribute("class","piste"); s.setAttribute("viewBox","0 0 100 100");
  let h = '<circle cx="50" cy="50" r="44" stroke-dasharray="215 400" transform="rotate(130 50 50)"/>'
        + '<circle class="actif" cx="50" cy="50" r="44" stroke-dasharray="0 400" transform="rotate(130 50 50)"/>';
  /* Cran de repos, uniquement sur les commandes qui en ont un. Sans lui, l'arc de
     longueur nulle au centre se lit comme une commande morte, pas au repos. */
  if(c.centre !== undefined){
    const a = (A0 + (c.centre - c.min)/(c.max - c.min)*(A1-A0)) * Math.PI/180;
    const sx = Math.sin(a), sy = -Math.cos(a);
    h += '<line class="cran" x1="'+(50+sx*39)+'" y1="'+(50+sy*39)+'" x2="'+(50+sx*48)+'" y2="'+(50+sy*48)+'"/>';
  }
  s.innerHTML = h;
  return s;
}

function fabrique(c, montrer){
  const bloc = document.createElement("div"); bloc.className = "commande";
  const et = document.createElement("div"); et.className = "etiquette"; et.textContent = c.nom;
  const b  = document.createElement("div"); b.className = "bouton"; b.tabIndex = 0;
  b.setAttribute("role","slider"); b.setAttribute("aria-label", c.nom);
  const val = document.createElement("div"); val.className = "valeur";
  b.appendChild(piste(c));
  bloc.append(et, b, val);

  let v = c.def;
  /* ⚠️ Le double-clic ramène au réglage D'USINE DU PROGRAMME COURANT, pas à une
     valeur unique pour tout le plugin. Demandé par l'utilisateur, et c'est le bon
     comportement : Decay « par défaut » ne veut rien dire dans l'absolu — il vaut
     0,6 s sur Gate et 2,6 s sur Hall B. Un défaut global reviendrait à sortir du
     programme dès qu'on annule un geste. `defaut` est donc réécrit à chaque
     changement de programme, par poseDefaut(). */
  let defaut = c.def;
  function pose(afficher){
    const t = (v - c.min) / (c.max - c.min);
    b.style.setProperty("--angle", (A0 + t*(A1-A0)) + "deg");
    val.textContent = c.fmt(v);
    b.setAttribute("aria-valuenow", v);
    const tc = (c.centre === undefined) ? 0 : (c.centre - c.min)/(c.max - c.min);
    b.querySelector(".actif").setAttribute("stroke-dasharray",
      "0 " + (Math.min(t,tc)*L_ARC) + " " + (Math.abs(t-tc)*L_ARC) + " 400");
    if(afficher && montrer) montrer(c, v);
  }
  pose(false);

  let y0 = 0, v0 = 0, actif = false;
  b.addEventListener("pointerdown", e => { actif=true; y0=e.clientY; v0=v; b.setPointerCapture(e.pointerId); pose(true); });
  b.addEventListener("pointermove", e => {
    if(!actif) return;
    const fin = e.shiftKey ? 0.25 : 1;                    // Maj = réglage fin
    v = v0 + (y0 - e.clientY) * (c.max - c.min) / 220 * fin;
    v = Math.max(c.min, Math.min(c.max, Math.round(v / c.pas) * c.pas));
    pose(true);
  });
  b.addEventListener("pointerup", () => { actif = false; });
  b.addEventListener("pointerenter", () => { if(!actif) pose(true); });
  b.addEventListener("keydown", e => {
    const d = {ArrowUp:1, ArrowRight:1, ArrowDown:-1, ArrowLeft:-1}[e.key];
    if(!d) return;
    e.preventDefault();
    v = Math.max(c.min, Math.min(c.max, v + d*c.pas*(e.shiftKey ? 1 : 10)));
    pose(true);
  });
  b.addEventListener("dblclick", () => { v = defaut; pose(true); });

  return {bloc:bloc,
          set:x=>{ v = x; pose(false); },
          poseDefaut:x=>{ defaut = x; },
          def:()=>{ v = defaut; pose(false); }};
}

/* ---------------- la courbe, VIVANTE ----------------

   Base : l'enveloppe RÉELLEMENT MESURÉE du programme (mesures.js, généré depuis
   _proto/mesures/). Les commandes la transforment.

   ⚠️ Correction 2026-09-07 : le fond ne reste plus figé pendant que le trait
   bouge. « Le fond et les tracés devraient bouger, car sinon ça ressemble à un
   bug » — c'est exact : un remplissage immobile sous un trait mobile ne se lit
   pas comme « voici la référence », ça se lit comme un défaut d'affichage.
   Fond et trait ne font plus qu'un.

   ⚠️ La fenêtre de temps S'ADAPTE : « il faut que le graphique de la réverbe
   puisse être vu toujours en entier ». À Decay 9 s, une fenêtre fixe de 3 s
   coupait la queue. L'échelle change donc, et les graduations en secondes
   suivent — elles restent, elles sont utiles.

   Trois transformations sont EXACTES, ce sont les opérations du moteur :
     Decay     dilatation du temps après le pic (T60 demandé / T60 mesuré)
     Pre-Delay décalage, à la milliseconde
     Mix       décalage en dB
   La quatrième ne l'est pas — voir remplissage(), plus bas.                    */

const T_MESURE = 3000;                                   // portée du relevé, en ms
const ECHELLES = [0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 12]; // fenêtres, en s

function fenetre(m, etat){
  const besoin = etat ? (etat.delay/1000 + etat.decay*1.25) : 3;
  for(let i = 0; i < ECHELLES.length; i++) if(ECHELLES[i] >= besoin) return ECHELLES[i]*1000;
  return 12000;
}

/* lecture du relevé à une date quelconque, avec prolongement de la pente au-delà
   des 3 s mesurées — sans quoi un Decay long donnerait une queue tronquée net */
function releve(m, ms){
  const n = m.env.length, pas = T_MESURE/(n-1);
  if(ms <= 0) return m.env[0];
  const x = ms/pas;
  if(x <= n-1){
    const k = Math.floor(x);
    return m.env[k] + (m.env[Math.min(k+1, n-1)] - m.env[k]) * (x - k);
  }
  const pente = (m.env[n-1] - m.env[n-11]) / (10*pas);    // dB/ms
  return m.env[n-1] + pente * (ms - T_MESURE);
}

/* ---- COLOR : le remplissage, et non plus une ondulation obscure ----
   « Voir la courbe bouger sans savoir dans quel sens on est en la regardant
   semble assez obscur. » Exact — l'ondulation disait qu'il se passait quelque
   chose, pas quoi. Le remplissage, lui, se lit d'un coup d'œil :

     crémeux    -> plein et dense
     référence  -> un peu moins
     métallique -> strié, des raies verticales là où ça résonne

   ⚠️ Ce n'est PAS un relevé. COLOR n'a pas d'effet mesurable sur l'enveloppe,
   il agit sur la rugosité. Les raies REPRÉSENTENT ce grain ; leur position
   n'est pas mesurée. Je le signale plutôt que de le laisser passer pour une
   mesure — mais l'axe, lui, est juste : plus on va vers le métallique, plus la
   queue est striée de résonances.                                             */
function remplissage(pts, L, H, grain, yCourbe){
  const opacite = 0.30 - 0.13 * grain;        // crémeux dense, métallique aéré
  let h = '<polygon class="remplissage" style="opacity:' + opacite.toFixed(2) + '" points="'
        + pts + " " + L + "," + H + " 0," + H + '"/>';
  if(grain > 0.02){
    const n = Math.round(14 + grain*30);
    let raies = "";
    for(let i = 1; i < n; i++){
      /* écart pseudo-aléatoire mais STABLE : des raies qui sautent à chaque
         redessin donneraient exactement l'impression de bug qu'on corrige */
      const x = (i + 0.35*Math.sin(i*2.4)) / n * L;
      const o = (0.25 + 0.55*Math.abs(Math.sin(i*1.7))) * grain;
      raies += '<line x1="'+x.toFixed(1)+'" y1="'+yCourbe(x).toFixed(1)+'" x2="'+x.toFixed(1)
             + '" y2="'+H+'" style="opacity:'+o.toFixed(2)+'"/>';
    }
    h += '<g class="raies">' + raies + '</g>';
  }
  return h;
}

function courbe(svg, nom, etat){
  const m = MESURES[nom];
  if(!m) return;
  const L = 300, H = 96, PLANCHER = -60, N = 200;
  const y = d => H - Math.max(0, Math.min(1, (d - PLANCHER)/(0 - PLANCHER))) * H;

  const T    = fenetre(m, etat);
  const pas  = T_MESURE/(m.env.length-1);
  const tPic = m.env.indexOf(Math.max.apply(null, m.env)) * pas;
  const r    = etat ? Math.max(0.05, etat.decay / m.t60) : 1;
  const dec  = etat ? etat.delay : 0;
  const mix  = etat ? 20*Math.log10(Math.max(etat.mix/100, 1e-3)) : 0;
  const grain= etat ? (etat.color - 50)/50 : 0;

  const val = new Array(N);
  for(let i = 0; i < N; i++){
    const t = i/(N-1)*T, ts = t - dec;
    if(ts < 0){ val[i] = PLANCHER; continue; }
    const src = ts <= tPic ? ts : tPic + (ts - tPic)/r;
    val[i] = Math.max(PLANCHER, releve(m, src) + mix);
  }
  const pts = val.map((d,i)=>(i/(N-1)*L).toFixed(1)+","+y(d).toFixed(1)).join(" ");
  const yCourbe = x => y(val[Math.max(0, Math.min(N-1, Math.round(x/L*(N-1))))]);

  /* graduations en secondes — gardées, l'utilisateur les juge utiles */
  const secPas = T <= 1500 ? 0.25 : (T <= 4000 ? 0.5 : (T <= 8000 ? 1 : 2));
  let g = "";
  for(let s = secPas; s < T/1000 - 1e-6; s += secPas){
    const x = s/(T/1000)*L;
    const lib = (s % 1 === 0) ? String(s) : s.toFixed(2).replace(/0+$/,"").replace(/\.$/,"");
    g += '<line class="seconde" x1="'+x.toFixed(1)+'" y1="0" x2="'+x.toFixed(1)+'" y2="'+H+'"/>'
       + '<text class="graduation" x="'+(x+2).toFixed(1)+'" y="'+(H-3)+'">'+lib+'</text>';
  }

  /* le temps de mélange mesuré : l'instant où la queue devient un bruit —
     c'est lui qui sépare une salle d'une plaque à l'oreille */
  const xm = m.mel/T*L;
  let rep = "";
  if(xm < L - 4){
    rep = '<line class="melange" x1="'+xm.toFixed(1)+'" y1="0" x2="'+xm.toFixed(1)+'" y2="'+H+'"/>'
        + '<text class="graduation" x="'+(xm+3).toFixed(1)+'" y="9">mélange '+Math.round(m.mel)+' ms</text>';
  }

  svg.setAttribute("viewBox", "0 0 " + L + " " + H);
  svg.innerHTML = g
    + remplissage(pts, L, H, grain, yCourbe)
    + '<polyline class="trace" points="' + pts + '"/>'
    + rep
    + '<text class="graduation" x="'+(L-26)+'" y="9">'+(T/1000)+' s</text>';
}

/* ---------------- la réponse en fréquence ----------------
   Deux emplois réels, au lieu de remplir :
   1. elle AFFICHE TILT — la seule commande purement spectrale, qui n'aurait
      aucun sens sur une courbe temporelle ;
   2. elle PROUVE QUE COLOR NE TOUCHE PAS AU TIMBRE. Les deux courbes de
      compensation existent exactement pour ça : voir les barres ne pas bouger
      quand on tourne COLOR n'est pas une absence d'information, c'est la
      vérification.                                                            */
function spectre(svg, nom, etat){
  const m = MESURES[nom];
  if(!m) return;
  const L = 300, H = 42, PLAGE = 12, PIVOT = 700;
  const n = m.sp.length, larg = L/n - 3;
  let h = '<line class="zero" x1="0" y1="'+(H/2)+'" x2="'+L+'" y2="'+(H/2)+'"/>';
  m.sp.forEach(function(p, i){
    let v = p[1];
    if(etat && etat.tilt){
      /* tilt du premier ordre : asymptotes à ±tilt/2 de part et d'autre du pivot */
      v += (etat.tilt/2) * Math.max(-1, Math.min(1, Math.log2(p[0]/PIVOT)/3));
    }
    const t  = Math.max(-1, Math.min(1, v/PLAGE));
    const hh = Math.max(1, Math.abs(t) * H/2);
    h += '<rect class="barre" x="'+(i*(L/n)+1.5)+'" y="'+(t>=0 ? H/2-hh : H/2)+'"'
       + ' width="'+larg+'" height="'+hh+'"><title>'+p[0]+' Hz : '+v.toFixed(1)+' dB</title></rect>';
  });
  h += '<text class="graduation" x="2" y="'+(H-2)+'">250 Hz</text>'
     + '<text class="graduation" x="'+(L-32)+'" y="'+(H-2)+'">16 kHz</text>';
  svg.setAttribute("viewBox", "0 0 " + L + " " + H);
  svg.innerHTML = h;
}

/* ---------------- STÉRÉO : deux cercles ----------------
   Idée de l'utilisateur (2026-09-07), et elle règle un problème que j'avais
   déclaré sans solution honnête : « deux cercles qui s'éloignent horizontalement
   quand c'est plus wide, se rejoignent pour n'en faire qu'un en mono, et en
   position standard se rentrent un peu dedans à la manière du signe Gucci ».

   C'est juste, et sans rien inventer : l'écartement EST la décorrélation.
   Mono = un seul disque, corrélation +1. Référence = deux disques imbriqués,
   corrélation 0. Large = ils se détachent, corrélation négative. La figure dit
   ce que mesure la corrélation, sans chiffre à lire.                          */
function cercles(svg, etat){
  const L = 74, H = 30, R = 9.5;
  const w = etat ? etat.stereo/100 : 1;
  /* Échelle revue le 2026-09-07 sur l'œil de l'utilisateur : « ce qui était à
     50 %, je le verrais bien à 100 %, et le maximum comme si c'était à 75 % ».
     L'écartement est donc divisé par deux — à la référence les deux disques
     s'imbriquent nettement, et même au maximum ils se touchent encore. Ce qui
     est fidèle : à 160 % la corrélation vaut −0,44, pas −1. */
  const e = R * 0.65 * w;                   // 0 en mono, 0,65 R à la référence
  svg.setAttribute("viewBox", "0 0 " + L + " " + H);
  svg.innerHTML =
      '<circle class="oeil" cx="'+(L/2 - e).toFixed(1)+'" cy="'+(H/2)+'" r="'+R+'"/>'
    + '<circle class="oeil" cx="'+(L/2 + e).toFixed(1)+'" cy="'+(H/2)+'" r="'+R+'"/>';
}

/* ---------------- HEADROOM ----------------
   Idée de l'utilisateur : reprendre le bandeau de quatre témoins des unités
   Lexicon, qui dit si on entre fort dans la réverbe. Il se lit avec l'étage
   d'entrée (_proto/preampli.py) : la plage du manuel s'arrête à +15 dBV, et
   c'est là que le quatrième témoin s'allume.                                  */
const SEUILS = [-24, -12, -4, 0];           // dB sous le plafond d'entrée

function headroom(hote, dbfs){
  Array.prototype.forEach.call(hote.children, function(b, i){
    b.classList.toggle("on",    dbfs >= SEUILS[i]);
    b.classList.toggle("chaud", i === SEUILS.length-1 && dbfs >= SEUILS[i]);
  });
}
