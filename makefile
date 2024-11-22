CC = gcc
CFLAGS = -D_GNU_SOURCE -fdiagnostics-color=always -std=gnu99 -Wall -fanalyzer -g
LFLAGS = -lncurses -lpanel -lpthread
TARGET = demo.out
# 주의: Source 추가 시 해당 object file, header file 추가
OBJS = main.o commons.o dir_window.o title_bar.o bottom_area.o thread_commons.o dir_listener.o file_operator.o file_functions.o colors.o dir_entry_utils.o process_window.o list_process.o
HEADERS = config.h commons.h dir_window.h title_bar.h bottom_area.h thread_commons.h dir_listener.h file_operator.h file_functions.h colors.h dir_entry_utils.h proc_win.h list_process.h


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

main.o: $(HEADERS) main.c
	$(CC) $(CFLAGS) -c main.c

commons.o: commons.h commons.c
	$(CC) $(CFLAGS) -c commons.c

# ncurses windows
commons.o: commons.h commons.c
	$(CC) $(CFLAGS) -c commons.c

# ncurses windows
dir_window.o: config.h commons.h dir_window.h dir_window.c
	$(CC) $(CFLAGS) -c dir_window.c

title_bar.o: config.h commons.h title_bar.h title_bar.c
	$(CC) $(CFLAGS) -c title_bar.c

bottom_area.o: config.h commons.h file_operator.h bottom_area.h bottom_area.c
	$(CC) $(CFLAGS) -c bottom_area.c

process_window.o: config.h commons.h list_process.h proc_win.h process_window.c
	$(CC) $(CFLAGS) -c process_window.c

# Threads
thread_commons.o: config.h thread_commons.h thread_commons.c
	$(CC) $(CFLAGS) -c thread_commons.c

dir_listener.o: config.h commons.h thread_commons.h dir_entry_utils.h dir_listener.h dir_listener.c
	$(CC) $(CFLAGS) -c dir_listener.c

file_operator.o: config.h commons.h thread_commons.h file_functions.h file_operator.h file_operator.c
	$(CC) $(CFLAGS) -c file_operator.c

list_process.o: config.h commons.h thread_commons.h proc_win.h list_process.c
	$(CC) $(CFLAGS) -c list_process.c

# Color Set
colors.o: colors.h colors.c commons.h
	$(CC) $(CFLAGS) -c colors.c

# Misc
dir_entry_utils.o: config.h dir_window.h thread_commons.h dir_entry_utils.h dir_entry_utils.c
	$(CC) $(CFLAGS) -c dir_entry_utils.c

file_functions.o: file_functions.h file_functions.c
	$(CC) $(CFLAGS) -c file_functions.c

clean:
	rm $(OBJS)
	rm $(TARGET)
