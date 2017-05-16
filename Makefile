SRCDIR	= src
INCDIR	= src
OBJDIR	= build/obj

CC	= clang
CFLAGS	= -std=c99 -O2 -Wall -Wno-pointer-sign
DEFS	= -D_XOPEN_SOURCE=700
LIBS	= -lpthread

_OBJ	= adler32.o array.o cmdline.o connection.o	\
	creature.o game.o log.o main.o message.o	\
	protocol_game.o protocol_login.o protocol_old.o	\
	protocol_test.o scheduler.o server.o work.o	\
	posix/system.o posix/thread.o linux/network.o

_DEPS	= array.h cmdline.h connection.h log.h message.h \
	network.h scheduler.h server.h system.h thread.h \
	types.h util.h work.h

OBJ 	= $(patsubst %, $(OBJDIR)/%, $(_OBJ))
DEPS	= $(patsubst %, $(INCDIR)/%, $(_DEPS))

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(DEFS) $(CFLAGS)

kaplar: $(OBJ)
	$(CC) -s -o build/$@ $^ $(LIBS) $(DEFS) $(CFLAGS)
