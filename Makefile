
CC	= clang
CFLAGS	= -std=c99 -Wall -Wno-pointer-sign -D_XOPEN_SOURCE=700
LDFLAGS	= -s -O2
LDLIBS	= -lc -lpthread

DEPS	=		\
	src/atomic.h	\
	src/array.h	\
	src/cmdline.h	\
	src/connection.h	\
	src/log.h	\
	src/message.h	\
	src/network.h	\
	src/scheduler.h	\
	src/server.h	\
	src/system.h	\
	src/thread.h	\
	src/types.h	\
	src/util.h	\
	src/work.h

kaplar:			\
	build/obj/adler32.o	\
	build/obj/array.o	\
	build/obj/cmdline.o	\
	build/obj/connection.o	\
	build/obj/creature.o	\
	build/obj/game.o	\
	build/obj/log.o	\
	build/obj/main.o	\
	build/obj/message.o	\
	build/obj/protocol_game.o	\
	build/obj/protocol_login.o	\
	build/obj/protocol_old.o	\
	build/obj/protocol_test.o	\
	build/obj/scheduler.o	\
	build/obj/server.o	\
	build/obj/work.o	\
	build/obj/posix/system.o	\
	build/obj/posix/thread.o	\
	build/obj/linux/network.o
	$(CC) -o build/kaplar $^ $(LDLIBS) $(LDFLAGS)

.PHONY: clean
clean:
	@ rm -fR build/

build/obj/adler32.o: src/adler32.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/array.o: src/array.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/cmdline.o: src/cmdline.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/connection.o: src/connection.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/creature.o: src/creature.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/game.o: src/game.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/log.o: src/log.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/main.o: src/main.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/message.o: src/message.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/protocol_game.o: src/protocol_game.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/protocol_login.o: src/protocol_login.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/protocol_old.o: src/protocol_old.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/protocol_test.o: src/protocol_test.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/scheduler.o: src/scheduler.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/server.o: src/server.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/work.o: src/work.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/posix/system.o: src/posix/system.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/posix/thread.o: src/posix/thread.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

build/obj/linux/network.o: src/linux/network.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)
