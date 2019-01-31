add_library(ddp SHARED Instrument.cpp BuildSignature.cpp BuildSignatureAPI.cpp GenerateQueries.cpp ProfileDBHelper.cpp ProfilerDatabase.cpp SQLite3Helper.cpp SetAssign.cpp SetProfiler.cpp sqlite3.c)

install(TARGETS ddp
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
