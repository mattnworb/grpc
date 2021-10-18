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

#include "src/core/lib/compression/message_compress.h"

#include <stdlib.h>
#include <string.h>

#include <grpc/grpc.h>
#include <grpc/support/log.h>

#include "src/core/lib/gpr/murmur_hash.h"
#include "src/core/lib/gpr/string.h"
#include "src/core/lib/gpr/useful.h"
#include "src/core/lib/iomgr/exec_ctx.h"
#include "test/core/util/slice_splitter.h"
#include "test/core/util/test_config.h"

typedef enum { ONE_A = 0, ONE_KB_A, ONE_MB_A, TEST_VALUE_COUNT } test_value;

typedef enum {
  SHOULD_NOT_COMPRESS,
  SHOULD_COMPRESS,
  MAYBE_COMPRESSES
} compressability;

static void assert_passthrough(grpc_slice value,
                               grpc_message_compression_algorithm algorithm,
                               grpc_slice_split_mode uncompressed_split_mode,
                               grpc_slice_split_mode compressed_split_mode,
                               compressability compress_result_check) {
  grpc_slice_buffer input;
  grpc_slice_buffer compressed_raw;
  grpc_slice_buffer compressed;
  grpc_slice_buffer output;
  grpc_slice final;
  int was_compressed;
  const char* algorithm_name;

  GPR_ASSERT(
      grpc_message_compression_algorithm_name(algorithm, &algorithm_name) != 0);
  gpr_log(GPR_INFO,
          "assert_passthrough: value_length=%" PRIuPTR
          " value_hash=0x%08x "
          "algorithm='%s' uncompressed_split='%s' compressed_split='%s'",
          GRPC_SLICE_LENGTH(value),
          gpr_murmur_hash3(GRPC_SLICE_START_PTR(value),
                           GRPC_SLICE_LENGTH(value), 0),
          algorithm_name, grpc_slice_split_mode_name(uncompressed_split_mode),
          grpc_slice_split_mode_name(compressed_split_mode));

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&compressed_raw);
  grpc_slice_buffer_init(&compressed);
  grpc_slice_buffer_init(&output);

  grpc_split_slices_to_buffer(uncompressed_split_mode, &value, 1, &input);

  {
    grpc_core::ExecCtx exec_ctx;
    was_compressed = grpc_msg_compress(algorithm, &input, &compressed_raw);
  }
  GPR_ASSERT(input.count > 0);

  switch (compress_result_check) {
    case SHOULD_NOT_COMPRESS:
      GPR_ASSERT(was_compressed == 0);
      break;
    case SHOULD_COMPRESS:
      GPR_ASSERT(was_compressed == 1);
      break;
    case MAYBE_COMPRESSES:
      /* no check */
      break;
  }

  grpc_split_slice_buffer(compressed_split_mode, &compressed_raw, &compressed);

  {
    grpc_core::ExecCtx exec_ctx;
    GPR_ASSERT(grpc_msg_decompress(
        was_compressed ? algorithm : GRPC_MESSAGE_COMPRESS_NONE, &compressed,
        &output));
  }

  final = grpc_slice_merge(output.slices, output.count);
  GPR_ASSERT(grpc_slice_eq(value, final));

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&compressed);
  grpc_slice_buffer_destroy(&compressed_raw);
  grpc_slice_buffer_destroy(&output);
  grpc_slice_unref(final);
}

static grpc_slice repeated(char c, size_t length) {
  grpc_slice out = grpc_slice_malloc(length);
  memset(GRPC_SLICE_START_PTR(out), c, length);
  return out;
}

static compressability get_compressability(
    test_value id, grpc_message_compression_algorithm algorithm) {
  if (algorithm == GRPC_MESSAGE_COMPRESS_NONE) return SHOULD_NOT_COMPRESS;
  switch (id) {
    case ONE_A:
      return SHOULD_NOT_COMPRESS;
    case ONE_KB_A:
    case ONE_MB_A:
      return SHOULD_COMPRESS;
    case TEST_VALUE_COUNT:
      abort();
  }
  return MAYBE_COMPRESSES;
}

static grpc_slice create_test_value(test_value id) {
  switch (id) {
    case ONE_A:
      return grpc_slice_from_copied_string("a");
    case ONE_KB_A:
      return repeated('a', 1024);
    case ONE_MB_A:
      return repeated('a', 1024 * 1024);
    case TEST_VALUE_COUNT:
      abort();
  }
  return grpc_slice_from_copied_string("bad value");
}

static void test_tiny_data_compress(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer output;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&output);
  grpc_slice_buffer_add(&input, create_test_value(ONE_A));

  for (int i = 0; i < GRPC_MESSAGE_COMPRESS_ALGORITHMS_COUNT; i++) {
    if (i == GRPC_MESSAGE_COMPRESS_NONE) continue;
    grpc_core::ExecCtx exec_ctx;
    GPR_ASSERT(0 == grpc_msg_compress(

                        static_cast<grpc_message_compression_algorithm>(i),
                        &input, &output));
    GPR_ASSERT(1 == output.count);
  }

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&output);
}

static void test_bad_decompression_data_crc(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer corrupted;
  grpc_slice_buffer output;
  size_t idx;
  const uint32_t bad = 0xdeadbeef;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&corrupted);
  grpc_slice_buffer_init(&output);
  grpc_slice_buffer_add(&input, create_test_value(ONE_MB_A));

  grpc_core::ExecCtx exec_ctx;
  /* compress it */
  grpc_msg_compress(GRPC_MESSAGE_COMPRESS_GZIP, &input, &corrupted);
  /* corrupt the output by smashing the CRC */
  GPR_ASSERT(corrupted.count > 1);
  GPR_ASSERT(GRPC_SLICE_LENGTH(corrupted.slices[1]) > 8);
  idx = GRPC_SLICE_LENGTH(corrupted.slices[1]) - 8;
  memcpy(GRPC_SLICE_START_PTR(corrupted.slices[1]) + idx, &bad, 4);

  /* try (and fail) to decompress the corrupted compresed buffer */
  GPR_ASSERT(0 == grpc_msg_decompress(GRPC_MESSAGE_COMPRESS_GZIP, &corrupted,
                                      &output));

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&corrupted);
  grpc_slice_buffer_destroy(&output);
}

static void test_bad_decompression_data_missing_trailer(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer decompressed;
  grpc_slice_buffer garbage;
  grpc_slice_buffer output;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&decompressed);
  grpc_slice_buffer_init(&garbage);
  grpc_slice_buffer_init(&output);
  grpc_slice_buffer_add(&input, create_test_value(ONE_MB_A));

  grpc_core::ExecCtx exec_ctx;
  /* compress it */
  grpc_msg_compress(GRPC_MESSAGE_COMPRESS_GZIP, &input, &decompressed);
  GPR_ASSERT(decompressed.length > 8);
  /* Remove the footer from the decompressed message */
  grpc_slice_buffer_trim_end(&decompressed, 8, &garbage);
  /* try (and fail) to decompress the compressed buffer without the footer */
  GPR_ASSERT(0 == grpc_msg_decompress(GRPC_MESSAGE_COMPRESS_GZIP, &decompressed,
                                      &output));

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&decompressed);
  grpc_slice_buffer_destroy(&garbage);
  grpc_slice_buffer_destroy(&output);
}

static void test_bad_decompression_data_trailing_garbage(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer output;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&output);
  /* append 0x99 to the end of an otherwise valid stream */
  grpc_slice_buffer_add(
      &input, grpc_slice_from_copied_buffer(
                  "\x78\xda\x63\x60\x60\x60\x00\x00\x00\x04\x00\x01\x99", 13));

  /* try (and fail) to decompress the invalid compresed buffer */
  grpc_core::ExecCtx exec_ctx;
  GPR_ASSERT(
      0 == grpc_msg_decompress(GRPC_MESSAGE_COMPRESS_DEFLATE, &input, &output));

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&output);
}

static void test_bad_decompression_data_stream(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer output;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&output);
  grpc_slice_buffer_add(&input,
                        grpc_slice_from_copied_buffer("\x78\xda\xff\xff", 4));

  /* try (and fail) to decompress the invalid compresed buffer */
  grpc_core::ExecCtx exec_ctx;
  GPR_ASSERT(
      0 == grpc_msg_decompress(GRPC_MESSAGE_COMPRESS_DEFLATE, &input, &output));

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&output);
}

static void test_bad_compression_algorithm(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer output;
  int was_compressed;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&output);
  grpc_slice_buffer_add(
      &input, grpc_slice_from_copied_string("Never gonna give you up"));

  grpc_core::ExecCtx exec_ctx;
  was_compressed = grpc_msg_compress(GRPC_MESSAGE_COMPRESS_ALGORITHMS_COUNT,
                                     &input, &output);
  GPR_ASSERT(0 == was_compressed);

  was_compressed =
      grpc_msg_compress(static_cast<grpc_message_compression_algorithm>(
                            GRPC_MESSAGE_COMPRESS_ALGORITHMS_COUNT + 123),
                        &input, &output);
  GPR_ASSERT(0 == was_compressed);

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&output);
}

static void test_bad_decompression_algorithm(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer output;
  int was_decompressed;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_init(&output);
  grpc_slice_buffer_add(&input,
                        grpc_slice_from_copied_string(
                            "I'm not really compressed but it doesn't matter"));
  grpc_core::ExecCtx exec_ctx;
  was_decompressed = grpc_msg_decompress(GRPC_MESSAGE_COMPRESS_ALGORITHMS_COUNT,
                                         &input, &output);
  GPR_ASSERT(0 == was_decompressed);

  was_decompressed =
      grpc_msg_decompress(static_cast<grpc_message_compression_algorithm>(
                              GRPC_MESSAGE_COMPRESS_ALGORITHMS_COUNT + 123),
                          &input, &output);
  GPR_ASSERT(0 == was_decompressed);

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&output);
}

static void test_compressor_registry(void) {
  CompressorRegistry& registry = CompressorRegistry::getInstance();
  // test that NULL is returned for unregistered compressors, assuming this name
  // is never used
  GPR_ASSERT(registry.get_compressor("fake-should-not-exist") == nullptr);

  // test that the built-in compressors are registered
  GPR_ASSERT(registry.get_compressor("noop") != nullptr);
  GPR_ASSERT(registry.get_compressor("deflate") != nullptr);
  GPR_ASSERT(registry.get_compressor("gzip") != nullptr);
}

static void test_noop_compressor(void) {
  grpc_slice_buffer input;
  grpc_slice_buffer output;
  int was_compressed;

  grpc_slice_buffer_init(&input);
  grpc_slice_buffer_add(&input, grpc_slice_from_copied_string("test value"));

  grpc_slice_buffer_init(&output);

  NoopCompressor null_compressor;
  was_compressed = null_compressor.msg_compress(&input, &output);
  GPR_ASSERT(1 == was_compressed);

  // NoopCompressor just copies input into output, so the values should be the
  // same
  GPR_ASSERT(input.count == output.count);
  // TODO (mattbrown) is there a more conventional way to test equality of two
  // grpc_slice_buffers?
  for (int i = 0; i < input.count; i++) {
    GPR_ASSERT(input.slices[i] == output.slices[i]);
  }

  grpc_slice_buffer_destroy(&input);
  grpc_slice_buffer_destroy(&output);
}

int main(int argc, char** argv) {
  unsigned i, j, k, m;
  grpc_slice_split_mode uncompressed_split_modes[] = {
      GRPC_SLICE_SPLIT_IDENTITY, GRPC_SLICE_SPLIT_ONE_BYTE};
  grpc_slice_split_mode compressed_split_modes[] = {GRPC_SLICE_SPLIT_MERGE_ALL,
                                                    GRPC_SLICE_SPLIT_IDENTITY,
                                                    GRPC_SLICE_SPLIT_ONE_BYTE};

  grpc::testing::TestEnvironment env(argc, argv);
  grpc_init();

  test_compressor_registry();
  test_noop_compressor();

  for (i = 0; i < GRPC_MESSAGE_COMPRESS_ALGORITHMS_COUNT; i++) {
    for (j = 0; j < GPR_ARRAY_SIZE(uncompressed_split_modes); j++) {
      for (k = 0; k < GPR_ARRAY_SIZE(compressed_split_modes); k++) {
        for (m = 0; m < TEST_VALUE_COUNT; m++) {
          grpc_slice slice = create_test_value(static_cast<test_value>(m));
          assert_passthrough(
              slice, static_cast<grpc_message_compression_algorithm>(i),
              static_cast<grpc_slice_split_mode>(j),
              static_cast<grpc_slice_split_mode>(k),
              get_compressability(
                  static_cast<test_value>(m),
                  static_cast<grpc_message_compression_algorithm>(i)));
          grpc_slice_unref(slice);
        }
      }
    }
  }

  test_tiny_data_compress();
  test_bad_decompression_data_crc();
  test_bad_decompression_data_missing_trailer();
  test_bad_decompression_data_stream();
  test_bad_decompression_data_trailing_garbage();
  test_bad_compression_algorithm();
  test_bad_decompression_algorithm();
  grpc_shutdown();

  return 0;
}
