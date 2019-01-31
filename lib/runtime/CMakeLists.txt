add_library(runtime-static STATIC Instrument.cpp HashTable.cpp sqlite3.c Database.cpp PerfectSet.cpp DumpSet.cpp RangeSet.cpp)
add_library(runtime-shared SHARED Instrument.cpp HashTable.cpp sqlite3.c Database.cpp PerfectSet.cpp DumpSet.cpp RangeSet.cpp)

SET_TARGET_PROPERTIES(runtime-static PROPERTIES OUTPUT_NAME ddprt)
SET_TARGET_PROPERTIES(runtime-shared PROPERTIES OUTPUT_NAME ddprt)

#add_library(runtime32 STATIC Instrument.cpp HashTable.cpp sqlite3.c Database.cpp PerfectSet.cpp DumpSet.cpp RangeSet.cpp)
#target_compile_options(runtime32 PUBLIC -m32)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION")

install(TARGETS runtime-shared runtime-static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
