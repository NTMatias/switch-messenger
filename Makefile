#---------------------------------------------------------------------------------
# switch-messenger - Makefile for devkitPro / devkitA64 / libnx
#
# Requiere:
#   - devkitPro (con devkitA64 y libnx instalados vía dkp-pacman)
#   - switch-sdl2
#   - switch-sdl2_ttf
#   - switch-freetype
#   - switch-bzip2
#   - switch-zlib
#   - switch-libpng
#
# Instalación típica:
#   (dkp-)pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-freetype \
#                    switch-zlib switch-bzip2 switch-libpng
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
$(error "Por favor define la variable de entorno DEVKITPRO (export DEVKITPRO=<ruta a devkitpro>)")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Metadatos de la aplicación (usados por elf2nro / nacptool)
#---------------------------------------------------------------------------------
APP_TITLE   := Switch Messenger
APP_AUTHOR  := switch-messenger
APP_VERSION := 1.0.0

# Si existe un icono .jpg en la raíz del proyecto se usa; si no, elf2nro
# genera el .nro sin icono personalizado (no rompe la compilación).
APP_ICON    := $(TOPDIR)/icon.jpg

#---------------------------------------------------------------------------------
# TARGET   : nombre final del binario (sin extensión)
# BUILD    : carpeta donde se generan los objetos intermedios
# SOURCES  : lista EXPLÍCITA de carpetas con código fuente (no recursiva).
#            Se listan todas las carpetas reales del proyecto para evitar
#            que el Makefile dependa de un "find" que pueda comportarse
#            distinto según el sistema, y para que sea evidente y estable
#            qué carpetas se compilan.
# INCLUDES : carpetas con archivos .hpp
#---------------------------------------------------------------------------------
TARGET      := switch-messenger
BUILD       := build
SOURCES     := source source/app source/ui source/net
DATA        := data
INCLUDES    := source source/app source/ui source/net

#---------------------------------------------------------------------------------
# Flags de arquitectura para Nintendo Switch (Cortex-A57)
#---------------------------------------------------------------------------------
ARCH    :=  -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE

# $(DEVKITPRO)/portlibs/switch/bin/sdl2-config es la ruta real que
# instala el paquete switch-sdl2 de devkitPro; se usa directamente en
# lugar de depender de que esa carpeta esté en el PATH.
SDL2_CONFIG := $(DEVKITPRO)/portlibs/switch/bin/sdl2-config

CFLAGS  :=  -g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES)
CFLAGS  +=  $(shell $(SDL2_CONFIG) --cflags)
CFLAGS  +=  -D__SWITCH__

CXXFLAGS := $(CFLAGS) -std=c++17 -fno-rtti -fexceptions

ASFLAGS :=  -g $(ARCH)
LDFLAGS  =  -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Librerías necesarias. IMPORTANTE:
#   - Solo se enlazan las librerías realmente usadas por el código:
#     SDL2, SDL2_ttf, freetype (usada internamente por SDL2_ttf en el
#     entorno de devkitPro), bz2/png/z (dependencias de freetype/SDL2 en
#     los portlibs de devkitPro) y libnx.
#   - NO se enlaza EGL, GLESv2 ni harfbuzz porque el proyecto no usa
#     renderizado OpenGL/EGL manual ni shaping de texto avanzado, evitando
#     los errores "undefined reference to eglDestroySurface" /
#     "undefined reference to hb_font_destroy".
#   - El orden es: objetos primero, librerías después (regla estándar del
#     linker), y las librerías propias del proyecto antes de libnx.
#---------------------------------------------------------------------------------
LIBS    := -lSDL2_ttf -lSDL2 -lfreetype -lbz2 -lpng -lz -lm -lnx

#---------------------------------------------------------------------------------
# Rutas de librerías: portlibs de devkitPro + libnx
#---------------------------------------------------------------------------------
LIBDIRS := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
# No tocar nada por debajo de aquí (usa las reglas estándar de devkitA64)
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                    $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES    :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES    :=  $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
export LD   :=  $(CC)
else
export LD   :=  $(CXX)
endif

export OFILES_BIN  :=  $(addsuffix .o,$(BINFILES))
export OFILES_SRC   :=  $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES   :=  $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN   :=  $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE   :=  $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                      $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                      -I$(CURDIR)/$(BUILD)

export LIBPATHS  :=  $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nacp $(TARGET).nro $(TARGET).pfs0

#---------------------------------------------------------------------------------
else

DEPENDS   :=  $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
all   :   $(OUTPUT).nro

# Si icon.jpg no existe en la raíz del proyecto, el .nro se genera sin
# icono personalizado en lugar de fallar la compilación.
ifeq ($(wildcard $(APP_ICON)),)
	NO_ICON := 1
endif

ifeq ($(strip $(NO_ICON)),)
	NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
	NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
	NACPFLAGS += --titleid=$(APP_TITLEID)
endif

ifneq ($(ROMID),)
	NROFLAGS += --romfsdir=$(CURDIR)/$(ROMID)
endif

#---------------------------------------------------------------------------------
# Genera primero el .nacp (metadatos) y luego, a partir del .elf ya
# enlazado, el .nro final.
#---------------------------------------------------------------------------------
$(OUTPUT).nro : $(OUTPUT).elf
ifeq ($(strip $(NO_NACP)),)
	@nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $(CURDIR)/$(TARGET).nacp $(NACPFLAGS)
endif
	@elf2nro $(OUTPUT).elf $(OUTPUT).nro $(NROFLAGS)
	@echo "built ... $(notdir $@)"

#---------------------------------------------------------------------------------
# Regla de enlace: TODOS los objetos van antes que las librerías, que es
# el orden que exige el linker de GNU ld para resolver símbolos
# correctamente (evita "undefined reference" a símbolos definidos en
# librerías que se buscaron antes de conocer que se necesitaban).
#---------------------------------------------------------------------------------
$(OUTPUT).elf   :   $(OFILES)
	@echo linking $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@

#---------------------------------------------------------------------------------
%.o: %.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.s
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) $(INCLUDE) -c $< -o $@

#---------------------------------------------------------------------------------
-include $(DEPENDS)

endif
