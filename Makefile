CC = gcc
CFLAGS = -Wall -O2 -I/usr/include/freetype2 -I/usr/include/libpng16 -lXft
LDFLAGS = -lX11 -lXft -lfreetype -I/usr/include/freetype2 -I/usr/include/libpng16

SRC = main.c \
      graphics/graphics.c \
      gif/parser.c \
      lzw/lzwdecode.c \
      modules.c \
      modules/preview.c \
      timeline/timeline.c

OBJ = $(SRC:.c=.o)
EXEC = grrgif

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
