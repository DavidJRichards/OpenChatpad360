SRC=openKeyboard.c
OBJ=openKeyboard.o

CC=sdcc
LINK=gplink

FAMILY=pic14
PROC=16f883

all: $(SRC:.c=.hex)

$(SRC:.c=.o): $(SRC)
	$(CC) --use-non-free -c -m$(FAMILY) -p$(PROC) $^

$(OBJ:.o=.hex): $(OBJ)
	$(LINK) -C -m --mplink-compatible  -I/usr/local/bin/../share/sdcc/lib/pic14 -I/usr/local/share/sdcc/lib/pic14 -I/usr/local/bin/../share/sdcc/non-free/lib/pic14 -I/usr/local/share/sdcc/non-free/lib/pic14  -I/usr/local/bin/../share/sdcc/lib/pic14 -I/usr/local/share/sdcc/lib/pic14 -I/usr/local/bin/../share/sdcc/non-free/lib/pic14 -I/usr/local/share/sdcc/non-free/lib/pic14   -w -r -o $^_ $^   libsdcc.lib pic16f887.lib 	



clean:
	rm -f $(SRC:.c=.asm) $(SRC:.c=.cod) $(SRC:.c=.hex) $(SRC:.c=.lst) $(SRC:.c=.o) $(SRC:.c=.map) 

.PHONY: all clean
