/* Copyright 2018 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <brotli/decode.h>

#define BUFFER_SIZE (5u << 20)

typedef struct Context {
  int fd_in;
  int in_size;
  uint8_t* input_buffer;
  uint8_t* output_buffer;
  BrotliDecoderState* decoder;
} Context;

void init(Context* ctx) {
  ctx->fd_in = -1;
  ctx->in_size = -1;
  ctx->input_buffer = (uint8_t*) -1;
  ctx->output_buffer = 0;
  ctx->decoder = 0;
}

void cleanup(Context* ctx) {
  if (ctx->decoder) BrotliDecoderDestroyInstance(ctx->decoder);
  if (ctx->output_buffer) free(ctx->output_buffer);
  if (ctx->input_buffer != (uint8_t*) -1) munmap(ctx->input_buffer, ctx->in_size);
  if (ctx->fd_in != -1) close(ctx->fd_in);
}

void fail(Context* ctx, const char* message) {
  fprintf(stderr, "%s\n", message);
  cleanup(ctx);
  exit(1);
}

int main(int argc, char** argv) {
  Context ctx;
  BrotliDecoderResult result = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
  size_t available_in;
  const uint8_t* next_in;
  size_t available_out = BUFFER_SIZE;
  uint8_t* next_out;
  int start_offset;

  init(&ctx);

  if (argc != 3) {
    fprintf(stderr, "Usage: %s FILE OFFSET\n", argv[0]);
  }
  start_offset = atoi(argv[2]);

  ctx.fd_in = open(argv[1], O_RDONLY);
  if (ctx.fd_in == -1) fail(&ctx, "can't open input file");
  ctx.in_size = lseek(ctx.fd_in, 0, SEEK_END);
  if (ctx.in_size == -1) fail(&ctx, "can't open input file");
  ctx.input_buffer = (uint8_t*) mmap(
      NULL, ctx.in_size, PROT_READ, MAP_PRIVATE, ctx.fd_in, 0);
  if (ctx.input_buffer == (uint8_t*) -1) {
    fail(&ctx, "out of memory / input buffer");
  }
  ctx.output_buffer = (uint8_t*)malloc(BUFFER_SIZE);
  if (!ctx.output_buffer) fail(&ctx, "out of memory / output buffer");
  ctx.decoder = BrotliDecoderCreateInstance(0, 0, 0);
  if (!ctx.decoder) fail(&ctx, "out of memory / decoder");
  BrotliDecoderSetParameter(ctx.decoder, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1);

  available_in = ctx.in_size - start_offset;
  next_in = ctx.input_buffer + start_offset;

  next_out = ctx.output_buffer;
  while (1) {
    if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
      if (available_in == 0) {
        fail(&ctx, "impossible");
        break;
      }
    } else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
      available_out = BUFFER_SIZE;
      next_out = ctx.output_buffer;
    } else {
      break;
    }
    result = BrotliDecoderDecompressStream(
        ctx.decoder, &available_in, &next_in, &available_out, &next_out, 0);
  }
  if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
    fail(&ctx, "failed to write output");
  } else if (result != BROTLI_DECODER_RESULT_SUCCESS) {
    fail(&ctx, "corrupt input");
  }
  cleanup(&ctx);
  fprintf(stderr, "done\n");
  return 0;
}
