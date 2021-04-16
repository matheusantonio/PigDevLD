#compilador
CC = g++

#flags de compilacao
CC_FLAGS = -Wall -fexceptions -O3 -w -std=c++11

#flags de arquitetura
CC_ARCH = native

#flag para decidir qual libLinux usar (x64 ou x86)
OS_BITS = x64

#diretorios de busca de arquivos .h
CC_INCLUDE = -I../include -I../include/SDL -I../include/SDL_image -I../include/video 
 

#diretorios de busca de bibliotecas est·ticas
#CC_LIB = -L../libLinux/$(OS_BITS) -L../bin
CC_LIB = -L../SDL2 -L../bin

#bibliotecas necessarias
LIBRARIES = -pthread -lm -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lSDL2_net

$(info CC_ARCH="$(CC_ARCH)")
$(info CC_INCLUDE="$(CC_INCLUDE)")
$(info CC_LIB="$(CC_LIB)")
$(info LIBRARIES="$(LIBRARIES)")

all: Projeto

Projeto: main.o
	$(CC) $(CC_FLAGS) -c cJSON.c -o cjson.o
#	$(CC) $(CC_FLAGS) $(CC_INCLUDE) -o main2.o main.o cjson.o
	$(CC) main.o cjson.o $(CC_LIB) $(LIBRARIES) -o Projeto
	

main.o: main.cpp
	$(CC) $(CC_FLAGS) $(CC_INCLUDE) -c main.cpp -o main.o 

run:
	DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:.:../libMacOsX ./Projeto

clean:
	rm *.o Projeto

