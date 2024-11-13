CC = gcc
CFLAGS = -fdiagnostics-color=always -std=gnu99 -Wall -fanalyzer -g
LFLAGS = -lncurses
OBJS = main.o dir_window.o commons.o title_bar.o bottom_area.o list_dir.o colors.o
TARGET = demo.out

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

main.o: config.h commons.h dir_window.h title_bar.h bottom_area.h main.c
	$(CC) $(CFLAGS) -c main.c

dir_window.o: config.h commons.h dir_window.h dir_window.c
	$(CC) $(CFLAGS) -c dir_window.c

commons.o: commons.h commons.c
	$(CC) $(CFLAGS) -c commons.c

title_bar.o: config.h commons.h title_bar.h title_bar.c
	$(CC) $(CFLAGS) -c title_bar.c

bottom_area.o: config.h commons.h bottom_area.h bottom_area.c
	$(CC) $(CFLAGS) -c bottom_area.c

list_dir.o: config.h commons.h list_dir.h list_dir.c
	$(CC) $(CFLAGS) -c list_dir.c

colors.o: colors.h colors.c commons.h
	$(CC) $(CFLAGS) -c colors.c

clean:
	rm $(OBJS)
	rm $(TARGET)
