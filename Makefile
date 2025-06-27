.SUFFIXES: .l .o

CC=clang
CFLAGS=-O2 -Wall -Wextra -ggdb

LIBS!=find lib -name '*.l' | sed 's/\.l/.o/g'

all: eru
lib/libs.c: $(LIBS)
	echo '#include <wchar.h>' > $@
	echo '#include <inttypes.h>' >> $@
	echo 'void* libname2ptr(wchar_t *s, size_t *sz) {' >> $@
	( for n in `find lib -name '*.l'`; do \
		nam=`echo $$n | sed -E 's![/.]!_!g'` ; \
		bn=`basename $$n` ; \
		echo "if (wcscmp(s, L\"$$bn\") == 0) { extern char _binary_"$$nam"_start; " >> $@ ; \
		echo "extern char _binary_"$$nam"_end;" >> $@ ; \
		echo "*sz = &_binary_"$$nam"_end - &_binary_"$$nam"_start; " >> $@ ; \
		echo "return (void*)&_binary_"$$nam"_start; }" >> $@ ; done )
	echo 'return NULL; }' >> $@
eru: eru.c $(LIBS) lib/libs.c
	$(CC) $(CFLAGS) -o $@ $< lib/libs.c $(LIBS) $(LDFLAGS)
.l.o:
	$(LD) -melf_x86_64 -r -b binary -o $@ $<
clean:
	rm -f eru lib/*.o lib/libs.c
