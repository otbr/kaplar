#!/usr/bin/python

import sys

Usage = r'''
    USAGE: python configure.py [options]

Options: (options in the same section are mutually exclusive)

    -o <file>               - write output to <file>
    -srcdir <dir>           - change source directory
    -builddir <dir>         - change build directory
    -test                   - compiles ./main.c instead of <srcdir>/main.c
                              (this is useful for unit testing)

    [compiler]:
        -clang (default)    -
        -gcc                -

    [build type]:
        -release (default)  -
	-debug              -

    [platform]:
        -win32 (default)    -
        -linux              -
        -freebsd            -

    [endianess]:
        -le (default)       compile for little endian arch
	-be                 compile for big endian arch
'''

MakefileHeader = r'''
CC	= %s
CFLAGS	= %s
LDFLAGS	= %s
LDLIBS	= %s

DEPS	=		\
	%s

kaplar:			\
	%s
	$(CC) -o %s $^ $(LDLIBS) $(LDFLAGS)

.PHONY: clean
clean:
	@ rm -fR %s
'''

MakefileObject = r'''
%s: %s $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)
'''

DEPS = [
	"atomic.h", "cmdline.h", "connection.h", "log.h",
	"message.h", "mmblock.h", "mm.h", "network.h",
	"scheduler.h", "server.h", "system.h", "thread.h",
	"types.h", "util.h", "work.h", "work_group.h",
]

COMMON = [
	"adler32.o", "cmdline.o", "connection.o", "log.o",
	"main.o", "message.o", "mmblock.o", "mm.o",
	"protocol_game.o", "protocol_login.o", "protocol_old.o",
	"protocol_test.o", "scheduler.o", "server.o", "work.o",
	"work_group.o",
]

WIN32 = [
	"win32/atomic.o",
	"win32/system.o", "win32/thread.o", "win32/network.o",
]

LINUX = [
	"linux/atomic.o",
	"posix/system.o", "posix/thread.o", "linux/network.o",
]

FREEBSD = [
	"freebsd/atomic.o",
	"posix/system.o", "posix/thread.o", "freebsd/network.o",
]

if __name__ == "__main__":
	# parse parameters
	output		= 'kaplar'
	srcdir		= 'src/'
	builddir	= 'build/'
	test		= False
	compiler	= "CLANG"
	build		= "RELEASE"
	platform	= "WIN32"

	#default to this platform byteorder
	byteorder	= "LITTLE"
	if sys.byteorder == "big":
		endianess = "BIG"

	args = iter(sys.argv[1:])
	for opt in args:
		#output name
		if opt == "-o":
			output = next(args)

		#source dir
		elif opt == "-srcdir":
			srcdir = next(args)

		#build dir
		elif opt == "-builddir":
			builddir = next(args)

		#enable unit testing
		elif opt == "-test":
			test = True

		#compilers
		elif opt == "-clang":
			compiler = "CLANG"

		elif opt == "-gcc":
			compiler = "GCC"

		#build types
		elif opt == "-release":
			build = "RELEASE"

		elif opt == "-debug":
			build = "DEBUG"

		#platforms
		elif opt == "-win32":
			platform = "WIN32"

		elif opt == "-linux":
			platform = "LINUX"

		elif opt == "-freebsd":
			platform = "FREEBSD"

		#endianess
		elif opt == "-le":
			byteorder = "LITTLE"

		elif opt == "-be":
			byteorder = "BIG"

		# invalid option
		else:
			print("[warning] Invalid option used: \"%s\"" % opt)
			print(Usage)
			sys.exit()

	# set parameters
	CC	= ""
	CFLAGS	= "-std=c99 -Wall -Wno-pointer-sign"
	CDEFS	= "-D_XOPEN_SOURCE=700"
	LDFLAGS	= ""
	LDLIBS	= "-lc -lpthread"
	OBJECTS	= COMMON[:]

	#check compiler
	if compiler == "GCC":
		CC = "gcc"
	elif compiler == "CLANG":
		CC = "clang"
	else:
		print("[error] invalid compiler")
		sys.exit()

	#check platform
	if platform == "WIN32":
		OBJECTS.extend(WIN32)
	elif platform == "LINUX":
		OBJECTS.extend(LINUX)
	elif platform == "FREEBSD":
		OBJECTS.extend(FREEBSD)
		CDEFS += " -D__BSD_VISIBLE=1"
	else:
		print("[error] invalid platform")
		sys.exit()

	#check build
	if build == "RELEASE":
		LDFLAGS = "-s -O2"
	elif build == "DEBUG":
		CFLAGS = "-g " + CFLAGS
		LDFLAGS = "-g"
	else:
		print("[error] invalid build type")
		sys.exit()

	#check endianess
	if byteorder == "LITTLE":
		pass
	elif byteorder == "BIG":
		CDEFS += " -D__BIG_ENDIAN__"
	else:
		print("[error] invalid byteorder")
		sys.exit()

	#concat CFLAGS and CDEFS
	CFLAGS += " " + CDEFS

	#add path to dependencies
	DEPS	= [srcdir + dep for dep in DEPS]

	#create tuple (obj, src) for each object
	OBJECTS = [(builddir + "obj/" + obj, srcdir + obj[:-2] + ".c")
				for obj in OBJECTS]

	#if testing, change <srcdir>/main.c to ./main.c
	if test == True:
		for (a, b) in OBJECTS:
			if ("main.o" in a) or ("main.c" in b):
				OBJECTS.remove((a, b))
				OBJECTS.append((a, "main.c"))
				break

	# output to file
	with open("Makefile", "w") as file:
		file.write(MakefileHeader % (CC, CFLAGS, LDFLAGS, LDLIBS,
			'\t\\\n\t'.join(DEPS),
			'\t\\\n\t'.join(list(zip(*OBJECTS))[0]),
			builddir + output, builddir))

		for obj in OBJECTS:
			file.write(MakefileObject % obj)

