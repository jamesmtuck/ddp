##===- projects/sample/tools/sample/Makefile ---------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

#
# Give the name of the tool.
#
TOOLNAME=ddp-query

#
# List libraries that we'll need
# We use LIBS because sample is a dynamic library.
#

USEDLIBS = setprof.a profdb.a edgeprof.a instrument.a
LINK_COMPONENTS := bitreader bitwriter asmparser irreader instrumentation scalaropts ipo vectorize 

# Required for using boost::lexical_cast
# Required in whichever file (directly or indirectly) includes the boost/lexical_cast.hpp file
REQUIRES_EH = 1

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

#LIBS += 

