/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPC_CORE_LIB_COMPRESSION_MESSAGE_COMPRESS_H
#define GRPC_CORE_LIB_COMPRESSION_MESSAGE_COMPRESS_H

#include <grpc/support/port_platform.h>

#include <grpc/slice_buffer.h>

#include "src/core/lib/compression/compression_internal.h"

#include <string>
#include <unordered_map>

//using namespace std;

/* compress 'input' to 'output' using 'algorithm'.
   On success, appends compressed slices to output and returns 1.
   On failure, appends uncompressed slices to output and returns 0. */
int grpc_msg_compress(grpc_message_compression_algorithm algorithm,
                      grpc_slice_buffer* input, grpc_slice_buffer* output);

/* decompress 'input' to 'output' using 'algorithm'.
   On success, appends slices to output and returns 1.
   On failure, output is unchanged, and returns 0. */
int grpc_msg_decompress(grpc_message_compression_algorithm algorithm,
                        grpc_slice_buffer* input, grpc_slice_buffer* output);

/* Copy the input buffer into the output buffer, with no compression. */
int copy(grpc_slice_buffer* input, grpc_slice_buffer* output);

class Compressor {
   // 5 methods
   // - identity / name of algo
   // - lifecycle methods: stop and start
   // - compress message
   // - decompress message

   public:
      // TODO: proper return type for plain strings in this codebase
      virtual std::string encodingType() = 0;

      virtual void start() = 0;

      virtual void stop() = 0;

      virtual int msg_compress(grpc_slice_buffer* input, grpc_slice_buffer* output) = 0;

      virtual int msg_decompress(grpc_slice_buffer* input, grpc_slice_buffer* output) = 0;
};

class CompressorRegistry {
 public:
  static CompressorRegistry& getInstance() {
     static CompressorRegistry instance;
     return instance;
  }

  void register_compressor(Compressor* c);
  Compressor* get_compressor(std::string encoding_type);
  void remove_compressor(std::string encoding_type);

 private:
  CompressorRegistry() {}
  CompressorRegistry(CompressorRegistry const&) = delete;
  void operator=(CompressorRegistry const&) = delete;

  std::unordered_map<std::string, Compressor*> compressors;
};

/* A Compressor that does nothing */
class NullCompressor : public Compressor {
 public:
  std::string encodingType();

  void start();

  void stop();

  int msg_compress(grpc_slice_buffer* input, grpc_slice_buffer* output);

  int msg_decompress(grpc_slice_buffer* input, grpc_slice_buffer* output);
};

#endif /* GRPC_CORE_LIB_COMPRESSION_MESSAGE_COMPRESS_H */
