
if WANT_JANSSON
JANSSON_INCLUDES= -I$(top_srcdir)/compat/jansson
else
JANSSON_INCLUDES=
endif

EXTRA_DIST	= example-cfg.json nomacro.pl

SUBDIRS		= compat

bin_PROGRAMS	= minerd

dist_man_MANS	= minerd.1

minerd_SOURCES	= elist.h miner.h rigor_net.h compat.h \
		  cpu-miner.c util.c \
		  sha2.c scrypt.c \
		  opus_hash.cpp opus_hash.h \
		  arith_uint256.c arith_uint256.h
if USE_ASM
if ARCH_x86
minerd_SOURCES += sha2-x86.S scrypt-x86.S
endif
if ARCH_x86_64
minerd_SOURCES += sha2-x64.S scrypt-x64.S
endif
if ARCH_ARM
minerd_SOURCES += sha2-arm.S scrypt-arm.S
endif
if ARCH_PPC
minerd_SOURCES += sha2-ppc.S scrypt-ppc.S
endif
endif
minerd_LDFLAGS	= $(PTHREAD_FLAGS)
minerd_LDADD	= @LIBCURL@ @JANSSON_LIBS@ @PTHREAD_LIBS@ @WS2_LIBS@
minerd_CFLAGS	= -fno-strict-aliasing
minerd_CPPFLAGS	= @LIBCURL_CPPFLAGS@ $(JANSSON_INCLUDES) $(PTHREAD_FLAGS)

# Re-run configure then rebuild. Default ./configure has stderr log lock OFF (production).
.PHONY: debug release
debug:
	'$(srcdir)/configure' --enable-stderr-log-lock 'CFLAGS=-g -O0' && $(MAKE) $(AM_MAKEFLAGS)

release:
	'$(srcdir)/configure' --disable-stderr-log-lock 'CFLAGS=-O2' && $(MAKE) $(AM_MAKEFLAGS)

