#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <cstdint>

namespace ui {

// Un mensaje individual de una conversación.
struct Message {
    std::string sender;
    std::string text;
    bool        outgoing;
    uint64_t    timestamp;
};

// Un chat (contacto) con su historial de mensajes.
struct Chat {
    std::string          name;
    bool                  online = false;
    std::string           lastPreview;
    std::vector<Message>  messages;
};

// Dibuja la lista de chats y la pantalla de conversación, con scroll y
// caché de texturas de texto para no regenerar el mismo bitmap cada frame.
class ChatUI {
public:
    static constexpr size_t kMaxMessages = 200;

    ChatUI() = default;
    ~ChatUI();

    // Añade un mensaje a un chat, recortando el historial a kMaxMessages
    // eliminando los mensajes más antiguos cuando se supera el límite.
    static void addMessage(Chat& chat, Message msg);

    // --- Pantalla: lista de chats -----------------------------------
    // D-Pad arriba/abajo mueve selectedIndex (gestionado por App).
    void renderChatList(SDL_Renderer* renderer, TTF_Font* font,
                         const std::vector<Chat>& chats, int selectedIndex);

    // --- Pantalla: conversación ---------------------------------------
    // scrollOffset es el índice del primer mensaje visible (0 = principio
    // del historial). App la ajusta para hacer auto-scroll al último
    // mensaje cuando llega uno nuevo.
    void renderConversation(SDL_Renderer* renderer, TTF_Font* font,
                             const Chat& chat, int scrollOffset);

    // Número de mensajes visibles a la vez en la conversación (usado por
    // App para calcular límites de scroll).
    static constexpr int kVisibleMessages = 6;

private:
    struct CacheEntry {
        std::string   key;
        SDL_Texture*  texture = nullptr;
    };

    // Caché acotada (LRU simple por orden de inserción) de burbujas de
    // texto ya renderizadas, para no llamar a TTF_RenderUTF8_Blended en
    // cada frame para el mismo texto.
    std::unordered_map<std::string, SDL_Texture*> m_bubbleCache;
    std::deque<std::string> m_cacheOrder;
    static constexpr size_t kMaxCacheEntries = 256;

    SDL_Texture* getOrCreateBubbleTexture(SDL_Renderer* renderer, TTF_Font* font,
                                           const std::string& text, SDL_Color color);

    void releaseCache();
};

} // namespace ui
