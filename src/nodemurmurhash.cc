#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include "nodemurmurhash.h"
#include "inputdata.h"
#include "MurmurHash2.h"
#include "MurmurHash3.h"
#include "murmurhashutils.h"
#include "asyncworker.h"

#ifdef NODE_MURMURHASH_DEFAULT_32BIT
#  define MurmurHash2_64  MurmurHash2_x86_64
#  define MurmurHash3_128 MurmurHash3_x86_128
#  define MurmurHash3_128Length 4
#  define MurmurHash3_128ValueType uint32_t
#else
#  define MurmurHash2_64  MurmurHash2_x64_64
#  define MurmurHash3_128 MurmurHash3_x64_128
#  define MurmurHash3_128Length 2
#  define MurmurHash3_128ValueType uint64_t
#endif

NAN_INLINE void MurmurHash2_x64_64 (
    const void * key, int len, uint32_t seed, void * out)
{
  *(uint64_t *)out = MurmurHash64A( key, len, (uint64_t) seed );
}

NAN_INLINE void MurmurHash2_x86_64 (
    const void * key, int len, uint32_t seed, void * out)
{
  *(uint64_t *)out = MurmurHash64B( key, len, (uint64_t) seed );
}

namespace MurmurHash {
  using v8::Uint32;
  using v8::Function;

  #define GET_ARG_OFFSET(INFO,INDEX,ARGC)                        \
            ((INDEX) + 1 < (ARGC)                                \
            ? Nan::To<int32_t>((INFO)[(INDEX) + 1]).FromMaybe(0) \
            : 0)

  #define GET_ARG_LENGTH(INFO,INDEX,ARGC,DEF)              \
            ((INDEX) + 2 < (ARGC)                          \
            ? Nan::To<int32_t>((INFO)[(INDEX) + 2]).       \
                      FromMaybe(static_cast<int32_t>(DEF)) \
            : (DEF))

  /**
   * Calculate MurmurHash from data
   * 
   * murmurHash(data[, callback])
   * murmurHash(data, output[, offset[, length]][, callback])
   * murmurHash(data{String}, encoding|output_type[, callback])
   * murmurHash(data, output_type[, callback])
   * murmurHash(data, seed[, output[, offset[, length]]][, callback])
   * murmurHash(data, seed[, output_type][, callback])
   * murmurHash(data, output_type, seed[, callback])
   * murmurHash(data{String}, encoding, output[, offset[, length]][, callback])
   * murmurHash(data{String}, encoding, output_type[, callback])
   * murmurHash(data{String}, encoding, seed[, output[, offset[, length]]][, callback])
   * murmurHash(data{String}, encoding, seed[, output_type][, callback])
   * 
   * @param {string|Buffer} data - a byte-string to calculate hash from
   * @param {string} encoding - data string encoding, should be:
   *       'utf8', 'ucs2', 'ascii', 'hex', 'base64' or 'binary';
   *       'binary' by default
   * @param {Uint32} seed - murmur hash seed, 0 by default
   * @param {Buffer} output - a Buffer object to write hash bytes to;
   *       the same object will be returned
   *       the order of output bytes is platform dependent
   * @param {number} offset - start writing into output at offset byte;
   *       negative offset starts from the end of the output buffer
   * @param {number} length - a number of bytes to write from calculated hash;
   *       negative length starts from the end of the hash;
   *       if absolute value of length is greater than the size of a calculated
   *       hash, bytes are written only up to the hash size
   * @param {string} output_type - a string indicating return type:
   *       'number' - for murmurHash32 an unsigned 32-bit integer,
   *                  other hashes - hexadecimal string
   *       'hex'    - hexadecimal string
   *       'base64' - base64 string
   *       'binary' - binary string
   *       'buffer' - a new Buffer object;
   *       'number' by default
   * @param {Function} callback - optional callback(err, result)
   *       if provided the hash will be calculated asynchronously using libuv
   *       worker queue, the return value in this instance will be `undefined`
   *       and the result will be provided to the callback function;
   *       Be carefull as reading and writing by multiple threads to the same
   *       memory may render undetermined results
   * 
   * The order of bytes written to a Buffer is platform independent.
   * 
   * `data` and `output` arguments might reference the same Buffer object
   * or buffers referencing the same memory (views).
   * 
   * @return {number|Buffer|String|undefined}
  **/
  template<MurmurHashFunctionType HashFunction, typename HashValueType, int32_t HashLength>
  NAN_METHOD(MurmurHash)
  {
    InputData data;

    OutputType outputType( DefaultOutputType );

    uint32_t seed = 0;

    /* parse args */
    int argc = std::min( 7, info.Length() );
    int outputTypeIndex = argc;
    int callbackIndex = -1;
    bool validEncoding = true;
    enum Nan::Encoding encoding = Nan::BUFFER;

    if ( argc > 0 && info[argc - 1]->IsFunction() ) {
      callbackIndex = --argc;
    }

    if ( argc > 0 ) {

      if ( info[0]->IsString() ) {
        encoding = Nan::BINARY;
      }

      if ( argc >= 2 ) {

        if ( info[1]->IsString() ) { // input_encoding or output_type
          InputData::ReadEncodingString( info[1].As<String>() );
          if ( argc == 2 ) { // try output_type
            outputType = InputData::DetermineOutputType();
            switch(outputType) {
              case HexStringOutputType:
              case BinaryStringOutputType:
              case Base64StringOutputType:
                // ambiguous if input is string
              case UnknownOutputType: // input_encoding
                if (encoding != Nan::BUFFER) {
                  validEncoding = InputData::DetermineEncoding( encoding );
                  outputType = DefaultOutputType; // revert to default
                }
                break;
              default:
                void(0); // unambiguous - "number" or "buffer"
            }
          } else if (encoding == Nan::BUFFER) { // output_type
            outputType = InputData::DetermineOutputType();
            if ( info[2]->IsNumber() ) seed = Nan::To<uint32_t>(info[2]).FromMaybe(0U);
          } else { // try input_encoding
            if ( !(validEncoding = InputData::DetermineEncoding( encoding )) ) {
              outputType = InputData::DetermineOutputType(); // try output_type
              if (outputType == UnknownOutputType) {
                outputType = DefaultOutputType;
              } else {
                validEncoding = true;
              if ( info[2]->IsNumber() ) seed = Nan::To<uint32_t>(info[2]).FromMaybe(0U);
              }
            }
            outputTypeIndex = 2; // continue from 2
          }
        } else {
          // output or seed
          if ( node::Buffer::HasInstance(info[1]) ) {
            outputType = ProvidedBufferOutputType;
            outputTypeIndex = 1;
          } else {
            if ( info[1]->IsNumber() ) seed = Nan::To<uint32_t>(info[1]).FromMaybe(0U);
            outputTypeIndex = 2; // continue from 2
          }
        }
        if ( outputType == DefaultOutputType ) { // output_type or output or seed
          for (; outputTypeIndex < argc; ++outputTypeIndex ) {
            if ( info[outputTypeIndex]->IsNumber() ) {
              seed = Nan::To<uint32_t>(info[outputTypeIndex]).FromMaybe(0U);
            } else if ( info[outputTypeIndex]->IsString() ) {
              InputData::ReadEncodingString( info[outputTypeIndex].As<String>() );
              outputType = InputData::DetermineOutputType();
              break;
            } else if ( node::Buffer::HasInstance(info[outputTypeIndex]) ) {
              outputType = ProvidedBufferOutputType;
              break;
            } else
              break;
          }
        }

      }

    }


    if ( callbackIndex > -1 ) {
      MurmurHashWorker<HashFunction,HashValueType,HashLength> *asyncWorker;
      Nan::Callback *callback = new Nan::Callback(
                                  Local<Function>::Cast(info[callbackIndex]));

      if ( argc > 0 ) {
        asyncWorker = new MurmurHashWorker<HashFunction,HashValueType,HashLength>(
          callback, outputType, seed, info[0], encoding, validEncoding);
      } else {
        asyncWorker = new MurmurHashWorker<HashFunction,HashValueType,HashLength>(callback);
      }

      if (outputType == ProvidedBufferOutputType) {
        asyncWorker->SaveOutputBuffer(
            info[outputTypeIndex],
            GET_ARG_OFFSET(info, outputTypeIndex, argc),
            GET_ARG_LENGTH(info, outputTypeIndex, argc, HashSize));
      }

      Nan::AsyncQueueWorker(asyncWorker);

      info.GetReturnValue().Set(Nan::Undefined());

    } else {

      if ( argc > 0 ) {
        data.Setup( info[0], encoding, validEncoding );
      }

      if ( ! data.IsValid() )
        return Nan::ThrowTypeError(data.Error());

      Local<Value> result;

      HashValueType hash[HashLength];

      HashFunction( (const void *) *data, (int) data.length(), seed, (void *)hash );

      switch(outputType) {
        case DefaultOutputType:
        case NumberOutputType:
          if (HashSize == sizeof(uint32_t)) {
            result = Nan::New<Uint32>( (uint32_t) (*hash) );
          } else {
            result = HashToHexString<HashLength>( hash );
          }
          break;

        case HexStringOutputType:
          result = HashToHexString<HashLength>( hash );
          break;

        case BinaryStringOutputType:
          result = HashToEncodedString<HashLength>( hash, Nan::BINARY );
          break;

        case Base64StringOutputType:
          result = HashToEncodedString<HashLength>( hash, Nan::BASE64 );
          break;

        case BufferOutputType:
          result = Nan::NewBuffer( HashSize ).ToLocalChecked();
          WriteHashBytes<HashLength>(hash, (uint8_t *) node::Buffer::Data(result));
          break;

        case ProvidedBufferOutputType:
          result = info[outputTypeIndex];
          WriteHashToBuffer<HashLength>(
                hash,
                node::Buffer::Data(result),
                (int32_t) node::Buffer::Length(result),
                GET_ARG_OFFSET(info, outputTypeIndex, argc),
                GET_ARG_LENGTH(info, outputTypeIndex, argc, HashSize));
          break;

        default:
          return Nan::ThrowTypeError("Unknown output type: should be \"number\", \"buffer\", \"binary\", \"base64\" or \"hex\"");
      }

      info.GetReturnValue().Set(result);
    }
  }

  #undef GET_ARG_OFFSET
  #undef GET_ARG_LENGTH

  NAN_MODULE_INIT(Init)
  {
    Nan::SetMethod(target, "murmurHash",       MurmurHash<MurmurHash3_x86_32 , uint32_t, 1>);
    Nan::SetMethod(target, "murmurHash32",     MurmurHash<MurmurHash3_x86_32 , uint32_t, 1>);
    Nan::SetMethod(target, "murmurHash64x64",  MurmurHash<MurmurHash2_x64_64 , uint64_t, 1>);
    Nan::SetMethod(target, "murmurHash64x86",  MurmurHash<MurmurHash2_x86_64 , uint64_t, 1>);
    Nan::SetMethod(target, "murmurHash64",     MurmurHash<MurmurHash2_64     , uint64_t, 1>);
    Nan::SetMethod(target, "murmurHash128x64", MurmurHash<MurmurHash3_x64_128, uint64_t, 2>);
    Nan::SetMethod(target, "murmurHash128x86", MurmurHash<MurmurHash3_x86_128, uint32_t, 4>);
    Nan::SetMethod(target, "murmurHash128",    MurmurHash<MurmurHash3_128    , MurmurHash3_128ValueType
                                                                                       , MurmurHash3_128Length>);
  }

}

NODE_MODULE(murmurhash, MurmurHash::Init)
