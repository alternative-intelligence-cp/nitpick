/**
 * Aria Collection Type Tags — Shared Header
 * Used by astack (ustack) and ahash (uhash) for type-tagged value storage.
 *
 * Each value is stored as an 8-byte int64 (widened via ZExt/BitCast)
 * alongside a type tag. The tag enables runtime type checking on retrieval.
 */

#ifndef ARIA_RUNTIME_TYPE_TAGS_H
#define ARIA_RUNTIME_TYPE_TAGS_H

#include <stdint.h>

#define ARIA_TAG_INT8    0
#define ARIA_TAG_INT16   1
#define ARIA_TAG_INT32   2
#define ARIA_TAG_INT64   3
#define ARIA_TAG_FLT32   4
#define ARIA_TAG_FLT64   5
#define ARIA_TAG_BOOL    6
#define ARIA_TAG_STRING  7
#define ARIA_TAG_POINTER 8

#endif /* ARIA_RUNTIME_TYPE_TAGS_H */
