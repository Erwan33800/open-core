# Open Core

Le code commun aux plugins de la série Open (Open Filter, Open Comp, Open Groove...) : ce qui était dupliqué d'un plugin à l'autre, mis à un seul endroit.

Chaque plugin garde son dépôt, sa CI et sa page Releases. Open Core est tiré à une version précise, comme JUCE.

## Ce qu'il contient

| Dossier | Rôle |
|---|---|
| `cmake/OpenPlugin.cmake` | `open_add_plugin()` (plugin JUCE complet : VST3, Standalone, AU, WebView2) et `open_use_catch2()` |
| `Source/open/ParamBridgeEditor.*` | Fenêtre WebView2 reliée aux paramètres (APVTS) : gestes d'automation, surveillance des valeurs, état d'interface |
| `ui/open-base.css` | Identité visuelle : tokens (couleurs, polices), en-tête, onglets, boutons, valeurs à glisser, segments, menus |
| `ui/bridge-core.js` | Côté page du pont : paramètres, gestes, état d'interface (protocole décrit en tête du fichier) |
| `ui/assemble_ui.py` | Assemble la page `ui/<slug>_ui.html` d'un plugin (CSS commun + police en base64 + scripts) |
| `ui/manrope-latin.woff2` | Police Manrope (SIL OFL) |
| `.github/workflows/plugin.yml` | CI commune : tests, Windows (+ pluginval), macOS, release « latest » |

Pas encore dans le noyau, volontairement : le DSP (enveloppes, lissage, lookahead) et la théorie musicale (gammes, accords). On les y met quand un deuxième plugin en a besoin, pas avant.

## Utiliser Open Core dans un plugin

### 1. Version épinglée

Un fichier `OPEN_CORE_VERSION` à la racine du plugin, une ligne : `v0.1.0`. C'est la seule source de vérité pour CMake, `build_ui.py` et la CI.

### 2. CMakeLists.txt du plugin

```cmake
include(FetchContent)
if(DEFINED ENV{OPEN_CORE_DIR})
    file(TO_CMAKE_PATH "$ENV{OPEN_CORE_DIR}" OPEN_CORE_SOURCE_DIR)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../open-core/cmake/OpenPlugin.cmake")
    get_filename_component(OPEN_CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../open-core" ABSOLUTE)
else()
    file(STRINGS OPEN_CORE_VERSION OPEN_CORE_TAG LIMIT_COUNT 1)
    FetchContent_Declare(open-core
        GIT_REPOSITORY https://github.com/Erwan33800/open-core.git
        GIT_TAG        ${OPEN_CORE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(open-core)
    set(OPEN_CORE_SOURCE_DIR ${open-core_SOURCE_DIR})
endif()
include(${OPEN_CORE_SOURCE_DIR}/cmake/OpenPlugin.cmake)

open_use_catch2()                       # si le plugin a des tests du DSP

open_add_plugin(
    TARGET        OpenFilter            # cible CMake
    PRODUCT_NAME  "Open Filter"         # nom dans Live
    SLUG          openfilter            # fr.erwan.<slug>, ui/<slug>_ui.html
    PLUGIN_CODE   OpnF                  # 4 caractères, unique dans la série
    KIND          EFFECT                # EFFECT, ou INSTRUMENT pour un générateur MIDI
    CATEGORIES    "Fx EQ"
    SOURCES       Source/PluginProcessor.cpp Source/PluginEditor.cpp
    LINK          OpenFilterDsp)
```

Ordre de résolution d'Open Core : variable d'environnement `OPEN_CORE_DIR`, sinon dossier `open-core` voisin du plugin (utilisé tel quel, sans tenir compte de `OPEN_CORE_VERSION`), sinon téléchargement à la version épinglée. La CI télécharge toujours la version épinglée.

### 3. Éditeur du plugin

```cpp
class MyEditor final : public open::ParamBridgeEditor
{
public:
    explicit MyEditor (MyProcessor& p) : open::ParamBridgeEditor (p, p.apvts, makeConfig (p)) {}
};
```

`Config` : nom de l'événement du pont, dossier de données de la WebView2, page embarquée (`BinaryData::<slug>_ui_html`), accès à l'état d'interface, limites de taille. Voir `Source/open/ParamBridgeEditor.h` et, en exemple, `open-filter/Source/PluginEditor.cpp`. Tous les paramètres de l'APVTS sont exposés à la page, sans liste à tenir à jour.

### 4. Interface

`ui/src/plugin-head.html` contient le `<head>`, le corps de la page, et à l'intérieur de `<style>` le marqueur `/*__OPEN_CORE_CSS__*/` (le CSS commun y est inséré), puis le CSS propre au plugin. Il se termine par `<script>` ouvert.

`ui/build_ui.py` du plugin :

```python
import os, sys
from pathlib import Path
root = Path(__file__).parent
core = next(Path(c) for c in (os.environ.get("OPEN_CORE_DIR"), root.parent.parent / "open-core")
            if c and (Path(c) / "ui" / "assemble_ui.py").exists())
sys.path.insert(0, str(core / "ui"))
from assemble_ui import assemble
assemble(root, slug="openfilter", scripts=["app.js", "bridge.js", "init.js"])
```

Le script du plugin doit définir `BRIDGE_EVENT`, `modelToParams()`, `applyParams()`, `uiStateJson()`, `applyUiState()`, et appeler `bridgePush()` / `bridgeStart()` (contrat détaillé en tête de `ui/bridge-core.js`).

Règles de la charte (dans `open-base.css`) : look plat, tuiles arrondies, un seul accent à la fois (`--accent`, redéfinissable par le plugin), pas de couleur en dur dans un composant. **Ne jamais écrire `/*` dans un commentaire CSS** : `assemble_ui.py` le refuse.

### 5. CI

`.github/workflows/build.yml` du plugin appelle le workflow commun (voir `open-filter/.github/workflows/build.yml`). Le plugin doit avoir un `README.md` à la racine (copié dans le zip Windows).

## Faire évoluer Open Core

1. Travailler dans `open-core/` avec le plugin à côté : le plugin utilise le dossier voisin, sans rien à publier.
2. Régénérer la page de chaque plugin concerné (`python ui/build_ui.py`) et lancer ses tests.
3. Committer, puis poser un tag de version : `git tag v0.2.0 && git push --tags`. Une version publiée n'est jamais modifiée.
4. Dans chaque plugin qui adopte la nouvelle version : `OPEN_CORE_VERSION`, la référence `@v0.2.0` dans `build.yml`, régénérer la page, committer.

Un changement qui casserait un plugin (renommer une fonction du pont, changer `Config`...) passe par une nouvelle version majeure ou reste compatible.

## Tester

Le noyau n'a pas de tests propres pour l'instant : il est vérifié par ceux des plugins qui l'utilisent (28 vérifications du pont dans `open-filter/tests/ui/bridge_test.py`, tests du DSP, pluginval en CI).

Licences : JUCE 8 (Starter / AGPLv3), police Manrope (SIL OFL), Catch2 (BSL-1.0).
