/* ------------------------------------------------------------------
   LuxRev-1 — briques communes aux maquettes de façade.

   Partagé pour que les variantes ne diffèrent QUE par la mise en page :
   si un bouton se comporte différemment d'une maquette à l'autre, on
   compare deux comportements au lieu de comparer deux formes.

   Chaque variante fournit son propre HTML et son propre CSS, puis
   appelle monter() avec les conteneurs qu'elle a créés.
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
  b.addEventListener("dblclick", () => { v = c.def; pose(true); });

  return {bloc:bloc, set:x=>{ v=x; pose(false); }, def:()=>{ v=c.def; pose(false); }};
}

/* ---------------- la courbe mesurée ----------------
   ⚠️ Ce n'est PAS un décor. Les 100 points viennent de `enveloppe_db` des
   mesures réelles sur les 14 IR (mesures.js, généré depuis _proto/mesures/).
   C'est ce qui distingue cet écran d'un sélecteur de presets : il montre la
   forme mesurée du programme, y compris ce qu'elle a d'atypique — le plateau
   de Gate, la montée d'Inverse.                                              */

function courbe(svg, nom){
  const m = MESURES[nom];
  if(!m) return;
  const L = 300, H = 78, PLANCHER = -60;
  const y = d => H - Math.max(0, Math.min(1, (d - PLANCHER)/(0 - PLANCHER))) * H;
  const pts = m.env.map((d,i) => (i/(m.env.length-1)*L).toFixed(1) + "," + y(d).toFixed(1));

  /* le temps de mélange : l'instant où la queue devient indistinguable d'un bruit.
     Repère utile — c'est lui qui sépare une salle d'une plaque à l'oreille. */
  const xm = Math.min(L, m.mel / 3000 * L);

  svg.setAttribute("viewBox", "0 0 " + L + " " + H);
  svg.innerHTML =
      '<polyline class="trace-fond" points="' + pts.join(" ") + " " + L + "," + H + " 0," + H + '"/>'
    + '<polyline class="trace" points="' + pts.join(" ") + '"/>'
    + '<line class="melange" x1="'+xm+'" y1="0" x2="'+xm+'" y2="'+H+'"/>'
    + '<text class="graduation" x="'+(xm+4)+'" y="10">mélange ' + Math.round(m.mel) + ' ms</text>'
    + '<text class="graduation" x="2" y="'+(H-3)+'">0</text>'
    + '<text class="graduation" x="'+(L-24)+'" y="'+(H-3)+'">3 s</text>';
}

function spectre(svg, nom){
  const m = MESURES[nom];
  if(!m) return;
  const L = 300, H = 34, PLAGE = 12;   // +/- 12 dB autour du plat
  const n = m.sp.length, larg = L/n - 3;
  let h = '<line class="zero" x1="0" y1="' + (H/2) + '" x2="' + L + '" y2="' + (H/2) + '"/>';
  m.sp.forEach(function(p, i){
    const t = Math.max(-1, Math.min(1, p[1]/PLAGE));
    const hh = Math.max(1, Math.abs(t) * H/2);
    const y = t >= 0 ? H/2 - hh : H/2;
    h += '<rect class="barre" x="' + (i*(L/n)+1.5) + '" y="' + y + '" width="' + larg
       + '" height="' + hh + '"><title>' + p[0] + ' Hz : ' + p[1].toFixed(1) + ' dB</title></rect>';
  });
  svg.setAttribute("viewBox", "0 0 " + L + " " + H);
  svg.innerHTML = h;
}
