# Open Core : contexte du projet

Lu automatiquement par Claude Code à l'ouverture du dépôt. Voir `README.md` pour l'utilisation, et `../ROADMAP.md` pour la série.

## Le projet en une phrase

Le code commun aux plugins Open (VST3/AU pour Ableton Live, Erwan, français) : module CMake, éditeur WebView2 avec pont de paramètres, CSS et pont JS partagés, script d'assemblage de la page, CI réutilisable. Chaque plugin garde son dépôt et tire Open Core à une version épinglée.

## Règles

- **Rien n'entre dans le noyau avant d'être utile à deux plugins** (ou d'être déjà dupliqué). Le DSP et la théorie musicale n'y sont pas encore : ils y vont quand Open Comp / Open Chords en ont besoin.
- **Compatibilité** : un plugin déjà publié doit continuer à compiler avec sa version épinglée. Une version taguée ne change jamais. Un changement incompatible = nouvelle version majeure.
- **Pas de bibliothèque statique de modules JUCE** : le code C++ commun est ajouté aux sources de chaque plugin (voir `OpenPlugin.cmake`), pour ne compiler les modules JUCE qu'une fois par plugin.
- **Identifiants de paramètres : jamais renommés** (casse l'automation dans Live). Le noyau n'impose aucun identifiant : ils viennent de l'APVTS du plugin.
- Langue : texte, commentaires, commits en français ; identifiants en anglais.
- Pas de `/*` dans un commentaire CSS (`assemble_ui.py` le vérifie : un commentaire imbriqué fait disparaître la règle suivante, vécu avec `@font-face`).
- Pas de nom de marqueur (`__MANROPE__`, marqueur CSS) écrit en toutes lettres dans un commentaire : il serait remplacé aussi.

## Vérifier un changement

Il n'y a pas encore de tests propres au noyau. Un changement se vérifie dans un plugin qui l'utilise (aujourd'hui Open Filter) :

1. `python ui/build_ui.py` dans le plugin, puis `python tests/ui/bridge_test.py` (28 vérifications, Chrome).
2. Rendu : comparer la page avant / après (captures Chrome headless, ou styles calculés de chaque élément : méthode utilisée pour la création du noyau).
3. DSP : `cmake -B build-tests -DOPENFILTER_PLUGIN=OFF && cmake --build build-tests && ctest --test-dir build-tests` dans WSL.
4. C++ : la compilation complète n'est possible qu'avec la CI Windows / macOS (WSL n'a pas pkg-config ni WebKit). En local, `g++ -fsyntax-only` contre les en-têtes JUCE vérifie la syntaxe.
5. La page générée du plugin est versionnée et la CI vérifie qu'elle est à jour : après tout changement du CSS ou du pont du noyau, régénérer et committer la page de chaque plugin concerné.

## Versions

Tag `vX.Y.Z` sur ce dépôt. Les plugins l'épinglent dans `OPEN_CORE_VERSION` (CMake, `build_ui.py`, CI) et dans la référence `@vX.Y.Z` de leur `build.yml`.

## État

- v0.1.0 (en préparation, pas encore publiée sur GitHub) : extrait d'Open Filter.
- Utilisé par : Open Filter. Open Groove n'est pas migré (décision : il le sera plus tard ou jamais).
