#!gmake

############# define MACROS #########################################

# define PROJECT to be the directory name containing this software.  If not
# defined, it defaults to name of this directory.
PROJECT = 

# these are the binaries that get compiled AND installed as public binaries
# in $HEV_SAVG_DIR/bin/*
PUBLIC_BINS = hev-animateWindow

# these are the binaries that get compiled AND installed as private binaries
# in $HEV_SAVG_DIR/etc/$(PROJECT)/bin/*
PRIVATE_BINS =

# these are DSOs that get compiled AND installed as public DSOs
# in $HEV_SAVG_DIR/lib/*
PUBLIC_DSOS = 

# these are the DSOs that get compiled AND installed as private DSOs
# in $HEV_SAVG_DIR/etc/$(PROJECT)/lib/*
PRIVATE_DSOS =

# same thing for scripts that get installed
PUBLIC_SCRIPTS = 
PRIVATE_SCRIPTS =

# these are data files used by the project, and others too
# in $HEV_SAVG_DIR/data
PUBLIC_DATA = 

# these are data files used by the project, private,
# in $HEV_SAVG_DIR/etc/$(PROJECT)/data
PRIVATE_DATA = 

# these are the man pages that get installed in $HEV_SAVG_DIR/man
MAN_PAGES = hev-animateWindow.1


################## makefile.include ##################################
# define CXX, CFLAGS, LIBS, etc.

# creates phoney target build

# creates real target test, and targets for PUBLIC_DSOS, PRIVATE_DSOS,
# PUBLIC_BINS, PRIVATE_BINS, install, clean, cleaner, distclean

# add or modify or make dependencies to them as needed after the include
# statement

include $(HEV_IDEA_DIR)/gnumakefile.include

########################################################################
# this is specific to bullet physics engine
########################################################################

BULLET_HOME = $$HEVROOT/external/bulletphysics/bullet-2.76
BULLETT_LIBS=$(shell find $(BULLET_HOME) -name \*.a | sort -u)
BULLETT_INCLUDES= -g $(BULLET_HOME) $(shell find $(BULLET_HOME) -name \*.h  | grep -v '\c$$' | sed -e 's/\/[^/]*$$//' -e 's,^,-I ,' | sort -u)
CXXFLAGS := $(CXXFLAGS) $(BULLETT_INCLUDES)
LIBS := -g $(LIBS) -lglut $(BULLETT_LIBS) $(BULLETT_LIBS) 



