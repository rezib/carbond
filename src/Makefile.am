AM_CFLAGS = -Wall -DSYSCONFDIR="\"${sysconfdir}\""
AM_LDFLAGS =

carbond_LDADD = @PCRE_LIBS@

bin_PROGRAMS = carbond
carbond_SOURCES = \
  main.c \
  common.h \
  log.c log.h \
  conf.c conf.h \
  protocol.c procotol.h \
  receiver_tcp.c receiver_tcp.h \
  receiver_udp.c receiver_udp.h \
  monitoring.c monitoring.h \
  database.c database.h \
  threads.c threads.h \
  whisper.c whisper.h \
  writer.c writer.h
