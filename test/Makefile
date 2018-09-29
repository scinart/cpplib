BINDIR := .
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
$(shell mkdir -p $(BINDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

## Boost related variables
RELEASE = f
BOOST_LIB_DIR = /usr/lib64
DYN_BOOST_LIB := -lboost_program_options -lboost_system -lboost_timer -lboost_locale -lboost_serialization -lboost_filesystem -lboost_unit_test_framework
STA_BOOST_LIB := libboost_program_options.a libboost_system.a libboost_timer.a libboost_locale.a libboost_serialization.a libboost_filesystem.a
STA_BOOST_LIB := $(foreach d, $(STA_BOOST_LIB), $(BOOST_LIB_DIR)/$d)
## End Boost part
################################################################

INCLUDE := -I../include

SRC := $(wildcard *.cpp) ## all cpp files

DEBUG = t
ifneq ($(DEBUG), false)
endif

EXTRA_CXXFLAGS =
CXXFLAGS += -pipe -std=c++17 -g $(INCLUDE) $(EXTRA_CXXFLAGS)
LDFLAGS += -lrt -ldl -pthread $(DYN_BOOST_LIB)
LIB = ../src/mylib.a

.PHONY: all src clean

.DEFAULT_GOAL :=
all: src main-test test

src :
	cd ../src && $(MAKE)

main-test : $(SRC:%.cpp=%.o)
	$(CXX) -o $(BINDIR)/$@ $(filter %.o,$^) $(LIB) $(LDFLAGS)

test:
	./main-test -t segment_tree_test

testall:
	./main-test --log_level=warning

%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

clean:
	-rm *.o main-test

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRC)))
