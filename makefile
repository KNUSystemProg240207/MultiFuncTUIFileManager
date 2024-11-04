CC = gcc
CFLAGS = -fdiagnostics-color=always -std=gnu99 -Wall -fanalyzer -g
LFLAGS = -lncurses
OBJS = main.o dir_window.o commons.o title_bar.o footer_area.o list_dir.o
TARGET = demo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

main.o: config.h commons.h dir_window.h title_bar.h footer_area.h main.c
	$(CC) $(CFLAGS) -c main.c

dir_window.o: config.h commons.h dir_window.h dir_window.c
	$(CC) $(CFLAGS) -c dir_window.c

commons.o: commons.h commons.c
	$(CC) $(CFLAGS) -c commons.c

title_bar.o: config.h commons.h title_bar.h title_bar.c
	$(CC) $(CFLAGS) -c title_bar.c

footer_area.o: config.h commons.h footer_area.h footer_area.c
	$(CC) $(CFLAGS) -c footer_area.c

list_dir.o: config.h commons.h list_dir.h list_dir.c
	$(CC) $(CFLAGS) -c list_dir.c

clean:
	rm $(OBJS)
	rm $(TARGET)
