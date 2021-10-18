// Copyright 2021 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <grpc/impl/codegen/port_platform.h>

//#include "src/core/lib/compression/message_compress.h"
#include "src/core/lib/compression/null_compressor.h"
//#include <string>

std::string NullCompressor::encodingType() { return "null"; }

void NullCompressor::start() {}

void NullCompressor::stop() {}

int NullCompressor::msg_compress(grpc_slice_buffer* input,
                                 grpc_slice_buffer* output) {
  return copy(input, output);
}

int NullCompressor::msg_decompress(grpc_slice_buffer* input,
                                   grpc_slice_buffer* output) {
  return copy(input, output);
}
