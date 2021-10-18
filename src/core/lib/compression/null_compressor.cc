//#include "src/core/lib/compression/message_compress.h"
#include "src/core/lib/compression/null_compressor.h"
//#include <string>


std::string NullCompressor::encodingType() { return "null"; }

void NullCompressor::start() {}

void NullCompressor::stop() {}

int NullCompressor::msg_compress(grpc_slice_buffer* input, grpc_slice_buffer* output) {
  return copy(input, output);
}

int NullCompressor::msg_decompress(grpc_slice_buffer* input, grpc_slice_buffer* output) {
  return copy(input, output);
}
