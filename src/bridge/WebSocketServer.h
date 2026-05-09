/*
  ==============================================================================
  WebSocketServer.h
  WhyCremisi VST Plugin - WebSocket Server for React UI Communication

  Minimal WebSocket server that runs inside the VST plugin process.
  Listens on a configurable port (default 8080) and accepts connections
  from the React UI running in a browser.

  Protocol: RFC 6455 (WebSocket) with JSON text frames.
  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

class WebSocketServer
{
public:
    //==============================================================================
    /** Callback when a JSON message is received from a connected client */
    using MessageCallback = std::function<void(const nlohmann::json& message)>;

    /** Callback when a client connects or disconnects */
    using ConnectionCallback = std::function<void(int clientId, bool connected)>;

    //==============================================================================
    WebSocketServer(int port = 8080);
    ~WebSocketServer();

    //==============================================================================
    /** Start the server. Returns true if successfully started. */
    bool start();

    /** Stop the server and disconnect all clients. */
    void stop();

    /** Check if the server is running. */
    bool isRunning() const { return running.load(); }

    //==============================================================================
    /** Broadcast a JSON message to all connected clients. */
    void broadcast(const nlohmann::json& message);

    /** Send a JSON message to a specific client. */
    void sendToClient(int clientId, const nlohmann::json& message);

    //==============================================================================
    /** Set callbacks */
    void setMessageCallback(MessageCallback callback);
    void setConnectionCallback(ConnectionCallback callback);

    //==============================================================================
    /** Get server info */
    int getPort() const { return port; }
    int getConnectedClientsCount();
    juce::String getServerURL() const;

    //==============================================================================
    /** Set port (must be called before start()) */
    void setPort(int newPort);

private:
    //==============================================================================
    int port;

    // Server state
    std::atomic<bool> running{false};
    std::unique_ptr<juce::StreamingSocket> serverSocket;

    // Accept thread
    std::unique_ptr<std::thread> acceptThread;
    void acceptLoop();

    // Client management
    struct ClientInfo
    {
        int id;
        std::unique_ptr<juce::StreamingSocket> socket;
        std::unique_ptr<std::thread> thread;
        std::atomic<bool> connected{true};
        std::vector<uint8_t> receiveBuffer;
        bool handshakeComplete{false};
    };

    std::vector<std::unique_ptr<ClientInfo>> clients;
    std::mutex clientsMutex;
    std::atomic<int> nextClientId{1};

    // Client thread handler
    void clientLoop(ClientInfo* client);

    // WebSocket handshake
    bool performHandshake(ClientInfo* client);
    juce::String computeWebSocketAcceptKey(const juce::String& key);

    // WebSocket frame handling
    struct WebSocketFrame
    {
        bool fin;
        uint8_t opcode;
        bool masked;
        uint8_t mask[4];
        std::vector<uint8_t> payload;
    };

    WebSocketFrame parseFrame(const uint8_t* data, size_t length, size_t& bytesConsumed);
    void sendFrame(juce::StreamingSocket* socket, uint8_t opcode, const std::vector<uint8_t>& payload, bool mask = false);
    void sendTextFrame(juce::StreamingSocket* socket, const juce::String& text);
    void sendPongFrame(juce::StreamingSocket* socket, const std::vector<uint8_t>& payload);
    void closeConnection(ClientInfo* client, uint16_t code = 1000, const juce::String& reason = "");

    // Remove disconnected clients
    void cleanupClients();

    // Callbacks
    MessageCallback messageCallback;
    ConnectionCallback connectionCallback;

    // Utility
    juce::String getCurrentTimestamp() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebSocketServer)
};