#include "ChatUI.hpp"

namespace ui {

ChatUI::~ChatUI() {
    releaseCache();
}

void ChatUI::releaseCache() {
    for (auto& kv : m_bubbleCache) {
        if (kv.second) SDL_DestroyTexture(kv.second);
    }
    m_bubbleCache.clear();
    m_cacheOrder.clear();
}

void ChatUI::addMessage(Chat& chat, Message msg) {
    chat.lastPreview = msg.text;
    chat.messages.push_back(std::move(msg));

    if (chat.messages.size() > kMaxMessages) {
        size_t excess = chat.messages.size() - kMaxMessages;
        chat.messages.erase(chat.messages.begin(), chat.messages.begin() + excess);
    }
}

SDL_Texture* ChatUI::getOrCreateBubbleTexture(SDL_Renderer* renderer, TTF_Font* font,
                                               const std::string& text, SDL_Color color) {
    std::string key = text + "|" + std::to_string(color.r) + std::to_string(color.g) +
                       std::to_string(color.b);

    auto it = m_bubbleCache.find(key);
    if (it != m_bubbleCache.end()) {
        return it->second;
    }

    SDL_Surface* surf = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color, 520);
    if (!surf) return nullptr;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;

    // Si la caché crece demasiado, se libera la entrada más antigua
    // (política simple FIFO, suficiente para el volumen de texto de un
    // chat: evita crecimiento ilimitado de memoria).
    if (m_cacheOrder.size() >= kMaxCacheEntries) {
        const std::string& oldestKey = m_cacheOrder.front();
        auto oldIt = m_bubbleCache.find(oldestKey);
        if (oldIt != m_bubbleCache.end()) {
            if (oldIt->second) SDL_DestroyTexture(oldIt->second);
            m_bubbleCache.erase(oldIt);
        }
        m_cacheOrder.pop_front();
    }

    m_bubbleCache[key] = tex;
    m_cacheOrder.push_back(key);
    return tex;
}

void ChatUI::renderChatList(SDL_Renderer* renderer, TTF_Font* font,
                             const std::vector<Chat>& chats, int selectedIndex) {
    SDL_SetRenderDrawColor(renderer, 18, 18, 18, 255);
    SDL_RenderClear(renderer);

    // Encabezado.
    SDL_Rect header{0, 0, 1280, 70};
    SDL_SetRenderDrawColor(renderer, 32, 44, 40, 255);
    SDL_RenderFillRect(renderer, &header);

    SDL_Color white{240, 240, 240, 255};
    SDL_Texture* title = getOrCreateBubbleTexture(renderer, font, "SWITCH MESSENGER", white);
    if (title) {
        int w, h;
        SDL_QueryTexture(title, nullptr, nullptr, &w, &h);
        SDL_Rect dst{20, (70 - h) / 2, w, h};
        SDL_RenderCopy(renderer, title, nullptr, &dst);
    }

    const int rowH = 90;
    const int listTop = 70;
    const int visibleRows = (720 - listTop) / rowH;

    // Solo se recorren y dibujan las filas visibles en pantalla, no la
    // lista completa de chats.
    int firstVisible = 0;
    if (selectedIndex >= visibleRows) {
        firstVisible = selectedIndex - visibleRows + 1;
    }

    for (int i = firstVisible; i < static_cast<int>(chats.size()) && i < firstVisible + visibleRows; ++i) {
        const Chat& chat = chats[i];
        int rowIndex = i - firstVisible;
        SDL_Rect row{0, listTop + rowIndex * rowH, 1280, rowH - 4};

        bool selected = (i == selectedIndex);
        SDL_SetRenderDrawColor(renderer, selected ? 45 : 26, selected ? 60 : 26, selected ? 52 : 26, 255);
        SDL_RenderFillRect(renderer, &row);

        // Indicador de presencia (círculo verde/gris).
        SDL_Rect dot{30, row.y + rowH / 2 - 10, 20, 20};
        if (chat.online) {
            SDL_SetRenderDrawColor(renderer, 37, 211, 102, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
        }
        SDL_RenderFillRect(renderer, &dot);

        SDL_Texture* nameTex = getOrCreateBubbleTexture(renderer, font, chat.name, white);
        if (nameTex) {
            int w, h;
            SDL_QueryTexture(nameTex, nullptr, nullptr, &w, &h);
            SDL_Rect dst{70, row.y + 10, w, h};
            SDL_RenderCopy(renderer, nameTex, nullptr, &dst);
        }

        std::string preview = chat.lastPreview.empty() ? "Sin mensajes" : chat.lastPreview;
        SDL_Color gray{170, 170, 170, 255};
        SDL_Texture* previewTex = getOrCreateBubbleTexture(renderer, font, preview, gray);
        if (previewTex) {
            int w, h;
            SDL_QueryTexture(previewTex, nullptr, nullptr, &w, &h);
            SDL_Rect dst{70, row.y + 44, w, h};
            SDL_RenderCopy(renderer, previewTex, nullptr, &dst);
        }
    }
}

void ChatUI::renderConversation(SDL_Renderer* renderer, TTF_Font* font,
                                 const Chat& chat, int scrollOffset) {
    SDL_SetRenderDrawColor(renderer, 12, 22, 20, 255);
    SDL_RenderClear(renderer);

    SDL_Rect header{0, 0, 1280, 70};
    SDL_SetRenderDrawColor(renderer, 32, 44, 40, 255);
    SDL_RenderFillRect(renderer, &header);

    SDL_Color white{240, 240, 240, 255};
    SDL_Texture* title = getOrCreateBubbleTexture(renderer, font, chat.name, white);
    if (title) {
        int w, h;
        SDL_QueryTexture(title, nullptr, nullptr, &w, &h);
        SDL_Rect dst{20, (70 - h) / 2, w, h};
        SDL_RenderCopy(renderer, title, nullptr, &dst);
    }

    const int top = 90;
    const int bubbleGap = 14;
    int y = top;

    int total = static_cast<int>(chat.messages.size());
    int start = scrollOffset;
    if (start < 0) start = 0;
    if (start > total) start = total;

    // Solo se renderizan los mensajes desde el offset de scroll hacia
    // adelante, y se corta en cuanto se sale del área visible: nunca se
    // itera sobre todo el historial de 200 mensajes en cada frame.
    for (int i = start; i < total; ++i) {
        const Message& msg = chat.messages[i];

        SDL_Color bubbleTextColor{20, 20, 20, 255};
        SDL_Texture* bubble = getOrCreateBubbleTexture(renderer, font, msg.text, bubbleTextColor);
        if (!bubble) continue;

        int texW, texH;
        SDL_QueryTexture(bubble, nullptr, nullptr, &texW, &texH);

        int bubbleW = texW + 24;
        int bubbleH = texH + 16;
        int x = msg.outgoing ? (1280 - bubbleW - 30) : 30;

        SDL_Rect box{x, y, bubbleW, bubbleH};
        if (msg.outgoing) {
            SDL_SetRenderDrawColor(renderer, 37, 211, 102, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
        }
        SDL_RenderFillRect(renderer, &box);

        SDL_Rect dst{x + 12, y + 8, texW, texH};
        SDL_RenderCopy(renderer, bubble, nullptr, &dst);

        y += bubbleH + bubbleGap;
        if (y > 720) break; // fuera del área visible: no seguir dibujando
    }
}

} // namespace ui
