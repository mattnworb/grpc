#ifndef GRPC_CORE_LIB_COMPRESSION_NULL_COMPRESSOR_H
#define GRPC_CORE_LIB_COMPRESSION_NULL_COMPRESSOR_H

#include "src/core/lib/compression/message_compress.h"

/* A Compressor that does nothing */
class NullCompressor : public Compressor {
 public:
  std::string encodingType();

  void start();

  void stop();

  int msg_compress(grpc_slice_buffer* input, grpc_slice_buffer* output);

  int msg_decompress(grpc_slice_buffer* input, grpc_slice_buffer* output);
};

#endif /* GRPC_CORE_LIB_COMPRESSION_NULL_COMPRESSOR_H */
