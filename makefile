CC = gcc
CFLAGS = -fdiagnostics-color=always -std=gnu99 -Wall -fanalyzer -g
LFLAGS = -lncurses
OBJS = main.o commons.o thread_commons.o dir_window.o title_bar.o bottom_area.o list_dir.o process_window.o list_process.o 
HEADERS = config.h commons.h dir_window.h title_bar.h bottom_area.h thread_commons.h proc_win.h list_process.h
TARGET = demo.out

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

main.o: $(HEADERS) main.c
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

process_window.o: config.h commons.h list_process.h proc_win.h process_window.c
	$(CC) $(CFLAGS) -c process_window.c

list_process.o: config.h commons.h thread_commons.h proc_win.h list_process.c
	$(CC) $(CFLAGS) -c list_process.c

thread_commons.o: config.h commons.h thread_commons.c thread_commons.h
	$(CC) $(CFLAGS) -c thread_commons.c

clean:
	rm $(OBJS)
	rm $(TARGET)
