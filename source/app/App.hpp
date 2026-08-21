#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <cstdint>

#include "ChatUI.hpp"
#include "Keyboard.hpp"
#include "Notification.hpp"
#include "WebSocketClient.hpp"

namespace app {

// ---------------------------------------------------------------------
// Configuración del servidor. Edita estos valores para apuntar al
// servidor WebSocket (ver README.md, sección "Configuración del
// servidor"). SERVER_HOST admite tanto una IP literal ("192.168.1.50")
// como un nombre de dominio (por ejemplo "tu-app.onrender.com"): el
// cliente resuelve el nombre con getaddrinfo si hace falta.
//
// SERVER_PORT_TLS se documenta para referencia: este cliente
// minimalista se conecta con sockets TCP planos (ws://), por lo que un
// despliegue en Render detrás de wss:// requiere revisar la sección de
// despliegue del README antes de usarse en producción.
// ---------------------------------------------------------------------
constexpr const char* SERVER_HOST = "127.0.0.1";
constexpr int         SERVER_PORT = 8765;

enum class Screen {
    ChatList,
    Conversation,
};

// Clase principal de la aplicación: posee las ventanas/recursos SDL, el
// estado de los chats, el cliente de red y coordina el bucle principal.
class App {
public:
    App();
    ~App();

    // Inicializa SDL, SDL_ttf, la red (socket service de libnx) y el pad.
    // Devuelve false si algo crítico falla.
    bool init();

    // Ejecuta el bucle principal hasta que el usuario cierra la app.
    void run();

    // Libera todos los recursos (SDL, red, etc.).
    void shutdown();

private:
    SDL_Window*   m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    TTF_Font*     m_font = nullptr;

    bool m_running = false;

    Screen m_screen = Screen::ChatList;

    std::vector<ui::Chat> m_chats;
    int m_selectedChatIndex = 0;
    int m_activeChatIndex = -1;
    int m_scrollOffset = 0;

    ui::ChatUI            m_chatUI;
    ui::VirtualKeyboard    m_keyboard;
    ui::NotificationManager m_notifications;
    net::WebSocketClient   m_net;

    std::string m_username = "switch_user";

    void pollInput();
    void update();
    void render();

    void openChat(int index);
    void closeChat();

    // Callback invocado por WebSocketClient::poll() cuando llega un
    // evento de red. Se ejecuta de forma síncrona dentro de poll(), que
    // a su vez se llama una vez por frame desde el bucle principal, así
    // que nunca bloquea la interfaz.
    void onNetworkEvent(const net::IncomingEvent& event);

    // Busca (o crea) el chat correspondiente a un remitente.
    ui::Chat& findOrCreateChat(const std::string& sender);
};

} // namespace app
