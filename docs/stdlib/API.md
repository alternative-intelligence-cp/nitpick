# Aria Standard Library API Reference

**Version**: 0.0.7-dev  
**Last Updated**: January 5, 2026  
**Total Functions**: 284+  
**Total Modules**: 22

## Table of Contents

1. [Core I/O & System (98 functions)](#core-io--system)
2. [Mathematics (60 functions)](#mathematics)
3. [Data Types (25 functions)](#data-types)
4. [Collections & Memory (19 functions)](#collections--memory)
5. [Conversion & Output (10 functions)](#conversion--output)
6. [Utilities (72 functions)](#utilities)

---

## Core I/O & System

### io.aria (5 functions)

#### `print_char`
```aria
func:print_char = void(int8:c)
```
Print single character to stdout.

**Example**:
```aria
print_char(65);  // Prints 'A'
print_char(10);  // Newline
```

---

#### `print_newline`
```aria
func:print_newline = void()
```
Print newline character (`\n`) to stdout.

---

#### `print_digit`
```aria
func:print_digit = void(int8:d)
```
Print single digit (0-9) to stdout.  
**Input**: Value 0-9  
**Undefined**: Values outside 0-9 range

---

#### `print_space`
```aria
func:print_space = void()
```
Print space character to stdout.

---

#### `print_tab`
```aria
func:print_tab = void()
```
Print tab character (`\t`) to stdout.

---

### sys.aria (6 functions)

#### `exit_program`
```aria
func:exit_program = void(int32:code)
```
Exit program with status code.

**Parameters**:
- `code`: Exit code (0 = success, non-zero = error)

**Example**:
```aria
if (critical_error) {
    exit_program(1);
} else {}
```

---

#### `get_process_id`
```aria
func:get_process_id = int64()
```
Get current process ID (PID).

**Returns**: Process ID as int64

---

#### `get_parent_process_id`
```aria
func:get_parent_process_id = int64()
```
Get parent process ID (PPID).

---

#### `get_user_id`
```aria
func:get_user_id = int64()
```
Get current user ID (UID).

---

#### `get_group_id`
```aria
func:get_group_id = int64()
```
Get current group ID (GID).

---

#### `sleep_seconds`
```aria
func:sleep_seconds = void(int32:seconds)
```
Sleep for specified seconds.

**Example**:
```aria
print_newline();
sleep_seconds(2);  // Wait 2 seconds
print_newline();
```

---

### cstring.aria (7 functions)

#### `string_length`
```aria
func:string_length = int64(int8*:str)
```
Get length of null-terminated C string.

**Returns**: Length excluding null terminator

**Example**:
```aria
int8*:text = "Hello";
int64:len = string_length(text);  // len = 5
```

---

#### `string_compare`
```aria
func:string_compare = int32(int8*:s1, int8*:s2)
```
Compare two strings lexicographically.

**Returns**:
- Negative if s1 < s2
- 0 if s1 == s2
- Positive if s1 > s2

---

#### `string_equals`
```aria
func:string_equals = int8(int8*:s1, int8*:s2)
```
Check if two strings are equal.

**Returns**: 1 if equal, 0 if different

---

#### `string_copy`
```aria
func:string_copy = void(int8*:dest, int8*:src)
```
Copy source string to destination.

**Warning**: Destination must have sufficient space

---

#### `string_concat`
```aria
func:string_concat = void(int8*:dest, int8*:src)
```
Append source to end of destination.

**Warning**: Destination must have space for combined length

---

#### `string_find_char`
```aria
func:string_find_char = int64(int8*:str, int8:ch)
```
Find first occurrence of character in string.

**Returns**: Index of character, or -1 if not found

---

#### `string_find`
```aria
func:string_find = int64(int8*:haystack, int8*:needle)
```
Find first occurrence of substring.

**Returns**: Index of substring start, or -1 if not found

---

### string.aria (29 functions)

#### Parsing Functions

##### `parse_int`
```aria
func:parse_int = int32(int8*:str)
```
Parse integer from string.

**Handles**: Optional whitespace, optional sign (+/-), decimal digits

**Returns**: Parsed integer value

**Example**:
```aria
int32:x = parse_int("  -42  ");  // x = -42
```

---

##### `parse_int_range`
```aria
func:parse_int_range = int32(int8*:str, int64:start, int64:end)
```
Parse integer from substring [start, end).

---

##### `parse_float`
```aria
func:parse_float = flt64(int8*:str)
```
Parse floating-point number from string.

**Handles**: Optional sign, decimal point, whole and fractional parts

**Example**:
```aria
flt64:pi = parse_float("3.14159");
```

---

#### Searching Functions

##### `find_char`
```aria
func:find_char = int64(int8*:str, int8:ch)
```
Find first occurrence of character.

**Returns**: Index or -1

---

##### `find_char_from`
```aria
func:find_char_from = int64(int8*:str, int8:ch, int64:start)
```
Find character starting from given index.

---

##### `count_char`
```aria
func:count_char = int64(int8*:str, int8:ch)
```
Count occurrences of character in string.

---

#### Trimming Functions

##### `trim_start_offset`
```aria
func:trim_start_offset = int64(int8*:str)
```
Find offset where non-whitespace begins.

**Returns**: Index of first non-whitespace character

---

##### `trim_end_pos`
```aria
func:trim_end_pos = int64(int8*:str)
```
Find position where trailing whitespace begins.

**Returns**: Index after last non-whitespace character

---

#### Padding Functions

##### `pad_left_spaces`
```aria
func:pad_left_spaces = void(int8*:dest, int8*:src, int32:width)
```
Left-pad string with spaces to given width.

**Example**:
```aria
int8*:buf = aria_alloc_string(20);
pad_left_spaces(buf, "42", 5);  // buf = "   42"
```

---

##### `pad_right_spaces`
```aria
func:pad_right_spaces = void(int8*:dest, int8*:src, int32:width)
```
Right-pad string with spaces.

---

#### Comparison Functions

##### `starts_with`
```aria
func:starts_with = int8(int8*:str, int8*:prefix)
```
Check if string starts with prefix.

**Returns**: 1 if starts with prefix, 0 otherwise

---

##### `ends_with`
```aria
func:ends_with = int8(int8*:str, int8*:suffix)
```
Check if string ends with suffix.

---

##### `equals_ignore_case`
```aria
func:equals_ignore_case = int8(int8*:s1, int8*:s2)
```
Case-insensitive string comparison.

**Returns**: 1 if equal (ignoring case), 0 otherwise

---

#### Manipulation Functions

##### `replace_char`
```aria
func:replace_char = void(int8*:str, int8:old_ch, int8:new_ch)
```
Replace all occurrences of old_ch with new_ch (in-place).

**Example**:
```aria
int8*:text = "hello_world";
replace_char(text, 95, 32);  // text = "hello world"
```

---

##### `copy_substring`
```aria
func:copy_substring = void(int8*:dest, int8*:src, int64:start, int64:length)
```
Copy substring to destination buffer.

---

#### Character Classification

##### `is_whitespace`
```aria
func:is_whitespace = int8(int8:ch)
```
Check if character is whitespace (space, tab, newline, CR).

---

##### `is_upper`
```aria
func:is_upper = int8(int8:ch)
```
Check if character is uppercase letter (A-Z).

---

##### `is_lower`
```aria
func:is_lower = int8(int8:ch)
```
Check if character is lowercase letter (a-z).

---

##### `is_alpha_char`
```aria
func:is_alpha_char = int8(int8:ch)
```
Check if character is alphabetic (A-Z or a-z).

---

##### `is_digit_char`
```aria
func:is_digit_char = int8(int8:ch)
```
Check if character is digit (0-9).

---

##### `is_alphanumeric`
```aria
func:is_alphanumeric = int8(int8:ch)
```
Check if character is alphanumeric (letter or digit).

---

### time.aria (36 functions)

#### Basic Time Functions

##### `get_time`
```aria
func:get_time = int64()
```
Get current Unix timestamp (seconds since Jan 1, 1970).

**Returns**: Seconds as int64

**Example**:
```aria
int64:now = get_time();
```

---

##### `get_clock`
```aria
func:get_clock = int64()
```
Get processor clock ticks for performance measurement.

**Returns**: Clock ticks

---

##### `delay_ms`
```aria
func:delay_ms = void(int32:milliseconds)
```
Busy-wait delay (approximate, uses clock ticks).

**Warning**: Blocks CPU, not recommended for long delays

---

##### `elapsed_ticks`
```aria
func:elapsed_ticks = int64(int64:start, int64:end)
```
Calculate elapsed clock ticks between two time points.

---

##### `benchmark_start`
```aria
func:benchmark_start = int64()
```
Start timing benchmark.

**Returns**: Start time in clock ticks

---

##### `benchmark_end`
```aria
func:benchmark_end = int64(int64:start)
```
End timing benchmark.

**Returns**: Elapsed ticks since start

**Example**:
```aria
int64:t0 = benchmark_start();
// ... code to benchmark ...
int64:elapsed = benchmark_end(t0);
```

---

#### Duration Conversions

##### `time_since`
```aria
func:time_since = int64(int64:start, int64:end)
```
Calculate seconds between two timestamps.

---

##### `seconds_to_minutes`, `seconds_to_hours`, `seconds_to_days`
```aria
func:seconds_to_X = int64(int64:seconds)
```
Convert seconds to minutes/hours/days.

---

##### `minutes_to_seconds`, `hours_to_seconds`, `days_to_seconds`
```aria
func:X_to_seconds = int64(int64:X)
```
Convert minutes/hours/days to seconds.

**Example**:
```aria
int64:one_day = days_to_seconds(1);  // 86400
```

---

#### Date Component Extraction

##### `get_year`
```aria
func:get_year = int32(int64:timestamp)
```
Extract year from Unix timestamp (simplified, post-2000).

---

##### `get_day_of_year`
```aria
func:get_day_of_year = int32(int64:timestamp)
```
Get day of year (1-366).

---

##### `get_hour`, `get_minute`, `get_second`
```aria
func:get_X = int32(int64:timestamp)
```
Extract hour (0-23), minute (0-59), or second (0-59) from timestamp.

---

#### Leap Year Handling

##### `is_leap_year`
```aria
func:is_leap_year = int8(int32:year)
```
Check if year is leap year (divisible by 4, except centuries unless divisible by 400).

**Returns**: 1 if leap year, 0 otherwise

---

##### `days_in_year`
```aria
func:days_in_year = int32(int32:year)
```
Get number of days in year (365 or 366).

---

##### `days_in_month`
```aria
func:days_in_month = int32(int32:year, int32:month)
```
Get number of days in month (accounts for leap years).

**Parameters**:
- `month`: 1-12

---

#### Timestamp Construction

##### `make_timestamp`
```aria
func:make_timestamp = int64(int32:year, int32:month, int32:day, 
                             int32:hour, int32:minute, int32:second)
```
Create Unix timestamp from date components.

**Example**:
```aria
// New Year 2026
int64:ts = make_timestamp(2026, 1, 1, 0, 0, 0);
```

---

#### Formatting Functions

##### `format_iso8601`
```aria
func:format_iso8601 = void(int8*:buf, int64:timestamp)
```
Format timestamp as ISO 8601: `YYYY-MM-DD HH:MM:SS`

**Buffer Requirement**: Minimum 20 bytes

**Example**:
```aria
int8*:buf = aria_alloc_string(20);
format_iso8601(buf, get_time());
// buf = "2026-01-05 14:30:00"
```

---

##### `format_time`
```aria
func:format_time = void(int8*:buf, int64:timestamp)
```
Format time only: `HH:MM:SS`

**Buffer Requirement**: Minimum 9 bytes

---

##### `format_duration`
```aria
func:format_duration = void(int8*:buf, int64:seconds)
```
Format duration as `HH:MM:SS`.

---

##### `write_2digit`, `write_4digit`
```aria
func:write_Ndigit = void(int8*:buf, int32:offset, int32:value)
```
Helper functions to write 2-digit or 4-digit numbers to buffer.

---

#### Time Comparison

##### `time_before`, `time_after`, `time_equal`
```aria
func:time_X = int8(int64:t1, int64:t2)
```
Compare two timestamps.

**Returns**: 1 if condition true, 0 otherwise

---

### file.aria (15 functions)

#### File Opening

##### `file_open_read`, `file_open_write`, `file_open_append`
```aria
func:file_open_X = void*(int8*:filepath)
```
Open file for reading, writing, or appending.

**Returns**: File handle (void*) or NULL on error

---

#### File Operations

##### `file_close`
```aria
func:file_close = void(void*:file)
```
Close file handle.

---

##### `file_read`
```aria
func:file_read = int64(void*:file, void*:buffer, int64:size)
```
Read bytes from file into buffer.

**Returns**: Number of bytes read

---

##### `file_write`
```aria
func:file_write = int64(void*:file, void*:data, int64:size)
```
Write bytes from buffer to file.

**Returns**: Number of bytes written

---

##### `file_seek`, `file_tell`
```aria
func:file_seek = int64(void*:file, int64:offset)
func:file_tell = int64(void*:file)
```
Seek to position or get current position.

---

##### `file_is_eof`, `file_has_error`
```aria
func:file_is_eof = int8(void*:file)
func:file_has_error = int8(void*:file)
```
Check end-of-file or error status.

---

##### `file_flush`
```aria
func:file_flush = void(void*:file)
```
Flush buffered writes to disk.

---

##### `file_read_char`, `file_write_char`
```aria
func:file_read_char = int32(void*:file)
func:file_write_char = void(void*:file, int8:ch)
```
Read/write single character.

---

##### `file_size`
```aria
func:file_size = int64(void*:file)
```
Get total file size in bytes.

---

## Mathematics

### math.aria (44 functions)

#### Basic Transcendental Functions

##### `square_root`, `power`
```aria
func:square_root = flt64(flt64:x)
func:power = flt64(flt64:base, flt64:exponent)
```

---

##### `sine`, `cosine`, `tangent`
```aria
func:sine = flt64(flt64:x)
func:cosine = flt64(flt64:x)
func:tangent = flt64(flt64:x)
```
Trigonometric functions (input in radians).

---

##### `arc_sine`, `arc_cosine`, `arc_tangent`, `arc_tangent2`
```aria
func:arc_tangent2 = flt64(flt64:y, flt64:x)
```
Inverse trig functions. `arc_tangent2` handles quadrants correctly.

---

##### `ln`, `log_10`, `exponential`
```aria
func:ln = flt64(flt64:x)
func:log_10 = flt64(flt64:x)
func:exponential = flt64(flt64:x)
```
Natural log, base-10 log, and e^x.

---

##### `abs`
```aria
func:abs = flt64(flt64:x)
```
Absolute value.

---

#### Rounding Functions

##### `floor_f64`, `ceil_f64`, `round_f64`, `trunc_f64`
```aria
func:round_f64 = flt64(flt64:x)
```
Round to nearest, down, up, or toward zero.

---

#### Modular Arithmetic

##### `mod_f64`
```aria
func:mod_f64 = flt64(flt64:x, flt64:y)
```
Floating-point modulo.

---

##### `gcd_i32`, `lcm_i32`
```aria
func:gcd_i32 = int32(int32:a, int32:b)
func:lcm_i32 = int32(int32:a, int32:b)
```
Greatest common divisor and least common multiple.

---

#### Factorial & Combinatorics

##### `factorial_i32`, `factorial_f64`
```aria
func:factorial_i32 = int32(int32:n)
func:factorial_f64 = flt64(int32:n)
```
Factorial (i32 safe for n <= 12, f64 for larger).

---

##### `binomial_i32`
```aria
func:binomial_i32 = int32(int32:n, int32:k)
```
Binomial coefficient (n choose k).

---

#### Interpolation

##### `lerp_f64`
```aria
func:lerp_f64 = flt64(flt64:a, flt64:b, flt64:t)
```
Linear interpolation: `a + (b - a) * t`

---

##### `inverse_lerp`
```aria
func:inverse_lerp = flt64(flt64:a, flt64:b, flt64:value)
```
Find t for value between a and b.

---

##### `map_range`
```aria
func:map_range = flt64(flt64:value, flt64:in_min, flt64:in_max, 
                        flt64:out_min, flt64:out_max)
```
Remap value from one range to another.

**Example**:
```aria
// Convert 50% to 0-255 range
flt64:byte = map_range(0.5, 0.0, 1.0, 0.0, 255.0);  // 127.5
```

---

##### `smoothstep`, `smootherstep`
```aria
func:smoothstep = flt64(flt64:edge0, flt64:edge1, flt64:x)
```
Smooth interpolation (cubic or quintic Hermite).

---

#### Range Utilities

##### `clamp_f64`
```aria
func:clamp_f64 = flt64(flt64:value, flt64:min_val, flt64:max_val)
```
Clamp value to [min, max] range.

---

##### `wrap_f64`
```aria
func:wrap_f64 = flt64(flt64:value, flt64:max_val)
```
Wrap value to [0, max) range (modulo with negative handling).

---

##### `ping_pong`
```aria
func:ping_pong = flt64(flt64:t, flt64:length)
```
Ping-pong value between 0 and length (triangle wave).

---

#### Angle Conversion

##### `deg_to_rad`, `rad_to_deg`
```aria
func:deg_to_rad = flt64(flt64:degrees)
func:rad_to_deg = flt64(flt64:radians)
```

---

#### Sign & Utilities

##### `sign_f64`
```aria
func:sign_f64 = flt64(flt64:x)
```
Returns -1.0, 0.0, or 1.0.

---

##### `copysign`
```aria
func:copysign = flt64(flt64:magnitude, flt64:sign)
```
Copy sign from one number to magnitude of another.

---

#### Mathematical Constants

```aria
const flt64:PI = 3.141592653589793
const flt64:E = 2.718281828459045
const flt64:TAU = 6.283185307179586
const flt64:SQRT_2 = 1.414213562373095
const flt64:SQRT_3 = 1.732050807568877
const flt64:PHI = 1.618033988749895
```

---

### numeric.aria (8 functions)

##### `gcd_i32`, `lcm_i32`, `factorial_i32`, `pow_i32`
Integer versions of mathematical functions.

##### `is_even`, `is_odd`
```aria
func:is_even = int8(int32:x)
```

##### `sign_i32`, `clamp_i32`
Integer sign and clamping.

---

### compare.aria (8 functions)

##### `compare_i32`, `compare_f64`
```aria
func:compare_i32 = int32(int32:a, int32:b)
```
Returns -1, 0, or 1.

##### `in_range_i32`, `in_range_f64`
Check if value is in [min, max].

##### `max3_i32`, `min3_i32`
Find min/max of three values.

##### `is_ascending_i32`, `is_descending_i32`
Check if three values are sorted.

---

## Data Types

### int.aria (3 functions)

##### `abs_i32`, `min_i32`, `max_i32`
```aria
func:abs_i32 = int32(int32:x)
```

---

### float.aria (5 functions)

##### `min_f64`, `max_f64`, `clamp_f64`
##### `lerp`, `approx_equal`
```aria
func:approx_equal = int8(flt64:a, flt64:b, flt64:epsilon)
```
Check if floats are approximately equal within epsilon.

---

### logic.aria (8 functions)

##### `and`, `or`, `xor`, `not`
Logical operations.

##### `nand`, `nor`
```aria
func:nand = int8(int8:a, int8:b)
```

##### `implies`, `equiv`
Logical implication and equivalence.

---

### bits.aria (9 functions)

##### `popcount_i32`
Count set bits.

##### `bit_test`, `bit_set`, `bit_clear`, `bit_flip`
```aria
func:bit_test = int8(int32:value, int32:bit_pos)
func:bit_set = int32(int32:value, int32:bit_pos)
```

##### `rotate_left_i32`, `rotate_right_i32`
Bitwise rotation.

##### `is_power_of_two`, `bit_length`
Utility functions.

---

## Collections & Memory

### arrays.aria (12 functions)

##### `fill_i32`
```aria
func:fill_i32 = void(int32*:arr, int32:size, int32:value)
```

##### `min_i32`, `max_i32`, `sum_i32`
Array aggregation.

##### `reverse_i32`, `find_i32`
Array manipulation.

##### `copy_i32`, `equals_i32`
Array copying and comparison.

##### `binary_search_i32`, `count_i32`, `contains_i32`, `find_last_i32`

---

### mem.aria (7 extern functions)

##### `malloc`, `free`, `calloc`, `realloc`
##### `memset`, `memcpy`, `memcmp`

---

## Conversion & Output

### convert.aria (10 functions)

##### `digit_at`, `count_digits`
##### `print_int`, `print_signed_int`
##### `to_upper`, `to_lower`
##### `is_digit`, `is_alpha`

---

## Utilities

### random.aria (9 functions)

##### `seed_random`, `seed_random_value`
##### `random_int`, `random_range`
##### `random_float`, `random_float_range`
##### `coin_flip`, `dice_roll`, `random_index`

---

### hash.aria (7 functions)

##### `hash_int32`, `hash_combine`
##### `hash_string_djb2`, `hash_string_sdbm`
##### `checksum_simple`, `checksum_xor`, `checksum_fletcher16`

---

### result.aria (8 functions + 13 constants)

##### `is_success`, `is_error`, `is_null_ptr`, `is_valid_ptr`
##### `is_in_bounds`, `clamp_index`, `wrap_index`

---

### algorithms.aria (24 functions)

##### `sort_i32`, `sort_i64`, `sort_f64`
Quicksort implementation.

##### `bubble_sort_i32`, `insertion_sort_i32`, `merge_sort_i32`
Alternative sorting algorithms.

##### `binary_search_sorted_i32`, `binary_search_sorted_i64`
##### `quickselect_i32`, `median_i32`
##### `reverse_i32`, `reverse_i64`, `is_sorted_i32`, `is_sorted_i64`

---

### path.aria (19 functions)

##### `basename`, `dirname`, `find_last_separator`
Path component extraction.

##### `extension`, `stem`, `find_extension_pos`, `has_extension`, `same_extension`, `replace_extension`
File extension handling.

##### `join_path`
Cross-platform path joining.

##### `is_absolute`, `is_relative`
Path classification.

##### `normalize_separators`, `count_components`, `paths_equal`

---

## Usage Examples

### Complete Program Example

```aria
use std.io.*;
use std.string.*;
use std.time.*;
use std.algorithms.*;

func:main = int32() {
    // Time formatting
    int64:now = get_time();
    int8*:time_buf = aria_alloc_string(20);
    format_iso8601(time_buf, now);
    
    // String manipulation
    int8*:text = "  hello  ";
    int64:start = trim_start_offset(text);
    int64:end = trim_end_pos(text);
    
    // Array sorting
    int32*:numbers = aria_alloc_array(4, 5);
    numbers[0] = 42;
    numbers[1] = 17;
    numbers[2] = 99;
    numbers[3] = 3;
    numbers[4] = 55;
    
    sort_i32(numbers, 5);
    int32:median = median_i32(numbers, 5);
    
    // Path manipulation
    int8*:path = "/usr/local/bin/program";
    int8*:name_buf = aria_alloc_string(256);
    basename(name_buf, path);  // "program"
    
    // Cleanup
    aria_free(time_buf);
    aria_free(numbers);
    aria_free(name_buf);
    
    return 0;
}
```

---

## Performance Tips

1. **Prefer specialized functions**: `fill_i32` is faster than manual loops
2. **Use arena allocators** for temporary data: O(1) cleanup
3. **Binary search requires sorted data**: Call `sort_i32` first
4. **String operations allocate**: Remember to free buffers
5. **TBB operations propagate sentinels**: Check with `@is_sentinel_tbb32`

---

## See Also

- [Intrinsics Reference](INTRINSICS.md) - Low-level compiler builtins
- [Type System Guide](../guides/TYPE_SYSTEM.md) - TBB, LBIM, fix256
- [FFI Guide](../guides/FFI.md) - Calling C libraries
- [Testing Guide](../testing/) - Writing tests

---

**Maintained by**: Aria Compiler Team  
**License**: MIT
