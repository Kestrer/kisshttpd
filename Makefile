# c.mk 2.1.0

include config.mk

NOMAIN::=${shell pcre2grep -Mr 'int\s+main\s*\(' . >/dev/null 2>&1; echo $$?}

BUILDDIR=build
DEPDIR::=$(BUILDDIR)/deps
OBJDIR::=$(BUILDDIR)/objects

CSOURCES::=$(shell find . -name '*.c')
CXXSOURCES::=$(shell find . -name '*.cpp')
ifeq ($(NOMAIN), 1)
PICDIR::=$(BUILDDIR)/pic
BASENAME::=$(notdir $(CURDIR))
TARGET::=lib$(BASENAME).a
SOTARGET::=lib$(BASENAME).so
PREFIXTARGET::=$(PREFIX)/lib/$(TARGET)
PREFIXSOTARGET::=$(PREFIX)/lib/$(SOTARGET)
ifneq ("$(wildcard $(BASENAME).h)","")
INCLUDE::=$(BASENAME).h
PREFIXINCLUDE::=$(PREFIX)/include/$(INCLUDE)
endif
ifneq ("$(wildcard $(BASENAME).hpp)","")
INCLUDE::=$(BASENAME).hpp
PREFIXINCLUDE::=$(PREFIX)/include/$(INCLUDE)
endif
else
TARGET::=$(notdir $(CURDIR))
PREFIXTARGET::=$(PREFIX)/bin/$(TARGET)
endif

all: $(TARGET) $(or $(SOTARGET))

clean:
	rm -r $(TARGET) $(or $(SOTARGET)) $(OBJDIR) $(DEPDIR) $(or $(PICDIR)) $(BUILDDIR)

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

$(CSOURCES:%.c=$(DEPDIR)/%.dep): $(DEPDIR)/%.dep: %.c
	@mkdir -p $(@D)
	echo "$(OBJDIR)/$$($(CC) -M $< $(CFLAGS))" | perl -pe 's/([^ ]+) *:/$$1 $(subst /,\/,$@):/' > $@

$(CXXSOURCES:%.cpp=$(DEPDIR)/%.dep): $(DEPDIR)/%.dep: %.cpp
	@mkdir -p $(@D)
	echo "$(OBJDIR)/$$($(CXX) -M $< $(CFLAGS) $(CXXFLAGS))" | perl -pe 's/([^ ]+) *:/$$1 $(subst /,\/,$@):/' > $@

include $(CSOURCES:%.c=$(DEPDIR)/%.dep)
include $(CXXSOURCES:%.cpp=$(DEPDIR)/%.dep)

$(CSOURCES:%.c=$(OBJDIR)/%.o): $(OBJDIR)/%.o: %.c $(DEPDIR)/%.dep
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(CXXSOURCES:%.cpp=$(OBJDIR)/%.o): $(OBJDIR)/%.o: %.cpp $(DEPDIR)/%.dep
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -c -o $@ $< 

ifeq ($(NOMAIN), 1)
$(CSOURCES:%.c=$(PICDIR)/%.o): $(PICDIR)/%.o: %.c $(DEPDIR)/%.dep
	@mkdir -p $(@D)
	$(CC) -fPIC $(CFLAGS) -c -o $@ $<

$(CXXSOURCES:%.cpp=$(PICDIR)/%.o): $(PICDIR)/%.o: %.cpp $(DEPDIR)/%.dep
	@mkdir -p $(@D)
	$(CXX) -fPIC $(CFLAGS) $(CXXFLAGS) -c -o $@ $<

$(TARGET): $(CSOURCES:%.c=$(OBJDIR)/%.o) $(CXXSOURCES:%.cpp=$(OBJDIR)/%.o)
	ar rcs $@ $^

$(SOTARGET): $(CSOURCES:%.c=$(PICDIR)/%.o) $(CXXSOURCES:%.cpp=$(PICDIR)/%.o)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS) -shared
else
$(TARGET): $(CSOURCES:%.c=$(OBJDIR)/%.o) $(CXXSOURCES:%.cpp=$(OBJDIR)/%.o)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)
endif

.PHONY: all clean install uninstall
