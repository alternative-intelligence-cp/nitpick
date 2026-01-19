/**
 * @file media_processor.hpp
 * @brief Picture Processing Utility - Six-stream image conversion
 * 
 * Implements image processing with Aria's architectural patterns:
 * - Six-stream topology (stdout/UI, stddbg/telemetry, stddato/binary)
 * - Image format conversion (PNG, JPEG, BMP)
 * - TBB-safe progress tracking
 * - Zero-copy where possible
 * - Binary image protocol
 * 
 * Reference: sysUtils_6_ffmpeg.txt (660 lines)
 * 
 * Note: Uses stb_image.h (header-only, public domain) for simplicity
 *       and portability. Focuses on image processing (apic = Aria picture).
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

// TBB type aliases
using tbb8 = uint8_t;
using tbb32 = uint32_t;
using tbb64 = uint64_t;

// TBB error constant
constexpr tbb64 TBB64_ERR = 0xFFFFFFFFFFFFFFFFULL;

namespace aria {
namespace apic {

/**
 * @brief Image format enumeration
 */
enum class ImageFormat {
    UNKNOWN,
    PNG,
    JPEG,
    BMP,
    TGA,
    AUTO  // Detect from file extension
};

/**
 * @brief Image color space
 */
enum class ColorSpace {
    RGB,
    RGBA,
    GRAYSCALE,
    GRAYSCALE_ALPHA
};

/**
 * @brief Media file metadata
 */
struct MediaFile {
    std::string filepath;
    ImageFormat format;
    ColorSpace colorspace;
    tbb32 width;
    tbb32 height;
    tbb32 channels;
    tbb64 file_size_bytes;
    tbb64 pixels;  // width * height
    
    // Image data (raw pixel buffer)
    std::vector<uint8_t> data;
    
    MediaFile() 
        : format(ImageFormat::UNKNOWN)
        , colorspace(ColorSpace::RGB)
        , width(0)
        , height(0)
        , channels(0)
        , file_size_bytes(0)
        , pixels(0) {}
    
    bool is_loaded() const { return !data.empty() && width > 0 && height > 0; }
};

/**
 * @brief Conversion options
 */
struct ConversionOptions {
    ImageFormat output_format;
    tbb32 target_width;     // 0 = keep original
    tbb32 target_height;    // 0 = keep original
    tbb32 jpeg_quality;     // 1-100 (for JPEG output)
    bool maintain_aspect;   // Maintain aspect ratio when resizing
    bool verbose;           // Enable detailed telemetry
    
    ConversionOptions()
        : output_format(ImageFormat::PNG)
        , target_width(0)
        , target_height(0)
        , jpeg_quality(90)
        , maintain_aspect(true)
        , verbose(false) {}
};

/**
 * @brief Processing statistics
 */
struct ProcessStats {
    tbb64 images_processed;
    tbb64 images_failed;
    tbb64 bytes_read;
    tbb64 bytes_written;
    tbb64 total_time_us;
    tbb32 error_count;
    std::string last_error;
    
    ProcessStats()
        : images_processed(0)
        , images_failed(0)
        , bytes_read(0)
        , bytes_written(0)
        , total_time_us(0)
        , error_count(0) {}
    
    void record_error(const std::string& msg) {
        error_count++;
        last_error = msg;
    }
};

/**
 * @brief Media processor - main processing engine
 */
class MediaProcessor {
public:
    MediaProcessor();
    ~MediaProcessor();
    
    // Image loading
    bool load_image(const std::string& filepath, MediaFile& out_file);
    bool save_image(const MediaFile& file, const std::string& output_path, ImageFormat format, int quality = 90);
    
    // Format conversion
    bool convert(const MediaFile& input, MediaFile& output, const ConversionOptions& opts);
    
    // Image manipulation
    bool resize(const MediaFile& input, MediaFile& output, tbb32 new_width, tbb32 new_height);
    bool flip_vertical(MediaFile& file);
    bool flip_horizontal(MediaFile& file);
    
    // Metadata extraction
    bool extract_metadata(const std::string& filepath, MediaFile& metadata);
    
    // Batch processing
    std::vector<std::string> batch_convert(
        const std::vector<std::string>& input_files,
        ImageFormat output_format,
        const std::string& output_dir,
        const ConversionOptions& opts
    );
    
    // Statistics
    const ProcessStats& get_stats() const { return stats_; }
    void reset_stats() { stats_ = ProcessStats(); }
    
    // Error handling (TBB-safe)
    bool has_error() const { return error_code_ == TBB64_ERR; }
    std::string error_message() const { return error_msg_; }
    
    // Helper functions (public for FFI and StreamProcessor)
    ImageFormat detect_format(const std::string& filepath);
    std::string format_to_string(ImageFormat fmt);
    std::string format_to_extension(ImageFormat fmt);
    int format_to_channels(ImageFormat fmt, ColorSpace cs);
    
private:
    void set_error(const std::string& msg);
    void clear_error();
    
    // Resize helpers
    void resize_bilinear(const uint8_t* src, int src_w, int src_h, int channels,
                         uint8_t* dst, int dst_w, int dst_h);
    
    ProcessStats stats_;
    tbb64 error_code_;
    std::string error_msg_;
};

/**
 * @brief Stream-aware processor for six-stream topology
 */
class StreamProcessor {
public:
    explicit StreamProcessor(bool verbose = false);
    
    // Process single file with UI feedback
    bool process_file(const std::string& input_path, 
                     const std::string& output_path,
                     const ConversionOptions& opts);
    
    // Process directory
    std::vector<std::string> process_directory(
        const std::string& input_dir,
        const std::string& output_dir,
        const ConversionOptions& opts
    );
    
    // Interactive mode
    void process_interactive();
    
    const ProcessStats& stats() const { return processor_stats_; }
    
private:
    // Stream outputs
    void emit_ui(const std::string& msg);
    void emit_debug(const std::string& tag, const std::string& msg);
    void emit_progress(tbb32 current, tbb32 total, const std::string& filename);
    void emit_image_binary(const MediaFile& file);
    
    bool verbose_;
    ProcessStats processor_stats_;
    MediaProcessor processor_;
};

// Stream writers (separate namespace for clarity)
namespace streams {
    /**
     * @brief Write telemetry event to FD 3 (stddbg)
     */
    void write_telemetry(const std::string& event, const std::map<std::string, std::string>& data);
    
    /**
     * @brief Write image data in binary protocol to FD 5 (stddato)
     * Format: [magic:4][width:4][height:4][channels:4][format:4][data_size:8][pixels...]
     */
    void write_image_binary(const MediaFile& file);
    
    /**
     * @brief Write statistics to FD 5 (stddato)
     */
    void write_stats_binary(const ProcessStats& stats);
}

} // namespace apic
} // namespace aria

// C FFI API
extern "C" {
    /**
     * @brief Convert image format
     * @param input_path Input image file
     * @param output_path Output image file
     * @param format_str Output format ("png", "jpeg", "bmp")
     * @return 0 on success, error code otherwise
     */
    int aria_media_convert(const char* input_path, const char* output_path, const char* format_str);
    
    /**
     * @brief Resize image
     * @param input_path Input image file
     * @param output_path Output image file
     * @param width New width
     * @param height New height
     * @return 0 on success, error code otherwise
     */
    int aria_media_resize(const char* input_path, const char* output_path, int width, int height);
    
    /**
     * @brief Get image metadata
     * @param filepath Image file
     * @return Formatted metadata string (caller must free)
     */
    const char* aria_media_info(const char* filepath);
    
    /**
     * @brief Free result string
     */
    void aria_media_free(const char* result);
}
