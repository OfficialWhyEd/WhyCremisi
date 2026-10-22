/*
  ==============================================================================
  OscHandler.cpp
  WhyCremisi™ · A WhyEd Project
  © 2026 WhyEd™ — @whyed.music · MIT License
  OSC Communication Handler Implementation
  
  Sections:
    Lifecycle        - constructor, destructor, start/stop
    DAW Transport    - play, stop, record, tempo, position
    DAW Mixer        - gain, pan, mute, solo per track
    Sending          - sendMessage overloads + setSendTarget
    Listener Thread  - UDP receive loop
    OSC Parsing      - binary packet -> address + values
    OSC Encoding     - value -> big-endian binary
    Callbacks        - setter methods
    Logging          - thread-safe message log
  ==============================================================================
*/

#include "OscHandler.h"
#include <cstring>

//==============================================================================
// Lifecycle
//==============================================================================

OscHandler::OscHandler(int p) : port(p) {}

OscHandler::~OscHandler()
{
    stop();
    if (sendSocket)
        sendSocket.reset();
}

void OscHandler::start()
{
    if (running.load() && connected.load())
        return;

    socket = std::make_unique<juce::DatagramSocket>();
    socket->setEnablePortReuse(false);
    if (!socket->bindToPort(port))
    {
        juce::String err = "[OSC] ERROR: Cannot bind to port " + juce::String(port) + " — retrying in 1s";
        addToLog(err);
        fprintf(stderr, "%s\n", err.toRawUTF8());
        juce::Thread::sleep(1000);
        socket = std::make_unique<juce::DatagramSocket>();
        socket->setEnablePortReuse(false);
        if (!socket->bindToPort(port))
        {
            juce::String err2 = "[OSC] ERROR: Still cannot bind to port " + juce::String(port);
            addToLog(err2);
            fprintf(stderr, "%s\n", err2.toRawUTF8());
            connected.store(false);
            running.store(false);
            return;
        }
    }

    connected.store(true);
    running.store(true);
    addToLog("[OSC] Listening on port " + juce::String(port));

    listenerThread = std::make_unique<std::thread>([this]() { listenerLoop(); });
}

void OscHandler::stop()
{
    if (!running.load())
        return;

    running.store(false);
    connected.store(false);

    if (socket)
        socket->shutdown();

    if (listenerThread && listenerThread->joinable())
    {
        listenerThread->join();
        listenerThread.reset();
    }

    socket.reset();
    addToLog("[OSC] Stopped");
}

void OscHandler::setPort(int newPort)
{
    if (newPort == port)
        return;

    bool wasRunning = running.load();
    if (wasRunning)
        stop();

    port = newPort;

    if (wasRunning)
        start();
}

//==============================================================================
// DAW Transport Commands
//==============================================================================

void OscHandler::sendPlay()
{
    sendMessage("/play", 1.0f);
}

void OscHandler::sendStop()
{
    sendMessage("/stop", 1.0f);
}

void OscHandler::sendRecord()
{
    sendMessage("/record", 1.0f);
}

void OscHandler::sendTransportAction(const juce::String& action)
{
    if (action == "play")       sendPlay();
    else if (action == "stop")  sendStop();
    else if (action == "record") sendRecord();
    else sendMessage("/" + action, 1.0f);
}

void OscHandler::sendTempo(float bpm)
{
    sendMessage("/tempo", bpm);
}

void OscHandler::sendPosition(double seconds)
{
    sendMessage("/position", static_cast<float>(seconds));
}

//==============================================================================
// DAW Mixer Commands
//==============================================================================

void OscHandler::sendGainChange(int trackIndex, float gainDb)
{
    sendMessage("/track/" + juce::String(trackIndex) + "/volume", gainDb);
}

void OscHandler::sendPanChange(int trackIndex, float pan)
{
    sendMessage("/track/" + juce::String(trackIndex) + "/pan", pan);
}

void OscHandler::sendMuteToggle(int trackIndex)
{
    sendMessage("/track/" + juce::String(trackIndex) + "/mute", 1.0f);
}

void OscHandler::sendSoloToggle(int trackIndex)
{
    sendMessage("/track/" + juce::String(trackIndex) + "/solo", 1.0f);
}

void OscHandler::sendMixerCommand(const juce::String& command, float value)
{
    sendMessage("/mixer/" + command, value);
}

//==============================================================================
// Sending
//==============================================================================

void OscHandler::setSendTarget(const juce::String& host, int sendPort)
{
    sendHost = host;
    sendPortNum = sendPort;

    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }

    addToLog("[OSC] Send target set to " + host + ":" + juce::String(sendPort));
}

void OscHandler::sendMessage(const juce::String& address, float value)
{
    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }

    juce::MemoryBlock packet;
    writeOscAddress(packet, address);
    writeOscTypeTag(packet, "f");
    writeOscFloatArg(packet, value);

    int bytesSent = sendSocket->write(sendHost, sendPortNum,
                                        packet.getData(),
                                        static_cast<int>(packet.getSize()));

    if (bytesSent > 0)
    {
        addToLog("[OSC] SENT: " + address + " -> " + juce::String(value, 3) +
                 " (" + juce::String(bytesSent) + " bytes)");
        messagesSent.fetch_add(1);
    }
    else
    {
        addToLog("[OSC] ERROR: Failed to send to " + sendHost + ":" + juce::String(sendPortNum));
    }
}

void OscHandler::sendMessage(const juce::String& address, const std::vector<float>& values,
                             const std::vector<bool>& intMask)
{
    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }

    // Il type tag elenca i tipi nell'ordine degli argomenti: "if" significa
    // primo intero, secondo float. AbletonOSC vuole l'indice della traccia
    // come intero, quindi il tipo non e' uniforme e va composto qui.
    juce::String typeTag;
    juce::String logArgs;
    for (size_t i = 0; i < values.size(); ++i)
    {
        const bool asInt = (i < intMask.size()) ? intMask[i] : false;
        typeTag += asInt ? "i" : "f";
        logArgs += (i > 0 ? " " : "");
        logArgs += asInt ? juce::String ((int) values[i]) : juce::String (values[i], 3);
    }

    juce::MemoryBlock packet;
    writeOscAddress(packet, address);
    writeOscTypeTag(packet, typeTag);
    for (size_t i = 0; i < values.size(); ++i)
    {
        const bool asInt = (i < intMask.size()) ? intMask[i] : false;
        if (asInt) writeOscIntArg   (packet, (int) values[i]);
        else       writeOscFloatArg (packet, values[i]);
    }

    int bytesSent = sendSocket->write(sendHost, sendPortNum,
                                        packet.getData(),
                                        static_cast<int>(packet.getSize()));

    if (bytesSent > 0)
    {
        addToLog("[OSC] SENT: " + address + " -> " + logArgs +
                 " (" + juce::String(bytesSent) + " bytes)");
        messagesSent.fetch_add(1);
    }
    else
    {
        addToLog("[OSC] ERROR: Failed to send to " + sendHost + ":" + juce::String(sendPortNum));
    }
}

void OscHandler::sendMessage(const juce::String& address, const juce::String& value)
{
    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }

    juce::MemoryBlock packet;
    writeOscAddress(packet, address);
    writeOscTypeTag(packet, "s");
    writeOscStringArg(packet, value);

    int bytesSent = sendSocket->write(sendHost, sendPortNum,
                                        packet.getData(),
                                        static_cast<int>(packet.getSize()));

    if (bytesSent > 0)
    {
        addToLog("[OSC] SENT: " + address + " -> \"" + value + "\"");
        messagesSent.fetch_add(1);
    }
    else
    {
        addToLog("[OSC] ERROR: Failed to send string to " + sendHost + ":" + juce::String(sendPortNum));
    }
}

void OscHandler::sendMessage(const juce::String& address, int value)
{
    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }

    juce::MemoryBlock packet;
    writeOscAddress(packet, address);
    writeOscTypeTag(packet, "i");
    writeOscIntArg(packet, value);

    int bytesSent = sendSocket->write(sendHost, sendPortNum,
                                        packet.getData(),
                                        static_cast<int>(packet.getSize()));

    if (bytesSent > 0)
    {
        addToLog("[OSC] SENT: " + address + " -> " + juce::String(value));
        messagesSent.fetch_add(1);
    }
    else
    {
        addToLog("[OSC] ERROR: Failed to send int to " + sendHost + ":" + juce::String(sendPortNum));
    }
}

//==============================================================================
// Listener Thread
//==============================================================================

void OscHandler::listenerLoop()
{
    std::vector<char> buffer(65536);
    addToLog("[OSC] ascolto avviato sulla porta " + juce::String(port));
    int ultimoEsito = 99;

    while (running.load())
    {
        if (!socket)
        {
            juce::Thread::sleep(100);
            continue;
        }

        juce::String senderIp;
        int senderPort = 0;
        // Con blockUntilSpecifiedAmountHasArrived = true JUCE aspetta tutti i
        // 65536 byte del buffer: un pacchetto OSC ne ha poche decine e non
        // verrebbe mai consegnato. Si aspetta che il socket sia pronto e si
        // legge un datagramma alla volta.
        const int pronto = socket->waitUntilReady(true, 100);
        if (pronto != ultimoEsito && pronto != 0)
        {
            ultimoEsito = pronto;
            addToLog("[OSC] attesa socket: esito " + juce::String(pronto));
        }
        if (pronto != 1)
        {
            if (pronto < 0)
                juce::Thread::sleep(100);
            continue;
        }
        int bytesRead = socket->read(buffer.data(), static_cast<int>(buffer.size()), false, senderIp, senderPort);

        if (bytesRead <= 0)
        {
            juce::Thread::sleep(10);
            continue;
        }

        static std::atomic<int> primiPacchetti { 0 };
        if (primiPacchetti.fetch_add(1) < 12)
            addToLog("[OSC] pacchetto di " + juce::String(bytesRead) + " byte da " + senderIp + ":" + juce::String(senderPort)
                     + " (porta " + juce::String(port) + ")");

        handleOscPacket(buffer.data(), bytesRead);
    }
}

//==============================================================================
// OSC Packet Parsing
//==============================================================================

void OscHandler::handleOscPacket(const char* data, int size)
{
    if (size < 4)
        return;

    if (data[0] == '/')
    {
        const char* ptr = data;

        juce::String addressPattern;
        while (*ptr != '\0' && ptr < data + size)
            addressPattern += *ptr++;
        // Il ciclo si ferma sul byte 0 che chiude la stringa: va saltato anche
        // lui, poi si arriva al multiplo di 4. Senza questo ogni tipo di
        // valore veniva letto un byte troppo presto e nessuna risposta di Live
        // era mai riconosciuta.
        ptr++;
        ptr += (4 - ((addressPattern.length() + 1) % 4)) % 4;
        if (ptr >= data + size) return;

        if (ptr >= data + size || *ptr != ',')
        {
            addToLog("[OSC] WARNING: No type tag in message from " + addressPattern);
            return;
        }

        juce::String typeTag;
        ptr++;
        while (*ptr != '\0' && ptr < data + size)
            typeTag += *ptr++;
        if (ptr < data + size) ptr++;
        while (ptr < data + size && (ptr - data) % 4 != 0) ptr++;

        const char* args = ptr;
        int argsSize = size - static_cast<int>(args - data);

        handleOscMessage(addressPattern, typeTag, args, argsSize);
    }
    // ── Bundle ────────────────────────────────────────────────────
    //
    //  REAPER manda il feedback dentro un bundle OSC:
    //      "#bundle\0" + 8 byte di timetag + [int32 lunghezza][messaggio]...
    //
    //  Prima questo ramo scriveva "Bundle received (not parsed)" e buttava
    //  via tutto: nessuno stato arrivava dalla DAW, la DAW non veniva mai
    //  riconosciuta, e nessun comando poteva essere confermato. Le porte
    //  erano solo meta' del problema.
    else if (size >= 16 && std::memcmp(data, "#bundle", 7) == 0)
    {
        int ptr = 16;   // 8 byte "#bundle\0" (arrotondati) + 8 di timetag
        while (ptr + 4 <= size)
        {
            // Lunghezza big-endian letta a mano: <arpa/inet.h> non esiste su MSVC.
            const auto* b = reinterpret_cast<const unsigned char*>(data + ptr);
            uint32_t len = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
                         | ((uint32_t)b[2] << 8)  |  (uint32_t)b[3];
            ptr += 4;

            if (len == 0 || len > (uint32_t)(size - ptr)) break;

            handleOscPacket(data + ptr, (int)len);
            ptr += (int)len;
        }
    }
}

void OscHandler::handleOscMessage(const juce::String& address, const juce::String& typeTag,
                                  const char* arguments, int argSize)
{
    messagesReceived.fetch_add(1);

    if (learnMode.load() && learnedAddress.isEmpty())
    {
        learnedAddress = address;
        addToLog("[OSC] LEARNED: " + address);
        learnMode.store(false);
    }

    const char* ptr = arguments;
    juce::var firstValue;
    bool hasValue = false;
    // Argomenti numerici raccolti per consegnarli tutti insieme a fine
    // parsing: separati non si capisce quale sia l'indice e quale il valore.
    std::vector<float> numericArgs;
    std::vector<OscArg> allArgs;   // numeri e testi in ordine, per le risposte sui device

    // Niente controllo su ptr nella condizione: T e F non hanno byte di dato,
    // quindi un messaggio fatto solo di booleani ha argSize 0. Ogni caso
    // controlla da se' i limiti prima di leggere.
    for (int i = 0; i < typeTag.length(); i++)
    {
        char type = typeTag[i];

        switch (type)
        {
            case 'f':
            {
                if (ptr + 4 > arguments + argSize) break;

                uint8_t bytes[4];
                std::memcpy(bytes, ptr, 4);

                float value;
                uint32_t intValue = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
                std::memcpy(&value, &intValue, 4);

                addToLog("[OSC] " + address + " -> " + juce::String(value, 3));

                if (!hasValue) { firstValue = value; hasValue = true; }
                numericArgs.push_back(value);
                allArgs.push_back({ false, value, {} });
                if (callback) callback(address, value);
                ptr += 4;
                break;
            }

            case 'i':
            {
                if (ptr + 4 > arguments + argSize) break;

                uint8_t bytes[4];
                std::memcpy(bytes, ptr, 4);
                int32_t value = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];

                addToLog("[OSC] " + address + " -> " + juce::String(value));

                if (!hasValue) { firstValue = value; hasValue = true; }
                numericArgs.push_back(static_cast<float>(value));
                allArgs.push_back({ false, static_cast<float>(value), {} });
                if (callback) callback(address, static_cast<float>(value));
                ptr += 4;
                break;
            }

            case 's':
            {
                juce::String str;
                while (ptr < arguments + argSize && *ptr != '\0')
                    str += *ptr++;

                addToLog("[OSC] " + address + " -> \"" + str + "\"");

                if (!hasValue) { firstValue = str; hasValue = true; }
                allArgs.push_back({ true, 0.0f, str });
                if (stringCallback) stringCallback(address, str);

                ptr += (4 - ((str.length() + 1) % 4)) % 4;
                if (ptr < arguments + argSize && *ptr == '\0') ptr++;
                while (ptr < arguments + argSize && (ptr - arguments) % 4 != 0) ptr++;
                break;
            }

            case 'T':
            case 'F':
            {
                // Booleani OSC: nel pacchetto non hanno byte di dato, il valore
                // sta tutto nel type tag. AbletonOSC risponde cosi' per
                // is_playing, mute e solo, e senza questo caso quelle
                // risposte venivano scartate come tipo sconosciuto.
                const float value = (type == 'T') ? 1.0f : 0.0f;
                addToLog("[OSC] " + address + " -> " + (type == 'T' ? "true" : "false"));

                if (!hasValue) { firstValue = value; hasValue = true; }
                numericArgs.push_back(value);
                allArgs.push_back({ false, value, {} });
                if (callback) callback(address, value);
                break;
            }

            default:
                addToLog("[OSC] " + address + " -> <unknown type: " + juce::String::charToString(type) + ">");
                break;
        }
    }

    if (messageCallback && hasValue)
        messageCallback(address, firstValue);

    // Consegna in blocco: chi ascolta qui vede indice e valore nello stesso
    // ordine in cui la DAW li ha mandati.
    if (multiCallback && ! numericArgs.empty())
        multiCallback(address, numericArgs);

    if (fullCallback && ! allArgs.empty())
        fullCallback(address, allArgs);
}

//==============================================================================
// OSC Binary Encoding
//==============================================================================

void OscHandler::writeOscAddress(juce::MemoryBlock& dest, const juce::String& address)
{
    dest.append(address.toRawUTF8(), address.getNumBytesAsUTF8());
    dest.append("\0", 1);
    padToFourBytes(dest);
}

void OscHandler::writeOscTypeTag(juce::MemoryBlock& dest, const juce::String& typeTag)
{
    dest.append(",", 1);
    dest.append(typeTag.toRawUTF8(), typeTag.getNumBytesAsUTF8());
    dest.append("\0", 1);
    padToFourBytes(dest);
}

void OscHandler::writeOscFloatArg(juce::MemoryBlock& dest, float value)
{
    uint32_t intValue;
    std::memcpy(&intValue, &value, 4);

    uint8_t bytes[4];
    bytes[0] = (intValue >> 24) & 0xFF;
    bytes[1] = (intValue >> 16) & 0xFF;
    bytes[2] = (intValue >> 8) & 0xFF;
    bytes[3] = intValue & 0xFF;

    dest.append(bytes, 4);
}

void OscHandler::writeOscIntArg(juce::MemoryBlock& dest, int32_t value)
{
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;

    dest.append(bytes, 4);
}

void OscHandler::writeOscStringArg(juce::MemoryBlock& dest, const juce::String& value)
{
    dest.append(value.toRawUTF8(), value.getNumBytesAsUTF8());
    dest.append("\0", 1);
    padToFourBytes(dest);
}

void OscHandler::padToFourBytes(juce::MemoryBlock& dest)
{
    size_t currentSize = dest.getSize();
    size_t padding = (4 - (currentSize % 4)) % 4;

    if (padding > 0)
        dest.append("\0\0\0", padding);
}

//==============================================================================
// Callbacks
//==============================================================================

void OscHandler::setCallback(OscCallback cb) { callback = std::move(cb); }
void OscHandler::setStringCallback(OscStringCallback cb) { stringCallback = std::move(cb); }
void OscHandler::setMessageCallback(MessageCallback cb) { messageCallback = std::move(cb); }

//==============================================================================
// Logging
//==============================================================================

void OscHandler::addToLog(const juce::String& msg)
{
    const juce::ScopedLock sl(logLock);

    auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    auto line = "[" + timestamp + "] " + msg;
    messageLog.add(line);

    if (messageLog.size() > 100)
        messageLog.remove(0);

    if (logCallback)
        logCallback(line);

    DBG(msg);
}
