; ModuleID = 'tests/allocator/quick_builtin_test.aria'
source_filename = "tests/allocator/quick_builtin_test.aria"

define i64 @main() {
entry:
  %arena_handle = alloca i64, align 8
  %arena_handle1 = call i64 @aria_arena_new_handle(i64 4096)
  store i64 %arena_handle1, ptr %arena_handle, align 4
  %arena_ptr = alloca i64, align 8
  %arena_handle2 = load i64, ptr %arena_handle, align 4
  %alloc_ptr = call i64 @aria_arena_alloc_handle(i64 %arena_handle2, i64 64)
  store i64 %alloc_ptr, ptr %arena_ptr, align 4
  %arena_handle3 = load i64, ptr %arena_handle, align 4
  call void @aria_arena_reset_handle(i64 %arena_handle3)
  %arena_allocated = alloca i64, align 8
  %arena_handle4 = load i64, ptr %arena_handle, align 4
  %allocated_bytes = call i64 @aria_arena_get_allocated_handle(i64 %arena_handle4)
  store i64 %allocated_bytes, ptr %arena_allocated, align 4
  %arena_reserved = alloca i64, align 8
  %arena_handle5 = load i64, ptr %arena_handle, align 4
  %reserved_bytes = call i64 @aria_arena_get_reserved_handle(i64 %arena_handle5)
  store i64 %reserved_bytes, ptr %arena_reserved, align 4
  %arena_handle6 = load i64, ptr %arena_handle, align 4
  call void @aria_arena_destroy_handle(i64 %arena_handle6)
  %pool_handle = alloca i64, align 8
  %pool_handle7 = call i64 @aria_pool_new_handle(i64 32, i64 1024)
  store i64 %pool_handle7, ptr %pool_handle, align 4
  %pool_ptr = alloca i64, align 8
  %pool_handle8 = load i64, ptr %pool_handle, align 4
  %alloc_ptr9 = call i64 @aria_pool_alloc_handle(i64 %pool_handle8)
  store i64 %alloc_ptr9, ptr %pool_ptr, align 4
  %pool_handle10 = load i64, ptr %pool_handle, align 4
  %pool_ptr11 = load i64, ptr %pool_ptr, align 4
  call void @aria_pool_free_handle(i64 %pool_handle10, i64 %pool_ptr11)
  %pool_handle12 = load i64, ptr %pool_handle, align 4
  call void @aria_pool_reset_handle(i64 %pool_handle12)
  %pool_total = alloca i64, align 8
  %pool_handle13 = load i64, ptr %pool_handle, align 4
  %total_blocks = call i64 @aria_pool_get_total_blocks_handle(i64 %pool_handle13)
  store i64 %total_blocks, ptr %pool_total, align 4
  %pool_used = alloca i64, align 8
  %pool_handle14 = load i64, ptr %pool_handle, align 4
  %used_blocks = call i64 @aria_pool_get_used_blocks_handle(i64 %pool_handle14)
  store i64 %used_blocks, ptr %pool_used, align 4
  %pool_handle15 = load i64, ptr %pool_handle, align 4
  call void @aria_pool_destroy_handle(i64 %pool_handle15)
  %slab_handle = alloca i64, align 8
  %slab_handle16 = call i64 @aria_slab_cache_new_handle(i64 64, i64 4096)
  store i64 %slab_handle16, ptr %slab_handle, align 4
  %slab_ptr = alloca i64, align 8
  %slab_handle17 = load i64, ptr %slab_handle, align 4
  %alloc_ptr18 = call i64 @aria_slab_cache_alloc_handle(i64 %slab_handle17)
  store i64 %alloc_ptr18, ptr %slab_ptr, align 4
  %slab_handle19 = load i64, ptr %slab_handle, align 4
  %slab_ptr20 = load i64, ptr %slab_ptr, align 4
  call void @aria_slab_cache_free_handle(i64 %slab_handle19, i64 %slab_ptr20)
  %slab_total = alloca i64, align 8
  %slab_handle21 = load i64, ptr %slab_handle, align 4
  %total_objects = call i64 @aria_slab_cache_get_total_objects_handle(i64 %slab_handle21)
  store i64 %total_objects, ptr %slab_total, align 4
  %slab_allocated = alloca i64, align 8
  %slab_handle22 = load i64, ptr %slab_handle, align 4
  %allocated_objects = call i64 @aria_slab_cache_get_allocated_objects_handle(i64 %slab_handle22)
  store i64 %allocated_objects, ptr %slab_allocated, align 4
  %slab_handle23 = load i64, ptr %slab_handle, align 4
  call void @aria_slab_cache_destroy_handle(i64 %slab_handle23)
  ret i64 0
}

declare i64 @aria_arena_new_handle(i64)

declare i64 @aria_arena_alloc_handle(i64, i64)

declare void @aria_arena_reset_handle(i64)

declare i64 @aria_arena_get_allocated_handle(i64)

declare i64 @aria_arena_get_reserved_handle(i64)

declare void @aria_arena_destroy_handle(i64)

declare i64 @aria_pool_new_handle(i64, i64)

declare i64 @aria_pool_alloc_handle(i64)

declare void @aria_pool_free_handle(i64, i64)

declare void @aria_pool_reset_handle(i64)

declare i64 @aria_pool_get_total_blocks_handle(i64)

declare i64 @aria_pool_get_used_blocks_handle(i64)

declare void @aria_pool_destroy_handle(i64)

declare i64 @aria_slab_cache_new_handle(i64, i64)

declare i64 @aria_slab_cache_alloc_handle(i64)

declare void @aria_slab_cache_free_handle(i64, i64)

declare i64 @aria_slab_cache_get_total_objects_handle(i64)

declare i64 @aria_slab_cache_get_allocated_objects_handle(i64)

declare void @aria_slab_cache_destroy_handle(i64)
