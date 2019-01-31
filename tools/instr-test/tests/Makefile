LIBS = sign.bc -L$(DDP_INSTALL)/lib/ -lruntime

.PHONY: sign.bc all trace

DEFS := SimpleSignature32 SimpleSignature64 SimpleSignature128 SimpleSignature256 ArraySignature_32_32 ArraySignature_32_128 DDPPerfectSet DDPHashTableSet BankedSignature_3x512 BankedSignature_4x256 BankedSignature_3x1024 BankedSignature_2x1024 BankedSignature_2x512 BankedSignature_3x2048 BankedSignature_2x4096 BankedSignature_2x8192 DumpSetBankedSignature_2x8192 RangeAndBankedSignature_2x512 RangeAndBankedSignature_2x1024 RangeAndBankedSignature_2x2048 RangeAndBankedSignature_3x1024 RangeAndBankedSignature_2x4096

TRACE = $(addsuffix _trace,$(DEFS))

all: $(DEFS)
trace: $(TRACE)

$(TRACE): sign.bc
	clang++ -std=c++0x -c -D$(subst _trace,,$@) -o $(addsuffix .o,$@) trace.cpp
	clang++ -o $@ $(addsuffix .o,$@) $(LIBS)

$(DEFS): sign.bc
	clang -c -D$@ -o $(addsuffix .o,$@) simple.c
	clang++ -o $@ $(addsuffix .o,$@) $(LIBS)  

compare: sign.bc
	clang -c -o $(addsuffix .o,$@) compare.c
	clang++ -o $@ $(addsuffix .o,$@) $(LIBS)  

debug: sign.bc
	clang -g -c -o $(addsuffix .o,$@) debug.c
	clang++ -g -o $@ $(addsuffix .o,$@) $(LIBS)  

sign.bc:
	$(DDP_INSTALL)/bin/instr-test -o sign.bc

clean:
	rm -Rf $(DEFS) $(addsuffix .o,$(DEFS)) compare.o compare compare.c~ sign.bc sign.ll Makefile~ simple.c~ $(TRACE) $(addsuffix .o,$(TRACE))
