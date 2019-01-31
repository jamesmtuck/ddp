
typedef int* Signature;

#ifdef __cplusplus
#define DeclareFns(Type,Name)  \
  const char *SigName = #Name; \
  extern "C" {\
  Type Name##AllocateFn(); \
  void Name##InsertFn(Type,int8_t*); \
  int Name##MembershipFn(Type,int8_t*); \
  }\
  Type Allocate() { return Name##AllocateFn(); } \
  void Insert(Type t,int8_t* p) { Name##InsertFn(t,p); } \
  int Membership(Type t, int8_t*p) { return Name##MembershipFn(t,p); }
#else
#define DeclareFns(Type,Name)  \
  const char *SigName = #Name; \
  Type Name##AllocateFn(); \
  void Name##InsertFn(Type,int8_t*); \
  int Name##MembershipFn(Type,int8_t*); \
  Type Allocate() { return Name##AllocateFn(); } \
  void Insert(Type t,int8_t* p) { Name##InsertFn(t,p); } \
  int Membership(Type t, int8_t*p) { return Name##MembershipFn(t,p); }
#endif

#if (defined SimpleSignature32)
DeclareFns(Signature,SimpleSignature32)
#define LIMIT (32/32)
#elif (defined SimpleSignature64)
DeclareFns(Signature,SimpleSignature64)
#define LIMIT (64/32)
#elif (defined SimpleSignature128)
DeclareFns(Signature,SimpleSignature128)
#define LIMIT (128/32)
#elif (defined SimpleSignature256)
DeclareFns(Signature,SimpleSignature256)
#define LIMIT (256/32)
#elif (defined SimpleSignature1024)
#define LIMIT (1024/32)
DeclareFns(Signature,SimpleSignature1024)
#elif (defined ArraySignature_32_32)
#define LIMIT (1024/32)
DeclareFns(Signature,ArraySignature_32_32)
#elif (defined ArraySignature_32_128)
#define LIMIT (32*128/32)
DeclareFns(Signature,ArraySignature_32_128)
#elif (defined DDPPerfectSet)
#define LIMIT 1
DeclareFns(Signature,DDPPerfectSet)
#elif (defined DDPHashTableSet)
#define LIMIT 1
DeclareFns(Signature,DDPHashTableSet)
#elif (defined BankedSignature_3x512)
#define LIMIT (3*512/32)
DeclareFns(Signature,BankedSignature_3x512)
#elif (defined BankedSignature_2x512)
#define LIMIT (2*512/32)
DeclareFns(Signature,BankedSignature_2x512)
#elif (defined BankedSignature_3x1024)
#define LIMIT (3*1024/32)
DeclareFns(Signature,BankedSignature_3x1024)
#elif (defined BankedSignature_3x2048)
#define LIMIT (3*2048/32)
DeclareFns(Signature,BankedSignature_3x2048)
#elif (defined BankedSignature_2x1024)
#define LIMIT (2*1024/32)
DeclareFns(Signature,BankedSignature_2x1024)

#elif (defined BankedSignature_2x4096)
#define LIMIT (2*4096/32)
DeclareFns(Signature,BankedSignature_2x4096)

#elif (defined BankedSignature_2x8192)
#define LIMIT (2*8192/32)
DeclareFns(Signature,BankedSignature_2x8192)

#elif (defined BankedSignature_4x256)
#define LIMIT (4*256/32)
DeclareFns(Signature,BankedSignature_4x256)

#elif (defined DumpSetBankedSignature_2x8192)
#define LIMIT (2*8192/32)
DeclareFns(Signature,DumpSet_BankedSignature_2x8192)

#elif (defined RangeAndBankedSignature_2x512)
#define LIMIT (2*512/32)
DeclareFns(Signature,RangeAndBankedSignature_2x512)

#elif (defined RangeAndBankedSignature_2x1024)
#define LIMIT (2*1024/32)
DeclareFns(Signature,RangeAndBankedSignature_2x1024)

#elif (defined RangeAndBankedSignature_2x2048)
#define LIMIT (2*2048/32)
DeclareFns(Signature,RangeAndBankedSignature_2x2048)

#elif (defined RangeAndBankedSignature_3x1024)
#define LIMIT (3*1024/32)
DeclareFns(Signature,RangeAndBankedSignature_3x1024)

#elif (defined RangeAndBankedSignature_2x4096)
#define LIMIT (2*4096/32)
DeclareFns(Signature,RangeAndBankedSignature_2x4096)

#else
#define LIMIT 1
DeclareFns(Signature,SimpleSignature32)
#endif
