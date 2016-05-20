#if !defined(INCREMENTAL_HASHER_HEADER)
#define INCREMENTAL_HASHER_HEADER

#include "nodemurmurhash.h"

namespace MurmurHash {
  using v8::Local;
  using v8::Value;
  using v8::FunctionTemplate;
  using Nan::Persistent;
  using Nan::ObjectWrap;

  NAN_MODULE_INIT(Init);

  typedef uint32_t total_t;

  template<template <typename,int32_t>class H, typename HashValueType, int32_t HashLength>
  class IncrementalHasher : public ObjectWrap {
    public:
      typedef IncrementalHasher<H,HashValueType,HashLength> IncrementalHasher_T;
      typedef uint32_t checksum_t;

      static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE& target,
                                 const char* name, const char *altname = NULL);

      static NAN_METHOD(New);
      static NAN_METHOD(Serialize);
      static NAN_METHOD(Copy);
      static NAN_METHOD(Update);
      static NAN_METHOD(Digest);
      static NAN_GETTER(GetTotal);

      static Persistent<FunctionTemplate> constructor;
    private:
      NAN_INLINE IncrementalHasher(const uint32_t seed = 0U);
      NAN_INLINE IncrementalHasher(const uint8_t *serial);
      NAN_INLINE IncrementalHasher(const IncrementalHasher_T& other);
      NAN_INLINE void operator=(const IncrementalHasher_T&);
      NAN_INLINE void Serialize(uint8_t *serial);
      NAN_INLINE void Update(void *data, uint32_t length);
      NAN_INLINE void Digest(HashValueType *hash);
      static NAN_INLINE bool IsSerialTypeValid(uint8_t *serial);

      H<HashValueType,HashLength> hasher;
      total_t total;

      #define BASE64_ENCODED_SIZE(size) ((size + 2 - ((size + 2) % 3)) / 3 * 4)

      static const uint32_t kHashSerialTotalIndex = HashSize + HashSize;
      static const checksum_t kHashSerialCkSeed = 0xDEADBACA;
      static const uint32_t kHashSerialCkSize = 3;
      static const checksum_t kHashSerialCkMask = static_cast<checksum_t>((1LLU << (kHashSerialCkSize * 8)) - 1);
      static const uint32_t kHashSerialCkIndex = kHashSerialTotalIndex + sizeof(total_t);
      static const uint32_t kHashSerialSize = kHashSerialCkIndex + kHashSerialCkSize;
      static const uint32_t kHashSerialStringSize = BASE64_ENCODED_SIZE(kHashSerialSize);
      /*
       Serial data in network byte order
                            0: hstate[MSByte] ... hstate[LSByte]
                    +HashSize:  carry[MSByte] ... carry[LSByte]
                    +HashSize:  total[MSByte] ... total[LSByte]
               +sizeof(total):  chksum = ((ck[0] ^ chksum >>24)<<16) | ck[1]<<8 | ck[2]
      */
      static const uint8_t  kHashSerialType = static_cast<uint8_t>((0x0F ^ HashLength ^ sizeof(HashValueType)) << 4);
      static const uint8_t  kHashSerialTypeMask = static_cast<uint8_t>(0xF0 | (0x10 - HashSize));
      static const uint32_t kHashSerialTypeIndex = kHashSerialTotalIndex - 1;
      /*
                               kHashSerialType           kHashSerialTypeMask
              MurmurHash3A     0b101000nn 15 ^ 1 ^ 4 = 0xA0  0xF0 | 0x10 - 4  = 0xFC
              MurmurHash128x64 0b0101nnnn 15 ^ 2 ^ 8 = 0x50  0xF0 | 0x10 - 16 = 0xF0
              MurmurHash128x86 0b1111nnnn 15 ^ 4 ^ 4 = 0xF0  0xF0 | 0x10 - 16 = 0xF0
              MurmurHash64x64  0b01100nnn 15 ^ 1 ^ 8 = 0x60  0xF0 | 0x10 - 8  = 0xF8
              MurmurHash64x86  0b10010nnn 15 ^ 2 ^ 4 = 0x90  0xF0 | 0x10 - 8  = 0xF8
      */
      #undef BASE64_ENCODED_SIZE
  };

}

#endif