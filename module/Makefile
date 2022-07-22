###########################################
# Warden
# -------------------------------------
# file       : Makefile
# author     : Ben Kietzman
# begin      : 2021-04-07
# copyright  : kietzman.org
# email      : ben@kietzman.org
###########################################
# {{{ prep work
prefix=/usr/local
all: bin/authn bin/authz bin/bridge bin/central bin/password bin/radial bin/vault bin/windows
install: bin/authn bin/authz bin/bridge bin/central bin/password bin/radial bin/vault bin/windows $(prefix)/warden/module
	install --mode=775 bin/authn $(prefix)/warden/module/
	install --mode=775 bin/authz $(prefix)/warden/module/
	install --mode=775 bin/bridge $(prefix)/warden/module/
	install --mode=775 bin/central $(prefix)/warden/module/
	install --mode=775 bin/password $(prefix)/warden/module/
	install --mode=775 bin/radial $(prefix)/warden/module/
	install --mode=775 bin/vault $(prefix)/warden/module/
	install --mode=775 bin/windows $(prefix)/warden/module/
# }}}

# {{{ authn
authn: bin/authn $(prefix)/warden/module
	install --mode=775 bin/authn $(prefix)/warden/module/
bin/authn: ../../common/libcommon.a obj/authn.o bin 
	g++ -o bin/authn obj/authn.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/authn.o: authn.cpp obj ../../common/Makefile
	g++ -g -Wall -c authn.cpp -o obj/authn.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ authz
authz: bin/authz $(prefix)/warden/module
	install --mode=775 bin/authz $(prefix)/warden/module/
bin/authz: ../../common/libcommon.a obj/authz.o bin
	g++ -o bin/authz obj/authz.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/authz.o: authz.cpp obj ../../common/Makefile
	g++ -g -Wall -c authz.cpp -o obj/authz.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ bridge
bridge: bin/bridge $(prefix)/warden/module
	install --mode=775 bin/bridge $(prefix)/warden/module/
bin/bridge: ../../common/libcommon.a obj/bridge.o bin
	g++ -o bin/bridge obj/bridge.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/bridge.o: bridge.cpp obj ../../common/Makefile
	g++ -g -Wall -c bridge.cpp -o obj/bridge.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ central
central: bin/central $(prefix)/warden/module
	install --mode=775 bin/central $(prefix)/warden/module/
bin/central: ../../common/libcommon.a obj/central.o bin
	g++ -o bin/central obj/central.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/central.o: central.cpp obj ../../common/Makefile
	g++ -g -Wall -c central.cpp -o obj/central.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ password
password: bin/password $(prefix)/warden/module
	install --mode=775 bin/password $(prefix)/warden/module/
bin/password: ../../common/libcommon.a obj/password.o bin
	g++ -o bin/password obj/password.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/password.o: password.cpp obj ../../common/Makefile
	g++ -g -Wall -c password.cpp -o obj/password.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ radial
radial: bin/radial $(prefix)/warden/module
	install --mode=775 bin/radial $(prefix)/warden/module/
bin/radial: ../../common/libcommon.a obj/radial.o bin
	g++ -o bin/radial obj/radial.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/radial.o: radial.cpp obj ../../common/Makefile
	g++ -g -Wall -c radial.cpp -o obj/radial.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ vault
vault: bin/vault $(prefix)/warden/module
	install --mode=775 bin/vault $(prefix)/warden/module/
bin/vault: ../../common/libcommon.a obj/vault.o bin
	g++ -o bin/vault obj/vault.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/vault.o: vault.cpp obj ../../common/Makefile
	g++ -g -Wall -c vault.cpp -o obj/vault.o $(CPPFLAGS) -I../../common;
# }}}
# {{{ windows
windows: bin/windows $(prefix)/warden/module
	install --mode=775 bin/windows $(prefix)/warden/module/
bin/windows: ../../common/libcommon.a obj/windows.o bin
	g++ -o bin/windows obj/windows.o $(LDFLAGS) -L../../common -lcommon -lb64 -lcrypto -lexpat -lmjson -lpthread -lssl -ltar -lz
obj/windows.o: windows.cpp obj ../../common/Makefile
	g++ -g -Wall -c windows.cpp -o obj/windows.o $(CPPFLAGS) -I../../common;
# }}}

# {{{ post work
bin:
	if [ ! -d bin ]; then mkdir bin; fi;
../../common/libcommon.a: ../../common/Makefile
	cd ../../common; make;
../../common/Makefile: ../../common/configure
	cd ../../common; ./configure;
../../common/configure:
	cd ../../; git clone https://github.com/benkietzman/common.git
obj:
	if [ ! -d obj ]; then mkdir obj; fi;
$(prefix)/warden/module: $(prefix)/warden
	if [ ! -d $(prefix)/warden/module ]; then mkdir $(prefix)/warden/module; fi;
$(prefix)/warden:
	if [ ! -d $(prefix)/warden ]; then mkdir $(prefix)/warden; fi;
clean:
	-rm -fr obj bin
# }}}
