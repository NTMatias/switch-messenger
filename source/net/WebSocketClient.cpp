#include "WebSocketClient.hpp"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace net {

namespace {

// ---------------------------------------------------------------------
// Utilidades pequeñas y autocontenidas (sin dependencias externas)
// ---------------------------------------------------------------------

std::string base64Encode(const uint8_t* data, size_t len) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += table[(n >> 6) & 0x3F];
        out += table[n & 0x3F];
        i += 3;
    }

    size_t remaining = len - i;
    if (remaining == 1) {
        uint32_t n = data[i] << 16;
        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += "==";
    } else if (remaining == 2) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += table[(n >> 6) & 0x3F];
        out += "=";
    }

    return out;
}

// Genera una clave Sec-WebSocket-Key de 16 bytes aleatorios en base64.
// No necesita ser criptográficamente segura: solo debe ser razonablemente
// distinta en cada conexión, como exige la RFC 6455 para el handshake.
std::string generateWebSocketKey() {
    uint8_t raw[16];
    for (auto& b : raw) {
        b = static_cast<uint8_t>(rand() & 0xFF);
    }
    return base64Encode(raw, sizeof(raw));
}

// Extrae de forma sencilla el valor de un campo de texto "clave":"valor"
// dentro de un JSON plano (sin objetos/arrays anidados), que es todo lo
// que necesita el protocolo del proyecto. Devuelve true si se encontró.
bool extractStringField(const std::string& json, const std::string& key, std::string& out) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;

    size_t firstQuote = json.find('"', pos);
    if (firstQuote == std::string::npos) return false;

    size_t cursor = firstQuote + 1;
    std::string value;
    while (cursor < json.size() && json[cursor] != '"') {
        if (json[cursor] == '\\' && cursor + 1 < json.size()) {
            cursor++;
        }
        value += json[cursor];
        cursor++;
    }

    out = value;
    return true;
}

bool extractBoolField(const std::string& json, const std::string& key, bool& out) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;

    size_t truePos = json.find("true", pos);
    size_t falsePos = json.find("false", pos);
    size_t commaPos = json.find_first_of(",}", pos);

    if (truePos != std::string::npos && (commaPos == std::string::npos || truePos < commaPos)) {
        out = true;
        return true;
    }
    if (falsePos != std::string::npos && (commaPos == std::string::npos || falsePos < commaPos)) {
        out = false;
        return true;
    }
    return false;
}

// Escapa comillas y backslashes para incrustar texto en un JSON plano.
std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

// Resuelve host (IP literal o nombre de dominio, p. ej. un dominio de
// Render) a una dirección IPv4. Primero intenta interpretarlo como IP
// literal (camino rápido, sin red); si no lo es, usa getaddrinfo, que es
// la API estándar de resolución DNS y sí está disponible en libnx (a
// diferencia de la API antigua gethostbyname, que no lo está).
bool resolveHost(const std::string& host, struct in_addr* outAddr) {
    if (inet_aton(host.c_str(), outAddr) != 0) {
        return true;
    }

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    int rc = getaddrinfo(host.c_str(), nullptr, &hints, &result);
    if (rc != 0 || result == nullptr) {
        return false;
    }

    auto* resolved = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    *outAddr = resolved->sin_addr;

    freeaddrinfo(result);
    return true;
}

} // namespace

WebSocketClient::WebSocketClient() {
    srand(static_cast<unsigned>(time(nullptr)));
}

WebSocketClient::~WebSocketClient() {
    disconnect();
}

bool WebSocketClient::connect(const std::string& host, int port) {
    if (m_connected) {
        disconnect();
    }

    m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket < 0) {
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    // Resuelve el host (acepta tanto IP literal como nombre de dominio,
    // p. ej. una URL de Render) usando getaddrinfo.
    if (!resolveHost(host, &addr.sin_addr)) {
        closeSocket();
        return false;
    }

    if (::connect(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeSocket();
        return false;
    }

    // Deshabilita Nagle: los mensajes de chat son pequeños y frecuentes,
    // así que preferimos latencia baja sobre eficiencia de ancho de banda.
    int one = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    if (!performHandshake(host, port)) {
        closeSocket();
        return false;
    }

    // A partir de aquí el socket pasa a modo no bloqueante: poll() nunca
    // debe congelar la interfaz esperando datos que aún no llegaron.
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

    m_connected = true;
    m_recvBuffer.clear();

    if (m_callback) {
        IncomingEvent ev;
        ev.type = EventType::Connected;
        m_callback(ev);
    }

    return true;
}

bool WebSocketClient::performHandshake(const std::string& host, int port) {
    std::string key = generateWebSocketKey();

    std::string request;
    request += "GET / HTTP/1.1\r\n";
    request += "Host: " + host + ":" + std::to_string(port) + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + key + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "\r\n";

    size_t totalSent = 0;
    while (totalSent < request.size()) {
        ssize_t sent = ::send(m_socket, request.data() + totalSent,
                               request.size() - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }

    // Lee la respuesta HTTP del handshake (bloqueante pero acotada: el
    // socket todavía no se puso en modo no bloqueante en este punto, y el
    // handshake es un único intercambio corto al inicio de la conexión).
    std::string response;
    char buffer[512];
    const std::string terminator = "\r\n\r\n";

    while (response.find(terminator) == std::string::npos) {
        ssize_t received = ::recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            return false;
        }
        buffer[received] = '\0';
        response.append(buffer, static_cast<size_t>(received));

        if (response.size() > 8192) {
            // Respuesta anormalmente grande para un handshake: abortar.
            return false;
        }
    }

    return response.find("101") != std::string::npos &&
           response.find("Upgrade") != std::string::npos;
}

void WebSocketClient::disconnect() {
    if (m_socket >= 0 && m_connected) {
        // Frame de cierre WebSocket mínimo (opcode 0x8), enmascarado.
        uint8_t closeFrame[6] = {0x88, 0x80, 0, 0, 0, 0};
        ::send(m_socket, closeFrame, sizeof(closeFrame), 0);
    }

    bool wasConnected = m_connected;
    closeSocket();

    if (wasConnected && m_callback) {
        IncomingEvent ev;
        ev.type = EventType::Disconnected;
        m_callback(ev);
    }
}

void WebSocketClient::closeSocket() {
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }
    m_connected = false;
}

bool WebSocketClient::isConnected() const {
    return m_connected;
}

void WebSocketClient::setEventCallback(std::function<void(const IncomingEvent&)> cb) {
    m_callback = std::move(cb);
}

void WebSocketClient::sendMessage(const std::string& sender, const std::string& text) {
    if (!m_connected) return;

    std::string json = "{\"type\":\"message\",\"sender\":\"" + jsonEscape(sender) +
                        "\",\"text\":\"" + jsonEscape(text) + "\"}";
    sendTextFrame(json);
}

void WebSocketClient::sendJoin(const std::string& username) {
    if (!m_connected) return;

    std::string json = "{\"type\":\"join\",\"username\":\"" + jsonEscape(username) + "\"}";
    sendTextFrame(json);
}

bool WebSocketClient::sendTextFrame(const std::string& payload) {
    if (m_socket < 0) return false;

    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN=1, opcode=0x1 (texto)

    size_t len = payload.size();
    uint8_t maskBit = 0x80; // los frames cliente->servidor deben ir enmascarados

    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len) | maskBit);
    } else if (len <= 0xFFFF) {
        frame.push_back(126 | maskBit);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127 | maskBit);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.push_back(static_cast<uint8_t>((len >> shift) & 0xFF));
        }
    }

    uint8_t mask[4];
    for (auto& m : mask) m = static_cast<uint8_t>(rand() & 0xFF);
    frame.insert(frame.end(), mask, mask + 4);

    size_t headerSize = frame.size();
    frame.resize(headerSize + len);
    for (size_t i = 0; i < len; ++i) {
        frame[headerSize + i] = static_cast<uint8_t>(payload[i]) ^ mask[i % 4];
    }

    size_t totalSent = 0;
    while (totalSent < frame.size()) {
        ssize_t sent = ::send(m_socket, frame.data() + totalSent,
                               frame.size() - totalSent, 0);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // reintenta: el envío de un mensaje de chat es pequeño
            }
            disconnect();
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }

    return true;
}

void WebSocketClient::poll() {
    if (!m_connected || m_socket < 0) return;

    char buffer[1024];
    ssize_t received = ::recv(m_socket, buffer, sizeof(buffer), 0);

    if (received > 0) {
        m_recvBuffer.append(buffer, static_cast<size_t>(received));
        processBuffer();
    } else if (received == 0) {
        // El servidor cerró la conexión.
        disconnect();
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error real de socket (no "no hay datos todavía").
            disconnect();
        }
    }
}

void WebSocketClient::processBuffer() {
    // Procesa todos los frames completos disponibles en el buffer. Los
    // frames servidor->cliente NO van enmascarados según la RFC 6455.
    while (true) {
        if (m_recvBuffer.size() < 2) return;

        const uint8_t* raw = reinterpret_cast<const uint8_t*>(m_recvBuffer.data());
        uint8_t opcode = raw[0] & 0x0F;
        bool masked = (raw[1] & 0x80) != 0;
        uint64_t len = raw[1] & 0x7F;

        size_t pos = 2;

        if (len == 126) {
            if (m_recvBuffer.size() < pos + 2) return;
            len = (static_cast<uint16_t>(raw[pos]) << 8) | raw[pos + 1];
            pos += 2;
        } else if (len == 127) {
            if (m_recvBuffer.size() < pos + 8) return;
            len = 0;
            for (int i = 0; i < 8; ++i) {
                len = (len << 8) | raw[pos + i];
            }
            pos += 8;
        }

        size_t maskOffset = pos;
        if (masked) pos += 4;

        if (m_recvBuffer.size() < pos + len) {
            // Frame incompleto: espera al siguiente poll().
            return;
        }

        std::string payload(m_recvBuffer.data() + pos, static_cast<size_t>(len));

        if (masked) {
            uint8_t maskKey[4];
            std::memcpy(maskKey, raw + maskOffset, 4);
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<char>(static_cast<uint8_t>(payload[i]) ^ maskKey[i % 4]);
            }
        }

        size_t frameSize = pos + len;

        if (opcode == 0x1) { // frame de texto
            handleTextPayload(payload);
        } else if (opcode == 0x8) { // close
            m_recvBuffer.erase(0, frameSize);
            disconnect();
            return;
        }
        // Los opcodes de ping (0x9) y pong (0xA) se ignoran deliberadamente:
        // el protocolo de la aplicación no depende de keep-alives WebSocket
        // y mantener esto simple reduce la superficie de errores.

        m_recvBuffer.erase(0, frameSize);
    }
}

void WebSocketClient::handleTextPayload(const std::string& payload) {
    if (!m_callback) return;

    std::string type;
    if (!extractStringField(payload, "type", type)) return;

    IncomingEvent ev;

    if (type == "message") {
        ev.type = EventType::Message;
        extractStringField(payload, "sender", ev.sender);
        extractStringField(payload, "text", ev.text);
        m_callback(ev);
    } else if (type == "presence") {
        ev.type = EventType::Presence;
        extractStringField(payload, "username", ev.sender);
        extractBoolField(payload, "online", ev.online);
        m_callback(ev);
    }
    // Otros tipos de mensaje (por ejemplo "join" reflejado por el
    // servidor) se ignoran silenciosamente: no forman parte de los
    // eventos que la interfaz necesita mostrar.
}

} // namespace net
