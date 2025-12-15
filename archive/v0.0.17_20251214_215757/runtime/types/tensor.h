#include <cstdint>

// Descriptor struct for the 'tensor' type
struct Tensor {
   void* data;          // Pointer to data payload (Managed or Wild)
   uint64_t* shape;     // Array of dimension sizes
   uint64_t* strides;   // Array of memory steps for indexing optimization
   uint8_t dtype;       // Element type enum (e.g., INT8, FLT32, TRIT)
   uint8_t rank;        // Number of dimensions (e.g., 2 for matrix)
   uint8_t padding;     // Explicit padding to ensure 64-bit struct alignment
};

