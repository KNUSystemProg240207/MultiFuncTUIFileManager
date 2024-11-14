CC = gcc
CFLAGS = -fdiagnostics-color=always -std=gnu99 -Wall -fanalyzer -g
LFLAGS = -lncurses
OBJS = main.o commons.o dir_window.o title_bar.o bottom_area.o thread_commons.o dir_listener.o
TARGET = demo.out

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

main.o: config.h commons.h dir_window.h title_bar.h bottom_area.h main.c
	$(CC) $(CFLAGS) -c main.c

commons.o: commons.h commons.c
	$(CC) $(CFLAGS) -c commons.c

# ncurses windows
dir_window.o: config.h commons.h dir_window.h dir_window.c
	$(CC) $(CFLAGS) -c dir_window.c

title_bar.o: config.h commons.h title_bar.h title_bar.c
	$(CC) $(CFLAGS) -c title_bar.c

bottom_area.o: config.h commons.h bottom_area.h bottom_area.c
	$(CC) $(CFLAGS) -c bottom_area.c

# Threads
thread_commons.o: config.h thread_commons.h thread_commons.c
	$(CC) $(CFLAGS) -c thread_commons.c

dir_listener.o: config.h commons.h dir_listener.h dir_listener.c
	$(CC) $(CFLAGS) -c dir_listener.c

# misc
clean:
	rm $(OBJS)
	rm $(TARGET)
