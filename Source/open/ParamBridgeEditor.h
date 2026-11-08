#pragma once

#include <atomic>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace open
{
    // Fenêtre d'un plugin Open dont l'interface est une page HTML/JS dans une WebView2, reliée aux paramètres (APVTS).
    //
    // Pont : un seul événement JUCE (Config::eventId), tableau [commande, ...arguments]. Le côté page est
    // open-core/ui/bridge-core.js, qui décrit le protocole :
    //   Page -> plugin : ready ; begin <id> ; set <id> <valeur> ; end <id> ; ui <json>
    //   Plugin -> page : snapshot {id: valeur} <json> ; params [[id, valeur], ...]
    // Les valeurs échangées sont les vraies valeurs des paramètres (Hz, dB, index de choix), jamais normalisées.
    //
    // Tous les paramètres de l'APVTS sont exposés à la page, sans liste à tenir à jour.
    class ParamBridgeEditor : public juce::AudioProcessorEditor,
                              private juce::Timer
    {
    public:
        struct Config
        {
            juce::String eventId;                 // ex. "openfilter" : identique à BRIDGE_EVENT dans la page
            juce::String dataFolderName;          // dossier de données de la WebView2, ex. "OpenFilter"
            const char* pageHtml = nullptr;       // page embarquée (BinaryData::<slug>_ui_html)
            size_t pageSize = 0;                  // sa taille (BinaryData::<slug>_ui_htmlSize)

            // Réglages d'interface sauvegardés avec le set, sous forme de JSON fourni par la page.
            std::function<juce::String()> getUiState;
            std::function<void (const juce::String&)> setUiState;

            // Taille de fenêtre : mêmes limites que le minimum CSS de la page (open-base.css).
            int defaultWidth = 1120, defaultHeight = 740;
            int minWidth = 980, minHeight = 620;
            int maxWidth = 1800, maxHeight = 1000;
        };

        ParamBridgeEditor (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&, Config);
        ~ParamBridgeEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void handleMessage (const juce::var& message);
        void sendSnapshot();
        void send (const juce::Array<juce::var>& message);
        std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

        juce::AudioProcessorValueTreeState& apvts;
        Config config;
        juce::Identifier eventId;
        juce::WebBrowserComponent browser;

        // Surveillance des paramètres (automation de l'hôte, chargement d'un preset de Live...) : on compare 30 fois par
        // seconde les valeurs atomiques à celles déjà connues de la page. Aucun écouteur : rien à faire dans le thread audio.
        std::vector<juce::String> paramIds;
        std::vector<std::atomic<float>*> paramValues;
        std::vector<float> lastSent;
        std::unordered_map<std::string, size_t> paramIndex;

        std::set<juce::String> activeGestures; // gestes d'automation ouverts par la page, refermés si la fenêtre se ferme
        bool pageReady = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamBridgeEditor)
    };
}
