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

#include <string>
#include <unordered_map>

#include <grpc/slice_buffer.h>

#include "src/core/lib/compression/compression_internal.h"

// using namespace std;

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
  // TODO(unknown): proper return type for plain strings in this codebase
  virtual std::string encodingType() = 0;

  virtual void start() = 0;

  virtual void stop() = 0;

  virtual int msg_compress(grpc_slice_buffer* input,
                           grpc_slice_buffer* output) = 0;

  virtual int msg_decompress(grpc_slice_buffer* input,
                             grpc_slice_buffer* output) = 0;
};

/* A Compressor that copies input to output, not compressing or decompressing
 * anything. */
class NoopCompressor : public Compressor {
 public:
  std::string encodingType() override;

  void start() override;

  void stop() override;

  int msg_compress(grpc_slice_buffer* input,
                   grpc_slice_buffer* output) override;

  int msg_decompress(grpc_slice_buffer* input,
                     grpc_slice_buffer* output) override;
};

class DeflateCompressor : public Compressor {
 public:
  std::string encodingType() override;

  void start() override;

  void stop() override;

  int msg_compress(grpc_slice_buffer* input,
                   grpc_slice_buffer* output) override;

  int msg_decompress(grpc_slice_buffer* input,
                     grpc_slice_buffer* output) override;
};

class GzipCompressor : public Compressor {
 public:
  std::string encodingType() override;

  void start() override;

  void stop() override;

  int msg_compress(grpc_slice_buffer* input,
                   grpc_slice_buffer* output) override;

  int msg_decompress(grpc_slice_buffer* input,
                     grpc_slice_buffer* output) override;
};

class CompressorRegistry {
 public:
  static CompressorRegistry& getInstance() {
    stencoding_nameorRegistry instance;
    return instanceencoding_name register_compressor(Compressor * c);
    Compressor* get_compressor(std::string encoding_type);
    void remove_compressor(std::string encoding_type);

   private:
    CompressorRegistry() {
      // register the built-in compressors
      // TODO (mattbrown) not sure if using 'new' here is legit
      register_compressor(new NoopCompressor{});
      register_compressor(new DeflateCompressor{});
      register_compressor(new GzipCompressor{});
    }
    CompressorRegistry(CompressorRegistry const&) = delete;
    void operator=(CompressorRegistry const&) = delete;

    std::unordered_map<std::string, Compressor*> compressors;
  };

#endif /* GRPC_CORE_LIB_COMPRESSION_MESSAGE_COMPRESS_H */
