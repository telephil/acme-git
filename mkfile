CC=9c
LD=9l
CFLAGS=-Wall
LDFLAGS=

DESTDIR=$HOME/bin
TARG=Git
OFILES=acme-git.o

all:V:	$TARG

$TARG:	$OFILES
	$LD -o $target $prereq $LDFLAGS

%.o:	%.c
	$CC $CFLAGS $stem.c

install:V:	$TARG
	cp $prereq $DESTDIR
	
clean:V:
	rm $OFILES $TARG
