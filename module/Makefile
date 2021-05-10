###########################################
# Warden
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2021-04-07
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################

all: bin/authn bin/authz bin/bridge bin/central bin/password bin/vault bin/windows

bin/authn: ../../common/libcommon.a obj/authn.o bin
	g++ -o bin/authn obj/authn.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/authz: ../../common/libcommon.a obj/authz.o bin
	g++ -o bin/authz obj/authz.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/bridge: ../../common/libcommon.a obj/bridge.o bin
	g++ -o bin/bridge obj/bridge.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/central: ../../common/libcommon.a obj/central.o bin
	g++ -o bin/central obj/central.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/password: ../../common/libcommon.a obj/password.o bin
	g++ -o bin/password obj/password.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/vault: ../../common/libcommon.a obj/vault.o bin
	g++ -o bin/vault obj/vault.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin/windows: ../../common/libcommon.a obj/windows.o bin
	g++ -o bin/windows obj/windows.o -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz

bin:
	if [ ! -d bin ]; then mkdir bin; fi;

../../common/libcommon.a: ../../common/Makefile
	cd ../../common; make;

../../common/Makefile: ../../common/configure
	cd ../../common; ./configure;

../../common/configure:
	cd ../../; git clone https://github.com/benkietzman/common.git

obj/authn.o: authn.cpp obj ../../common/Makefile
	g++ -g -Wall -c authn.cpp -o obj/authn.o -I../../common;

obj/authz.o: authz.cpp obj ../../common/Makefile
	g++ -g -Wall -c authz.cpp -o obj/authz.o -I../../common;

obj/bridge.o: bridge.cpp obj ../../common/Makefile
	g++ -g -Wall -c bridge.cpp -o obj/bridge.o -I../../common;

obj/central.o: central.cpp obj ../../common/Makefile
	g++ -g -Wall -c central.cpp -o obj/central.o -I../../common;

obj/password.o: password.cpp obj ../../common/Makefile
	g++ -g -Wall -c password.cpp -o obj/password.o -I../../common;

obj/vault.o: vault.cpp obj ../../common/Makefile
	g++ -g -Wall -c vault.cpp -o obj/vault.o -I../../common;

obj/windows.o: windows.cpp obj ../../common/Makefile
	g++ -g -Wall -c windows.cpp -o obj/windows.o -I../../common;

obj:
	if [ ! -d obj ]; then mkdir obj; fi;

clean:
	-rm -fr obj bin
