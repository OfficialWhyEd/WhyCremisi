/*
  ==============================================================================
  PluginEditor.cpp — WhyCremisi VST Bridge AI
  ARCHITETTURA: UI Nativa JUCE + Ponte OSC (NO WebView embedded)
  Decisione Aura: WebBrowserComponent crasha nei VST. UI in browser esterno.
  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
WhyCremisiBrowser::WhyCremisiBrowser() {}

bool WhyCremisiBrowser::pageAboutToLoad(const juce::String& url)
{
    if (onPageAboutToLoad) return onPageAboutToLoad(url);
    return true;
}

void WhyCremisiBrowser::pageFinishedLoading(const juce::String& url)
{
    if (onPageFinishedLoading) onPageFinishedLoading(url);
}

//==============================================================================
WhyCremisiProcessorEditor::WhyCremisiProcessorEditor(WhyCremisiProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(900, 700);
    setupWebView();
    DBG("[WhyCremisi] Editor avviato con WebView integrata");
}

WhyCremisiProcessorEditor::~WhyCremisiProcessorEditor()
{
    if (webView)
    {
        webView->onPageAboutToLoad = nullptr;
        webView->onPageFinishedLoading = nullptr;
        webView.reset();
    }
    webViewBridge.shutdown();
}

//==============================================================================
void WhyCremisiProcessorEditor::loadUI()  { setupWebView(); }

void WhyCremisiProcessorEditor::setupWebView()
{
    webView = std::make_unique<WhyCremisiBrowser>();
    
    // Configura il ponte tra JS e C++
    webViewBridge.initialize(webView.get());
    webViewBridge.setFrontendMessageCallback([this](const nlohmann::json& msg) {
        handleFrontendMessage(msg);
    });

    // Collega l'intercettazione dei messaggi via URL
    webView->onPageAboutToLoad = [this](const juce::String& url) {
        return !webViewBridge.handleMessageFromURL(url); // Ritorna false se consumato (non naviga)
    };

    // Carica l'URL (Localhost per sviluppo, file locale per produzione)
    juce::String url = getUIURL();
    webView->goToURL(url);
    
    addAndMakeVisible(webView.get());
}

void WhyCremisiProcessorEditor::setupFallbackUI()
{
    // Titolo
    titleLabel.setText("WhyCremisi // VST Bridge AI", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc084fc));
    addAndMakeVisible(titleLabel);

    // Sottotitolo con istruzioni
    subtitleLabel.setText("Apri localhost:9000 nel browser per la UI completa", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(11.0f));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6b7280));
    addAndMakeVisible(subtitleLabel);

    // Gain Sliders
    gainSlider1.setSliderStyle(juce::Slider::RotaryHorizontalDrag);
    gainSlider1.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    gainSlider1.setRange(-60.0, 12.0, 0.1);
    gainSlider1.setTextValueSuffix(" dB");
    gainSlider1.setColour(juce::Slider::thumbColourId, juce::Colour(0xffc084fc));
    addAndMakeVisible(gainSlider1);

    gainLabel1.setText("Gain 1", juce::dontSendNotification);
    gainLabel1.setJustificationType(juce::Justification::centred);
    gainLabel1.setColour(juce::Label::textColourId, juce::Colour(0xff9ca3af));
    gainLabel1.attachToComponent(&gainSlider1, false);
    addAndMakeVisible(gainLabel1);

    gainSlider2.setSliderStyle(juce::Slider::RotaryHorizontalDrag);
    gainSlider2.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    gainSlider2.setRange(-60.0, 12.0, 0.1);
    gainSlider2.setTextValueSuffix(" dB");
    gainSlider2.setColour(juce::Slider::thumbColourId, juce::Colour(0xffc084fc));
    addAndMakeVisible(gainSlider2);

    gainLabel2.setText("Gain 2", juce::dontSendNotification);
    gainLabel2.setJustificationType(juce::Justification::centred);
    gainLabel2.setColour(juce::Label::textColourId, juce::Colour(0xff9ca3af));
    gainLabel2.attachToComponent(&gainSlider2, false);
    addAndMakeVisible(gainLabel2);

    // AI
    aiButton.setButtonText("Ask AI");
    aiButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7c3aed));
    aiButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    aiButton.onClick = [this]() {
        audioProcessor.sendAiPrompt(aiPrompt.getText());
        aiPrompt.clear();
    };
    addAndMakeVisible(aiButton);

    aiPrompt.setMultiLine(false);
    aiPrompt.setReturnKeyStartsNewLine(false);
    aiPrompt.setTextToShowWhenEmpty("Scrivi prompt AI...", juce::Colour(0xff4b5563));
    aiPrompt.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0f0f1a));
    aiPrompt.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a1a6e));
    aiPrompt.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe0e0f0));
    addAndMakeVisible(aiPrompt);

    // Parameter attachments
    gainAttachment1 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "gain1", gainSlider1);
    gainAttachment2 = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "gain2", gainSlider2);
}

juce::String WhyCremisiProcessorEditor::getUIURL() const
{
#if JUCE_DEBUG
    return "http://localhost:5173";
#else
    // In Release: carica il bundle React dall'app package
    auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    auto indexFile = appFile.getChildFile("Contents/Resources/webview-ui/index.html");
    if (indexFile.existsAsFile())
        return juce::URL(indexFile).toString(false);
    // Fallback al dev server se il bundle non c'è
    return "http://localhost:5173";
#endif
}

//==============================================================================
void WhyCremisiProcessorEditor::handleFrontendMessage(const nlohmann::json& message)
{
    if (!message.contains("type")) return;
    std::string type = message["type"];
    juce::String msgType(type.data(), type.size());

    if (msgType == "daw.command" && message.contains("payload") && message["payload"].contains("command"))
    {
        std::string cmd = message["payload"]["command"];
        DBG("[WhyCremisi] Comando DAW: " + juce::String(cmd.data()));
    }
    else if (msgType == "ai.prompt" && message.contains("payload") && message["payload"].contains("prompt"))
    {
        std::string prompt = message["payload"]["prompt"];
        audioProcessor.sendAiPrompt(juce::String(prompt.data()));
    }
}

void WhyCremisiProcessorEditor::sendToFrontend(const nlohmann::json& message)
{
    webViewBridge.sendToFrontend(message);
}

//==============================================================================
void WhyCremisiProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0f));

    // Header bar
    g.setColour(juce::Colour(0xff1a0a2e));
    g.fillRect(0, 0, getWidth(), 50);
    g.setColour(juce::Colour(0xff3a1a6e));
    g.drawLine(0, 50, getWidth(), 50, 1.0f);

    // Footer bar
    g.setColour(juce::Colour(0xff0f0f1a));
    g.fillRect(0, getHeight() - 30, getWidth(), 30);
    g.setColour(juce::Colour(0xff3a1a6e));
    g.drawLine(0, getHeight() - 30, getWidth(), 30, 1.0f);

    // OSC hint in footer
    g.setColour(juce::Colour(0xff4b5563));
    g.setFont(10.0f);
    g.drawText("OSC Server: porta 9000  |  UI Web: http://localhost:9000",
               10, getHeight() - 25, getWidth() - 20, 20,
               juce::Justification::centred, false);
}

void WhyCremisiProcessorEditor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}