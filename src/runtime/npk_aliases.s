/* npk_aliases.s — zero-overhead npk_* → aria_* symbol aliases
 * Auto-generated during Phase 6 rebrand. aria_* kept for compat.
 * When aria_* is removed (Phase 7), delete this file.
 */

.text

.global npk_alloc
.type npk_alloc, @function
npk_alloc:
    jmp aria_alloc@PLT
.size npk_alloc, .-npk_alloc

.global npk_alloc_exec
.type npk_alloc_exec, @function
npk_alloc_exec:
    jmp aria_alloc_exec@PLT
.size npk_alloc_exec, .-npk_alloc_exec

.global npk_arena_alloc_handle
.type npk_arena_alloc_handle, @function
npk_arena_alloc_handle:
    jmp aria_arena_alloc_handle@PLT
.size npk_arena_alloc_handle, .-npk_arena_alloc_handle

.global npk_arena_destroy_handle
.type npk_arena_destroy_handle, @function
npk_arena_destroy_handle:
    jmp aria_arena_destroy_handle@PLT
.size npk_arena_destroy_handle, .-npk_arena_destroy_handle

.global npk_arena_get_allocated_handle
.type npk_arena_get_allocated_handle, @function
npk_arena_get_allocated_handle:
    jmp aria_arena_get_allocated_handle@PLT
.size npk_arena_get_allocated_handle, .-npk_arena_get_allocated_handle

.global npk_arena_get_reserved_handle
.type npk_arena_get_reserved_handle, @function
npk_arena_get_reserved_handle:
    jmp aria_arena_get_reserved_handle@PLT
.size npk_arena_get_reserved_handle, .-npk_arena_get_reserved_handle

.global npk_arena_new_handle
.type npk_arena_new_handle, @function
npk_arena_new_handle:
    jmp aria_arena_new_handle@PLT
.size npk_arena_new_handle, .-npk_arena_new_handle

.global npk_arena_reset_handle
.type npk_arena_reset_handle, @function
npk_arena_reset_handle:
    jmp aria_arena_reset_handle@PLT
.size npk_arena_reset_handle, .-npk_arena_reset_handle

.global npk_args_init
.type npk_args_init, @function
npk_args_init:
    jmp aria_args_init@PLT
.size npk_args_init, .-npk_args_init

.global npk_array_get_simple
.type npk_array_get_simple, @function
npk_array_get_simple:
    jmp aria_array_get_simple@PLT
.size npk_array_get_simple, .-npk_array_get_simple

.global npk_array_length
.type npk_array_length, @function
npk_array_length:
    jmp aria_array_length@PLT
.size npk_array_length, .-npk_array_length

.global npk_array_new_simple
.type npk_array_new_simple, @function
npk_array_new_simple:
    jmp aria_array_new_simple@PLT
.size npk_array_new_simple, .-npk_array_new_simple

.global npk_array_pop_simple
.type npk_array_pop_simple, @function
npk_array_pop_simple:
    jmp aria_array_pop_simple@PLT
.size npk_array_pop_simple, .-npk_array_pop_simple

.global npk_array_push_simple
.type npk_array_push_simple, @function
npk_array_push_simple:
    jmp aria_array_push_simple@PLT
.size npk_array_push_simple, .-npk_array_push_simple

.global npk_array_set_simple
.type npk_array_set_simple, @function
npk_array_set_simple:
    jmp aria_array_set_simple@PLT
.size npk_array_set_simple, .-npk_array_set_simple

.global npk_atomic_bool_compare_exchange_strong
.type npk_atomic_bool_compare_exchange_strong, @function
npk_atomic_bool_compare_exchange_strong:
    jmp aria_atomic_bool_compare_exchange_strong@PLT
.size npk_atomic_bool_compare_exchange_strong, .-npk_atomic_bool_compare_exchange_strong

.global npk_atomic_bool_compare_exchange_weak
.type npk_atomic_bool_compare_exchange_weak, @function
npk_atomic_bool_compare_exchange_weak:
    jmp aria_atomic_bool_compare_exchange_weak@PLT
.size npk_atomic_bool_compare_exchange_weak, .-npk_atomic_bool_compare_exchange_weak

.global npk_atomic_bool_create
.type npk_atomic_bool_create, @function
npk_atomic_bool_create:
    jmp aria_atomic_bool_create@PLT
.size npk_atomic_bool_create, .-npk_atomic_bool_create

.global npk_atomic_bool_destroy
.type npk_atomic_bool_destroy, @function
npk_atomic_bool_destroy:
    jmp aria_atomic_bool_destroy@PLT
.size npk_atomic_bool_destroy, .-npk_atomic_bool_destroy

.global npk_atomic_bool_exchange
.type npk_atomic_bool_exchange, @function
npk_atomic_bool_exchange:
    jmp aria_atomic_bool_exchange@PLT
.size npk_atomic_bool_exchange, .-npk_atomic_bool_exchange

.global npk_atomic_bool_load
.type npk_atomic_bool_load, @function
npk_atomic_bool_load:
    jmp aria_atomic_bool_load@PLT
.size npk_atomic_bool_load, .-npk_atomic_bool_load

.global npk_atomic_bool_store
.type npk_atomic_bool_store, @function
npk_atomic_bool_store:
    jmp aria_atomic_bool_store@PLT
.size npk_atomic_bool_store, .-npk_atomic_bool_store

.global npk_atomic_int32_compare_exchange_strong
.type npk_atomic_int32_compare_exchange_strong, @function
npk_atomic_int32_compare_exchange_strong:
    jmp aria_atomic_int32_compare_exchange_strong@PLT
.size npk_atomic_int32_compare_exchange_strong, .-npk_atomic_int32_compare_exchange_strong

.global npk_atomic_int32_create
.type npk_atomic_int32_create, @function
npk_atomic_int32_create:
    jmp aria_atomic_int32_create@PLT
.size npk_atomic_int32_create, .-npk_atomic_int32_create

.global npk_atomic_int32_destroy
.type npk_atomic_int32_destroy, @function
npk_atomic_int32_destroy:
    jmp aria_atomic_int32_destroy@PLT
.size npk_atomic_int32_destroy, .-npk_atomic_int32_destroy

.global npk_atomic_int32_exchange
.type npk_atomic_int32_exchange, @function
npk_atomic_int32_exchange:
    jmp aria_atomic_int32_exchange@PLT
.size npk_atomic_int32_exchange, .-npk_atomic_int32_exchange

.global npk_atomic_int32_fetch_add
.type npk_atomic_int32_fetch_add, @function
npk_atomic_int32_fetch_add:
    jmp aria_atomic_int32_fetch_add@PLT
.size npk_atomic_int32_fetch_add, .-npk_atomic_int32_fetch_add

.global npk_atomic_int32_fetch_sub
.type npk_atomic_int32_fetch_sub, @function
npk_atomic_int32_fetch_sub:
    jmp aria_atomic_int32_fetch_sub@PLT
.size npk_atomic_int32_fetch_sub, .-npk_atomic_int32_fetch_sub

.global npk_atomic_int32_load
.type npk_atomic_int32_load, @function
npk_atomic_int32_load:
    jmp aria_atomic_int32_load@PLT
.size npk_atomic_int32_load, .-npk_atomic_int32_load

.global npk_atomic_int32_store
.type npk_atomic_int32_store, @function
npk_atomic_int32_store:
    jmp aria_atomic_int32_store@PLT
.size npk_atomic_int32_store, .-npk_atomic_int32_store

.global npk_atomic_int64_compare_exchange_strong
.type npk_atomic_int64_compare_exchange_strong, @function
npk_atomic_int64_compare_exchange_strong:
    jmp aria_atomic_int64_compare_exchange_strong@PLT
.size npk_atomic_int64_compare_exchange_strong, .-npk_atomic_int64_compare_exchange_strong

.global npk_atomic_int64_create
.type npk_atomic_int64_create, @function
npk_atomic_int64_create:
    jmp aria_atomic_int64_create@PLT
.size npk_atomic_int64_create, .-npk_atomic_int64_create

.global npk_atomic_int64_destroy
.type npk_atomic_int64_destroy, @function
npk_atomic_int64_destroy:
    jmp aria_atomic_int64_destroy@PLT
.size npk_atomic_int64_destroy, .-npk_atomic_int64_destroy

.global npk_atomic_int64_exchange
.type npk_atomic_int64_exchange, @function
npk_atomic_int64_exchange:
    jmp aria_atomic_int64_exchange@PLT
.size npk_atomic_int64_exchange, .-npk_atomic_int64_exchange

.global npk_atomic_int64_fetch_add
.type npk_atomic_int64_fetch_add, @function
npk_atomic_int64_fetch_add:
    jmp aria_atomic_int64_fetch_add@PLT
.size npk_atomic_int64_fetch_add, .-npk_atomic_int64_fetch_add

.global npk_atomic_int64_fetch_sub
.type npk_atomic_int64_fetch_sub, @function
npk_atomic_int64_fetch_sub:
    jmp aria_atomic_int64_fetch_sub@PLT
.size npk_atomic_int64_fetch_sub, .-npk_atomic_int64_fetch_sub

.global npk_atomic_int64_load
.type npk_atomic_int64_load, @function
npk_atomic_int64_load:
    jmp aria_atomic_int64_load@PLT
.size npk_atomic_int64_load, .-npk_atomic_int64_load

.global npk_atomic_int64_store
.type npk_atomic_int64_store, @function
npk_atomic_int64_store:
    jmp aria_atomic_int64_store@PLT
.size npk_atomic_int64_store, .-npk_atomic_int64_store

.global npk_atomic_int8_compare_exchange_strong
.type npk_atomic_int8_compare_exchange_strong, @function
npk_atomic_int8_compare_exchange_strong:
    jmp aria_atomic_int8_compare_exchange_strong@PLT
.size npk_atomic_int8_compare_exchange_strong, .-npk_atomic_int8_compare_exchange_strong

.global npk_atomic_int8_create
.type npk_atomic_int8_create, @function
npk_atomic_int8_create:
    jmp aria_atomic_int8_create@PLT
.size npk_atomic_int8_create, .-npk_atomic_int8_create

.global npk_atomic_int8_destroy
.type npk_atomic_int8_destroy, @function
npk_atomic_int8_destroy:
    jmp aria_atomic_int8_destroy@PLT
.size npk_atomic_int8_destroy, .-npk_atomic_int8_destroy

.global npk_atomic_int8_exchange
.type npk_atomic_int8_exchange, @function
npk_atomic_int8_exchange:
    jmp aria_atomic_int8_exchange@PLT
.size npk_atomic_int8_exchange, .-npk_atomic_int8_exchange

.global npk_atomic_int8_fetch_add
.type npk_atomic_int8_fetch_add, @function
npk_atomic_int8_fetch_add:
    jmp aria_atomic_int8_fetch_add@PLT
.size npk_atomic_int8_fetch_add, .-npk_atomic_int8_fetch_add

.global npk_atomic_int8_fetch_sub
.type npk_atomic_int8_fetch_sub, @function
npk_atomic_int8_fetch_sub:
    jmp aria_atomic_int8_fetch_sub@PLT
.size npk_atomic_int8_fetch_sub, .-npk_atomic_int8_fetch_sub

.global npk_atomic_int8_load
.type npk_atomic_int8_load, @function
npk_atomic_int8_load:
    jmp aria_atomic_int8_load@PLT
.size npk_atomic_int8_load, .-npk_atomic_int8_load

.global npk_atomic_int8_store
.type npk_atomic_int8_store, @function
npk_atomic_int8_store:
    jmp aria_atomic_int8_store@PLT
.size npk_atomic_int8_store, .-npk_atomic_int8_store

.global npk_atomic_is_lock_free_bool
.type npk_atomic_is_lock_free_bool, @function
npk_atomic_is_lock_free_bool:
    jmp aria_atomic_is_lock_free_bool@PLT
.size npk_atomic_is_lock_free_bool, .-npk_atomic_is_lock_free_bool

.global npk_atomic_is_lock_free_int32
.type npk_atomic_is_lock_free_int32, @function
npk_atomic_is_lock_free_int32:
    jmp aria_atomic_is_lock_free_int32@PLT
.size npk_atomic_is_lock_free_int32, .-npk_atomic_is_lock_free_int32

.global npk_atomic_is_lock_free_int64
.type npk_atomic_is_lock_free_int64, @function
npk_atomic_is_lock_free_int64:
    jmp aria_atomic_is_lock_free_int64@PLT
.size npk_atomic_is_lock_free_int64, .-npk_atomic_is_lock_free_int64

.global npk_atomic_is_lock_free_int8
.type npk_atomic_is_lock_free_int8, @function
npk_atomic_is_lock_free_int8:
    jmp aria_atomic_is_lock_free_int8@PLT
.size npk_atomic_is_lock_free_int8, .-npk_atomic_is_lock_free_int8

.global npk_atomic_is_lock_free_ptr
.type npk_atomic_is_lock_free_ptr, @function
npk_atomic_is_lock_free_ptr:
    jmp aria_atomic_is_lock_free_ptr@PLT
.size npk_atomic_is_lock_free_ptr, .-npk_atomic_is_lock_free_ptr

.global npk_atomic_ptr_compare_exchange_strong
.type npk_atomic_ptr_compare_exchange_strong, @function
npk_atomic_ptr_compare_exchange_strong:
    jmp aria_atomic_ptr_compare_exchange_strong@PLT
.size npk_atomic_ptr_compare_exchange_strong, .-npk_atomic_ptr_compare_exchange_strong

.global npk_atomic_ptr_create
.type npk_atomic_ptr_create, @function
npk_atomic_ptr_create:
    jmp aria_atomic_ptr_create@PLT
.size npk_atomic_ptr_create, .-npk_atomic_ptr_create

.global npk_atomic_ptr_destroy
.type npk_atomic_ptr_destroy, @function
npk_atomic_ptr_destroy:
    jmp aria_atomic_ptr_destroy@PLT
.size npk_atomic_ptr_destroy, .-npk_atomic_ptr_destroy

.global npk_atomic_ptr_exchange
.type npk_atomic_ptr_exchange, @function
npk_atomic_ptr_exchange:
    jmp aria_atomic_ptr_exchange@PLT
.size npk_atomic_ptr_exchange, .-npk_atomic_ptr_exchange

.global npk_atomic_ptr_load
.type npk_atomic_ptr_load, @function
npk_atomic_ptr_load:
    jmp aria_atomic_ptr_load@PLT
.size npk_atomic_ptr_load, .-npk_atomic_ptr_load

.global npk_atomic_ptr_store
.type npk_atomic_ptr_store, @function
npk_atomic_ptr_store:
    jmp aria_atomic_ptr_store@PLT
.size npk_atomic_ptr_store, .-npk_atomic_ptr_store

.global npk_atomic_signal_fence
.type npk_atomic_signal_fence, @function
npk_atomic_signal_fence:
    jmp aria_atomic_signal_fence@PLT
.size npk_atomic_signal_fence, .-npk_atomic_signal_fence

.global npk_atomic_tbb32_compare_exchange_strong
.type npk_atomic_tbb32_compare_exchange_strong, @function
npk_atomic_tbb32_compare_exchange_strong:
    jmp aria_atomic_tbb32_compare_exchange_strong@PLT
.size npk_atomic_tbb32_compare_exchange_strong, .-npk_atomic_tbb32_compare_exchange_strong

.global npk_atomic_tbb32_create
.type npk_atomic_tbb32_create, @function
npk_atomic_tbb32_create:
    jmp aria_atomic_tbb32_create@PLT
.size npk_atomic_tbb32_create, .-npk_atomic_tbb32_create

.global npk_atomic_tbb32_destroy
.type npk_atomic_tbb32_destroy, @function
npk_atomic_tbb32_destroy:
    jmp aria_atomic_tbb32_destroy@PLT
.size npk_atomic_tbb32_destroy, .-npk_atomic_tbb32_destroy

.global npk_atomic_tbb32_exchange
.type npk_atomic_tbb32_exchange, @function
npk_atomic_tbb32_exchange:
    jmp aria_atomic_tbb32_exchange@PLT
.size npk_atomic_tbb32_exchange, .-npk_atomic_tbb32_exchange

.global npk_atomic_tbb32_fetch_add
.type npk_atomic_tbb32_fetch_add, @function
npk_atomic_tbb32_fetch_add:
    jmp aria_atomic_tbb32_fetch_add@PLT
.size npk_atomic_tbb32_fetch_add, .-npk_atomic_tbb32_fetch_add

.global npk_atomic_tbb32_fetch_sub
.type npk_atomic_tbb32_fetch_sub, @function
npk_atomic_tbb32_fetch_sub:
    jmp aria_atomic_tbb32_fetch_sub@PLT
.size npk_atomic_tbb32_fetch_sub, .-npk_atomic_tbb32_fetch_sub

.global npk_atomic_tbb32_load
.type npk_atomic_tbb32_load, @function
npk_atomic_tbb32_load:
    jmp aria_atomic_tbb32_load@PLT
.size npk_atomic_tbb32_load, .-npk_atomic_tbb32_load

.global npk_atomic_tbb32_store
.type npk_atomic_tbb32_store, @function
npk_atomic_tbb32_store:
    jmp aria_atomic_tbb32_store@PLT
.size npk_atomic_tbb32_store, .-npk_atomic_tbb32_store

.global npk_atomic_tbb64_compare_exchange_strong
.type npk_atomic_tbb64_compare_exchange_strong, @function
npk_atomic_tbb64_compare_exchange_strong:
    jmp aria_atomic_tbb64_compare_exchange_strong@PLT
.size npk_atomic_tbb64_compare_exchange_strong, .-npk_atomic_tbb64_compare_exchange_strong

.global npk_atomic_tbb64_create
.type npk_atomic_tbb64_create, @function
npk_atomic_tbb64_create:
    jmp aria_atomic_tbb64_create@PLT
.size npk_atomic_tbb64_create, .-npk_atomic_tbb64_create

.global npk_atomic_tbb64_destroy
.type npk_atomic_tbb64_destroy, @function
npk_atomic_tbb64_destroy:
    jmp aria_atomic_tbb64_destroy@PLT
.size npk_atomic_tbb64_destroy, .-npk_atomic_tbb64_destroy

.global npk_atomic_tbb64_exchange
.type npk_atomic_tbb64_exchange, @function
npk_atomic_tbb64_exchange:
    jmp aria_atomic_tbb64_exchange@PLT
.size npk_atomic_tbb64_exchange, .-npk_atomic_tbb64_exchange

.global npk_atomic_tbb64_fetch_add
.type npk_atomic_tbb64_fetch_add, @function
npk_atomic_tbb64_fetch_add:
    jmp aria_atomic_tbb64_fetch_add@PLT
.size npk_atomic_tbb64_fetch_add, .-npk_atomic_tbb64_fetch_add

.global npk_atomic_tbb64_fetch_sub
.type npk_atomic_tbb64_fetch_sub, @function
npk_atomic_tbb64_fetch_sub:
    jmp aria_atomic_tbb64_fetch_sub@PLT
.size npk_atomic_tbb64_fetch_sub, .-npk_atomic_tbb64_fetch_sub

.global npk_atomic_tbb64_load
.type npk_atomic_tbb64_load, @function
npk_atomic_tbb64_load:
    jmp aria_atomic_tbb64_load@PLT
.size npk_atomic_tbb64_load, .-npk_atomic_tbb64_load

.global npk_atomic_tbb64_store
.type npk_atomic_tbb64_store, @function
npk_atomic_tbb64_store:
    jmp aria_atomic_tbb64_store@PLT
.size npk_atomic_tbb64_store, .-npk_atomic_tbb64_store

.global npk_atomic_tbb8_compare_exchange_strong
.type npk_atomic_tbb8_compare_exchange_strong, @function
npk_atomic_tbb8_compare_exchange_strong:
    jmp aria_atomic_tbb8_compare_exchange_strong@PLT
.size npk_atomic_tbb8_compare_exchange_strong, .-npk_atomic_tbb8_compare_exchange_strong

.global npk_atomic_tbb8_create
.type npk_atomic_tbb8_create, @function
npk_atomic_tbb8_create:
    jmp aria_atomic_tbb8_create@PLT
.size npk_atomic_tbb8_create, .-npk_atomic_tbb8_create

.global npk_atomic_tbb8_destroy
.type npk_atomic_tbb8_destroy, @function
npk_atomic_tbb8_destroy:
    jmp aria_atomic_tbb8_destroy@PLT
.size npk_atomic_tbb8_destroy, .-npk_atomic_tbb8_destroy

.global npk_atomic_tbb8_exchange
.type npk_atomic_tbb8_exchange, @function
npk_atomic_tbb8_exchange:
    jmp aria_atomic_tbb8_exchange@PLT
.size npk_atomic_tbb8_exchange, .-npk_atomic_tbb8_exchange

.global npk_atomic_tbb8_fetch_add
.type npk_atomic_tbb8_fetch_add, @function
npk_atomic_tbb8_fetch_add:
    jmp aria_atomic_tbb8_fetch_add@PLT
.size npk_atomic_tbb8_fetch_add, .-npk_atomic_tbb8_fetch_add

.global npk_atomic_tbb8_fetch_sub
.type npk_atomic_tbb8_fetch_sub, @function
npk_atomic_tbb8_fetch_sub:
    jmp aria_atomic_tbb8_fetch_sub@PLT
.size npk_atomic_tbb8_fetch_sub, .-npk_atomic_tbb8_fetch_sub

.global npk_atomic_tbb8_load
.type npk_atomic_tbb8_load, @function
npk_atomic_tbb8_load:
    jmp aria_atomic_tbb8_load@PLT
.size npk_atomic_tbb8_load, .-npk_atomic_tbb8_load

.global npk_atomic_tbb8_store
.type npk_atomic_tbb8_store, @function
npk_atomic_tbb8_store:
    jmp aria_atomic_tbb8_store@PLT
.size npk_atomic_tbb8_store, .-npk_atomic_tbb8_store

.global npk_atomic_thread_fence
.type npk_atomic_thread_fence, @function
npk_atomic_thread_fence:
    jmp aria_atomic_thread_fence@PLT
.size npk_atomic_thread_fence, .-npk_atomic_thread_fence

.global npk_atomic_uint32_create
.type npk_atomic_uint32_create, @function
npk_atomic_uint32_create:
    jmp aria_atomic_uint32_create@PLT
.size npk_atomic_uint32_create, .-npk_atomic_uint32_create

.global npk_atomic_uint32_destroy
.type npk_atomic_uint32_destroy, @function
npk_atomic_uint32_destroy:
    jmp aria_atomic_uint32_destroy@PLT
.size npk_atomic_uint32_destroy, .-npk_atomic_uint32_destroy

.global npk_atomic_uint32_fetch_add
.type npk_atomic_uint32_fetch_add, @function
npk_atomic_uint32_fetch_add:
    jmp aria_atomic_uint32_fetch_add@PLT
.size npk_atomic_uint32_fetch_add, .-npk_atomic_uint32_fetch_add

.global npk_atomic_uint32_load
.type npk_atomic_uint32_load, @function
npk_atomic_uint32_load:
    jmp aria_atomic_uint32_load@PLT
.size npk_atomic_uint32_load, .-npk_atomic_uint32_load

.global npk_atomic_uint32_store
.type npk_atomic_uint32_store, @function
npk_atomic_uint32_store:
    jmp aria_atomic_uint32_store@PLT
.size npk_atomic_uint32_store, .-npk_atomic_uint32_store

.global npk_atomic_uint64_create
.type npk_atomic_uint64_create, @function
npk_atomic_uint64_create:
    jmp aria_atomic_uint64_create@PLT
.size npk_atomic_uint64_create, .-npk_atomic_uint64_create

.global npk_atomic_uint64_destroy
.type npk_atomic_uint64_destroy, @function
npk_atomic_uint64_destroy:
    jmp aria_atomic_uint64_destroy@PLT
.size npk_atomic_uint64_destroy, .-npk_atomic_uint64_destroy

.global npk_atomic_uint64_fetch_add
.type npk_atomic_uint64_fetch_add, @function
npk_atomic_uint64_fetch_add:
    jmp aria_atomic_uint64_fetch_add@PLT
.size npk_atomic_uint64_fetch_add, .-npk_atomic_uint64_fetch_add

.global npk_atomic_uint64_load
.type npk_atomic_uint64_load, @function
npk_atomic_uint64_load:
    jmp aria_atomic_uint64_load@PLT
.size npk_atomic_uint64_load, .-npk_atomic_uint64_load

.global npk_atomic_uint64_store
.type npk_atomic_uint64_store, @function
npk_atomic_uint64_store:
    jmp aria_atomic_uint64_store@PLT
.size npk_atomic_uint64_store, .-npk_atomic_uint64_store

.global npk_atomic_uint8_create
.type npk_atomic_uint8_create, @function
npk_atomic_uint8_create:
    jmp aria_atomic_uint8_create@PLT
.size npk_atomic_uint8_create, .-npk_atomic_uint8_create

.global npk_atomic_uint8_destroy
.type npk_atomic_uint8_destroy, @function
npk_atomic_uint8_destroy:
    jmp aria_atomic_uint8_destroy@PLT
.size npk_atomic_uint8_destroy, .-npk_atomic_uint8_destroy

.global npk_atomic_uint8_fetch_add
.type npk_atomic_uint8_fetch_add, @function
npk_atomic_uint8_fetch_add:
    jmp aria_atomic_uint8_fetch_add@PLT
.size npk_atomic_uint8_fetch_add, .-npk_atomic_uint8_fetch_add

.global npk_atomic_uint8_load
.type npk_atomic_uint8_load, @function
npk_atomic_uint8_load:
    jmp aria_atomic_uint8_load@PLT
.size npk_atomic_uint8_load, .-npk_atomic_uint8_load

.global npk_atomic_uint8_store
.type npk_atomic_uint8_store, @function
npk_atomic_uint8_store:
    jmp aria_atomic_uint8_store@PLT
.size npk_atomic_uint8_store, .-npk_atomic_uint8_store

.global npk_delete_file_simple
.type npk_delete_file_simple, @function
npk_delete_file_simple:
    jmp aria_delete_file_simple@PLT
.size npk_delete_file_simple, .-npk_delete_file_simple

.global npk_env_get_builtin
.type npk_env_get_builtin, @function
npk_env_get_builtin:
    jmp aria_env_get_builtin@PLT
.size npk_env_get_builtin, .-npk_env_get_builtin

.global npk_file_exists
.type npk_file_exists, @function
npk_file_exists:
    jmp aria_file_exists@PLT
.size npk_file_exists, .-npk_file_exists

.global npk_file_size
.type npk_file_size, @function
npk_file_size:
    jmp aria_file_size@PLT
.size npk_file_size, .-npk_file_size

.global npk_fix256_add
.type npk_fix256_add, @function
npk_fix256_add:
    jmp aria_fix256_add@PLT
.size npk_fix256_add, .-npk_fix256_add

.global npk_fix256_cmp
.type npk_fix256_cmp, @function
npk_fix256_cmp:
    jmp aria_fix256_cmp@PLT
.size npk_fix256_cmp, .-npk_fix256_cmp

.global npk_fix256_div
.type npk_fix256_div, @function
npk_fix256_div:
    jmp aria_fix256_div@PLT
.size npk_fix256_div, .-npk_fix256_div

.global npk_fix256_eq
.type npk_fix256_eq, @function
npk_fix256_eq:
    jmp aria_fix256_eq@PLT
.size npk_fix256_eq, .-npk_fix256_eq

.global npk_fix256_from_f64
.type npk_fix256_from_f64, @function
npk_fix256_from_f64:
    jmp aria_fix256_from_f64@PLT
.size npk_fix256_from_f64, .-npk_fix256_from_f64

.global npk_fix256_from_i64
.type npk_fix256_from_i64, @function
npk_fix256_from_i64:
    jmp aria_fix256_from_i64@PLT
.size npk_fix256_from_i64, .-npk_fix256_from_i64

.global npk_fix256_ge
.type npk_fix256_ge, @function
npk_fix256_ge:
    jmp aria_fix256_ge@PLT
.size npk_fix256_ge, .-npk_fix256_ge

.global npk_fix256_gt
.type npk_fix256_gt, @function
npk_fix256_gt:
    jmp aria_fix256_gt@PLT
.size npk_fix256_gt, .-npk_fix256_gt

.global npk_fix256_le
.type npk_fix256_le, @function
npk_fix256_le:
    jmp aria_fix256_le@PLT
.size npk_fix256_le, .-npk_fix256_le

.global npk_fix256_lt
.type npk_fix256_lt, @function
npk_fix256_lt:
    jmp aria_fix256_lt@PLT
.size npk_fix256_lt, .-npk_fix256_lt

.global npk_fix256_mul
.type npk_fix256_mul, @function
npk_fix256_mul:
    jmp aria_fix256_mul@PLT
.size npk_fix256_mul, .-npk_fix256_mul

.global npk_fix256_sub
.type npk_fix256_sub, @function
npk_fix256_sub:
    jmp aria_fix256_sub@PLT
.size npk_fix256_sub, .-npk_fix256_sub

.global npk_fix256_to_f64
.type npk_fix256_to_f64, @function
npk_fix256_to_f64:
    jmp aria_fix256_to_f64@PLT
.size npk_fix256_to_f64, .-npk_fix256_to_f64

.global npk_fix256_to_i64
.type npk_fix256_to_i64, @function
npk_fix256_to_i64:
    jmp aria_fix256_to_i64@PLT
.size npk_fix256_to_i64, .-npk_fix256_to_i64

.global npk_format_fix256
.type npk_format_fix256, @function
npk_format_fix256:
    jmp aria_format_fix256@PLT
.size npk_format_fix256, .-npk_format_fix256

.global npk_format_float64
.type npk_format_float64, @function
npk_format_float64:
    jmp aria_format_float64@PLT
.size npk_format_float64, .-npk_format_float64

.global npk_format_int1024
.type npk_format_int1024, @function
npk_format_int1024:
    jmp aria_format_int1024@PLT
.size npk_format_int1024, .-npk_format_int1024

.global npk_format_int128
.type npk_format_int128, @function
npk_format_int128:
    jmp aria_format_int128@PLT
.size npk_format_int128, .-npk_format_int128

.global npk_format_int2048
.type npk_format_int2048, @function
npk_format_int2048:
    jmp aria_format_int2048@PLT
.size npk_format_int2048, .-npk_format_int2048

.global npk_format_int256
.type npk_format_int256, @function
npk_format_int256:
    jmp aria_format_int256@PLT
.size npk_format_int256, .-npk_format_int256

.global npk_format_int4096
.type npk_format_int4096, @function
npk_format_int4096:
    jmp aria_format_int4096@PLT
.size npk_format_int4096, .-npk_format_int4096

.global npk_format_int512
.type npk_format_int512, @function
npk_format_int512:
    jmp aria_format_int512@PLT
.size npk_format_int512, .-npk_format_int512

.global npk_format_int64
.type npk_format_int64, @function
npk_format_int64:
    jmp aria_format_int64@PLT
.size npk_format_int64, .-npk_format_int64

.global npk_format_nit
.type npk_format_nit, @function
npk_format_nit:
    jmp aria_format_nit@PLT
.size npk_format_nit, .-npk_format_nit

.global npk_format_nyte
.type npk_format_nyte, @function
npk_format_nyte:
    jmp aria_format_nyte@PLT
.size npk_format_nyte, .-npk_format_nyte

.global npk_format_tbb16
.type npk_format_tbb16, @function
npk_format_tbb16:
    jmp aria_format_tbb16@PLT
.size npk_format_tbb16, .-npk_format_tbb16

.global npk_format_tbb32
.type npk_format_tbb32, @function
npk_format_tbb32:
    jmp aria_format_tbb32@PLT
.size npk_format_tbb32, .-npk_format_tbb32

.global npk_format_tbb64
.type npk_format_tbb64, @function
npk_format_tbb64:
    jmp aria_format_tbb64@PLT
.size npk_format_tbb64, .-npk_format_tbb64

.global npk_format_tbb8
.type npk_format_tbb8, @function
npk_format_tbb8:
    jmp aria_format_tbb8@PLT
.size npk_format_tbb8, .-npk_format_tbb8

.global npk_format_trit
.type npk_format_trit, @function
npk_format_trit:
    jmp aria_format_trit@PLT
.size npk_format_trit, .-npk_format_trit

.global npk_format_tryte
.type npk_format_tryte, @function
npk_format_tryte:
    jmp aria_format_tryte@PLT
.size npk_format_tryte, .-npk_format_tryte

.global npk_format_uint1024
.type npk_format_uint1024, @function
npk_format_uint1024:
    jmp aria_format_uint1024@PLT
.size npk_format_uint1024, .-npk_format_uint1024

.global npk_format_uint128
.type npk_format_uint128, @function
npk_format_uint128:
    jmp aria_format_uint128@PLT
.size npk_format_uint128, .-npk_format_uint128

.global npk_format_uint2048
.type npk_format_uint2048, @function
npk_format_uint2048:
    jmp aria_format_uint2048@PLT
.size npk_format_uint2048, .-npk_format_uint2048

.global npk_format_uint256
.type npk_format_uint256, @function
npk_format_uint256:
    jmp aria_format_uint256@PLT
.size npk_format_uint256, .-npk_format_uint256

.global npk_format_uint4096
.type npk_format_uint4096, @function
npk_format_uint4096:
    jmp aria_format_uint4096@PLT
.size npk_format_uint4096, .-npk_format_uint4096

.global npk_format_uint512
.type npk_format_uint512, @function
npk_format_uint512:
    jmp aria_format_uint512@PLT
.size npk_format_uint512, .-npk_format_uint512

.global npk_free
.type npk_free, @function
npk_free:
    jmp aria_free@PLT
.size npk_free, .-npk_free

.global npk_gc_alloc
.type npk_gc_alloc, @function
npk_gc_alloc:
    jmp aria_gc_alloc@PLT
.size npk_gc_alloc, .-npk_gc_alloc

.global npk_gc_free
.type npk_gc_free, @function
npk_gc_free:
    jmp aria_gc_free@PLT
.size npk_gc_free, .-npk_gc_free

.global npk_gc_init
.type npk_gc_init, @function
npk_gc_init:
    jmp aria_gc_init@PLT
.size npk_gc_init, .-npk_gc_init

.global npk_gc_safepoint
.type npk_gc_safepoint, @function
npk_gc_safepoint:
    jmp aria_gc_safepoint@PLT
.size npk_gc_safepoint, .-npk_gc_safepoint

.global npk_int64_to_str
.type npk_int64_to_str, @function
npk_int64_to_str:
    jmp aria_int64_to_str@PLT
.size npk_int64_to_str, .-npk_int64_to_str

.global npk_lbim_pow1024
.type npk_lbim_pow1024, @function
npk_lbim_pow1024:
    jmp aria_lbim_pow1024@PLT
.size npk_lbim_pow1024, .-npk_lbim_pow1024

.global npk_lbim_pow128
.type npk_lbim_pow128, @function
npk_lbim_pow128:
    jmp aria_lbim_pow128@PLT
.size npk_lbim_pow128, .-npk_lbim_pow128

.global npk_lbim_pow256
.type npk_lbim_pow256, @function
npk_lbim_pow256:
    jmp aria_lbim_pow256@PLT
.size npk_lbim_pow256, .-npk_lbim_pow256

.global npk_lbim_pow512
.type npk_lbim_pow512, @function
npk_lbim_pow512:
    jmp aria_lbim_pow512@PLT
.size npk_lbim_pow512, .-npk_lbim_pow512

.global npk_lbim_scmp2048
.type npk_lbim_scmp2048, @function
npk_lbim_scmp2048:
    jmp aria_lbim_scmp2048@PLT
.size npk_lbim_scmp2048, .-npk_lbim_scmp2048

.global npk_lbim_scmp4096
.type npk_lbim_scmp4096, @function
npk_lbim_scmp4096:
    jmp aria_lbim_scmp4096@PLT
.size npk_lbim_scmp4096, .-npk_lbim_scmp4096

.global npk_lbim_sdiv1024
.type npk_lbim_sdiv1024, @function
npk_lbim_sdiv1024:
    jmp aria_lbim_sdiv1024@PLT
.size npk_lbim_sdiv1024, .-npk_lbim_sdiv1024

.global npk_lbim_sdiv128
.type npk_lbim_sdiv128, @function
npk_lbim_sdiv128:
    jmp aria_lbim_sdiv128@PLT
.size npk_lbim_sdiv128, .-npk_lbim_sdiv128

.global npk_lbim_sdiv2048
.type npk_lbim_sdiv2048, @function
npk_lbim_sdiv2048:
    jmp aria_lbim_sdiv2048@PLT
.size npk_lbim_sdiv2048, .-npk_lbim_sdiv2048

.global npk_lbim_sdiv256
.type npk_lbim_sdiv256, @function
npk_lbim_sdiv256:
    jmp aria_lbim_sdiv256@PLT
.size npk_lbim_sdiv256, .-npk_lbim_sdiv256

.global npk_lbim_sdiv4096
.type npk_lbim_sdiv4096, @function
npk_lbim_sdiv4096:
    jmp aria_lbim_sdiv4096@PLT
.size npk_lbim_sdiv4096, .-npk_lbim_sdiv4096

.global npk_lbim_sdiv512
.type npk_lbim_sdiv512, @function
npk_lbim_sdiv512:
    jmp aria_lbim_sdiv512@PLT
.size npk_lbim_sdiv512, .-npk_lbim_sdiv512

.global npk_lbim_smod1024
.type npk_lbim_smod1024, @function
npk_lbim_smod1024:
    jmp aria_lbim_smod1024@PLT
.size npk_lbim_smod1024, .-npk_lbim_smod1024

.global npk_lbim_smod128
.type npk_lbim_smod128, @function
npk_lbim_smod128:
    jmp aria_lbim_smod128@PLT
.size npk_lbim_smod128, .-npk_lbim_smod128

.global npk_lbim_smod2048
.type npk_lbim_smod2048, @function
npk_lbim_smod2048:
    jmp aria_lbim_smod2048@PLT
.size npk_lbim_smod2048, .-npk_lbim_smod2048

.global npk_lbim_smod256
.type npk_lbim_smod256, @function
npk_lbim_smod256:
    jmp aria_lbim_smod256@PLT
.size npk_lbim_smod256, .-npk_lbim_smod256

.global npk_lbim_smod4096
.type npk_lbim_smod4096, @function
npk_lbim_smod4096:
    jmp aria_lbim_smod4096@PLT
.size npk_lbim_smod4096, .-npk_lbim_smod4096

.global npk_lbim_smod512
.type npk_lbim_smod512, @function
npk_lbim_smod512:
    jmp aria_lbim_smod512@PLT
.size npk_lbim_smod512, .-npk_lbim_smod512

.global npk_map_get_simple
.type npk_map_get_simple, @function
npk_map_get_simple:
    jmp aria_map_get_simple@PLT
.size npk_map_get_simple, .-npk_map_get_simple

.global npk_map_has
.type npk_map_has, @function
npk_map_has:
    jmp aria_map_has@PLT
.size npk_map_has, .-npk_map_has

.global npk_map_insert_simple
.type npk_map_insert_simple, @function
npk_map_insert_simple:
    jmp aria_map_insert_simple@PLT
.size npk_map_insert_simple, .-npk_map_insert_simple

.global npk_map_length
.type npk_map_length, @function
npk_map_length:
    jmp aria_map_length@PLT
.size npk_map_length, .-npk_map_length

.global npk_map_new_simple
.type npk_map_new_simple, @function
npk_map_new_simple:
    jmp aria_map_new_simple@PLT
.size npk_map_new_simple, .-npk_map_new_simple

.global npk_map_remove
.type npk_map_remove, @function
npk_map_remove:
    jmp aria_map_remove@PLT
.size npk_map_remove, .-npk_map_remove

.global npk_nit_add
.type npk_nit_add, @function
npk_nit_add:
    jmp aria_nit_add@PLT
.size npk_nit_add, .-npk_nit_add

.global npk_nit_and
.type npk_nit_and, @function
npk_nit_and:
    jmp aria_nit_and@PLT
.size npk_nit_and, .-npk_nit_and

.global npk_nit_is_err
.type npk_nit_is_err, @function
npk_nit_is_err:
    jmp aria_nit_is_err@PLT
.size npk_nit_is_err, .-npk_nit_is_err

.global npk_nit_mul
.type npk_nit_mul, @function
npk_nit_mul:
    jmp aria_nit_mul@PLT
.size npk_nit_mul, .-npk_nit_mul

.global npk_nit_neg
.type npk_nit_neg, @function
npk_nit_neg:
    jmp aria_nit_neg@PLT
.size npk_nit_neg, .-npk_nit_neg

.global npk_nit_not
.type npk_nit_not, @function
npk_nit_not:
    jmp aria_nit_not@PLT
.size npk_nit_not, .-npk_nit_not

.global npk_nit_or
.type npk_nit_or, @function
npk_nit_or:
    jmp aria_nit_or@PLT
.size npk_nit_or, .-npk_nit_or

.global npk_nit_sub
.type npk_nit_sub, @function
npk_nit_sub:
    jmp aria_nit_sub@PLT
.size npk_nit_sub, .-npk_nit_sub

.global npk_nyte_add
.type npk_nyte_add, @function
npk_nyte_add:
    jmp aria_nyte_add@PLT
.size npk_nyte_add, .-npk_nyte_add

.global npk_nyte_cmp
.type npk_nyte_cmp, @function
npk_nyte_cmp:
    jmp aria_nyte_cmp@PLT
.size npk_nyte_cmp, .-npk_nyte_cmp

.global npk_nyte_div
.type npk_nyte_div, @function
npk_nyte_div:
    jmp aria_nyte_div@PLT
.size npk_nyte_div, .-npk_nyte_div

.global npk_nyte_get_nit
.type npk_nyte_get_nit, @function
npk_nyte_get_nit:
    jmp aria_nyte_get_nit@PLT
.size npk_nyte_get_nit, .-npk_nyte_get_nit

.global npk_nyte_mod
.type npk_nyte_mod, @function
npk_nyte_mod:
    jmp aria_nyte_mod@PLT
.size npk_nyte_mod, .-npk_nyte_mod

.global npk_nyte_mul
.type npk_nyte_mul, @function
npk_nyte_mul:
    jmp aria_nyte_mul@PLT
.size npk_nyte_mul, .-npk_nyte_mul

.global npk_nyte_neg
.type npk_nyte_neg, @function
npk_nyte_neg:
    jmp aria_nyte_neg@PLT
.size npk_nyte_neg, .-npk_nyte_neg

.global npk_nyte_sub
.type npk_nyte_sub, @function
npk_nyte_sub:
    jmp aria_nyte_sub@PLT
.size npk_nyte_sub, .-npk_nyte_sub

.global npk_nyte_to_bin
.type npk_nyte_to_bin, @function
npk_nyte_to_bin:
    jmp aria_nyte_to_bin@PLT
.size npk_nyte_to_bin, .-npk_nyte_to_bin

.global npk_panic_oom
.type npk_panic_oom, @function
npk_panic_oom:
    jmp aria_panic_oom@PLT
.size npk_panic_oom, .-npk_panic_oom

.global npk_panic_overflow
.type npk_panic_overflow, @function
npk_panic_overflow:
    jmp aria_panic_overflow@PLT
.size npk_panic_overflow, .-npk_panic_overflow

.global npk_pool_alloc_handle
.type npk_pool_alloc_handle, @function
npk_pool_alloc_handle:
    jmp aria_pool_alloc_handle@PLT
.size npk_pool_alloc_handle, .-npk_pool_alloc_handle

.global npk_pool_destroy_handle
.type npk_pool_destroy_handle, @function
npk_pool_destroy_handle:
    jmp aria_pool_destroy_handle@PLT
.size npk_pool_destroy_handle, .-npk_pool_destroy_handle

.global npk_pool_free_handle
.type npk_pool_free_handle, @function
npk_pool_free_handle:
    jmp aria_pool_free_handle@PLT
.size npk_pool_free_handle, .-npk_pool_free_handle

.global npk_pool_get_total_blocks_handle
.type npk_pool_get_total_blocks_handle, @function
npk_pool_get_total_blocks_handle:
    jmp aria_pool_get_total_blocks_handle@PLT
.size npk_pool_get_total_blocks_handle, .-npk_pool_get_total_blocks_handle

.global npk_pool_get_used_blocks_handle
.type npk_pool_get_used_blocks_handle, @function
npk_pool_get_used_blocks_handle:
    jmp aria_pool_get_used_blocks_handle@PLT
.size npk_pool_get_used_blocks_handle, .-npk_pool_get_used_blocks_handle

.global npk_pool_new_handle
.type npk_pool_new_handle, @function
npk_pool_new_handle:
    jmp aria_pool_new_handle@PLT
.size npk_pool_new_handle, .-npk_pool_new_handle

.global npk_pool_reset_handle
.type npk_pool_reset_handle, @function
npk_pool_reset_handle:
    jmp aria_pool_reset_handle@PLT
.size npk_pool_reset_handle, .-npk_pool_reset_handle

.global npk_print_cstr
.type npk_print_cstr, @function
npk_print_cstr:
    jmp aria_print_cstr@PLT
.size npk_print_cstr, .-npk_print_cstr

.global npk_println_cstr
.type npk_println_cstr, @function
npk_println_cstr:
    jmp aria_println_cstr@PLT
.size npk_println_cstr, .-npk_println_cstr

.global npk_read_file_simple
.type npk_read_file_simple, @function
npk_read_file_simple:
    jmp aria_read_file_simple@PLT
.size npk_read_file_simple, .-npk_read_file_simple

.global npk_realloc
.type npk_realloc, @function
npk_realloc:
    jmp aria_realloc@PLT
.size npk_realloc, .-npk_realloc

.global npk_slab_cache_alloc_handle
.type npk_slab_cache_alloc_handle, @function
npk_slab_cache_alloc_handle:
    jmp aria_slab_cache_alloc_handle@PLT
.size npk_slab_cache_alloc_handle, .-npk_slab_cache_alloc_handle

.global npk_slab_cache_destroy_handle
.type npk_slab_cache_destroy_handle, @function
npk_slab_cache_destroy_handle:
    jmp aria_slab_cache_destroy_handle@PLT
.size npk_slab_cache_destroy_handle, .-npk_slab_cache_destroy_handle

.global npk_slab_cache_free_handle
.type npk_slab_cache_free_handle, @function
npk_slab_cache_free_handle:
    jmp aria_slab_cache_free_handle@PLT
.size npk_slab_cache_free_handle, .-npk_slab_cache_free_handle

.global npk_slab_cache_get_allocated_objects_handle
.type npk_slab_cache_get_allocated_objects_handle, @function
npk_slab_cache_get_allocated_objects_handle:
    jmp aria_slab_cache_get_allocated_objects_handle@PLT
.size npk_slab_cache_get_allocated_objects_handle, .-npk_slab_cache_get_allocated_objects_handle

.global npk_slab_cache_get_total_objects_handle
.type npk_slab_cache_get_total_objects_handle, @function
npk_slab_cache_get_total_objects_handle:
    jmp aria_slab_cache_get_total_objects_handle@PLT
.size npk_slab_cache_get_total_objects_handle, .-npk_slab_cache_get_total_objects_handle

.global npk_slab_cache_new_handle
.type npk_slab_cache_new_handle, @function
npk_slab_cache_new_handle:
    jmp aria_slab_cache_new_handle@PLT
.size npk_slab_cache_new_handle, .-npk_slab_cache_new_handle

.global npk_sleep_ms
.type npk_sleep_ms, @function
npk_sleep_ms:
    jmp aria_sleep_ms@PLT
.size npk_sleep_ms, .-npk_sleep_ms

.global npk_snprintf_c_locale
.type npk_snprintf_c_locale, @function
npk_snprintf_c_locale:
    jmp aria_snprintf_c_locale@PLT
.size npk_snprintf_c_locale, .-npk_snprintf_c_locale

.global npk_sort_lines
.type npk_sort_lines, @function
npk_sort_lines:
    jmp aria_sort_lines@PLT
.size npk_sort_lines, .-npk_sort_lines

.global npk_stddbg_write
.type npk_stddbg_write, @function
npk_stddbg_write:
    jmp aria_stddbg_write@PLT
.size npk_stddbg_write, .-npk_stddbg_write

.global npk_stderr_write
.type npk_stderr_write, @function
npk_stderr_write:
    jmp aria_stderr_write@PLT
.size npk_stderr_write, .-npk_stderr_write

.global npk_stdin_read_all
.type npk_stdin_read_all, @function
npk_stdin_read_all:
    jmp aria_stdin_read_all@PLT
.size npk_stdin_read_all, .-npk_stdin_read_all

.global npk_stdin_read_line
.type npk_stdin_read_line, @function
npk_stdin_read_line:
    jmp aria_stdin_read_line@PLT
.size npk_stdin_read_line, .-npk_stdin_read_line

.global npk_stdout_write
.type npk_stdout_write, @function
npk_stdout_write:
    jmp aria_stdout_write@PLT
.size npk_stdout_write, .-npk_stdout_write

.global npk_streams_init
.type npk_streams_init, @function
npk_streams_init:
    jmp aria_streams_init@PLT
.size npk_streams_init, .-npk_streams_init

.global npk_string_compare_str
.type npk_string_compare_str, @function
npk_string_compare_str:
    jmp aria_string_compare_str@PLT
.size npk_string_compare_str, .-npk_string_compare_str

.global npk_string_concat_n_simple
.type npk_string_concat_n_simple, @function
npk_string_concat_n_simple:
    jmp aria_string_concat_n_simple@PLT
.size npk_string_concat_n_simple, .-npk_string_concat_n_simple

.global npk_string_concat_simple
.type npk_string_concat_simple, @function
npk_string_concat_simple:
    jmp aria_string_concat_simple@PLT
.size npk_string_concat_simple, .-npk_string_concat_simple

.global npk_string_contains
.type npk_string_contains, @function
npk_string_contains:
    jmp aria_string_contains@PLT
.size npk_string_contains, .-npk_string_contains

.global npk_string_ends_with
.type npk_string_ends_with, @function
npk_string_ends_with:
    jmp aria_string_ends_with@PLT
.size npk_string_ends_with, .-npk_string_ends_with

.global npk_string_equals
.type npk_string_equals, @function
npk_string_equals:
    jmp aria_string_equals@PLT
.size npk_string_equals, .-npk_string_equals

.global npk_string_format_float_simple
.type npk_string_format_float_simple, @function
npk_string_format_float_simple:
    jmp aria_string_format_float_simple@PLT
.size npk_string_format_float_simple, .-npk_string_format_float_simple

.global npk_string_from_char_simple
.type npk_string_from_char_simple, @function
npk_string_from_char_simple:
    jmp aria_string_from_char_simple@PLT
.size npk_string_from_char_simple, .-npk_string_from_char_simple

.global npk_string_from_cstr_simple
.type npk_string_from_cstr_simple, @function
npk_string_from_cstr_simple:
    jmp aria_string_from_cstr_simple@PLT
.size npk_string_from_cstr_simple, .-npk_string_from_cstr_simple

.global npk_string_from_int_hex_simple
.type npk_string_from_int_hex_simple, @function
npk_string_from_int_hex_simple:
    jmp aria_string_from_int_hex_simple@PLT
.size npk_string_from_int_hex_simple, .-npk_string_from_int_hex_simple

.global npk_string_from_int_simple
.type npk_string_from_int_simple, @function
npk_string_from_int_simple:
    jmp aria_string_from_int_simple@PLT
.size npk_string_from_int_simple, .-npk_string_from_int_simple

.global npk_string_index_of_simple
.type npk_string_index_of_simple, @function
npk_string_index_of_simple:
    jmp aria_string_index_of_simple@PLT
.size npk_string_index_of_simple, .-npk_string_index_of_simple

.global npk_string_pad_left_simple
.type npk_string_pad_left_simple, @function
npk_string_pad_left_simple:
    jmp aria_string_pad_left_simple@PLT
.size npk_string_pad_left_simple, .-npk_string_pad_left_simple

.global npk_string_pad_right_simple
.type npk_string_pad_right_simple, @function
npk_string_pad_right_simple:
    jmp aria_string_pad_right_simple@PLT
.size npk_string_pad_right_simple, .-npk_string_pad_right_simple

.global npk_string_repeat_simple
.type npk_string_repeat_simple, @function
npk_string_repeat_simple:
    jmp aria_string_repeat_simple@PLT
.size npk_string_repeat_simple, .-npk_string_repeat_simple

.global npk_string_starts_with
.type npk_string_starts_with, @function
npk_string_starts_with:
    jmp aria_string_starts_with@PLT
.size npk_string_starts_with, .-npk_string_starts_with

.global npk_string_substring_simple
.type npk_string_substring_simple, @function
npk_string_substring_simple:
    jmp aria_string_substring_simple@PLT
.size npk_string_substring_simple, .-npk_string_substring_simple

.global npk_string_to_hex_simple
.type npk_string_to_hex_simple, @function
npk_string_to_hex_simple:
    jmp aria_string_to_hex_simple@PLT
.size npk_string_to_hex_simple, .-npk_string_to_hex_simple

.global npk_string_to_int_simple
.type npk_string_to_int_simple, @function
npk_string_to_int_simple:
    jmp aria_string_to_int_simple@PLT
.size npk_string_to_int_simple, .-npk_string_to_int_simple

.global npk_string_to_lower_simple
.type npk_string_to_lower_simple, @function
npk_string_to_lower_simple:
    jmp aria_string_to_lower_simple@PLT
.size npk_string_to_lower_simple, .-npk_string_to_lower_simple

.global npk_string_to_upper_simple
.type npk_string_to_upper_simple, @function
npk_string_to_upper_simple:
    jmp aria_string_to_upper_simple@PLT
.size npk_string_to_upper_simple, .-npk_string_to_upper_simple

.global npk_string_trim_end_simple
.type npk_string_trim_end_simple, @function
npk_string_trim_end_simple:
    jmp aria_string_trim_end_simple@PLT
.size npk_string_trim_end_simple, .-npk_string_trim_end_simple

.global npk_string_trim_simple
.type npk_string_trim_simple, @function
npk_string_trim_simple:
    jmp aria_string_trim_simple@PLT
.size npk_string_trim_simple, .-npk_string_trim_simple

.global npk_string_trim_start_simple
.type npk_string_trim_start_simple, @function
npk_string_trim_start_simple:
    jmp aria_string_trim_start_simple@PLT
.size npk_string_trim_start_simple, .-npk_string_trim_start_simple

.global npk_tbb8_from_int
.type npk_tbb8_from_int, @function
npk_tbb8_from_int:
    jmp aria_tbb8_from_int@PLT
.size npk_tbb8_from_int, .-npk_tbb8_from_int

.global npk_tbb8_to_int
.type npk_tbb8_to_int, @function
npk_tbb8_to_int:
    jmp aria_tbb8_to_int@PLT
.size npk_tbb8_to_int, .-npk_tbb8_to_int

.global npk_tfp32_add
.type npk_tfp32_add, @function
npk_tfp32_add:
    jmp aria_tfp32_add@PLT
.size npk_tfp32_add, .-npk_tfp32_add

.global npk_tfp32_cmp
.type npk_tfp32_cmp, @function
npk_tfp32_cmp:
    jmp aria_tfp32_cmp@PLT
.size npk_tfp32_cmp, .-npk_tfp32_cmp

.global npk_tfp32_cos
.type npk_tfp32_cos, @function
npk_tfp32_cos:
    jmp aria_tfp32_cos@PLT
.size npk_tfp32_cos, .-npk_tfp32_cos

.global npk_tfp32_div
.type npk_tfp32_div, @function
npk_tfp32_div:
    jmp aria_tfp32_div@PLT
.size npk_tfp32_div, .-npk_tfp32_div

.global npk_tfp32_err
.type npk_tfp32_err, @function
npk_tfp32_err:
    jmp aria_tfp32_err@PLT
.size npk_tfp32_err, .-npk_tfp32_err

.global npk_tfp32_exp
.type npk_tfp32_exp, @function
npk_tfp32_exp:
    jmp aria_tfp32_exp@PLT
.size npk_tfp32_exp, .-npk_tfp32_exp

.global npk_tfp32_from_double
.type npk_tfp32_from_double, @function
npk_tfp32_from_double:
    jmp aria_tfp32_from_double@PLT
.size npk_tfp32_from_double, .-npk_tfp32_from_double

.global npk_tfp32_from_parts
.type npk_tfp32_from_parts, @function
npk_tfp32_from_parts:
    jmp aria_tfp32_from_parts@PLT
.size npk_tfp32_from_parts, .-npk_tfp32_from_parts

.global npk_tfp32_is_err
.type npk_tfp32_is_err, @function
npk_tfp32_is_err:
    jmp aria_tfp32_is_err@PLT
.size npk_tfp32_is_err, .-npk_tfp32_is_err

.global npk_tfp32_is_zero
.type npk_tfp32_is_zero, @function
npk_tfp32_is_zero:
    jmp aria_tfp32_is_zero@PLT
.size npk_tfp32_is_zero, .-npk_tfp32_is_zero

.global npk_tfp32_log
.type npk_tfp32_log, @function
npk_tfp32_log:
    jmp aria_tfp32_log@PLT
.size npk_tfp32_log, .-npk_tfp32_log

.global npk_tfp32_mul
.type npk_tfp32_mul, @function
npk_tfp32_mul:
    jmp aria_tfp32_mul@PLT
.size npk_tfp32_mul, .-npk_tfp32_mul

.global npk_tfp32_neg
.type npk_tfp32_neg, @function
npk_tfp32_neg:
    jmp aria_tfp32_neg@PLT
.size npk_tfp32_neg, .-npk_tfp32_neg

.global npk_tfp32_pow
.type npk_tfp32_pow, @function
npk_tfp32_pow:
    jmp aria_tfp32_pow@PLT
.size npk_tfp32_pow, .-npk_tfp32_pow

.global npk_tfp32_sin
.type npk_tfp32_sin, @function
npk_tfp32_sin:
    jmp aria_tfp32_sin@PLT
.size npk_tfp32_sin, .-npk_tfp32_sin

.global npk_tfp32_sqrt
.type npk_tfp32_sqrt, @function
npk_tfp32_sqrt:
    jmp aria_tfp32_sqrt@PLT
.size npk_tfp32_sqrt, .-npk_tfp32_sqrt

.global npk_tfp32_sub
.type npk_tfp32_sub, @function
npk_tfp32_sub:
    jmp aria_tfp32_sub@PLT
.size npk_tfp32_sub, .-npk_tfp32_sub

.global npk_tfp32_to_double
.type npk_tfp32_to_double, @function
npk_tfp32_to_double:
    jmp aria_tfp32_to_double@PLT
.size npk_tfp32_to_double, .-npk_tfp32_to_double

.global npk_tfp32_to_string
.type npk_tfp32_to_string, @function
npk_tfp32_to_string:
    jmp aria_tfp32_to_string@PLT
.size npk_tfp32_to_string, .-npk_tfp32_to_string

.global npk_tfp32_zero
.type npk_tfp32_zero, @function
npk_tfp32_zero:
    jmp aria_tfp32_zero@PLT
.size npk_tfp32_zero, .-npk_tfp32_zero

.global npk_tfp64_add
.type npk_tfp64_add, @function
npk_tfp64_add:
    jmp aria_tfp64_add@PLT
.size npk_tfp64_add, .-npk_tfp64_add

.global npk_tfp64_cmp
.type npk_tfp64_cmp, @function
npk_tfp64_cmp:
    jmp aria_tfp64_cmp@PLT
.size npk_tfp64_cmp, .-npk_tfp64_cmp

.global npk_tfp64_cos
.type npk_tfp64_cos, @function
npk_tfp64_cos:
    jmp aria_tfp64_cos@PLT
.size npk_tfp64_cos, .-npk_tfp64_cos

.global npk_tfp64_div
.type npk_tfp64_div, @function
npk_tfp64_div:
    jmp aria_tfp64_div@PLT
.size npk_tfp64_div, .-npk_tfp64_div

.global npk_tfp64_err
.type npk_tfp64_err, @function
npk_tfp64_err:
    jmp aria_tfp64_err@PLT
.size npk_tfp64_err, .-npk_tfp64_err

.global npk_tfp64_exp
.type npk_tfp64_exp, @function
npk_tfp64_exp:
    jmp aria_tfp64_exp@PLT
.size npk_tfp64_exp, .-npk_tfp64_exp

.global npk_tfp64_from_double
.type npk_tfp64_from_double, @function
npk_tfp64_from_double:
    jmp aria_tfp64_from_double@PLT
.size npk_tfp64_from_double, .-npk_tfp64_from_double

.global npk_tfp64_from_parts
.type npk_tfp64_from_parts, @function
npk_tfp64_from_parts:
    jmp aria_tfp64_from_parts@PLT
.size npk_tfp64_from_parts, .-npk_tfp64_from_parts

.global npk_tfp64_is_err
.type npk_tfp64_is_err, @function
npk_tfp64_is_err:
    jmp aria_tfp64_is_err@PLT
.size npk_tfp64_is_err, .-npk_tfp64_is_err

.global npk_tfp64_is_zero
.type npk_tfp64_is_zero, @function
npk_tfp64_is_zero:
    jmp aria_tfp64_is_zero@PLT
.size npk_tfp64_is_zero, .-npk_tfp64_is_zero

.global npk_tfp64_log
.type npk_tfp64_log, @function
npk_tfp64_log:
    jmp aria_tfp64_log@PLT
.size npk_tfp64_log, .-npk_tfp64_log

.global npk_tfp64_mul
.type npk_tfp64_mul, @function
npk_tfp64_mul:
    jmp aria_tfp64_mul@PLT
.size npk_tfp64_mul, .-npk_tfp64_mul

.global npk_tfp64_neg
.type npk_tfp64_neg, @function
npk_tfp64_neg:
    jmp aria_tfp64_neg@PLT
.size npk_tfp64_neg, .-npk_tfp64_neg

.global npk_tfp64_pow
.type npk_tfp64_pow, @function
npk_tfp64_pow:
    jmp aria_tfp64_pow@PLT
.size npk_tfp64_pow, .-npk_tfp64_pow

.global npk_tfp64_sin
.type npk_tfp64_sin, @function
npk_tfp64_sin:
    jmp aria_tfp64_sin@PLT
.size npk_tfp64_sin, .-npk_tfp64_sin

.global npk_tfp64_sqrt
.type npk_tfp64_sqrt, @function
npk_tfp64_sqrt:
    jmp aria_tfp64_sqrt@PLT
.size npk_tfp64_sqrt, .-npk_tfp64_sqrt

.global npk_tfp64_sub
.type npk_tfp64_sub, @function
npk_tfp64_sub:
    jmp aria_tfp64_sub@PLT
.size npk_tfp64_sub, .-npk_tfp64_sub

.global npk_tfp64_to_double
.type npk_tfp64_to_double, @function
npk_tfp64_to_double:
    jmp aria_tfp64_to_double@PLT
.size npk_tfp64_to_double, .-npk_tfp64_to_double

.global npk_tfp64_to_string
.type npk_tfp64_to_string, @function
npk_tfp64_to_string:
    jmp aria_tfp64_to_string@PLT
.size npk_tfp64_to_string, .-npk_tfp64_to_string

.global npk_tfp64_zero
.type npk_tfp64_zero, @function
npk_tfp64_zero:
    jmp aria_tfp64_zero@PLT
.size npk_tfp64_zero, .-npk_tfp64_zero

.global npk_trit_add
.type npk_trit_add, @function
npk_trit_add:
    jmp aria_trit_add@PLT
.size npk_trit_add, .-npk_trit_add

.global npk_trit_add_carry
.type npk_trit_add_carry, @function
npk_trit_add_carry:
    jmp aria_trit_add_carry@PLT
.size npk_trit_add_carry, .-npk_trit_add_carry

.global npk_trit_and
.type npk_trit_and, @function
npk_trit_and:
    jmp aria_trit_and@PLT
.size npk_trit_and, .-npk_trit_and

.global npk_trit_mul
.type npk_trit_mul, @function
npk_trit_mul:
    jmp aria_trit_mul@PLT
.size npk_trit_mul, .-npk_trit_mul

.global npk_trit_neg
.type npk_trit_neg, @function
npk_trit_neg:
    jmp aria_trit_neg@PLT
.size npk_trit_neg, .-npk_trit_neg

.global npk_trit_not
.type npk_trit_not, @function
npk_trit_not:
    jmp aria_trit_not@PLT
.size npk_trit_not, .-npk_trit_not

.global npk_trit_or
.type npk_trit_or, @function
npk_trit_or:
    jmp aria_trit_or@PLT
.size npk_trit_or, .-npk_trit_or

.global npk_trit_sub
.type npk_trit_sub, @function
npk_trit_sub:
    jmp aria_trit_sub@PLT
.size npk_trit_sub, .-npk_trit_sub

.global npk_tryte_add
.type npk_tryte_add, @function
npk_tryte_add:
    jmp aria_tryte_add@PLT
.size npk_tryte_add, .-npk_tryte_add

.global npk_tryte_cmp
.type npk_tryte_cmp, @function
npk_tryte_cmp:
    jmp aria_tryte_cmp@PLT
.size npk_tryte_cmp, .-npk_tryte_cmp

.global npk_tryte_div
.type npk_tryte_div, @function
npk_tryte_div:
    jmp aria_tryte_div@PLT
.size npk_tryte_div, .-npk_tryte_div

.global npk_tryte_get_trit
.type npk_tryte_get_trit, @function
npk_tryte_get_trit:
    jmp aria_tryte_get_trit@PLT
.size npk_tryte_get_trit, .-npk_tryte_get_trit

.global npk_tryte_mod
.type npk_tryte_mod, @function
npk_tryte_mod:
    jmp aria_tryte_mod@PLT
.size npk_tryte_mod, .-npk_tryte_mod

.global npk_tryte_mul
.type npk_tryte_mul, @function
npk_tryte_mul:
    jmp aria_tryte_mul@PLT
.size npk_tryte_mul, .-npk_tryte_mul

.global npk_tryte_neg
.type npk_tryte_neg, @function
npk_tryte_neg:
    jmp aria_tryte_neg@PLT
.size npk_tryte_neg, .-npk_tryte_neg

.global npk_tryte_sub
.type npk_tryte_sub, @function
npk_tryte_sub:
    jmp aria_tryte_sub@PLT
.size npk_tryte_sub, .-npk_tryte_sub

.global npk_tryte_to_bin
.type npk_tryte_to_bin, @function
npk_tryte_to_bin:
    jmp aria_tryte_to_bin@PLT
.size npk_tryte_to_bin, .-npk_tryte_to_bin

.global npk_uhash_clear
.type npk_uhash_clear, @function
npk_uhash_clear:
    jmp aria_uhash_clear@PLT
.size npk_uhash_clear, .-npk_uhash_clear

.global npk_uhash_clear_fast
.type npk_uhash_clear_fast, @function
npk_uhash_clear_fast:
    jmp aria_uhash_clear_fast@PLT
.size npk_uhash_clear_fast, .-npk_uhash_clear_fast

.global npk_uhash_count
.type npk_uhash_count, @function
npk_uhash_count:
    jmp aria_uhash_count@PLT
.size npk_uhash_count, .-npk_uhash_count

.global npk_uhash_count_fast
.type npk_uhash_count_fast, @function
npk_uhash_count_fast:
    jmp aria_uhash_count_fast@PLT
.size npk_uhash_count_fast, .-npk_uhash_count_fast

.global npk_uhash_delete
.type npk_uhash_delete, @function
npk_uhash_delete:
    jmp aria_uhash_delete@PLT
.size npk_uhash_delete, .-npk_uhash_delete

.global npk_uhash_delete_fast
.type npk_uhash_delete_fast, @function
npk_uhash_delete_fast:
    jmp aria_uhash_delete_fast@PLT
.size npk_uhash_delete_fast, .-npk_uhash_delete_fast

.global npk_uhash_destroy
.type npk_uhash_destroy, @function
npk_uhash_destroy:
    jmp aria_uhash_destroy@PLT
.size npk_uhash_destroy, .-npk_uhash_destroy

.global npk_uhash_destroy_fast
.type npk_uhash_destroy_fast, @function
npk_uhash_destroy_fast:
    jmp aria_uhash_destroy_fast@PLT
.size npk_uhash_destroy_fast, .-npk_uhash_destroy_fast

.global npk_uhash_fits
.type npk_uhash_fits, @function
npk_uhash_fits:
    jmp aria_uhash_fits@PLT
.size npk_uhash_fits, .-npk_uhash_fits

.global npk_uhash_fits_fast
.type npk_uhash_fits_fast, @function
npk_uhash_fits_fast:
    jmp aria_uhash_fits_fast@PLT
.size npk_uhash_fits_fast, .-npk_uhash_fits_fast

.global npk_uhash_get
.type npk_uhash_get, @function
npk_uhash_get:
    jmp aria_uhash_get@PLT
.size npk_uhash_get, .-npk_uhash_get

.global npk_uhash_get_fast
.type npk_uhash_get_fast, @function
npk_uhash_get_fast:
    jmp aria_uhash_get_fast@PLT
.size npk_uhash_get_fast, .-npk_uhash_get_fast

.global npk_uhash_has
.type npk_uhash_has, @function
npk_uhash_has:
    jmp aria_uhash_has@PLT
.size npk_uhash_has, .-npk_uhash_has

.global npk_uhash_has_fast
.type npk_uhash_has_fast, @function
npk_uhash_has_fast:
    jmp aria_uhash_has_fast@PLT
.size npk_uhash_has_fast, .-npk_uhash_has_fast

.global npk_uhash_keys
.type npk_uhash_keys, @function
npk_uhash_keys:
    jmp aria_uhash_keys@PLT
.size npk_uhash_keys, .-npk_uhash_keys

.global npk_uhash_keys_fast
.type npk_uhash_keys_fast, @function
npk_uhash_keys_fast:
    jmp aria_uhash_keys_fast@PLT
.size npk_uhash_keys_fast, .-npk_uhash_keys_fast

.global npk_uhash_new
.type npk_uhash_new, @function
npk_uhash_new:
    jmp aria_uhash_new@PLT
.size npk_uhash_new, .-npk_uhash_new

.global npk_uhash_new_fast
.type npk_uhash_new_fast, @function
npk_uhash_new_fast:
    jmp aria_uhash_new_fast@PLT
.size npk_uhash_new_fast, .-npk_uhash_new_fast

.global npk_uhash_set
.type npk_uhash_set, @function
npk_uhash_set:
    jmp aria_uhash_set@PLT
.size npk_uhash_set, .-npk_uhash_set

.global npk_uhash_set_fast
.type npk_uhash_set_fast, @function
npk_uhash_set_fast:
    jmp aria_uhash_set_fast@PLT
.size npk_uhash_set_fast, .-npk_uhash_set_fast

.global npk_uhash_size
.type npk_uhash_size, @function
npk_uhash_size:
    jmp aria_uhash_size@PLT
.size npk_uhash_size, .-npk_uhash_size

.global npk_uhash_size_fast
.type npk_uhash_size_fast, @function
npk_uhash_size_fast:
    jmp aria_uhash_size_fast@PLT
.size npk_uhash_size_fast, .-npk_uhash_size_fast

.global npk_uhash_type
.type npk_uhash_type, @function
npk_uhash_type:
    jmp aria_uhash_type@PLT
.size npk_uhash_type, .-npk_uhash_type

.global npk_ustack_bytes_used
.type npk_ustack_bytes_used, @function
npk_ustack_bytes_used:
    jmp aria_ustack_bytes_used@PLT
.size npk_ustack_bytes_used, .-npk_ustack_bytes_used

.global npk_ustack_bytes_used_fast
.type npk_ustack_bytes_used_fast, @function
npk_ustack_bytes_used_fast:
    jmp aria_ustack_bytes_used_fast@PLT
.size npk_ustack_bytes_used_fast, .-npk_ustack_bytes_used_fast

.global npk_ustack_capacity_bytes
.type npk_ustack_capacity_bytes, @function
npk_ustack_capacity_bytes:
    jmp aria_ustack_capacity_bytes@PLT
.size npk_ustack_capacity_bytes, .-npk_ustack_capacity_bytes

.global npk_ustack_destroy
.type npk_ustack_destroy, @function
npk_ustack_destroy:
    jmp aria_ustack_destroy@PLT
.size npk_ustack_destroy, .-npk_ustack_destroy

.global npk_ustack_destroy_fast
.type npk_ustack_destroy_fast, @function
npk_ustack_destroy_fast:
    jmp aria_ustack_destroy_fast@PLT
.size npk_ustack_destroy_fast, .-npk_ustack_destroy_fast

.global npk_ustack_fits
.type npk_ustack_fits, @function
npk_ustack_fits:
    jmp aria_ustack_fits@PLT
.size npk_ustack_fits, .-npk_ustack_fits

.global npk_ustack_new
.type npk_ustack_new, @function
npk_ustack_new:
    jmp aria_ustack_new@PLT
.size npk_ustack_new, .-npk_ustack_new

.global npk_ustack_new_fast
.type npk_ustack_new_fast, @function
npk_ustack_new_fast:
    jmp aria_ustack_new_fast@PLT
.size npk_ustack_new_fast, .-npk_ustack_new_fast

.global npk_ustack_peek
.type npk_ustack_peek, @function
npk_ustack_peek:
    jmp aria_ustack_peek@PLT
.size npk_ustack_peek, .-npk_ustack_peek

.global npk_ustack_peek_fast
.type npk_ustack_peek_fast, @function
npk_ustack_peek_fast:
    jmp aria_ustack_peek_fast@PLT
.size npk_ustack_peek_fast, .-npk_ustack_peek_fast

.global npk_ustack_pop
.type npk_ustack_pop, @function
npk_ustack_pop:
    jmp aria_ustack_pop@PLT
.size npk_ustack_pop, .-npk_ustack_pop

.global npk_ustack_pop_fast
.type npk_ustack_pop_fast, @function
npk_ustack_pop_fast:
    jmp aria_ustack_pop_fast@PLT
.size npk_ustack_pop_fast, .-npk_ustack_pop_fast

.global npk_ustack_push
.type npk_ustack_push, @function
npk_ustack_push:
    jmp aria_ustack_push@PLT
.size npk_ustack_push, .-npk_ustack_push

.global npk_ustack_push_fast
.type npk_ustack_push_fast, @function
npk_ustack_push_fast:
    jmp aria_ustack_push_fast@PLT
.size npk_ustack_push_fast, .-npk_ustack_push_fast

.global npk_ustack_top_type
.type npk_ustack_top_type, @function
npk_ustack_top_type:
    jmp aria_ustack_top_type@PLT
.size npk_ustack_top_type, .-npk_ustack_top_type

.global npk_ustack_top_type_fast
.type npk_ustack_top_type_fast, @function
npk_ustack_top_type_fast:
    jmp aria_ustack_top_type_fast@PLT
.size npk_ustack_top_type_fast, .-npk_ustack_top_type_fast

.global npk_wildx_enable_audit
.type npk_wildx_enable_audit, @function
npk_wildx_enable_audit:
    jmp aria_wildx_enable_audit@PLT
.size npk_wildx_enable_audit, .-npk_wildx_enable_audit

.global npk_wildx_enable_guard_pages
.type npk_wildx_enable_guard_pages, @function
npk_wildx_enable_guard_pages:
    jmp aria_wildx_enable_guard_pages@PLT
.size npk_wildx_enable_guard_pages, .-npk_wildx_enable_guard_pages

.global npk_wildx_set_quota
.type npk_wildx_set_quota, @function
npk_wildx_set_quota:
    jmp aria_wildx_set_quota@PLT
.size npk_wildx_set_quota, .-npk_wildx_set_quota

.global npk_wildx_verify_hash
.type npk_wildx_verify_hash, @function
npk_wildx_verify_hash:
    jmp aria_wildx_verify_hash@PLT
.size npk_wildx_verify_hash, .-npk_wildx_verify_hash

.global npk_write_file_simple
.type npk_write_file_simple, @function
npk_write_file_simple:
    jmp aria_write_file_simple@PLT
.size npk_write_file_simple, .-npk_write_file_simple
