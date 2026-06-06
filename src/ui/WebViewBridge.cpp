/*
   ==============================================================================
   WebViewBridge.cpp
   WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  Communication bridge between C++ and React UI
   ==============================================================================
*/

#include "WebViewBridge.h"
#include <juce_core/juce_core.h>

WebViewBridge::WebViewBridge() = default;
WebViewBridge::~WebViewBridge() = default;

void WebViewBridge::initialize(juce::WebBrowserComponent* webBrowser)
{
    webView = webBrowser;
    DBG("[WebViewBridge] Inizializzato");
}

void WebViewBridge::shutdown()
{
    webView = nullptr;
    DBG("[WebViewBridge] Shutdown");
}

juce::String WebViewBridge::escapeForJavaScript(const juce::String& input)
{
    juce::String result = input;
    
    // Ordine importante: prima backslash, poi gli altri
    result = result.replace("\\", "\\\\");   // \\ -> \\\\
    result = result.replace("\"", "\\\"");     // " -> \\"
    result = result.replace("'", "\\'");       // ' -> \'
    result = result.replace("\n", "\\n");     // newline -> \n
    result = result.replace("\r", "\\r");     // carriage return -> \r
    result = result.replace("\t", "\\t");     // tab -> \t
    
    return result;
}

nlohmann::json WebViewBridge::createBaseMessage(const juce::String& type, const juce::String& id)
{
    nlohmann::json msg;
    msg["type"] = type.toStdString();
    msg["timestamp"] = juce::Time::currentTimeMillis();
    
    if (id.isNotEmpty())
        msg["id"] = id.toStdString();
    else
        msg["id"] = nullptr;
    
    return msg;
}

void WebViewBridge::sendToFrontend(const nlohmann::json& message)
{
    if (!webView)
    {
        DBG("[WebViewBridge] ERRORE: webView non inizializzato");
        return;
    }
    
    // Converti JSON in stringa
    std::string jsonStd = message.dump();
    juce::String jsonStr(jsonStd.data(), jsonStd.size());
    
    // Usa Base64 per trasmettere il payload in modo assolutamente sterile rispetto all'interprete URL
    juce::String base64Json = juce::Base64::toBase64(jsonStd.data(), jsonStd.size());
    
    // JS Injection essiccato: nessuna funzione anonima, nessun operatore logico complesso, nessun warning
    juce::String jsCode = "javascript:if(window.__whycremisiBridge){window.__whycremisiBridge.receiveMessage(decodeURIComponent(escape(window.atob('" + base64Json + "'))));}";
    
    webView->goToURL(jsCode);
    
    DBG("[WebViewBridge] Messaggio inviato: " + juce::String(message["type"].get<std::string>().data()));
}

// MARK: - Shortcuts per messaggi comuni

void WebViewBridge::sendTransport(bool isPlaying, bool isRecording, float bpm, float position)
{
    auto msg = createBaseMessage("daw.transport");
    msg["payload"]["isPlaying"] = isPlaying;
    msg["payload"]["isRecording"] = isRecording;
    msg["payload"]["bpm"] = bpm;
    msg["payload"]["positionSeconds"] = position;
    sendToFrontend(msg);
}

void WebViewBridge::sendTrackUpdate(int trackId, const juce::String& name, float volumeDb, float pan)
{
    auto msg = createBaseMessage("daw.track");
    msg["payload"]["trackId"] = trackId;
    msg["payload"]["name"] = name.toStdString();
    msg["payload"]["volumeDb"] = volumeDb;
    msg["payload"]["pan"] = pan;
    sendToFrontend(msg);
}

void WebViewBridge::sendMeter(int trackId, float leftDb, float rightDb)
{
    auto msg = createBaseMessage("daw.meter");
    msg["payload"]["trackId"] = trackId;
    msg["payload"]["leftDb"] = leftDb;
    msg["payload"]["rightDb"] = rightDb;
    sendToFrontend(msg);
}

void WebViewBridge::sendAIResponse(const juce::String& requestId, const juce::String& content,
                                    const juce::String& provider, bool isComplete)
{
    auto msg = createBaseMessage("ai.response", requestId);
    msg["payload"]["status"] = isComplete ? "success" : "pending";
    msg["payload"]["provider"] = provider.toStdString();
    msg["payload"]["content"] = content.toStdString();
    sendToFrontend(msg);
}

void WebViewBridge::sendAIStream(const juce::String& requestId, const juce::String& chunk, bool isDone)
{
    auto msg = createBaseMessage("ai.stream", requestId);
    msg["payload"]["chunk"] = chunk.toStdString();
    msg["payload"]["isDone"] = isDone;
    sendToFrontend(msg);
}

void WebViewBridge::sendOSCMessage(const juce::String& address, float value)
{
    auto msg = createBaseMessage("osc.message");
    msg["payload"]["address"] = address.toStdString();
    msg["payload"]["value"] = value;
    msg["payload"]["valueType"] = "float";
    sendToFrontend(msg);
}

void WebViewBridge::sendWidgetCreate(const juce::String& widgetId, const juce::String& widgetType,
                                      const juce::String& title, const nlohmann::json& config)
{
    auto msg = createBaseMessage("ui.widget.create", widgetId);
    msg["payload"]["widgetType"] = widgetType.toStdString();
    msg["payload"]["title"] = title.toStdString();
    
    // Merge config nel payload
    for (auto& [key, val] : config.items())
    {
        msg["payload"][key] = val;
    }
    
    sendToFrontend(msg);
}

void WebViewBridge::sendWidgetUpdate(const juce::String& widgetId, const nlohmann::json& values)
{
    auto msg = createBaseMessage("ui.widget.update");
    msg["payload"]["widgetId"] = widgetId.toStdString();
    
    for (auto& [key, val] : values.items())
    {
        msg["payload"][key] = val;
    }
    
    sendToFrontend(msg);
}

void WebViewBridge::sendWidgetRemove(const juce::String& widgetId)
{
    auto msg = createBaseMessage("ui.widget.remove");
    msg["payload"]["widgetId"] = widgetId.toStdString();
    sendToFrontend(msg);
}

void WebViewBridge::sendError(const juce::String& code, const juce::String& message, const juce::String& severity)
{
    auto msg = createBaseMessage("plugin.error");
    msg["payload"]["code"] = code.toStdString();
    msg["payload"]["message"] = message.toStdString();
    msg["payload"]["severity"] = severity.toStdString();
    sendToFrontend(msg);
}

// MARK: - Gestione messaggi dal frontend

bool WebViewBridge::handleMessageFromURL(const juce::String& url)
{
    // Intercetta URL speciali app://message/...
    const juce::String prefix = "app://message/";
    
    if (!url.startsWith(prefix))
        return false; // Non è un messaggio per noi, lascia navigare
    
    // Estrai il JSON dall'URL
    juce::String encodedJson = url.substring(prefix.length());
    juce::String jsonStr = juce::URL::removeEscapeChars(encodedJson);
    
    DBG("[WebViewBridge] Ricevuto da frontend: " + jsonStr.substring(0, 200));
    
    // Parsa e chiama callback
    if (frontendMessageCallback)
    {
        try
        {
            auto message = nlohmann::json::parse(jsonStr.toStdString());
            frontendMessageCallback(message);
        }
        catch (const std::exception& e)
        {
            DBG("[WebViewBridge] ERRORE parsing JSON: " + juce::String(e.what()));
        }
    }
    
    return true; // Consumato, blocca navigazione
}

void WebViewBridge::setFrontendMessageCallback(FrontendMessageCallback callback)
{
    frontendMessageCallback = std::move(callback);
}

// DEPRECATED
void WebViewBridge::handleMessageFromFrontend(const juce::String& jsonString)
{
    // Metodo legacy, chiama il nuovo
    juce::String fakeUrl = "app://message/" + juce::URL::addEscapeChars(jsonString, true);
    handleMessageFromURL(fakeUrl);
}