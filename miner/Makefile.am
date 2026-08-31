
if WANT_JANSSON
JANSSON_INCLUDES= -I$(top_srcdir)/compat/jansson
else
JANSSON_INCLUDES=
endif

EXTRA_DIST	= example-cfg.json nomacro.pl gen_fuzz_corpus.py \
		  fuzz_corpus/gbt_solo replay_fuzz_corpus.sh

SUBDIRS		= compat

bin_PROGRAMS	= minerd

dist_man_MANS	= minerd.1

minerd_SOURCES	= elist.h miner.h rigor_net.h compat.h \
		  cpu-miner.c util.c \
		  sha2.c scrypt.c \
		  opus_hash.cpp opus_hash.h \
		  arith_uint256.c arith_uint256.h \
		  gbt_solo.c gbt_solo.h
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

check_PROGRAMS	= test_gbt_solo fuzz_gbt_solo
TESTS		= test_gbt_solo replay_fuzz_corpus.sh
TEST_EXTENSIONS	= .sh
AM_TESTS_ENVIRONMENT = FUZZ_CORPUS='$(srcdir)/fuzz_corpus/gbt_solo'; export FUZZ_CORPUS;

test_gbt_solo_SOURCES	= test_gbt_solo.c gbt_solo.c sha256d_only.c \
			  opus_hash.cpp arith_uint256.c
test_gbt_solo_LDADD	= @JANSSON_LIBS@ @PTHREAD_LIBS@
test_gbt_solo_CPPFLAGS	= $(JANSSON_INCLUDES) $(PTHREAD_FLAGS)
test_gbt_solo_CFLAGS	= -fno-strict-aliasing
test_gbt_solo_CXXFLAGS	= -fno-strict-aliasing

fuzz_gbt_solo_SOURCES	= fuzz_gbt_solo.c gbt_solo.c sha256d_only.c arith_uint256.c
fuzz_gbt_solo_LDADD	= @JANSSON_LIBS@
fuzz_gbt_solo_CPPFLAGS	= $(JANSSON_INCLUDES)
fuzz_gbt_solo_CFLAGS	= -fno-strict-aliasing

# Re-run configure then rebuild. Default ./configure has stderr log lock OFF (production).
.PHONY: debug release
debug:
	'$(srcdir)/configure' --enable-stderr-log-lock 'CFLAGS=-g -O0' && $(MAKE) $(AM_MAKEFLAGS)

release:
	'$(srcdir)/configure' --disable-stderr-log-lock 'CFLAGS=-O2' && $(MAKE) $(AM_MAKEFLAGS)

