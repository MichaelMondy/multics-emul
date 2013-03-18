# Top level Makefile for the Multics emulator.  In addition to driving the
# compilation of the Multics emulator sources, this makefile also builds
# a library version of SIMH.  It also provides a mechanism of extracting
# the platform-dependent variables that would be set by the SIMH makefile.

LDFLAGS = -lm -lrt -lpthread -ldl
LDFLAGS += -lgmp
LDFLAGS += -g


all: testkit multics
	@

# ============================================================================

testkit: testkit1 testkit2
testkit1:
	@test -d simh || { echo "You need to download ./simh!"; exit 1; }
	@test -d decNumber || { echo "You need to download ./decNumber!"; exit 1; }
	@test -d bin || mkdir bin
	@test -d etc || mkdir etc
	@test -f bin/run.sh || cp sample/run.sh bin/.
testkit2: etc/multics.run.ini etc/multics.diag.ini decNumber/Makefile
etc/multics.run.ini:
	cp sample/multics.run.ini etc/.
etc/multics.diag.ini:
	cp sample/multics.diag.ini etc/.
decNumber/Makefile:
	cp sample/Makefile.decNumber decNumber/Makefile

makefile-samples: makefile.simh.cooked.include makefile.simh.raw.include
	@

# ============================================================================

multics: simh/scp.o src/show-cflags src/multics.a simh/simh.a decNumber/decNumber.a bin/multics
	@

bin/multics: simh/scp.o src/multics.a simh/simh.a decNumber/decNumber.a
	g++ -o $@ $(CFLAGS) simh/scp.o src/multics.a simh/simh.a decNumber/decNumber.a $(LDFLAGS)

src/show-cflags:
	cd src && ${MAKE} cflags

src/multics.a: src/.PHONY
	cd src && ${MAKE} multics.a
src/.PHONY:
	@

# ============================================================================

decNumber/decNumber.a: decNumber/.PHONY
	cd decNumber && ${MAKE} decNumber.a
decNumber/.PHONY:
	@

# ============================================================================

# A library version of SIMH

simh/scp.o: simh/.makefile-lib simh/.PHONY
	cd simh && ${MAKE} scp.o
simh/simh.a: simh/.makefile-lib simh/.PHONY
	cd simh && ${MAKE} simh.a
simh/.PHONY:
	@

# ============================================================================

# Makefile for a library version of SIMH
simh/.makefile-lib: simh/makefile.simh Makefile.simh.lib
	cp Makefile.simh.lib simh/Makefile
	touch $@
	@echo SIMH can now be built as a library
	@echo

# Essentially a backup of the original [Mm]akefile
simh/makefile.simh:
	@echo Renaming simh makefile in order to wrap it for library builds
	mv simh/makefile $@

# ============================================================================

# If you wish, you could include either the 'raw' or 'cooked' *.include file
# below near the top of src/Makefile to make sure the exact same compiler
# settings are used for both the SIMH library and Multics emulator sources.

makefile.simh.raw.include: simh/.makefile-lib Makefile
	@echo Creating sample make include file, raw version
	echo "# Compiler flags used to compile the SIMH library." > tmpf
	@echo "# Auto generated by top level Makefile." >> tmpf
	(cd simh; $(MAKE) dump-settings-raw) > tmpf2
	grep -v '^make' tmpf2 >> tmpf
	@rm -f tmpf2
	mv tmpf $@
	@echo "* Sample include $@ now contains SIMH compile-time settings (raw version)"
	@echo

makefile.simh.cooked.include: simh/.makefile-lib
	@echo Creating sample make include file, cooked version
	echo "# Compiler flags used to compile the SIMH library." > tmpf
	@echo "# Auto generated by top level Makefile." >> tmpf
	(cd simh; $(MAKE) dump-settings-cooked) | grep -v '^make' >> tmpf
	mv tmpf $@
	@echo "* Sample include $@ now contains SIMH compile-time settings (cooked version)"
	@echo

# ============================================================================

clean:
	cd simh && ${MAKE} clean
	cd simh && rm -f *.o *.a
	cd decNumber && ${MAKE} clean
	cd src && ${MAKE} clean
