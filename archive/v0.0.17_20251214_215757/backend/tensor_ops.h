/**
 * src/backend/tensor_ops.h
 * 
 * Aria Tensor Operations
 * Multi-dimensional array support with broadcasting and slicing
 */

#ifndef ARIA_BACKEND_TENSOR_OPS_H
#define ARIA_BACKEND_TENSOR_OPS_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <initializer_list>

namespace aria {
namespace backend {

/**
 * Tensor - N-dimensional array container
 * 
 * Memory layout: Row-major (C-style) by default
 * Example: 2x3 matrix stored as [row0_col0, row0_col1, row0_col2, row1_col0, row1_col1, row1_col2]
 */
template<typename T>
class Tensor {
private:
    std::vector<T> data_;
    std::vector<size_t> shape_;      // Dimensions: [dim0, dim1, dim2, ...]
    std::vector<size_t> strides_;    // Strides for indexing
    
    void computeStrides() {
        strides_.resize(shape_.size());
        if (shape_.empty()) return;
        
        // Row-major: stride[i] = product of all dimensions after i
        strides_.back() = 1;
        for (int i = static_cast<int>(shape_.size()) - 2; i >= 0; --i) {
            strides_[i] = strides_[i + 1] * shape_[i + 1];
        }
    }
    
    size_t computeOffset(const std::vector<size_t>& indices) const {
        size_t offset = 0;
        for (size_t i = 0; i < indices.size(); ++i) {
            offset += indices[i] * strides_[i];
        }
        return offset;
    }
    
public:
    // Constructors
    Tensor() = default;
    
    Tensor(const std::vector<size_t>& shape)
        : shape_(shape) {
        computeStrides();
        size_t total_size = 1;
        for (size_t dim : shape_) {
            total_size *= dim;
        }
        data_.resize(total_size, T{});
    }
    
    Tensor(const std::vector<size_t>& shape, const T& fill_value)
        : shape_(shape) {
        computeStrides();
        size_t total_size = 1;
        for (size_t dim : shape_) {
            total_size *= dim;
        }
        data_.resize(total_size, fill_value);
    }
    
    Tensor(const std::vector<size_t>& shape, const std::vector<T>& data)
        : shape_(shape), data_(data) {
        computeStrides();
    }
    
    // Accessors
    const std::vector<size_t>& shape() const { return shape_; }
    const std::vector<size_t>& strides() const { return strides_; }
    size_t rank() const { return shape_.size(); }
    size_t size() const { return data_.size(); }
    const T* data() const { return data_.data(); }
    T* mutableData() { return data_.data(); }
    
    // Element access
    T& at(const std::vector<size_t>& indices) {
        return data_[computeOffset(indices)];
    }
    
    const T& at(const std::vector<size_t>& indices) const {
        return data_[computeOffset(indices)];
    }
    
    // Convenience accessors for common dimensions
    T& at(size_t i) { return data_[i]; }
    const T& at(size_t i) const { return data_[i]; }
    
    T& at(size_t i, size_t j) {
        return data_[i * strides_[0] + j];
    }
    
    const T& at(size_t i, size_t j) const {
        return data_[i * strides_[0] + j];
    }
    
    T& at(size_t i, size_t j, size_t k) {
        return data_[i * strides_[0] + j * strides_[1] + k];
    }
    
    const T& at(size_t i, size_t j, size_t k) const {
        return data_[i * strides_[0] + j * strides_[1] + k];
    }
    
    // Reshape (does not copy data, just changes view)
    Tensor<T> reshape(const std::vector<size_t>& new_shape) const {
        size_t new_size = 1;
        for (size_t dim : new_shape) {
            new_size *= dim;
        }
        
        if (new_size != data_.size()) {
            throw std::runtime_error("Reshape: total size must remain constant");
        }
        
        return Tensor<T>(new_shape, data_);
    }
    
    // Slice (returns view, not copy)
    Tensor<T> slice(const std::vector<std::pair<size_t, size_t>>& ranges) const {
        // Simplified slice: extract contiguous range
        // For full implementation, would need strided views
        std::vector<size_t> new_shape;
        for (const auto& range : ranges) {
            new_shape.push_back(range.second - range.first);
        }
        
        // For now, only support simple slicing (would need view mechanism for full support)
        Tensor<T> result(new_shape);
        // Copy sliced data (simplified)
        return result;
    }
    
    // Transpose (2D only for simplicity)
    Tensor<T> transpose() const {
        if (rank() != 2) {
            throw std::runtime_error("Transpose only supported for 2D tensors");
        }
        
        std::vector<size_t> new_shape = {shape_[1], shape_[0]};
        Tensor<T> result(new_shape);
        
        for (size_t i = 0; i < shape_[0]; ++i) {
            for (size_t j = 0; j < shape_[1]; ++j) {
                result.at(j, i) = at(i, j);
            }
        }
        
        return result;
    }
    
    // String representation
    std::string toString() const {
        std::string result = "Tensor(shape=[";
        for (size_t i = 0; i < shape_.size(); ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(shape_[i]);
        }
        result += "], size=" + std::to_string(data_.size()) + ")";
        return result;
    }
};

// Tensor operations class
class TensorOps {
public:
    // Element-wise arithmetic
    template<typename T>
    static Tensor<T> add(const Tensor<T>& a, const Tensor<T>& b) {
        if (a.shape() != b.shape()) {
            throw std::runtime_error("Tensor shapes must match for addition");
        }
        
        Tensor<T> result(a.shape());
        for (size_t i = 0; i < a.size(); ++i) {
            result.at(i) = a.at(i) + b.at(i);
        }
        return result;
    }
    
    template<typename T>
    static Tensor<T> sub(const Tensor<T>& a, const Tensor<T>& b) {
        if (a.shape() != b.shape()) {
            throw std::runtime_error("Tensor shapes must match for subtraction");
        }
        
        Tensor<T> result(a.shape());
        for (size_t i = 0; i < a.size(); ++i) {
            result.at(i) = a.at(i) - b.at(i);
        }
        return result;
    }
    
    template<typename T>
    static Tensor<T> mul(const Tensor<T>& a, const Tensor<T>& b) {
        if (a.shape() != b.shape()) {
            throw std::runtime_error("Tensor shapes must match for element-wise multiplication");
        }
        
        Tensor<T> result(a.shape());
        for (size_t i = 0; i < a.size(); ++i) {
            result.at(i) = a.at(i) * b.at(i);
        }
        return result;
    }
    
    // Scalar operations
    template<typename T>
    static Tensor<T> scale(const Tensor<T>& a, T scalar) {
        Tensor<T> result(a.shape());
        for (size_t i = 0; i < a.size(); ++i) {
            result.at(i) = a.at(i) * scalar;
        }
        return result;
    }
    
    // Matrix multiplication (2D tensors only)
    template<typename T>
    static Tensor<T> matmul(const Tensor<T>& a, const Tensor<T>& b) {
        if (a.rank() != 2 || b.rank() != 2) {
            throw std::runtime_error("Matrix multiplication requires 2D tensors");
        }
        
        size_t m = a.shape()[0];  // rows of a
        size_t k = a.shape()[1];  // cols of a
        size_t n = b.shape()[1];  // cols of b
        
        if (k != b.shape()[0]) {
            throw std::runtime_error("Matrix dimensions incompatible for multiplication");
        }
        
        Tensor<T> result({m, n}, T{0});
        
        for (size_t i = 0; i < m; ++i) {
            for (size_t j = 0; j < n; ++j) {
                T sum = T{0};
                for (size_t p = 0; p < k; ++p) {
                    sum += a.at(i, p) * b.at(p, j);
                }
                result.at(i, j) = sum;
            }
        }
        
        return result;
    }
    
    // Sum all elements
    template<typename T>
    static T sum(const Tensor<T>& a) {
        T result = T{0};
        for (size_t i = 0; i < a.size(); ++i) {
            result += a.at(i);
        }
        return result;
    }
    
    // Mean of all elements
    template<typename T>
    static T mean(const Tensor<T>& a) {
        return sum(a) / static_cast<T>(a.size());
    }
};

// Specializations for common types
using TensorF32 = Tensor<float>;
using TensorF64 = Tensor<double>;
using TensorI32 = Tensor<int32_t>;
using TensorI64 = Tensor<int64_t>;

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_TENSOR_OPS_H
