/*
  ==============================================================================
  OscHandler.cpp
  WhyCremisi VST Plugin - OSC Communication Handler Implementation
  
  Phase 1 COMPLETE: Bidirectional OSC
  - Receives on UDP port (default 9000)
  - Sends to target host:port (default 127.0.0.1:9001)
  - Parses and constructs OSC binary packets
  - Thread-safe message log
  
  Multipiattaforma: Linux, Windows, macOS
  ==============================================================================
*/

#include "OscHandler.h"
#include <cstring>

//==============================================================================
OscHandler::OscHandler(int p) : port(p)
{
}

OscHandler::~OscHandler()
{
    stop();
    
    // Cleanup send socket
    if (sendSocket)
        sendSocket.reset();
}

//==============================================================================
void OscHandler::start()
{
    if (running.load() && connected.load())
        return;
    
    // Create UDP socket and bind BEFORE setting running
    socket = std::make_unique<juce::DatagramSocket>();
    
    if (!socket->bindToPort(port))
    {
        addToLog("[OSC] ERROR: Cannot bind to port " + juce::String(port));
        connected.store(false);
        running.store(false);
        return;
    }
    
    connected.store(true);
    running.store(true);
    addToLog("[OSC] Listening on port " + juce::String(port));
    
    // Start listener thread
    listenerThread = std::make_unique<std::thread>([this]() { listenerLoop(); });
}

void OscHandler::stop()
{
    if (!running.load())
        return;
    
    running.store(false);
    connected.store(false);
    
    // Close socket to unblock listener
    if (socket)
        socket->shutdown();
    
    // Wait for thread to finish
    if (listenerThread && listenerThread->joinable())
    {
        listenerThread->join();
        listenerThread.reset();
    }
    
    socket.reset();
    addToLog("[OSC] Stopped");
}

//==============================================================================
void OscHandler::listenerLoop()
{
    // Buffer for incoming packets (64KB max UDP)
    std::vector<char> buffer(65536);
    
    while (running.load())
    {
        if (!socket)
        {
            juce::Thread::sleep(100);
            continue;
        }
        
        // Wait for data (blocking)
        juce::String senderIp;
        int senderPort = 0;
        int bytesRead = socket->read(buffer.data(), static_cast<int>(buffer.size()), true, senderIp, senderPort);
        
        if (bytesRead <= 0)
        {
            // Socket closed or error
            juce::Thread::sleep(10);
            continue;
        }
        
        // Handle the packet
        handleOscPacket(buffer.data(), bytesRead);
    }
}

//==============================================================================
void OscHandler::handleOscPacket(const char* data, int size)
{
    // OSC packets start with '/' for messages or '#' for bundles
    if (size < 4)
        return;
    
    if (data[0] == '/')
    {
        // Regular OSC message
        const char* ptr = data;
        
        // Extract address pattern (null-terminated, padded to 4 bytes)
        juce::String addressPattern;
        while (*ptr != '\0' && ptr < data + size)
        {
            addressPattern += *ptr++;
        }
        ptr += (4 - ((addressPattern.length() + 1) % 4)) % 4;
        if (ptr >= data + size) return;
        
        // Extract type tag (starts with ',')
        if (ptr >= data + size || *ptr != ',')
        {
            addToLog("[OSC] WARNING: No type tag in message from " + addressPattern);
            return;
        }
        
        juce::String typeTag;
        ptr++; // Skip ','
        while (*ptr != '\0' && ptr < data + size)
        {
            typeTag += *ptr++;
        }
        // ptr is now on the null terminator of the type tag string.
        // Must skip the null explicitly before alignment, otherwise if
        // (ptr - data) is already 4-byte aligned the loop below won't
        // advance and args would point at the null instead of the payload.
        if (ptr < data + size) ptr++; // skip null terminator
        while (ptr < data + size && (ptr - data) % 4 != 0) ptr++;
        
        // Parse arguments
        const char* args = ptr;
        int argsSize = size - static_cast<int>(args - data);
        
        handleOscMessage(addressPattern, typeTag, args, argsSize);
    }
    else if (data[0] == '#' && size >= 16)
    {
        // OSC bundle (starts with "#bundle")
        // For Phase 1, we just log bundles
        addToLog("[OSC] Bundle received (not parsed in Phase 1)");
    }
}

//==============================================================================
void OscHandler::handleOscMessage(const juce::String& address, const juce::String& typeTag, 
                                  const char* arguments, int argSize)
{
    messagesReceived.fetch_add(1);
    
    // Learn mode: capture first address
    if (learnMode.load() && learnedAddress.isEmpty())
    {
        learnedAddress = address;
        addToLog("[OSC] LEARNED: " + address);
        learnMode.store(false);
    }
    
    // Parse arguments based on type tag
    const char* ptr = arguments;
    juce::var firstValue;  // For legacy callback
    bool hasValue = false;
    
    for (int i = 0; i < typeTag.length() && ptr < arguments + argSize; i++)
    {
        char type = typeTag[i];
        
        switch (type)
        {
            case 'f': // float32
            {
                if (ptr + 4 > arguments + argSize) break;
                
                uint8_t bytes[4];
                std::memcpy(bytes, ptr, 4);
                
                // Convert from big-endian
                float value;
                uint32_t intValue = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
                std::memcpy(&value, &intValue, 4);
                
                addToLog("[OSC] " + address + " → " + juce::String(value, 3));
                
                // Store for legacy callback
                if (!hasValue)
                {
                    firstValue = value;
                    hasValue = true;
                }
                
                // Call callback
                if (callback)
                    callback(address, value);
                
                ptr += 4;
                break;
            }
            
            case 'i': // int32
            {
                if (ptr + 4 > arguments + argSize) break;
                
                uint8_t bytes[4];
                std::memcpy(bytes, ptr, 4);
                int32_t value = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
                
                addToLog("[OSC] " + address + " → " + juce::String(value));
                
                // Store for legacy callback
                if (!hasValue)
                {
                    firstValue = value;
                    hasValue = true;
                }
                
                // Call callback with float conversion
                if (callback)
                    callback(address, static_cast<float>(value));
                
                ptr += 4;
                break;
            }
            
            case 's': // string
            {
                juce::String str;
                while (ptr < arguments + argSize && *ptr != '\0')
                {
                    str += *ptr++;
                }
                
                addToLog("[OSC] " + address + " → \"" + str + "\"");
                
                // Store for legacy callback
                if (!hasValue)
                {
                    firstValue = str;
                    hasValue = true;
                }
                
                if (stringCallback)
                    stringCallback(address, str);
                
                // Skip padding
                ptr += (4 - ((str.length() + 1) % 4)) % 4;
                if (ptr < arguments + argSize && *ptr == '\0') ptr++;
                while (ptr < arguments + argSize && (ptr - arguments) % 4 != 0) ptr++;
                break;
            }
            
            default:
                // Unknown type, skip (would need proper padding calculation)
                addToLog("[OSC] " + address + " → <unknown type: " + juce::String::charToString(type) + ">");
                break;
        }
    }
    
    // Legacy callback support
    if (messageCallback && hasValue)
        messageCallback(address, firstValue);
}

//==============================================================================
void OscHandler::setSendTarget(const juce::String& host, int sendPort)
{
    sendHost = host;
    sendPortNum = sendPort;
    
    // Create send socket if not exists
    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }
    
    addToLog("[OSC] Send target set to " + host + ":" + juce::String(sendPort));
}

//==============================================================================
void OscHandler::sendMessage(const juce::String& address, float value)
{
    if (!sendSocket)
    {
        sendSocket = std::make_unique<juce::DatagramSocket>();
        sendSocket->setEnablePortReuse(false);
    }
    
    // Build OSC packet: address pattern + type tag + argument
    juce::MemoryBlock packet;
    
    // Address pattern (null-terminated, padded to 4 bytes)
    writeOscAddress(packet, address);
    
    // Type tag: ",f" for single float
    writeOscTypeTag(packet, "f");
    
    // Float argument (big-endian)
    writeOscFloatArg(packet, value);
    
    // Send via UDP
    int bytesSent = sendSocket->write(sendHost, sendPortNum, 
                                        packet.getData(), 
                                        static_cast<int>(packet.getSize()));
    
    if (bytesSent > 0)
    {
        addToLog("[OSC] SENT: " + address + " → " + juce::String(value, 3) + 
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
    
    // Build OSC packet
    juce::MemoryBlock packet;
    
    writeOscAddress(packet, address);
    writeOscTypeTag(packet, "s");
    writeOscStringArg(packet, value);
    
    int bytesSent = sendSocket->write(sendHost, sendPortNum,
                                        packet.getData(),
                                        static_cast<int>(packet.getSize()));
    
    if (bytesSent > 0)
    {
        addToLog("[OSC] SENT: " + address + " → \"" + value + "\"");
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
        addToLog("[OSC] SENT: " + address + " → " + juce::String(value));
        messagesSent.fetch_add(1);
    }
    else
    {
        addToLog("[OSC] ERROR: Failed to send int to " + sendHost + ":" + juce::String(sendPortNum));
    }
}

//==============================================================================
// OSC Binary Encoding Helpers
//==============================================================================

void OscHandler::writeOscAddress(juce::MemoryBlock& dest, const juce::String& address)
{
    // Address must start with '/'
    dest.append(address.toRawUTF8(), address.getNumBytesAsUTF8());
    dest.append("\0", 1);
    padToFourBytes(dest);
}

void OscHandler::writeOscTypeTag(juce::MemoryBlock& dest, const juce::String& typeTag)
{
    // Type tag starts with ','
    dest.append(",", 1);
    dest.append(typeTag.toRawUTF8(), typeTag.getNumBytesAsUTF8());
    dest.append("\0", 1);
    padToFourBytes(dest);
}

void OscHandler::writeOscFloatArg(juce::MemoryBlock& dest, float value)
{
    // OSC uses big-endian (network byte order)
    uint32_t intValue;
    std::memcpy(&intValue, &value, 4);
    
    // Convert to big-endian
    uint8_t bytes[4];
    bytes[0] = (intValue >> 24) & 0xFF;
    bytes[1] = (intValue >> 16) & 0xFF;
    bytes[2] = (intValue >> 8) & 0xFF;
    bytes[3] = intValue & 0xFF;
    
    dest.append(bytes, 4);
}

void OscHandler::writeOscIntArg(juce::MemoryBlock& dest, int32_t value)
{
    // OSC uses big-endian (network byte order)
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
    // OSC requires all elements to be 4-byte aligned
    size_t currentSize = dest.getSize();
    size_t padding = (4 - (currentSize % 4)) % 4;
    
    if (padding > 0)
    {
        dest.append("\0\0\0", padding);
    }
}

//==============================================================================
void OscHandler::setCallback(OscCallback cb)
{
    callback = std::move(cb);
}

void OscHandler::setStringCallback(OscStringCallback cb)
{
    stringCallback = std::move(cb);
}

void OscHandler::setMessageCallback(MessageCallback cb)
{
    messageCallback = std::move(cb);
}

//==============================================================================
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
void OscHandler::addToLog(const juce::String& msg)
{
    // Thread-safe log
    const juce::ScopedLock sl(logLock);
    
    // Add timestamp
    auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    messageLog.add("[" + timestamp + "] " + msg);
    
    // Keep log size manageable (last 100 messages)
    if (messageLog.size() > 100)
        messageLog.remove(0);
    
    // Also write to debug console
    DBG(msg);
}
