#----------------------------------------------------------------------------
# Macros
#----------------------------------------------------------------------------

# Compiler to use
CC := gcc
CXX := g++

# Object, c, and cpp file extensions
OBJEXT := o
SRCEXTC := c
SRCEXTCPP := cpp

#----------------------------------------------------------------------------
# Setting of target
#----------------------------------------------------------------------------

# Source file directories
SRCDIR := $(PXI_CNTLR_HOME)/src

# Directory of executable
BINDIR := $(PXI_CNTLR_HOME)/bin

# Include header file directories
usrLocalInc := /usr/local/include
INC.main := -I $(PXI_CNTLR_HOME)/include  \
    -I $(PXI_CNTLR_HOME)/include/interface
INC.userlocal := -I $(usrLocalInc)
INC.glib := -I /usr/include/glib-2.0 -I /usr/lib64/glib-2.0/include
INC := $(INC.main) $(INC.userlocal) $(INC.glib)

# Compiler flags
CPPFLAGS := -fPIC -g -D_REENTRANT -Wall -Wno-write-strings
COMPILE.c := $(CC) $(CPPFLAGS) $(INC) -c
COMPILE.cc := $(CXX) $(CPPFLAGS) -std=c++1y $(INC) -c

# Dynamic library flags
LDLIBDIR.yaml := /usr/local/lib
LDFLAGS := -L $(LDLIBDIR.yaml)

# Dynamic libraries
LDLIBS.yaml := -lyaml -Wl,-rpath,$(LDLIBDIR.yaml)
LDLIBS.glib := -lglib-2.0
LDLIBS.mq := -lrt
LDLIBS := $(LDLIBS.yaml) $(LDLIBS.glib) $(LDLIBS.mq)

#----------------------------------------------------------------------------
# Setting of test
#----------------------------------------------------------------------------

# Source file directories
SRCDIRTEST := $(PXI_CNTLR_HOME)/tests

# Built object file directory
BUILDDIRTEST := $(PXI_CNTLR_HOME)/build/tests

# Target executable
TARGET := $(BINDIR)/test

# Include header file directories
INC.googletest := -I $(usrLocalInc)/gtest -I $(usrLocalInc)/gmock
INCTEST := $(INC.main) $(INC.googletest)

# Dynamic library flags
LDFLAGSTEST.googletest := -L /usr/local/lib64
LDFLAGSTEST := $(LDFLAGS) $(LDFLAGSTEST.googletest)

# Dynamic libraries
LDLIBSTEST.gtest := -lgtest -lpthread
LDLIBSTEST.gcov := -lgcov
LDLIBSTEST := $(LDLIBS) $(LDLIBSTEST.gtest) $(LDLIBSTEST.gcov)

# Code coverage by gcov
GCOV_FLAGS := -fprofile-arcs -ftest-coverage

# Object files
SOURCESTEST := $(shell find $(SRCDIRTEST) -type f -name '*.$(SRCEXTCPP)')
OBJECTSTEST := $(patsubst $(SRCDIRTEST)/%,$(BUILDDIRTEST)/%,$(SOURCESTEST:.$(SRCEXTCPP)=.$(OBJEXT)))

# Auxiliary object files (c/c++ code, the c code will be compiled by g++)
SOURCESTESTAUX := $(shell find $(SRCDIR) -type f -name '*.$(SRCEXTC)')
OBJECTSTESTAUX := $(patsubst $(SRCDIR)/%,$(BUILDDIRTEST)/%,$(SOURCESTESTAUX:.$(SRCEXTC)=.$(OBJEXT)))

#----------------------------------------------------------------------------
# Build the target and test
#----------------------------------------------------------------------------

# Test target
$(TARGET): $(OBJECTSTEST) $(OBJECTSTESTAUX)
	$(CXX) $^ -o $(TARGET) $(LDFLAGSTEST) $(LDLIBSTEST)

# Compile the test .cpp code
$(BUILDDIRTEST)/%.$(OBJEXT): $(SRCDIRTEST)/%.$(SRCEXTCPP)
	$(COMPILE.cc) $(INCTEST) -o $@ $<

# Compile the .c code
$(BUILDDIRTEST)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXTC)
	$(COMPILE.c) $(INC) -o $@ $< $(GCOV_FLAGS)

.PHONY: all clean

all: $(TARGET)
# all: info

clean:
	@echo "Cleaning..."
	-$(RM) $(TARGET)
	-$(RM) $(BINDIR)/*.bin
	-$(RM) $(BUILDDIRTEST)/*.$(OBJEXT)
	-$(RM) $(BUILDDIRTEST)/*.gcda
	-$(RM) $(BUILDDIRTEST)/*.gcno
	-$(RM) $(PXI_CNTLR_HOME)/*.log*
	-$(RM) $(BUILDDIRTEST)/*/*.$(OBJEXT)
	-$(RM) $(BUILDDIRTEST)/*/*.gcda
	-$(RM) $(BUILDDIRTEST)/*/*.gcno
	-$(RM) $(PXI_CNTLR_HOME)/*/*.log*
