#include "open/ParamBridgeEditor.h"

#include <cmath>

namespace open
{
    namespace
    {
        const juce::Colour backgroundColour { 0xff151516 }; // --bg de open-base.css

        juce::WebBrowserComponent::Options makeOptions (const juce::String& dataFolderName)
        {
            return juce::WebBrowserComponent::Options {}
                .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2 {}
                                             .withUserDataFolder (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                                                      .getChildFile (dataFolderName)
                                                                      .getChildFile ("WebView"))
                                             .withBackgroundColour (backgroundColour))
                .withNativeIntegrationEnabled()
                .withKeepPageLoadedWhenBrowserIsHidden();
        }
    }

    ParamBridgeEditor::ParamBridgeEditor (juce::AudioProcessor& owner,
                                          juce::AudioProcessorValueTreeState& state,
                                          Config cfg)
        : AudioProcessorEditor (owner),
          apvts (state),
          config (std::move (cfg)),
          eventId (config.eventId),
          browser (makeOptions (config.dataFolderName)
                       .withEventListener (eventId, [this] (const juce::var& message) { handleMessage (message); })
                       .withResourceProvider ([this] (const juce::String& url) { return getResource (url); }))
    {
        // Tous les paramètres de l'APVTS, dans l'ordre de déclaration.
        for (auto* p : owner.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);

            if (ranged == nullptr)
                continue;

            const auto id = ranged->getParameterID();
            auto* raw = apvts.getRawParameterValue (id);

            if (raw == nullptr)
                continue;

            paramIndex[id.toStdString()] = paramIds.size();
            paramIds.push_back (id);
            paramValues.push_back (raw);
        }

        lastSent.assign (paramIds.size(), std::nanf (""));

        addAndMakeVisible (browser);
        browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

        setResizable (true, true);
        setResizeLimits (config.minWidth, config.minHeight, config.maxWidth, config.maxHeight);
        setSize (config.defaultWidth, config.defaultHeight);

        startTimerHz (30);
    }

    ParamBridgeEditor::~ParamBridgeEditor()
    {
        stopTimer();

        // Si la fenêtre se ferme en plein geste, on referme les gestes ouverts : Live ne doit pas en garder d'ouverts.
        for (const auto& id : activeGestures)
            if (auto* param = apvts.getParameter (id))
                param->endChangeGesture();
    }

    void ParamBridgeEditor::paint (juce::Graphics& g)
    {
        g.fillAll (backgroundColour);
    }

    void ParamBridgeEditor::resized()
    {
        browser.setBounds (getLocalBounds());
    }

    std::optional<juce::WebBrowserComponent::Resource> ParamBridgeEditor::getResource (const juce::String& url)
    {
        const auto path = url.fromFirstOccurrenceOf ("/", false, false).upToFirstOccurrenceOf ("?", false, false);

        if (path.isEmpty() || path == "index.html")
        {
            const auto* data = reinterpret_cast<const std::byte*> (config.pageHtml);
            return juce::WebBrowserComponent::Resource { std::vector<std::byte> (data, data + config.pageSize),
                                                         "text/html" };
        }

        return std::nullopt;
    }

    void ParamBridgeEditor::send (const juce::Array<juce::var>& message)
    {
        browser.emitEventIfBrowserIsVisible (eventId, juce::var (message));
    }

    // État complet envoyé à la page quand elle est prête : toutes les valeurs de paramètres, puis les réglages d'interface.
    void ParamBridgeEditor::sendSnapshot()
    {
        auto* values = new juce::DynamicObject();

        for (size_t i = 0; i < paramIds.size(); ++i)
        {
            const float v = paramValues[i]->load();
            values->setProperty (juce::Identifier (paramIds[i]), (double) v);
            lastSent[i] = v;
        }

        juce::Array<juce::var> message;
        message.add ("snapshot");
        message.add (juce::var (values));
        message.add (config.getUiState ? config.getUiState() : juce::String());
        send (message);
    }

    void ParamBridgeEditor::timerCallback()
    {
        if (! pageReady)
            return;

        juce::Array<juce::var> changes;

        for (size_t i = 0; i < paramIds.size(); ++i)
        {
            const float v = paramValues[i]->load();

            if (v == lastSent[i])
                continue;

            lastSent[i] = v;

            juce::Array<juce::var> pair;
            pair.add (paramIds[i]);
            pair.add ((double) v);
            changes.add (juce::var (pair));
        }

        if (changes.isEmpty())
            return;

        juce::Array<juce::var> message;
        message.add ("params");
        message.add (juce::var (changes));
        send (message);
    }

    void ParamBridgeEditor::handleMessage (const juce::var& message)
    {
        const auto* args = message.getArray();

        if (args == nullptr || args->isEmpty())
            return;

        const auto command = (*args)[0].toString();

        if (command == "ready")
        {
            pageReady = true;
            sendSnapshot();
            return;
        }

        if (command == "ui")
        {
            if (args->size() > 1 && config.setUiState)
                config.setUiState ((*args)[1].toString());
            return;
        }

        if ((command != "begin" && command != "end" && command != "set") || args->size() < 2)
            return;

        const auto id = (*args)[1].toString();
        auto* param = apvts.getParameter (id);

        if (param == nullptr)
            return; // identifiant inconnu : ignoré

        if (command == "begin")
        {
            if (activeGestures.insert (id).second)
                param->beginChangeGesture();
        }
        else if (command == "end")
        {
            if (activeGestures.erase (id) > 0)
                param->endChangeGesture();
        }
        else if (args->size() > 2)
        {
            const float value = (float) (double) (*args)[2];
            param->setValueNotifyingHost (param->convertTo0to1 (value));

            // La page connaît déjà cette valeur : pas d'écho dans le prochain passage du minuteur.
            const auto it = paramIndex.find (id.toStdString());

            if (it != paramIndex.end())
                lastSent[it->second] = paramValues[it->second]->load();
        }
    }
}
