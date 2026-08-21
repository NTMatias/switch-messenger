#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace net {

// Tipos de eventos entregados a la aplicación por WebSocketClient::poll().
enum class EventType {
    Connected,
    Disconnected,
    Message,   // {type: "message", sender, text}
    Presence   // {type: "presence", username, online}
};

struct IncomingEvent {
    EventType   type;
    std::string sender;   // remitente (para Message) o username (para Presence)
    std::string text;     // texto del mensaje (solo Message)
    bool        online = false; // solo Presence
};

// Cliente WebSocket minimalista (RFC 6455, solo frames de texto) escrito
// sobre los sockets BSD de libnx. No bloquea la interfaz: connect() hace
// el handshake HTTP una sola vez de forma bloqueante y corta (rápido en LAN),
// y a partir de ahí el socket se pone en modo no bloqueante y toda la
// lectura ocurre dentro de poll(), que se debe llamar una vez por frame.
class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    // Conecta al servidor ws://host:port/ (path fijo "/").
    // Devuelve true si el handshake HTTP + upgrade a WebSocket tuvo éxito.
    bool connect(const std::string& host, int port);

    // Cierra la conexión de forma segura (envía frame de cierre si es posible).
    void disconnect();

    bool isConnected() const;

    // Envía un mensaje de chat al servidor usando el protocolo JSON simple
    // del proyecto: {"type":"message","sender":"...","text":"..."}
    void sendMessage(const std::string& sender, const std::string& text);

    // Envía el evento de unión al canal:
    // {"type":"join","username":"..."}
    void sendJoin(const std::string& username);

    // Debe llamarse una vez por frame. Lee datos pendientes del socket sin
    // bloquear, reconstruye frames WebSocket completos y los traduce a
    // IncomingEvent, entregándolos mediante el callback registrado con
    // setEventCallback(). También detecta desconexiones del peer.
    void poll();

    // Registra el callback que recibirá los eventos entrantes.
    void setEventCallback(std::function<void(const IncomingEvent&)> cb);

private:
    int  m_socket = -1;
    bool m_connected = false;
    std::string m_recvBuffer;      // bytes crudos de socket pendientes de parsear
    std::function<void(const IncomingEvent&)> m_callback;

    // Envía un frame de texto WebSocket (enmascarado, como exige la RFC
    // para tramas cliente->servidor). Usa ::send explícitamente para no
    // colisionar con WebSocketClient::sendMessage/sendJoin.
    bool sendTextFrame(const std::string& payload);

    // Intenta extraer y procesar frames completos de m_recvBuffer.
    void processBuffer();

    // Procesa un mensaje de texto ya decodificado (un frame completo)
    // interpretando el protocolo JSON simple del proyecto.
    void handleTextPayload(const std::string& payload);

    // Realiza el handshake HTTP de upgrade a WebSocket. Devuelve true si
    // el servidor respondió 101 Switching Protocols.
    bool performHandshake(const std::string& host, int port);

    void closeSocket();
};

} // namespace net
