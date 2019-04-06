CC=9c
LD=9l
CFLAGS=-Wall
LDFLAGS=
BIN=$HOME/bin

TARG=Git
OFILES=acme-git.o
HFILES=a.h

all:V:	$TARG

$TARG:	$OFILES
	$LD -o $target $prereq $LDFLAGS

%.o:	%.c $HFILES
	$CC $CFLAGS $stem.c

install:V:	$TARG
	cp $prereq $BIN
	
clean:V:
	rm $OFILES $TARG
