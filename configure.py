#!/usr/bin/python

import sys

MakefileHeader = r'''
CC	= %s
CFLAGS	= %s
LDFLAGS	= %s
LDLIBS	= %s

DEPS	=		\
	%s

kaplar:			\
	%s
	$(CC) -o build/$@ $^ $(LDLIBS) $(LDFLAGS)

.PHONY: clean
clean:
	@ rm -fR build
'''

MakefileObject = r'''
%s: %s $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)
'''

DEPS = [
	"array.h", "cmdline.h", "connection.h", "log.h",
	"message.h", "network.h", "scheduler.h", "server.h",
	"system.h", "thread.h", "types.h", "util.h", "work.h",
]

COMMON = [
	"adler32.o", "array.o", "cmdline.o", "connection.o",
	"creature.o", "game.o", "log.o", "main.o", "message.o",
	"protocol_game.o", "protocol_login.o", "protocol_old.o",
	"protocol_test.o", "scheduler.o", "server.o", "work.o",
]

WIN32 = [
	"win32/system.o", "win32/thread.o", "win32/network.o",
]

LINUX = [
	"posix/system.o", "posix/thread.o", "linux/network.o",
]

FREEBSD = [
	"posix/system.o", "posix/thread.o", "freebsd/network.o",
]

with open("Makefile", "w") as file:
	# default to release options
	CC	= "clang"
	CFLAGS	= "-std=c99 -Wall -Wno-pointer-sign"
	CFLAGS += " -D_XOPEN_SOURCE=700"
	LDFLAGS	= "-s -O2"
	LDLIBS	= "-lc -lpthread"
	OBJECTS	= COMMON[:]

	for opt in sys.argv[1:]:
		if opt == "-win32":
			OBJECTS.extend(WIN32)

		elif opt == "-linux":
			OBJECTS.extend(LINUX)

		elif opt == "-freebsd":
			OBJECTS.extend(FREEBSD)
			CFLAGS += " -D__BSD_VISIBLE=1"

		elif opt == "-debug":
			CFLAGS	= "-g " + CFLAGS
			LDFLAGS = "-g"

		else:
			print("[warning] Invalid option used: \"%s\"" % opt)

	DEPS	= ["src/" + dep for dep in DEPS]
	OBJECTS = [("build/obj/" + obj, "src/" + obj[:-2] + ".c")
					for obj in OBJECTS]
	file.write(MakefileHeader % (CC, CFLAGS, LDFLAGS, LDLIBS,
		'\t\\\n\t'.join(DEPS),
		'\t\\\n\t'.join(list(zip(*OBJECTS))[0])))

	for obj in OBJECTS:
		file.write(MakefileObject % obj)

