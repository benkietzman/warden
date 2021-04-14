###########################################
# Warden
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2021-04-07
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

all: bin/vault

bin/vault: ../../common/libcommon.a obj/vault.o bin
	g++ -o bin/vault obj/vault.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin:
	if [ ! -d bin ]; then mkdir bin; fi;

../../common/libcommon.a: ../../common/Makefile
	cd ../../common; make;

../../common/Makefile: ../../common/configure
	cd ../../common; ./configure;

../../common/configure:
	cd ../../; git clone https://github.com/benkietzman/common.git

obj/vault.o: vault.cpp obj ../../common/Makefile
	g++ -g -Wall -c vault.cpp -o obj/vault.o -I../../common;

obj:
	if [ ! -d obj ]; then mkdir obj; fi;

clean:
	-rm -fr obj bin
