"""
switch-messenger — servidor WebSocket mínimo.

Reenvía mensajes de chat entre todos los clientes conectados (clientes
Nintendo Switch y clientes web) y mantiene un registro básico de qué
usuarios están conectados (presencia). No persiste mensajes: todo vive en
memoria mientras el proceso está corriendo.

Protocolo (JSON, un objeto por mensaje de texto WebSocket):

  Cliente -> Servidor:
    {"type": "join", "username": "<nombre>"}
    {"type": "message", "sender": "<nombre>", "text": "<texto>"}

  Servidor -> Clientes:
    {"type": "message", "sender": "<nombre>", "text": "<texto>"}
    {"type": "presence", "username": "<nombre>", "online": true|false}

Ejecutar localmente:
    pip install -r requirements.txt
    python app.py

Variables de entorno opcionales:
    HOST  (por defecto 0.0.0.0)
    PORT  (por defecto 8765; Render inyecta su propio PORT automáticamente)
"""

import asyncio
import json
import logging
import os

import websockets

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("switch-messenger")

# username -> websocket connection
CLIENTS: dict[str, "websockets.WebSocketServerProtocol"] = {}
# websocket connection -> username (para poder limpiar al desconectar)
USERNAMES: dict["websockets.WebSocketServerProtocol", str] = {}


async def broadcast(payload: dict, exclude=None) -> None:
    """Envía un mensaje JSON a todos los clientes conectados."""
    if not CLIENTS:
        return

    message = json.dumps(payload)
    stale = []

    for username, ws in list(CLIENTS.items()):
        if ws is exclude:
            continue
        try:
            await ws.send(message)
        except websockets.ConnectionClosed:
            stale.append(username)

    for username in stale:
        CLIENTS.pop(username, None)


async def handle_client(websocket) -> None:
    log.info("Nueva conexión desde %s", websocket.remote_address)

    try:
        async for raw in websocket:
            try:
                data = json.loads(raw)
            except (json.JSONDecodeError, TypeError):
                log.warning("Mensaje no válido ignorado: %r", raw)
                continue

            msg_type = data.get("type")

            if msg_type == "join":
                username = str(data.get("username", "")).strip() or "anon"
                CLIENTS[username] = websocket
                USERNAMES[websocket] = username
                log.info("%s se unió", username)
                await broadcast({"type": "presence", "username": username, "online": True})

            elif msg_type == "message":
                sender = str(data.get("sender", USERNAMES.get(websocket, "anon")))
                text = str(data.get("text", ""))
                if not text:
                    continue
                log.info("Mensaje de %s: %s", sender, text)
                await broadcast({"type": "message", "sender": sender, "text": text})

            else:
                log.warning("Tipo de mensaje desconocido ignorado: %s", msg_type)

    except websockets.ConnectionClosed:
        pass
    except Exception:
        log.exception("Error manejando cliente")
    finally:
        username = USERNAMES.pop(websocket, None)
        if username:
            CLIENTS.pop(username, None)
            log.info("%s se desconectó", username)
            await broadcast({"type": "presence", "username": username, "online": False})


async def main() -> None:
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", "8765"))

    log.info("Servidor switch-messenger escuchando en %s:%d", host, port)

    async with websockets.serve(handle_client, host, port, ping_interval=20, ping_timeout=20):
        await asyncio.Future()  # corre para siempre


if __name__ == "__main__":
    asyncio.run(main())
