#if !defined(ASYNCWORKER_HEADER)
#define ASYNCWORKER_HEADER

namespace MurmurHash {
  using v8::Local;
  using v8::Value;
  using v8::String;
  using v8::Uint32;

  template<MurmurHashFunctionType HashFunction, typename HashValueType, int32_t HashLength>
  class MurmurHashWorker : public Nan::AsyncWorker {
    public:
        NAN_INLINE MurmurHashWorker(Nan::Callback *callback);
        NAN_INLINE MurmurHashWorker(Nan::Callback *callback, OutputType outputType, uint32_t seed,
                                      Local<Value> key, const enum Nan::Encoding encoding, const bool validEncoding);
        void SaveOutputBuffer(const Local<Value> &buffer, int32_t offset, int32_t length);
        void Execute();
        void HandleOKCallback();
        void HandleErrorCallback();

    private:
        InputData data_;
        OutputType outputType_;
        uint32_t seed_;
        int32_t offset_;
        int32_t length_;
        HashValueType hash_[HashLength];
  };

}

#include "asyncworker_impl.h"

#endif