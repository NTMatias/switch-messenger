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
// El puerto del servidor queda fijo (debe coincidir con el que usa
// server/app.py). La IP/host y el nombre de usuario ya NO se editan
// aquí: la primera vez que se abre la app, pregunta esos dos datos con
// el teclado en pantalla y los guarda en la tarjeta SD
// (CONFIG_PATH), así que en los siguientes usos entra directo al chat.
// ---------------------------------------------------------------------
constexpr int         SERVER_PORT = 8765;
constexpr const char* CONFIG_PATH = "/switch/switch-messenger/config.txt";

enum class Screen {
    SetupHost,
    SetupUsername,
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

    Screen m_screen = Screen::SetupHost;

    std::vector<ui::Chat> m_chats;
    int m_selectedChatIndex = 0;
    int m_activeChatIndex = -1;
    int m_scrollOffset = 0;

    ui::ChatUI            m_chatUI;
    ui::VirtualKeyboard    m_keyboard;
    ui::NotificationManager m_notifications;
    net::WebSocketClient   m_net;

    std::string m_serverHost;
    std::string m_username;

    void pollInput();
    void update();
    void render();

    void openChat(int index);
    void closeChat();

    // Intenta leer m_serverHost/m_username desde CONFIG_PATH. Devuelve
    // true si el archivo existía y tenía ambos datos.
    bool loadConfig();

    // Guarda m_serverHost/m_username en CONFIG_PATH para no volver a
    // preguntarlos en el siguiente inicio.
    void saveConfig();

    // Conecta al servidor usando m_serverHost/SERVER_PORT y se une con
    // m_username. Se llama tanto tras cargar la configuración guardada
    // como justo después de completar la configuración inicial.
    void connectAndJoin();

    // Callback invocado por WebSocketClient::poll() cuando llega un
    // evento de red. Se ejecuta de forma síncrona dentro de poll(), que
    // a su vez se llama una vez por frame desde el bucle principal, así
    // que nunca bloquea la interfaz.
    void onNetworkEvent(const net::IncomingEvent& event);

    // Busca (o crea) el chat correspondiente a un remitente.
    ui::Chat& findOrCreateChat(const std::string& sender);
};

} // namespace app
