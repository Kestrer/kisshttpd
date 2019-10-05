# c.mk 1.0

CC::=clang
CFLAGS::=-Wall -Wextra -pedantic -Werror -g -Og
LIBS::=-pthread -lz
LDFLAGS::=-rdynamic -Wl,-rpath=/usr/local/lib -L/usr/local/lib

PREFIX::=/usr/local

SRCEXT::=c
OBJEXT::=o
DEPEXT::=dep

HASMAINREGEX::='int\s+main\s*\('
NOMAIN::=${shell pcre2grep -Mr $(HASMAINREGEX) . >/dev/null 2>&1; echo $$?}

DEPDIR::=deps
OBJDIR::=objects

SOURCES::=$(shell find . -name '*.$(SRCEXT)' -printf '%f\n')
ifeq ($(NOMAIN), 1)
PICDIR::=picobjects
BASENAME::=$(shell basename $$(pwd))
TARGET::=lib$(BASENAME).a
SOTARGET::=lib$(BASENAME).so
PREFIXTARGET::=$(PREFIX)/lib/$(TARGET)
PREFIXSOTARGET::=$(PREFIX)/lib/$(SOTARGET)
ifneq ("$(wildcard $(BASENAME).h)","")
INCLUDE::=$(BASENAME).h
PREFIXINCLUDE::=$(PREFIX)/include/$(INCLUDE)
endif
else
TARGET::=$(shell basename $$(pwd))
PREFIXTARGET::=$(PREFIX)/bin/$(TARGET)
endif

all: $(TARGET) $(or $(SOTARGET))

clean:
	rm -r $(TARGET) $(or $(SOTARGET)) $(OBJDIR) $(DEPDIR) $(or $(PICDIR))

install: all
	install -CD $(TARGET) $(PREFIXTARGET)
ifeq ($(NOMAIN), 1)
	install -CD $(SOTARGET) $(PREFIXSOTARGET)
endif
ifneq ($(INCLUDE),)
	install -CD $(INCLUDE) $(PREFIXINCLUDE)
endif

uninstall:
	rm $(PREFIXTARGET) $(or $(PREFIXSOTARGET)) $(or $(PREFIXINCLUDE))

$(SOURCES:%.$(SRCEXT)=$(DEPDIR)/%.$(DEPEXT)): $(DEPDIR)/%.$(DEPEXT): %.$(SRCEXT)
	@mkdir -p $(DEPDIR)
	echo "$(OBJDIR)/$$($(CC) -M $< $(CFLAGS))" | perl -pe 's/([^ ]+) *:/$$1 $(subst /,\/,$@):/' > $@

include $(SOURCES:%.$(SRCEXT)=$(DEPDIR)/%.$(DEPEXT))

$(SOURCES:%.$(SRCEXT)=$(OBJDIR)/%.$(OBJEXT)): $(OBJDIR)/%.$(OBJEXT): %.$(SRCEXT) $(DEPDIR)/%.$(DEPEXT)
	@mkdir -p $(OBJDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

ifeq ($(NOMAIN), 1)
$(SOURCES:%.$(SRCEXT)=$(PICDIR)/%.$(OBJEXT)): $(PICDIR)/%.$(OBJEXT): %.$(SRCEXT) $(DEPDIR)/%.$(DEPEXT)
	@mkdir -p $(PICDIR)
	$(CC) -c $< -o $@ $(CFLAGS) -fPIC

$(TARGET): $(SOURCES:%.$(SRCEXT)=$(OBJDIR)/%.$(OBJEXT))
	ar rcs $@ $^

$(SOTARGET): $(SOURCES:%.$(SRCEXT)=$(PICDIR)/%.$(OBJEXT))
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS) -shared
else
$(TARGET): $(SOURCES:%.$(SRCEXT)=$(OBJDIR)/%.$(OBJEXT))
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
endif

.PHONY: all clean install uninstall
