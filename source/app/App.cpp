#include "App.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <algorithm>

namespace app {

namespace {

#ifdef __SWITCH__
PadState g_pad;
#endif

uint64_t nowMs() {
#ifdef __SWITCH__
    return armTicksToNs(armGetSystemTick()) / 1000000ULL;
#else
    return static_cast<uint64_t>(SDL_GetTicks());
#endif
}

} // namespace

App::App() = default;

App::~App() {
    shutdown();
}

bool App::init() {
#ifdef __SWITCH__
    // Inicializa el servicio de sockets de libnx ANTES de usar cualquier
    // función de red (WebSocketClient depende de esto).
    socketInitializeDefault();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    plInitialize(PlServiceType_User);
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    if (TTF_Init() != 0) {
        SDL_Quit();
        return false;
    }

    m_window = SDL_CreateWindow("switch-messenger", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
    if (!m_window) {
        return false;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        return false;
    }

#ifdef __SWITCH__
    // Carga la fuente compartida del sistema (no requiere empaquetar un
    // archivo .ttf propio en romfs, lo que mantiene el proyecto mínimo).
    PlFontData sharedFont;
    if (R_SUCCEEDED(plGetSharedFontByType(&sharedFont, PlSharedFontType_Standard))) {
        m_font = TTF_OpenFontRW(SDL_RWFromConstMem(sharedFont.address, sharedFont.size), 1, 22);
    }
#endif

    if (!m_font) {
        // En un entorno de escritorio (o si la fuente compartida no está
        // disponible) no se aborta la app: simplemente no habrá texto.
        SDL_Log("Aviso: no se pudo cargar una fuente; el texto no se renderizará.");
    }

    // Chats de ejemplo (los reales llegan/se crean dinámicamente cuando el
    // servidor reenvía mensajes de nuevos remitentes).
    m_chats.push_back({"Juan", true, "Hola, ¿cómo estás?", {}});
    m_chats.push_back({"María", false, "Nos vemos mañana", {}});
    m_chats.push_back({"Carlos", false, "Último mensaje...", {}});

    m_net.setEventCallback([this](const net::IncomingEvent& ev) { onNetworkEvent(ev); });
    m_net.connect(SERVER_HOST, SERVER_PORT);
    m_net.sendJoin(m_username);

    m_running = true;
    return true;
}

void App::run() {
#ifdef __SWITCH__
    while (appletMainLoop() && m_running) {
#else
    while (m_running) {
#endif
        pollInput();
        m_net.poll();      // la red nunca bloquea: solo procesa lo ya recibido
        update();
        render();
        SDL_RenderPresent(m_renderer);
    }
}

void App::pollInput() {
#ifdef __SWITCH__
    padUpdate(&g_pad);
    uint64_t keysDown = padGetButtonsDown(&g_pad);

    if (m_keyboard.isOpen()) {
        bool sent = m_keyboard.handleInput(keysDown);
        if (sent) {
            const std::string text = m_keyboard.currentText();
            if (m_activeChatIndex >= 0) {
                ui::Chat& chat = m_chats[m_activeChatIndex];
                ui::ChatUI::addMessage(chat, {m_username, text, true, nowMs()});
                m_scrollOffset = std::max(0, static_cast<int>(chat.messages.size()) - ui::ChatUI::kVisibleMessages);
                m_net.sendMessage(m_username, text);
            }
            m_keyboard.close();
        }
        return; // mientras el teclado está abierto, absorbe toda la entrada
    }

    if (m_screen == Screen::ChatList) {
        if (keysDown & HidNpadButton_Down) {
            m_selectedChatIndex = std::min(m_selectedChatIndex + 1,
                                            static_cast<int>(m_chats.size()) - 1);
        }
        if (keysDown & HidNpadButton_Up) {
            m_selectedChatIndex = std::max(m_selectedChatIndex - 1, 0);
        }
        if (keysDown & HidNpadButton_A) {
            openChat(m_selectedChatIndex);
        }
    } else if (m_screen == Screen::Conversation) {
        if (keysDown & HidNpadButton_B) {
            closeChat();
        }
        if (keysDown & HidNpadButton_Down) {
            m_scrollOffset++;
        }
        if (keysDown & HidNpadButton_Up) {
            m_scrollOffset = std::max(0, m_scrollOffset - 1);
        }
        if (keysDown & HidNpadButton_A) {
            m_keyboard.open();
        }
    }
#else
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            m_running = false;
        }
    }
#endif
}

void App::update() {
    m_notifications.update(nowMs());

    if (m_activeChatIndex >= 0) {
        ui::Chat& chat = m_chats[m_activeChatIndex];
        int maxStart = std::max(0, static_cast<int>(chat.messages.size()) - ui::ChatUI::kVisibleMessages);
        if (m_scrollOffset > maxStart) m_scrollOffset = maxStart;
    }
}

void App::render() {
    if (m_screen == Screen::ChatList) {
        m_chatUI.renderChatList(m_renderer, m_font, m_chats, m_selectedChatIndex);
    } else if (m_activeChatIndex >= 0) {
        m_chatUI.renderConversation(m_renderer, m_font, m_chats[m_activeChatIndex], m_scrollOffset);
    }

    m_keyboard.render(m_renderer, m_font);
    m_notifications.render(m_renderer, m_font);
}

void App::openChat(int index) {
    if (index < 0 || index >= static_cast<int>(m_chats.size())) return;
    m_activeChatIndex = index;
    m_screen = Screen::Conversation;

    const ui::Chat& chat = m_chats[index];
    m_scrollOffset = std::max(0, static_cast<int>(chat.messages.size()) - ui::ChatUI::kVisibleMessages);
}

void App::closeChat() {
    m_screen = Screen::ChatList;
    m_activeChatIndex = -1;
}

ui::Chat& App::findOrCreateChat(const std::string& sender) {
    for (auto& chat : m_chats) {
        if (chat.name == sender) return chat;
    }
    m_chats.push_back({sender, true, "", {}});
    return m_chats.back();
}

void App::onNetworkEvent(const net::IncomingEvent& event) {
    switch (event.type) {
        case net::EventType::Connected:
        case net::EventType::Disconnected:
            // Sin acción de UI específica: la próxima vez que el usuario
            // intente enviar un mensaje, isConnected() reflejará el estado.
            break;

        case net::EventType::Message: {
            ui::Chat& chat = findOrCreateChat(event.sender);
            ui::ChatUI::addMessage(chat, {event.sender, event.text, false, nowMs()});

            m_notifications.push(event.sender, event.text);

            // Auto-scroll al último mensaje si el chat que recibió el
            // mensaje es el que está actualmente abierto.
            if (m_activeChatIndex >= 0 && m_chats[m_activeChatIndex].name == event.sender) {
                m_scrollOffset = std::max(0, static_cast<int>(chat.messages.size()) - ui::ChatUI::kVisibleMessages);
            }
            break;
        }

        case net::EventType::Presence: {
            ui::Chat& chat = findOrCreateChat(event.sender);
            chat.online = event.online;
            break;
        }
    }
}

void App::shutdown() {
    m_net.disconnect();

    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    TTF_Quit();
    SDL_Quit();

#ifdef __SWITCH__
    plExit();
    socketExit();
#endif
}

} // namespace app
