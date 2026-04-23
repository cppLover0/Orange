// KernelGZ -- Single Header Build -- Generated With comp2header.pl
// Read the included README.md for how to use this library
// Find the source code at https://git.sr.ht/~evalyn/kgz
/*
==== Dual Licenced under Public Domain & 0BSD ====
==== 0BSD Licence ====
BSD Zero Clause License
Copyright (c) 2026 Evalyn Goemer & Luna
Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.
THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
==== Public Domain ====
This software is released to the public domain.
Anyone and anything may copy, edit, publish, use, compile, sell and
distribute this work and all its parts in any form for any purpose,
commercial and non-commercial, without any restrictions, without complying
with any conditions and by any means.
*/
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define KGZ_SINGLE_HEADER
#ifdef __cplusplus
extern "C" {
#endif
extern void* kgz_gzip_decompress(void* data, uint64_t data_size, uint64_t* data_size_out, uint64_t* buffer_size_out);
#ifdef __cplusplus
}
#endif
#ifdef KGZ_IMPLEMENTATION
#ifndef KGZ_HUFFMAN_CACHE
#define KGZ_HUFFMAN_CACHE 10
#endif
_Static_assert((KGZ_HUFFMAN_CACHE) >= 3, "KGZ_HUFFMAN_CACHE must be at least 3");
_Static_assert((KGZ_HUFFMAN_CACHE) <= 15, "KGZ_HUFFMAN_CACHE must be at most 15");
#ifndef KGZ_USE_OWN_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KGZ_MALLOC(size) malloc((size))
#define KGZ_FREE(ptr, size) free((ptr))
#define KGZ_MEMCPY(dst, src, n) memcpy((dst), (src), (n))
#define KGZ_MEMSET(ptr, val, size) memset((ptr), (val), (size))
#define KGZ_PRINTF(...) printf(__VA_ARGS__)
#endif
#ifndef KGZ_EXPECT
#define KGZ_EXPECT(...) __builtin_expect(__VA_ARGS__)
#endif
#ifndef KGZ_LIKELY
#define KGZ_LIKELY(x) KGZ_EXPECT(!!(x), 1)
#endif
#ifndef KGZ_UNLIKELY
#define KGZ_UNLIKELY(x) KGZ_EXPECT(!!(x), 0)
#endif
#ifndef KGZ_UNREACHABLE
#define KGZ_UNREACHABLE(x) __builtin_unreachable()
#endif
typedef struct kgz_arena {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} kgz_arena_t;
void kgz_arena_init(kgz_arena_t* arena, size_t capacity);
void kgz_arena_reset(kgz_arena_t* arena);
void* kgz_arena_allocate(kgz_arena_t* arena, size_t size, size_t alignment);
void kgz_arena_free(kgz_arena_t* arena);
typedef struct kgz_bitstream {
    uint8_t* data;
    uint64_t data_len;
    uint64_t current_byte;
    uint8_t current_bit;
    uint64_t bit_buffer;
    uint8_t bits_in_buffer;
} kgz_bitstream_t;
typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} kgz_buffer_t;
extern bool kgz_buffer_insert(kgz_buffer_t* buffer, uint8_t byte);
extern bool kgz_buffer_insert_bulk(kgz_buffer_t* buffer, const uint8_t* src, size_t len);
extern bool kgz_buffer_lz77copy(kgz_buffer_t* buffer, size_t distance, size_t length);
typedef struct {
    uint16_t symbol;
    int8_t length; // -1 = walk tree
} huffman_cache_entry_t;
typedef struct kgz_huffman_tree kgz_huffman_tree_t;
typedef struct kgz_decompression_context {
    kgz_huffman_tree_t* fixed_huffman_tree;
    kgz_bitstream_t* bitstream;
    kgz_buffer_t output_buffer;
    kgz_arena_t arena_alloc;
} kgz_decompression_context_t;
extern uint32_t kgz_bitstream_getbits(kgz_bitstream_t* stream, uint32_t bits);
extern uint32_t kgz_bitstream_peek(kgz_bitstream_t* stream, uint32_t bits);
extern void kgz_bitstream_consume(kgz_bitstream_t* stream, uint32_t bits);
extern uint8_t kgz_bitstream_read_u8(kgz_bitstream_t* stream);
extern uint16_t kgz_bitstream_read_u16(kgz_bitstream_t* stream);
extern void kgz_bitstream_align(kgz_bitstream_t* stream);
extern void* kgz_gzip_decompress(void* data, uint64_t data_size, uint64_t* data_size_out, uint64_t* buffer_size_out);
extern bool kgz_deflate_decompress(kgz_decompression_context_t* context);
extern kgz_huffman_tree_t* kgz_huffman_tree_create(uint16_t* codes, uint16_t codes_len, kgz_arena_t* arena);
extern void kgz_huffman_tree_debug(kgz_huffman_tree_t* tree);
extern bool kgz_huffman_tree_lookup(kgz_huffman_tree_t* tree, kgz_bitstream_t* stream, uint16_t* symbol);
static inline void kgz_bitstream_fill(kgz_bitstream_t* stream) {
    uint8_t bits = stream->bits_in_buffer;
    uint64_t cur_byte = stream->current_byte + ((stream->current_bit + bits) >> 3);
    const uint8_t* p = stream->data + cur_byte;
    uint64_t data_len = stream->data_len;
    uint64_t bit_buffer = stream->bit_buffer;
    while(bits <= 56 && cur_byte < data_len) {
        bit_buffer |= ((uint64_t) *p++ << bits);
        bits += 8;
        cur_byte++;
    }
    stream->bits_in_buffer = bits;
    stream->bit_buffer = bit_buffer;
}
uint32_t kgz_bitstream_getbits(kgz_bitstream_t* stream, uint32_t bits) {
    if(KGZ_UNLIKELY(bits == 0)) return 0;
    if(KGZ_UNLIKELY(stream->bits_in_buffer < bits)) kgz_bitstream_fill(stream);
    uint64_t mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1);
    uint32_t result = (uint32_t) (stream->bit_buffer & mask);
    stream->bit_buffer >>= bits;
    stream->bits_in_buffer -= bits;
    stream->current_bit += bits;
    stream->current_byte += stream->current_bit / 8;
    stream->current_bit &= 7;
    return result;
}
void kgz_bitstream_align(kgz_bitstream_t* stream) {
    if(KGZ_LIKELY(stream->current_bit != 0)) {
        stream->current_byte++;
        stream->current_bit = 0;
    }
    stream->bit_buffer = 0;
    stream->bits_in_buffer = 0;
}
uint8_t kgz_bitstream_read_u8(kgz_bitstream_t* stream) {
    return (uint8_t) kgz_bitstream_getbits(stream, 8);
}
uint16_t kgz_bitstream_read_u16(kgz_bitstream_t* stream) {
    if(KGZ_LIKELY(stream->current_bit == 0 && stream->current_byte + 1 < stream->data_len)) {
        const uint8_t* p = stream->data + stream->current_byte;
        uint16_t lo = *p++;
        uint16_t hi = *p++;
        uint16_t word = 0;
        word |= lo;
        word |= (hi << 8);
        stream->current_byte += 2;
        return word;
    }
    return (uint16_t) kgz_bitstream_getbits(stream, 16);
}
uint32_t kgz_bitstream_peek(kgz_bitstream_t* stream, uint32_t bits) {
    if(KGZ_UNLIKELY(bits == 0)) return 0;
    if(KGZ_UNLIKELY(stream->bits_in_buffer < bits)) kgz_bitstream_fill(stream);
    uint64_t mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1);
    return (uint32_t) (stream->bit_buffer & mask);
}
void kgz_bitstream_consume(kgz_bitstream_t* stream, uint32_t bits) {
    if(KGZ_UNLIKELY(bits == 0)) return;
    stream->bit_buffer >>= bits;
    stream->bits_in_buffer -= bits;
    stream->current_bit += bits;
    stream->current_byte += stream->current_bit / 8;
    stream->current_bit &= 7;
}
#define CHECK_BOUNDS(curr, size, needed)            \
    do {                                            \
        if((curr) + (needed) > (size)) return NULL; \
    } while(0)
#define FTEXT (1 << 0)
#define FHCRC (1 << 1)
#define FEXTRA (1 << 2)
#define FNAME (1 << 3)
#define FCOMMENT (1 << 4)
void* kgz_gzip_decompress(void* data, uint64_t data_size, uint64_t* data_size_out, uint64_t* buffer_size_out) {
    uint8_t* u8data = (uint8_t*)data;
    uint64_t current_byte = 0;
    CHECK_BOUNDS(current_byte, data_size, 2);
    if(u8data[current_byte] != 0x1F || u8data[current_byte + 1] != 0x8B) return NULL;
    current_byte += 2;
    CHECK_BOUNDS(current_byte, data_size, 1);
    if(u8data[current_byte] != 8) return NULL;
    current_byte += 1;
    CHECK_BOUNDS(current_byte, data_size, 1);
    uint8_t flag = u8data[current_byte];
    current_byte += 1;
    CHECK_BOUNDS(current_byte, data_size, 6);
    current_byte += 6;
    if(flag & FEXTRA) {
        CHECK_BOUNDS(current_byte, data_size, 2);
        uint16_t xlen = 0;
        xlen |= u8data[current_byte];
        xlen |= ((uint16_t) u8data[current_byte + 1] << 8);
        current_byte += 2;
        CHECK_BOUNDS(current_byte, data_size, xlen);
        current_byte += xlen;
    }
    if(flag & FNAME) {
        while(true) {
            CHECK_BOUNDS(current_byte, data_size, 1);
            if(u8data[current_byte++] == 0) break;
        }
    }
    if(flag & FCOMMENT) {
        while(true) {
            CHECK_BOUNDS(current_byte, data_size, 1);
            if(u8data[current_byte++] == 0) break;
        }
    }
    if(flag & FHCRC) {
        CHECK_BOUNDS(current_byte, data_size, 2);
        current_byte += 2;
    }
    CHECK_BOUNDS(current_byte, data_size, 9);
    uint8_t* deflate_data = u8data + current_byte;
    kgz_bitstream_t deflate_bitstream;
    deflate_bitstream.data = deflate_data;
    deflate_bitstream.data_len = data_size - current_byte - 8;
    deflate_bitstream.current_byte = 0;
    deflate_bitstream.current_bit = 0;
    deflate_bitstream.bit_buffer = 0;
    deflate_bitstream.bits_in_buffer = 0;
    uint64_t footer_offset = data_size - 8;
    uint32_t decompressed_size = 0;
    decompressed_size |= u8data[footer_offset + 4];
    decompressed_size |= ((uint32_t) u8data[footer_offset + 5] << 8);
    decompressed_size |= ((uint32_t) u8data[footer_offset + 6] << 16);
    decompressed_size |= ((uint32_t) u8data[footer_offset + 7] << 24);
    kgz_decompression_context_t context;
    context.bitstream = &deflate_bitstream;
    context.output_buffer.data = (uint8_t*)KGZ_MALLOC(decompressed_size + 1024);
    context.output_buffer.size = 0;
    context.output_buffer.capacity = decompressed_size + 1024;
    kgz_arena_init(&context.arena_alloc, (1024 * 32) + ((sizeof(huffman_cache_entry_t) * 3) * (1 << (KGZ_HUFFMAN_CACHE)))); // 32kb base + cache augment
    uint16_t codes[288] = { 0 };
    for(size_t i = 0; i < 288; i++) {
        if(i <= 143)
            codes[i] = 8;
        else if(i <= 255)
            codes[i] = 9;
        else if(i <= 279)
            codes[i] = 7;
        else
            codes[i] = 8;
    }
    kgz_arena_t fixed_huffman_arena;
    kgz_arena_init(&fixed_huffman_arena, (1024 * 24) + ((sizeof(huffman_cache_entry_t) * 1) * (1 << (KGZ_HUFFMAN_CACHE)))); // 24kb base + cache augment
    context.fixed_huffman_tree = kgz_huffman_tree_create(codes, 288, &fixed_huffman_arena);
    if(!context.fixed_huffman_tree) {
        kgz_arena_free(&context.arena_alloc);
        kgz_arena_free(&fixed_huffman_arena);
        KGZ_FREE(context.output_buffer.data, context.output_buffer.capacity);
        return NULL;
    }
    bool success = kgz_deflate_decompress(&context);
    kgz_arena_free(&context.arena_alloc);
    kgz_arena_free(&fixed_huffman_arena);
    if(KGZ_UNLIKELY(!success)) {
        KGZ_FREE(context.output_buffer.data, context.output_buffer.capacity);
        return NULL;
    }
    if(data_size_out) *data_size_out = decompressed_size;
    if(buffer_size_out) *buffer_size_out = context.output_buffer.capacity;
    return context.output_buffer.data;
}
static const uint16_t kgz_base_length[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};
static const uint8_t kgz_length_extra_bits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};
static const uint16_t kgz_base_dist[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};
static const uint8_t kgz_dist_extra_bits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};
static const uint8_t kgz_clen_alpha_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
bool kgz_dflt_handle_stored(kgz_decompression_context_t* context) {
    kgz_bitstream_align(context->bitstream);
    uint16_t len = kgz_bitstream_read_u16(context->bitstream);
    uint16_t nlen = kgz_bitstream_read_u16(context->bitstream);
    if(len != (~nlen & 0xffff)) {
        KGZ_PRINTF("invalid stored block with len %u and nlen %u\n", len, (~nlen & 0xffff));
        return false;
    }
    uint64_t avail = context->bitstream->data_len - context->bitstream->current_byte;
    if(KGZ_UNLIKELY((uint64_t) len > avail)) len = (uint16_t) avail;
    const uint8_t* src = context->bitstream->data + context->bitstream->current_byte;
    if(KGZ_UNLIKELY(!kgz_buffer_insert_bulk(&context->output_buffer, src, len))) return false;
    context->bitstream->current_byte += len;
    return true;
}
bool create_dynamic_huffman_tables(kgz_decompression_context_t* context, kgz_huffman_tree_t** ltree, kgz_huffman_tree_t** dtree) {
    uint32_t data = kgz_bitstream_peek(context->bitstream, 5 + 5 + 4);
    kgz_bitstream_consume(context->bitstream, 5 + 5 + 4);
    uint8_t hlit = data & 0x1f;
    uint8_t hdist = (data >> 5) & 0x1f;
    uint8_t hclen = (data >> 10) & 0xf;
    uint16_t hsym_lengths[19] = { 0 };
    for(int i = 0; i < hclen + 4; i++) { hsym_lengths[kgz_clen_alpha_order[i]] = (uint16_t) kgz_bitstream_getbits(context->bitstream, 3); }
    kgz_huffman_tree_t* htree = kgz_huffman_tree_create(hsym_lengths, 19, &context->arena_alloc);
    if(KGZ_UNLIKELY(!htree)) return false;
    uint16_t symbol;
    uint16_t sym_lengths[288 + 32] = { 0 };
    for(int i = 0; i < (hlit + 257) + (hdist + 1);) {
        bool v = kgz_huffman_tree_lookup(htree, context->bitstream, &symbol);
        if(KGZ_UNLIKELY(!v)) { return false; }
        if(symbol <= 15) {
            sym_lengths[i] = symbol;
            i++;
            continue;
        } else if(symbol == 16) {
            uint8_t times = (uint8_t) kgz_bitstream_getbits(context->bitstream, 2) + 3;
            if(i <= 0) { return false; }
            uint16_t value = sym_lengths[i - 1];
            for(int j = 0; j < times; j++, i++) { sym_lengths[i] = value; }
        } else if(symbol == 17) {
            uint8_t times = (uint8_t) kgz_bitstream_getbits(context->bitstream, 3) + 3;
            for(int j = 0; j < times && i < (hlit + 257) + (hdist + 1); j++, i++) { sym_lengths[i] = 0; }
        } else if(symbol == 18) {
            uint8_t times = (uint8_t) kgz_bitstream_getbits(context->bitstream, 7) + 11;
            for(int j = 0; j < times && i < (hlit + 257) + (hdist + 1); j++, i++) { sym_lengths[i] = 0; }
        }
    }
    kgz_huffman_tree_t* dtree_tmp = kgz_huffman_tree_create(&sym_lengths[hlit + 257], hdist + 1, &context->arena_alloc);
    kgz_huffman_tree_t* ltree_tmp = kgz_huffman_tree_create(sym_lengths, hlit + 257, &context->arena_alloc);
    if(KGZ_UNLIKELY(!dtree_tmp)) return false;
    if(KGZ_UNLIKELY(!ltree_tmp)) return false;
    *dtree = dtree_tmp;
    *ltree = ltree_tmp;
    return true;
}
bool kgz_dflt_handle_huffman(kgz_decompression_context_t* context, bool dynamic) {
    uint16_t symbol = 0;
    kgz_huffman_tree_t* dtree;
    kgz_huffman_tree_t* ltree;
    if(dynamic) {
        if(KGZ_UNLIKELY(!create_dynamic_huffman_tables(context, &ltree, &dtree))) return false;
    } else {
        ltree = context->fixed_huffman_tree;
    }
    while(true) {
        if(context->bitstream->current_byte >= context->bitstream->data_len) break;
        bool v = kgz_huffman_tree_lookup(ltree, context->bitstream, &symbol);
        if(KGZ_UNLIKELY(!v)) break;
        if(KGZ_LIKELY(symbol < 256)) {
            if(!kgz_buffer_insert(&context->output_buffer, (uint8_t) symbol)) return false;
            continue;
        }
        if(KGZ_UNLIKELY(symbol == 256)) break;
        uint8_t extra_length_bits = kgz_length_extra_bits[symbol - 257];
        uint16_t length = kgz_base_length[symbol - 257] + (uint16_t) kgz_bitstream_getbits(context->bitstream, extra_length_bits);
        uint16_t distance_symbol;
        if(dynamic) {
            if(!kgz_huffman_tree_lookup(dtree, context->bitstream, &distance_symbol)) return false;
        } else {
            distance_symbol = (uint16_t) kgz_bitstream_getbits(context->bitstream, 5);
            {
                uint16_t rev = 0;
                for(size_t i = 0; i < 5; i++) {
                    rev = (uint16_t) ((rev << 1) | (distance_symbol & 1));
                    distance_symbol >>= 1;
                }
                distance_symbol = rev;
            }
        }
        uint16_t extra_dist = (uint16_t) kgz_bitstream_getbits(context->bitstream, kgz_dist_extra_bits[distance_symbol]);
        uint16_t distance = kgz_base_dist[distance_symbol] + extra_dist;
        if(KGZ_UNLIKELY(!kgz_buffer_lz77copy(&context->output_buffer, distance, length))) return false;
    }
    return true;
}
bool kgz_deflate_decompress(kgz_decompression_context_t* context) {
    bool last_block = false;
    bool success = true;
    while(KGZ_UNLIKELY(!last_block)) {
        if(KGZ_UNLIKELY(kgz_bitstream_getbits(context->bitstream, 1) == true)) last_block = true;
        uint8_t block_type = (uint8_t) kgz_bitstream_getbits(context->bitstream, 2);
        switch(block_type) {
            case 0: success = kgz_dflt_handle_stored(context); break;
            case 1: success = kgz_dflt_handle_huffman(context, false); break;
            case 2: success = kgz_dflt_handle_huffman(context, true); break;
            case 3: return false;
        }
        kgz_arena_reset(&context->arena_alloc);
        if(KGZ_UNLIKELY(!success)) { break; }
    }
    return success;
}
typedef enum {
    HUFFMAN_NODE_TYPE_INTERNAL,
    HUFFMAN_NODE_TYPE_SYMBOL
} huffman_node_type;
typedef struct huffman_node huffman_node_t;
struct huffman_node {
    huffman_node_type type;
    union {
        struct {
            int32_t zero_index;
            int32_t one_index;
        } internal;
        struct {
            uint16_t symbol;
        } symbol;
    };
};
#define TABLE_BITS (KGZ_HUFFMAN_CACHE)
struct kgz_huffman_tree {
    huffman_cache_entry_t table[1 << TABLE_BITS];
    int32_t root_index;
    huffman_node_t* nodes;
    int32_t nodes_capacity;
    int32_t nodes_count;
};
#define MAX_BITS 15
static inline huffman_node_t* get_node(kgz_huffman_tree_t* tree, int32_t index) {
    if(KGZ_UNLIKELY(index < 0 || index >= tree->nodes_count)) return NULL;
    return &tree->nodes[index];
}
static inline int32_t alloc_new_node(kgz_huffman_tree_t* tree, kgz_arena_t* arena) {
    if(KGZ_UNLIKELY(tree->nodes_count >= tree->nodes_capacity)) {
        int32_t new_capacity = tree->nodes_capacity == 0 ? 16 : tree->nodes_capacity * 2;
        huffman_node_t* new_nodes = (huffman_node_t*)kgz_arena_allocate(arena, sizeof(huffman_node_t) * (size_t) new_capacity, 8);
        if(KGZ_UNLIKELY(!new_nodes)) return -1;
        if(tree->nodes) { KGZ_MEMCPY(new_nodes, tree->nodes, sizeof(huffman_node_t) * (size_t) tree->nodes_count); }
        tree->nodes = new_nodes;
        tree->nodes_capacity = new_capacity;
    }
    int32_t index = tree->nodes_count++;
    tree->nodes[index].type = HUFFMAN_NODE_TYPE_INTERNAL;
    tree->nodes[index].internal.zero_index = -1;
    tree->nodes[index].internal.one_index = -1;
    return index;
}
static inline bool insert_code(kgz_huffman_tree_t* tree, uint32_t code, uint16_t len, uint16_t symbol, kgz_arena_t* arena) {
    int32_t current_index = tree->root_index;
    for(int i = len - 1; i >= 0; i--) {
        uint32_t bit = (code >> i) & 1;
        if(bit == 0) {
            if(get_node(tree, current_index)->internal.zero_index == -1) {
                int32_t new_index = alloc_new_node(tree, arena);
                if(KGZ_UNLIKELY(new_index == -1)) return false;
                get_node(tree, current_index)->internal.zero_index = new_index;
            }
            current_index = get_node(tree, current_index)->internal.zero_index;
        } else {
            if(get_node(tree, current_index)->internal.one_index == -1) {
                int32_t new_index = alloc_new_node(tree, arena);
                if(KGZ_UNLIKELY(new_index == -1)) return false;
                get_node(tree, current_index)->internal.one_index = new_index;
            }
            current_index = get_node(tree, current_index)->internal.one_index;
        }
    }
    huffman_node_t* node = get_node(tree, current_index);
    node->type = HUFFMAN_NODE_TYPE_SYMBOL;
    node->symbol.symbol = symbol;
    return true;
}
uint32_t bit_reverse(uint32_t code, uint32_t bits) {
    uint32_t result = 0;
    for(uint32_t i = 0; i < bits; i++) {
        result <<= 1;
        if(code & 1) result |= 1;
        code >>= 1;
    }
    return result;
}
kgz_huffman_tree_t* kgz_huffman_tree_create(uint16_t* symbol_lengths, uint16_t symbol_count, kgz_arena_t* arena) {
    kgz_huffman_tree_t* tree = (kgz_huffman_tree_t*)kgz_arena_allocate(arena, sizeof(kgz_huffman_tree_t), 8);
    if(!tree) return NULL;
    tree->nodes = NULL;
    tree->nodes_capacity = 0;
    tree->nodes_count = 0;
    int32_t leaf_count = 0;
    for(uint16_t i = 0; i < symbol_count; i++) {
        if(symbol_lengths[i] != 0) leaf_count++;
    }
    int32_t initial_capacity = 16;
    if(leaf_count > 0) {
        int64_t need = (int64_t) leaf_count * 2 - 1;
        if(need > initial_capacity) initial_capacity = (int32_t) need;
    }
    tree->nodes = (huffman_node_t*)kgz_arena_allocate(arena, sizeof(huffman_node_t) * (size_t) initial_capacity, 8);
    if(KGZ_UNLIKELY(!tree->nodes)) return NULL;
    tree->nodes_capacity = initial_capacity;
    tree->nodes_count = 0;
    tree->root_index = alloc_new_node(tree, arena);
    if(tree->root_index == -1) return NULL;
    size_t bl_count[MAX_BITS + 1] = { 0 };
    for(size_t i = 0; i < symbol_count; i++) {
        if(symbol_lengths[i] > MAX_BITS) { return NULL; }
        if(symbol_lengths[i] == 0) { continue; }
        bl_count[symbol_lengths[i]]++;
    }
    uint16_t next_code[MAX_BITS + 1] = { 0 };
    {
        uint32_t code = 0;
        bl_count[0] = 0;
        for(uint32_t bits = 1; bits <= MAX_BITS; bits++) {
            code = (code + (uint32_t) bl_count[bits - 1]) << 1;
            next_code[bits] = (uint16_t) code;
        }
    }
    KGZ_MEMSET(tree->table, 0xff, sizeof(tree->table));
    for(uint16_t sym = 0; sym < symbol_count; sym++) {
        uint16_t symbol_length = symbol_lengths[sym];
        if(symbol_length == 0) { continue; }
        uint32_t code = next_code[symbol_length];
        if(!insert_code(tree, next_code[symbol_length], symbol_length, sym, arena)) return NULL;
        next_code[symbol_length]++;
        if(symbol_length > TABLE_BITS) continue;
        uint32_t stride = 1u << symbol_length;
        uint32_t rev_code = bit_reverse(code, symbol_length);
        for(uint32_t fill = rev_code; fill < (1u << TABLE_BITS); fill += stride) {
            tree->table[fill].symbol = sym;
            tree->table[fill].length = (int8_t) symbol_length;
        }
    }
    return tree;
}
bool kgz_huffman_tree_lookup_slow(kgz_huffman_tree_t* tree, kgz_bitstream_t* stream, uint16_t* symbol, uint32_t bits) {
    huffman_node_t* current_node = get_node(tree, tree->root_index);
    if(current_node == NULL) return false;
    uint32_t current_length = 0;
    while(1) {
        if(current_node->type == HUFFMAN_NODE_TYPE_SYMBOL) {
            *symbol = current_node->symbol.symbol;
            kgz_bitstream_consume(stream, current_length);
            return true;
        }
        uint32_t bit = (bits >> (current_length)) & 1;
        int32_t index = bit ? current_node->internal.one_index : current_node->internal.zero_index;
        if(KGZ_UNLIKELY(index == -1)) {
            kgz_bitstream_consume(stream, current_length);
            return false;
        }
        current_node = get_node(tree, index);
        current_length++;
        if(KGZ_UNLIKELY(current_length > 15)) {
            kgz_bitstream_consume(stream, current_length);
            return false;
        }
    }
    KGZ_UNREACHABLE();
}
bool kgz_huffman_tree_lookup(kgz_huffman_tree_t* tree, kgz_bitstream_t* stream, uint16_t* symbol) {
    *symbol = 0;
    uint32_t bits = kgz_bitstream_peek(stream, MAX_BITS);
    huffman_cache_entry_t entry = tree->table[bits & ((1 << TABLE_BITS) - 1)];
    if(entry.length > 0) {
        kgz_bitstream_consume(stream, (uint32_t) entry.length);
        *symbol = entry.symbol;
        return true;
    }
    return kgz_huffman_tree_lookup_slow(tree, stream, symbol, bits);
}
bool kgz_buffer_insert(kgz_buffer_t* buffer, uint8_t byte) {
    if(KGZ_UNLIKELY(buffer->size >= buffer->capacity)) {
        KGZ_PRINTF("output buffer overflow: size %zu capacity %zu\n", buffer->size, buffer->capacity);
        return false;
    }
    buffer->data[buffer->size++] = byte;
    return true;
}
bool kgz_buffer_insert_bulk(kgz_buffer_t* buffer, const uint8_t* src, size_t len) {
    if(KGZ_UNLIKELY(buffer->size + len > buffer->capacity)) {
        KGZ_PRINTF("output buffer overflow: need %zu have %zu\n", len, buffer->capacity - buffer->size);
        return false;
    }
    KGZ_MEMCPY(buffer->data + buffer->size, src, len);
    buffer->size += len;
    return true;
}
bool kgz_buffer_lz77copy(kgz_buffer_t* buffer, size_t distance, size_t length) {
    if(KGZ_UNLIKELY(distance > buffer->size)) return false;
    if(KGZ_UNLIKELY(buffer->size + length > buffer->capacity)) {
        KGZ_PRINTF("output buffer overflow during backcopy\n");
        return false;
    }
    uint8_t* dest = buffer->data + buffer->size;
    const uint8_t* src = dest - distance;
    if(distance == 1) {
        KGZ_MEMSET(dest, src[0], length);
    } else if(distance >= length) {
        KGZ_MEMCPY(dest, src, length);
    } else {
        for(size_t i = 0; i < length; i++) { dest[i] = src[i]; }
    }
    buffer->size += length;
    return true;
}
void kgz_arena_init(kgz_arena_t* arena, size_t capacity) {
    if(KGZ_UNLIKELY(!arena)) return;
    arena->buffer = (uint8_t*) KGZ_MALLOC(capacity);
    if(KGZ_UNLIKELY(!arena->buffer)) {
        arena->capacity = 0;
        arena->offset = 0;
        return;
    }
    arena->capacity = capacity;
    arena->offset = 0;
}
void kgz_arena_reset(kgz_arena_t* arena) {
    if(KGZ_UNLIKELY(!arena)) return;
    arena->offset = 0;
}
void* kgz_arena_allocate(kgz_arena_t* arena, size_t size, size_t alignment) {
    if(KGZ_UNLIKELY(!arena || !arena->buffer)) return NULL;
    if(KGZ_UNLIKELY(alignment == 0 || (alignment & (alignment - 1)) != 0)) return NULL;
    uintptr_t base = (uintptr_t) arena->buffer;
    uintptr_t current = base + arena->offset;
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t new_offset = (size_t) ((aligned - base) + size);
    if(KGZ_UNLIKELY(new_offset > arena->capacity)) { return NULL; }
    void* ptr = (void*) aligned;
    arena->offset = new_offset;
    return ptr;
}
void kgz_arena_free(kgz_arena_t* arena) {
    if(KGZ_UNLIKELY(!arena)) return;
    KGZ_FREE(arena->buffer, arena->capacity);
    arena->buffer = NULL;
    arena->capacity = 0;
    arena->offset = 0;
}
#endif
