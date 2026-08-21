# switch-messenger

Aplicación homebrew de mensajería tipo WhatsApp para Nintendo Switch, con
cliente en C++ (libnx + SDL2 + SDL2_ttf), servidor WebSocket en Python y
un cliente web de una sola página HTML para chatear desde el navegador.

```
switch-messenger/
├── Makefile
├── README.md
├── icon.jpg
├── .github/workflows/build.yml
├── source/
│   ├── main.cpp
│   ├── app/App.hpp, App.cpp
│   ├── ui/ChatUI.*, Keyboard.*, Notification.*
│   └── net/WebSocketClient.*
├── server/app.py, requirements.txt, README.md
└── web/index.html
```

## Requisitos

- [devkitPro](https://devkitpro.org/wiki/Getting_Started) con **devkitA64**
  y **libnx** instalados.
- Los portlibs de Nintendo Switch para SDL2:

  ```bash
  (dkp-)pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-freetype \
                   switch-zlib switch-bzip2 switch-libpng
  ```

- Variable de entorno `DEVKITPRO` apuntando a tu instalación (el
  instalador de devkitPro normalmente ya la configura).

## Compilación local

```bash
make
```

Esto genera:

```
switch-messenger.elf
switch-messenger.nro
```

en la raíz del proyecto.

Para limpiar los archivos generados:

```bash
make clean
```

## Instalación en Nintendo Switch

1. Copia `switch-messenger.nro` a la tarjeta SD de tu consola, dentro de:

   ```
   /switch/switch-messenger/switch-messenger.nro
   ```

2. Con un entorno homebrew compatible (por ejemplo el Homebrew Menu de
   Atmosphère), abre la aplicación desde ahí.

## Configuración del servidor

La dirección del servidor WebSocket al que se conecta el cliente de
Nintendo Switch está definida en `source/app/App.hpp`:

```cpp
constexpr const char* SERVER_HOST = "127.0.0.1";
constexpr int         SERVER_PORT = 8765;
```

Edita esos dos valores con la IP/host y el puerto de tu servidor antes de
compilar. **`SERVER_HOST` debe ser una dirección IPv4 literal** (por
ejemplo `"192.168.1.50"`), no un nombre de dominio: libnx no incluye un
resolver DNS (no hay `gethostbyname`/`getaddrinfo`), así que este cliente
no puede resolver nombres como `miservidor.onrender.com` por sí mismo.
Para desarrollo local, usa la IP local de la máquina donde corre
`server/app.py`. Este cliente se conecta mediante `ws://` (socket TCP
plano); si tu servidor está detrás de HTTPS/TLS (por ejemplo en Render,
ver más abajo), necesitarás adaptar `WebSocketClient` para hacer el
handshake TLS o exponer un endpoint `ws://` sin cifrar para pruebas en tu
propia red local.

## Servidor Python

```bash
cd server
pip install -r requirements.txt
python app.py
```

Por defecto escucha en `0.0.0.0:8765`. Ver `server/README.md` para más
detalles y variables de entorno.

## Cliente web

Abre `web/index.html` directamente en un navegador (o sírvelo con
cualquier servidor estático). Introduce la URL del servidor
(`ws://host:puerto` o `wss://host` en producción) y un nombre de usuario,
pulsa "Conectar" y podrás chatear. Usa el mismo protocolo JSON que el
cliente de Nintendo Switch, así que ambos pueden hablar con el mismo
servidor y verse los mensajes entre sí.

## Despliegue en Render

1. Crea un repositorio en GitHub y sube este proyecto completo.
2. En [Render](https://render.com), crea un nuevo **Web Service** y
   conéctalo a ese repositorio.
3. Configura el **Root Directory** (carpeta raíz del servicio) como
   `server`, ya que el servidor Python vive en esa subcarpeta.
4. Comando de instalación de dependencias:

   ```
   pip install -r requirements.txt
   ```

5. Comando de arranque:

   ```
   python app.py
   ```

   Render inyecta automáticamente la variable de entorno `PORT`; el
   servidor ya la lee (`os.environ.get("PORT", "8765")`), así que no hace
   falta configurarla manualmente.

6. Una vez desplegado, Render te da una URL pública del tipo
   `https://switch-messenger.onrender.com`.

7. Configura esa URL en el cliente de Nintendo Switch (`SERVER_HOST` /
   `SERVER_PORT` en `source/app/App.hpp`) y en el cliente web (campo
   "URL del servidor").

### `http://` vs `https://` vs `ws://` vs `wss://`

- `http://` / `https://` son los protocolos para páginas y APIs web
  normales; `https://` es la versión cifrada.
- `ws://` / `wss://` son los equivalentes para conexiones WebSocket:
  `ws://` es texto plano (como `http://`) y `wss://` va cifrado sobre TLS
  (como `https://`).
- Render sirve todo detrás de HTTPS, por lo que el servicio público solo
  acepta `wss://`, no `ws://`. El **cliente web** debe usar
  `wss://tu-servicio.onrender.com` en producción.
- El **cliente de Nintendo Switch** incluido en este proyecto habla
  `ws://` sin cifrar (para mantener la implementación mínima y estable).
  Para producción real contra Render tendrías que añadir soporte TLS al
  cliente de la Switch; para desarrollo y pruebas, apunta el cliente de
  Switch a un servidor `ws://` corriendo en tu propia red local.

## Sobre la revisión de compilación

Este proyecto fue revisado archivo por archivo para verificar
coherencia entre el Makefile, los `#include`, las declaraciones `.hpp` y
las implementaciones `.cpp` (ver la sección "Checklist de compilación"
más abajo). No fue posible ejecutar una compilación real con devkitA64
en el entorno donde se generó este proyecto, porque no dispone de
DEVKITPRO instalado ni de acceso a red para instalarlo; verifica el
resultado con `make` en un entorno con devkitPro antes de dar por hecho
que compila sin ajustes.

---

## Checklist de compilación

- [x] Todos los archivos fuente existen (`source/main.cpp`,
  `source/app/App.{hpp,cpp}`, `source/ui/{ChatUI,Keyboard,Notification}.{hpp,cpp}`,
  `source/net/WebSocketClient.{hpp,cpp}`).
- [x] Todos los archivos incluidos en el Makefile existen: el Makefile no
  lista archivos `.cpp` individualmente, detecta automáticamente todo
  `.cpp` dentro de las carpetas declaradas en `SOURCES` (`source`,
  `source/app`, `source/ui`, `source/net`), que son exactamente las
  carpetas reales del proyecto.
- [x] El Makefile detecta todos los `.cpp` (vía `$(wildcard $(dir)/*.cpp)`
  por cada carpeta en `SOURCES`).
- [x] No existen objetos inexistentes: los `.o` se generan a partir de
  `OFILES_SRC`, calculado directamente de los `.cpp` encontrados.
- [x] Todos los métodos declarados en los `.hpp` están implementados en su
  `.cpp` correspondiente (revisado uno por uno: `WebSocketClient`,
  `ChatUI`, `VirtualKeyboard`, `NotificationManager`, `App`).
- [x] Los nombres de archivos coinciden exactamente entre `#include`,
  Makefile y disco (sensible a mayúsculas/minúsculas: `ChatUI.hpp` /
  `ChatUI.cpp`, etc.).
- [x] No hay conflicto entre `WebSocketClient::sendMessage`/`sendJoin` y
  la función del sistema: el método de la clase se llama `sendMessage`
  (no `send`), y toda llamada interna al socket del sistema usa
  explícitamente `::send(...)` con el operador de ámbito global.
- [x] La red no bloquea la interfaz: tras el handshake HTTP inicial (corto
  y único), el socket se pone en modo no bloqueante (`O_NONBLOCK`) y
  `poll()` se llama una vez por frame sin esperar datos.
- [x] Las dependencias son mínimas: solo SDL2, SDL2_ttf y sus
  dependencias directas en el entorno de devkitPro (freetype, libpng,
  zlib, bzip2) más libnx.
- [x] SDL2 está configurado correctamente (`sdl2-config --cflags`, enlace
  `-lSDL2`).
- [x] SDL2_ttf está configurado correctamente (`-lSDL2_ttf` + freetype).
- [x] No se añadieron dependencias innecesarias de EGL/GLES (el proyecto
  no hace renderizado OpenGL/EGL manual).
- [x] No se añadieron dependencias innecesarias de HarfBuzz (no se usa
  shaping de texto avanzado, solo `SDL2_ttf` con la fuente compartida del
  sistema).
- [x] El orden del linker es correcto: `$(OFILES)` antes que `$(LIBS)` (lo
  gestionan las reglas estándar de `switch_rules`/`base_rules` que el
  Makefile usa; el propio Makefile solo declara el contenido de `LIBS`
  y `LIBDIRS`).
- [x] `make clean` funciona (elimina `build/`, `.elf`, `.nacp`, `.nro`,
  `.pfs0`).
- [x] `make` genera el `.elf` (`$(OUTPUT).elf` a partir de `$(OFILES)`).
- [x] `make` genera el `.nro` (`$(OUTPUT).nro` vía `elf2nro`, con manejo
  seguro de un `icon.jpg` ausente mediante `NO_ICON`).
- [x] El workflow YAML está correctamente ubicado en
  `.github/workflows/build.yml` y su sintaxis fue validada.
- [x] El workflow compila desde la raíz correcta del repositorio
  (`actions/checkout@v4` deja el repo completo en el workspace, y `make`
  se ejecuta ahí directamente).
- [x] El artifact apunta al `.nro` correcto: `switch-messenger.nro` en la
  raíz, que es exactamente donde el Makefile lo genera
  (`$(OUTPUT).nro` con `OUTPUT := $(CURDIR)/$(TARGET)` y
  `TARGET := switch-messenger`).

### Nota honesta sobre los límites de esta revisión

La revisión anterior es una revisión **estática** de código y
configuración (coherencia de nombres, includes, firmas, estructura del
Makefile y del YAML), no una compilación real ejecutada con devkitA64: el
entorno donde se generó este proyecto no tiene devkitPro instalado ni
acceso a red. El primer `make` (local o en GitHub Actions) puede revelar
algún detalle menor de la toolchain que una revisión de texto no puede
anticipar al 100%; si eso ocurre, el error debería ser puntual y fácil de
ubicar gracias a la estructura mínima y explícita del proyecto.
