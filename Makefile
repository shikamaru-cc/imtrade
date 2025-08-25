#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need SDL2 (http://www.libsdl.org):
# Linux:
#   apt-get install libsdl2-dev
# Mac OS X:
#   brew install sdl2
# MSYS2:
#   pacman -S mingw-w64-i686-SDL2
#

#CXX = g++
#CXX = clang++

CXXFLAGS = -std=c++11 -g -Wall -Wformat
LIBS =
UNAME_S = $(shell uname -s)

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lGL -ldl -lcurl `sdl2-config --libs`

	CXXFLAGS += `sdl2-config --cflags`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo `sdl2-config --libs`
	LIBS += -L/usr/local/lib -L/opt/local/lib

	CXXFLAGS += `sdl2-config --cflags`
	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lgdi32 -lopengl32 -limm32 `pkg-config --static --libs sdl2`

	CXXFLAGS += `pkg-config --cflags sdl2`
	CFLAGS = $(CXXFLAGS)
endif

OBJ = imtrade.o \
	imgui.o \
	imgui_demo.o \
	imgui_draw.o \
	imgui_tables.o \
	imgui_widgets.o \
	imgui_impl_sdl2.o \
	imgui_impl_sdlrenderer2.o \
	implot.o \
	implot_items.o \
	livermore.o \
	cJSON.o

all: imtrade

cJSON.o: cJSON.cpp cJSON.h
imgui.o: imgui.cpp imgui.h imconfig.h imgui_internal.h
imgui_demo.o: imgui_demo.cpp imgui.h imconfig.h
imgui_draw.o: imgui_draw.cpp imgui.h imconfig.h imgui_internal.h \
 imstb_rectpack.h imstb_truetype.h
imgui_impl_sdl2.o: imgui_impl_sdl2.cpp imgui.h imconfig.h \
 imgui_impl_sdl2.h
imgui_impl_sdlrenderer2.o: imgui_impl_sdlrenderer2.cpp imgui.h imconfig.h \
 imgui_impl_sdlrenderer2.h
imgui_tables.o: imgui_tables.cpp imgui.h imconfig.h imgui_internal.h
imgui_widgets.o: imgui_widgets.cpp imgui.h imconfig.h imgui_internal.h \
 imstb_textedit.h
implot.o: implot.cpp implot.h imgui.h imconfig.h implot_internal.h \
 imgui_internal.h
implot_items.o: implot_items.cpp implot.h imgui.h imconfig.h \
 implot_internal.h imgui_internal.h
imtrade.o: imtrade.cpp imgui.h imconfig.h imgui_impl_sdl2.h \
 imgui_impl_sdlrenderer2.h implot.h implot_internal.h imgui_internal.h \
 livermore.h
livermore.o: livermore.cpp livermore.h cJSON.h

imtrade: $(OBJ)
	$(CXX) -o imtrade $(OBJ) $(CXXFLAGS) $(LIBS)

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $<

clean:
	rm -rf imtrade *.o

dep:
	$(CXX) -MM *.cpp

