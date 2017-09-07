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

CXXFLAGS += -pipe -std=c++14 -g $(INCLUDE)
LDFLAGS += -lrt -ldl $(DYN_BOOST_LIB)

.PHONY: all clean

.DEFAULT_GOAL :=
all: main-test test

main-test : $(SRC:%.cpp=%.o)
	$(CXX) -o $(BINDIR)/$@ $(filter %.o,$^) $(LDFLAGS)

test:
	./main-test

%.o : %.cpp
%.o : %.cpp $(DEPDIR)/%.d
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

clean:
	-rm *.o main-test

-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRC)))
