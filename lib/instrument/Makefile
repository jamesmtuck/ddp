
CXX  = clang++
CC = clang
CXXFLAGS = `llvm-config --cflags --ldflags --libs --system-libs` -std=c++11 -g -O2 -w
CFLAGS = `llvm-config --cflags --ldflags --libs --system-libs` -g -O2 -w

SOURCES = SetProfiler.cpp Instrument.cpp BuildSignatureAPI.cpp BuildSignature.cpp GenerateQueries.cpp SetAssign.cpp ProfileDBHelper.cpp ProfilerDatabase.cpp SQLite3Helper.cpp 
SOURCE = sqlite3.c

OBJS += $(SOURCES:%.cpp=%.o)
OBJS2 += $(SOURCE:%.c=%.o)
SHARED_LIB = SetProfiler.so
EXEC = SetProf

all: $(OBJS) $(OBJS2)
	$(CXX) -shared -o $(SHARED_LIB) $(OBJS) $(OBJS2) -w -g -O2 
	#$(CXX) -o $(EXEC) $(OBJS) $(OBJS2) -w -g -O2

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@ -fPIC -fno-rtti -I../include/

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@ -fPIC -fno-rtti -I../include/

clean:
	rm -r *.so *.o
