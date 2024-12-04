CC = gcc
CFLAGS = -fdiagnostics-color=always -std=gnu99 -Wall -fanalyzer -g
DFLAGS = -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
# DFLAGS = 
LFLAGS = -lncurses -lpanel -lpthread
TARGET = file-manager.out
# 주의: Source 추가 시 해당 object file, header file 추가
OBJS = main.o commons.o dir_window.o title_bar.o bottom_area.o process_window.o popup_window.o selection_window.o thread_commons.o dir_listener.o file_operator.o list_process.o colors.o dir_entry_utils.o file_functions.o
HEADERS = bottom_area.h colors.h commons.h config.h dir_entry_utils.h dir_listener.h dir_window.h file_functions.h file_operator.h list_process.h popup_window.h process_window.h selection_window.h thread_commons.h title_bar.h


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LFLAGS)

main.o: $(HEADERS) main.c
	$(CC) $(DFLAGS) $(CFLAGS) -c main.c

commons.o: commons.h commons.c
	$(CC) $(DFLAGS) $(CFLAGS) -c commons.c

# ncurses windows
dir_window.o: colors.h commons.h config.h dir_entry_utils.h dir_listener.h dir_window.h file_operator.h dir_window.c
	$(CC) $(DFLAGS) $(CFLAGS) -c dir_window.c

title_bar.o: config.h commons.h title_bar.h title_bar.c
	$(CC) $(DFLAGS) $(CFLAGS) -c title_bar.c

bottom_area.o: bottom_area.h config.h file_operator.h bottom_area.c
	$(CC) $(DFLAGS) $(CFLAGS) -c bottom_area.c

process_window.o: colors.h commons.h config.h list_process.h process_window.h process_window.c
	$(CC) $(DFLAGS) $(CFLAGS) -c process_window.c

popup_window.o: colors.h config.h popup_window.h popup_window.c
	$(CC) $(DFLAGS) $(CFLAGS) -c popup_window.c

selection_window.o: colors.h config.h selection_window.h selection_window.c
	$(CC) $(DFLAGS) $(CFLAGS) -c selection_window.c

# Threads
thread_commons.o: commons.h thread_commons.h thread_commons.c
	$(CC) $(DFLAGS) $(CFLAGS) -c thread_commons.c

dir_listener.o: config.h dir_entry_utils.h dir_listener.h thread_commons.h dir_listener.c
	$(CC) $(DFLAGS) $(CFLAGS) -c dir_listener.c

file_operator.o: config.h file_functions.h file_operator.h thread_commons.h file_operator.c
	$(CC) $(DFLAGS) $(CFLAGS) -c file_operator.c

list_process.o: config.h thread_commons.h list_process.h list_process.c
	$(CC) $(DFLAGS) $(CFLAGS) -c list_process.c

# Color Set
colors.o: colors.h colors.c commons.h
	$(CC) $(DFLAGS) $(CFLAGS) -c colors.c

# Misc
dir_entry_utils.o: config.h dir_entry_utils.h dir_window.h dir_entry_utils.c
	$(CC) $(DFLAGS) $(CFLAGS) -c dir_entry_utils.c

file_functions.o: config.h file_functions.h file_operator.h file_functions.c
	$(CC) $(DFLAGS) $(CFLAGS) -c file_functions.c

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)
