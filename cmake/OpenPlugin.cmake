# Open Core : module CMake commun aux plugins Open.
#
# Un plugin le charge depuis son CMakeLists.txt (voir README.md d'open-core pour l'amorce de 10 lignes), puis appelle :
#
#   open_use_catch2()                      tests du DSP (Catch2 v3, tag épinglé)
#   open_add_plugin(TARGET ... )           plugin JUCE complet (VST3, Standalone, AU sur Mac) avec interface WebView2
#
# JUCE est tiré ici (tag épinglé) : un plugin ne le déclare pas lui-même.

include_guard(GLOBAL)
include(FetchContent)

get_filename_component(OPEN_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(OPEN_CORE_JUCE_TAG 8.0.4)
set(OPEN_CORE_CATCH2_TAG v3.7.1)

# --- Tests : Catch2 v3 ------------------------------------------------------------------------------------------
macro(open_use_catch2)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        ${OPEN_CORE_CATCH2_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    enable_testing()
    include(Catch)
endmacro()

# --- Plugin -----------------------------------------------------------------------------------------------------
#
# open_add_plugin(
#     TARGET        OpenFilter                 nom de la cible CMake (aussi le préfixe des cibles OpenFilter_VST3, ...)
#     PRODUCT_NAME  "Open Filter"              nom affiché dans Live
#     SLUG          openfilter                 identifiant court : fr.erwan.<slug>, page ui/<slug>_ui.html
#     PLUGIN_CODE   OpnF                       4 caractères, unique dans la série
#     KIND          EFFECT | INSTRUMENT        EFFECT = effet audio ; INSTRUMENT = instrument avec entrée et sortie MIDI
#                                              (Live n'accepte pas les VST « effet MIDI » : un générateur MIDI est un instrument)
#     CATEGORIES    "Fx EQ"                    catégories VST3
#     SOURCES       Source/...                 fichiers du plugin
#     LINK          OpenFilterDsp              bibliothèques du plugin (hors JUCE)
# )
function(open_add_plugin)
    cmake_parse_arguments(OP "" "TARGET;PRODUCT_NAME;SLUG;PLUGIN_CODE;KIND;CATEGORIES" "SOURCES;LINK" ${ARGN})

    foreach(required TARGET PRODUCT_NAME SLUG PLUGIN_CODE KIND CATEGORIES)
        if(NOT OP_${required})
            message(FATAL_ERROR "open_add_plugin : l'argument ${required} est obligatoire")
        endif()
    endforeach()

    if(OP_KIND STREQUAL "EFFECT")
        set(is_synth FALSE)
        set(midi_in  FALSE)
        set(midi_out FALSE)
    elseif(OP_KIND STREQUAL "INSTRUMENT")
        set(is_synth TRUE)
        set(midi_in  TRUE)
        set(midi_out TRUE)
    else()
        message(FATAL_ERROR "open_add_plugin : KIND doit valoir EFFECT ou INSTRUMENT (reçu : ${OP_KIND})")
    endif()

    if(NOT COMMAND juce_add_plugin)
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${OPEN_CORE_JUCE_TAG}
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(juce)
    endif()

    set(formats VST3 Standalone)
    if(APPLE)
        list(APPEND formats AU)
    endif()

    juce_add_plugin(${OP_TARGET}
        COMPANY_NAME                 "Erwan"
        PRODUCT_NAME                 "${OP_PRODUCT_NAME}"
        BUNDLE_ID                    "fr.erwan.${OP_SLUG}"
        PLUGIN_MANUFACTURER_CODE     Ewan
        PLUGIN_CODE                  ${OP_PLUGIN_CODE}
        FORMATS                      ${formats}
        IS_SYNTH                     ${is_synth}
        NEEDS_MIDI_INPUT             ${midi_in}
        NEEDS_MIDI_OUTPUT            ${midi_out}
        IS_MIDI_EFFECT               FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS  TRUE
        NEEDS_WEBVIEW2               TRUE
        COPY_PLUGIN_AFTER_BUILD      FALSE
        VST3_CATEGORIES              ${OP_CATEGORIES})

    # Page de l'interface, générée par ui/build_ui.py puis embarquée dans le binaire (BinaryData::<slug>_ui_html).
    juce_add_binary_data(${OP_TARGET}UI SOURCES ui/${OP_SLUG}_ui.html)

    # Le code commun (éditeur WebView2, pont des paramètres) est compilé dans chaque plugin plutôt que dans une
    # bibliothèque à part : les modules JUCE ne sont ainsi compilés qu'une fois par plugin (pas de symboles en double).
    target_sources(${OP_TARGET} PRIVATE
        ${OP_SOURCES}
        ${OPEN_CORE_DIR}/Source/open/ParamBridgeEditor.cpp)
    target_include_directories(${OP_TARGET} PRIVATE ${OPEN_CORE_DIR}/Source)

    target_compile_definitions(${OP_TARGET} PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    if(UNIX AND NOT APPLE)
        # Linux : sert seulement à vérifier que le code compile, la cible réelle est Windows / macOS.
        target_link_libraries(${OP_TARGET} PRIVATE juce::pkgconfig_JUCE_BROWSER_LINUX_DEPS)
    endif()

    target_link_libraries(${OP_TARGET}
        PRIVATE
            ${OP_TARGET}UI
            juce::juce_audio_utils
            juce::juce_gui_extra
            ${OP_LINK}
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
