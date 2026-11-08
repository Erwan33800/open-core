"use strict";
/* Open Core : pont générique entre la page et le plugin (paramètres automatisables + état d'interface).
   Placé en tête du script par assemble_ui.py, avant les scripts du plugin. Les scripts du plugin partagent ces globales.

   Protocole : événement JUCE unique, tableau [commande, ...arguments]. Côté C++ : open::ParamBridgeEditor.
     Page -> plugin : ready ; begin <id> ; set <id> <valeur> ; end <id> ; ui <json>
     Plugin -> page : snapshot {id: valeur} <json d'interface> ; params [[id, valeur], ...]
   Les valeurs sont les VRAIES valeurs des paramètres (Hz, dB, index de choix, 0/1), jamais normalisées.
   Hors plugin (page ouverte dans un navigateur), tout ceci est inactif : JUCE_MODE vaut false.

   Ce que le plugin doit définir (fonctions et constante globales) :
     BRIDGE_EVENT          nom de l'événement JUCE, identique à celui du C++ (ex. "openfilter")
     modelToParams()       -> { idParametre: valeur } : l'état de l'interface vers les paramètres
     applyParams(values)   valeurs { id: valeur } venues du plugin -> mise à jour du modèle et de l'affichage
                           (les valeurs déjà connues sont dans `known`)
     uiStateJson()         -> chaîne JSON des réglages d'interface à sauvegarder avec le set
     applyUiState(json)    restaure ces réglages (peut recevoir une chaîne vide)
   Et appeler : bridgePush() après chaque changement du modèle ; bridgeStart() une fois la page prête. */

/* Mode plugin : la page est dans le WebView du plugin (window.__JUCE__). Sinon : navigateur, données factices. */
const JUCE_MODE = !!(window.__JUCE__ && window.__JUCE__.backend);

var bridgeReady = false;      // vrai quand le plugin a envoyé l'état : avant, on ne pousse rien (on écraserait l'état sauvegardé)
const known = {};             // dernière valeur connue de chaque paramètre (envoyée ou reçue)
const touched = new Set();    // paramètres modifiés pendant le geste en cours
var gestureActive = false;

function toPlugin(...a) { if (JUCE_MODE) try { window.__JUCE__.backend.emitEvent(BRIDGE_EVENT, a); } catch (_) {} }

/* Envoie au plugin ce qui a changé depuis la dernière synchronisation. Pendant un geste (bouton de souris enfoncé),
   chaque paramètre ouvre un geste d'automation qui se referme au relâchement ; sinon (molette, menu), begin/set/end d'un coup. */
function bridgePush() {
  if (!JUCE_MODE || !bridgeReady) return;
  const values = modelToParams();

  for (const id in values) {
    const v = values[id];
    if (known[id] === v) continue;

    if (gestureActive) {
      if (!touched.has(id)) { touched.add(id); toPlugin("begin", id); }
      toPlugin("set", id, v);
    } else {
      toPlugin("begin", id); toPlugin("set", id, v); toPlugin("end", id);
    }
    known[id] = v;
  }
}

function endGesture() {
  gestureActive = false;
  touched.forEach(id => toPlugin("end", id));
  touched.clear();
}

var lastUiJson = "";
function persistUi() {
  if (!JUCE_MODE || !bridgeReady) return;
  const json = uiStateJson();
  if (json !== lastUiJson) { lastUiJson = json; toPlugin("ui", json); }
}

function bridgeStart() {
  if (!JUCE_MODE) return;

  window.__JUCE__.backend.addEventListener(BRIDGE_EVENT, a => {
    try {
      const arr = Array.isArray(a) ? a : [];
      if (arr[0] === "snapshot") {
        applyUiState(arr[2]);
        applyParams(arr[1] || {});
        lastUiJson = uiStateJson();
        bridgeReady = true;
      } else if (arr[0] === "params" && bridgeReady) {
        const values = {};
        (arr[1] || []).forEach(p => { values[p[0]] = p[1]; });
        applyParams(values);
      }
    } catch (e) { console.error(e); }
  });

  // Un geste dure du bouton enfoncé au bouton relâché.
  document.addEventListener("pointerdown", () => { gestureActive = true; }, true);
  window.addEventListener("pointerup", endGesture, true);
  window.addEventListener("pointercancel", endGesture, true);
  window.addEventListener("blur", endGesture);

  setInterval(persistUi, 700);
  toPlugin("ready");
}
