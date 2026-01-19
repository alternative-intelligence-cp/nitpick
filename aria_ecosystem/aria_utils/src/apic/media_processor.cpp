/**
 * @file media_processor.cpp
 * @brief Media processor implementation using stb_image
 */

#include "media_processor.hpp"
#include <cstring>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>

// stb_image - public domain image loader
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// stb_image_write - public domain image writer
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace aria {
namespace apic {

// ============================================================================
// MediaProcessor Implementation
// ============================================================================

MediaProcessor::MediaProcessor()
    : error_code_(0)
{}

MediaProcessor::~MediaProcessor() {}

void MediaProcessor::set_error(const std::string& msg) {
    error_code_ = TBB64_ERR;
    error_msg_ = msg;
    stats_.record_error(msg);
}

void MediaProcessor::clear_error() {
    error_code_ = 0;
    error_msg_.clear();
}

ImageFormat MediaProcessor::detect_format(const std::string& filepath) {
    size_t dot = filepath.find_last_of('.');
    if (dot == std::string::npos) {
        return ImageFormat::UNKNOWN;
    }
    
    std::string ext = filepath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "png") return ImageFormat::PNG;
    if (ext == "jpg" || ext == "jpeg") return ImageFormat::JPEG;
    if (ext == "bmp") return ImageFormat::BMP;
    if (ext == "tga") return ImageFormat::TGA;
    
    return ImageFormat::UNKNOWN;
}

std::string MediaProcessor::format_to_string(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::PNG: return "PNG";
        case ImageFormat::JPEG: return "JPEG";
        case ImageFormat::BMP: return "BMP";
        case ImageFormat::TGA: return "TGA";
        default: return "UNKNOWN";
    }
}

std::string MediaProcessor::format_to_extension(ImageFormat fmt) {
    switch (fmt) {
        case ImageFormat::PNG: return ".png";
        case ImageFormat::JPEG: return ".jpg";
        case ImageFormat::BMP: return ".bmp";
        case ImageFormat::TGA: return ".tga";
        default: return "";
    }
}

int MediaProcessor::format_to_channels(ImageFormat fmt, ColorSpace cs) {
    switch (cs) {
        case ColorSpace::GRAYSCALE: return 1;
        case ColorSpace::GRAYSCALE_ALPHA: return 2;
        case ColorSpace::RGB: return 3;
        case ColorSpace::RGBA: return 4;
        default: return 3;
    }
}

bool MediaProcessor::load_image(const std::string& filepath, MediaFile& out_file) {
    clear_error();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Detect format
    ImageFormat fmt = detect_format(filepath);
    if (fmt == ImageFormat::UNKNOWN) {
        set_error("Unknown image format: " + filepath);
        return false;
    }
    
    // Get file size
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        set_error("Cannot stat file: " + filepath);
        return false;
    }
    
    // Load image with stb_image
    int width, height, channels;
    uint8_t* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
    
    if (!pixels) {
        set_error(std::string("Failed to load image: ") + stbi_failure_reason());
        return false;
    }
    
    // Populate MediaFile
    out_file.filepath = filepath;
    out_file.format = fmt;
    out_file.width = static_cast<tbb32>(width);
    out_file.height = static_cast<tbb32>(height);
    out_file.channels = static_cast<tbb32>(channels);
    out_file.file_size_bytes = static_cast<tbb64>(st.st_size);
    out_file.pixels = static_cast<tbb64>(width) * static_cast<tbb64>(height);
    
    // Determine color space
    if (channels == 1) {
        out_file.colorspace = ColorSpace::GRAYSCALE;
    } else if (channels == 2) {
        out_file.colorspace = ColorSpace::GRAYSCALE_ALPHA;
    } else if (channels == 3) {
        out_file.colorspace = ColorSpace::RGB;
    } else if (channels == 4) {
        out_file.colorspace = ColorSpace::RGBA;
    }
    
    // Copy pixel data
    size_t data_size = width * height * channels;
    out_file.data.resize(data_size);
    std::memcpy(out_file.data.data(), pixels, data_size);
    
    // Free stb_image buffer
    stbi_image_free(pixels);
    
    // Update statistics
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.total_time_us += duration.count();
    stats_.bytes_read += st.st_size;
    stats_.images_processed++;
    
    return true;
}

bool MediaProcessor::save_image(const MediaFile& file, const std::string& output_path, 
                                ImageFormat format, int quality) {
    clear_error();
    
    if (!file.is_loaded()) {
        set_error("MediaFile not loaded");
        return false;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int result = 0;
    switch (format) {
        case ImageFormat::PNG:
            result = stbi_write_png(output_path.c_str(), file.width, file.height, 
                                   file.channels, file.data.data(), file.width * file.channels);
            break;
            
        case ImageFormat::JPEG:
            result = stbi_write_jpg(output_path.c_str(), file.width, file.height,
                                   file.channels, file.data.data(), quality);
            break;
            
        case ImageFormat::BMP:
            result = stbi_write_bmp(output_path.c_str(), file.width, file.height,
                                   file.channels, file.data.data());
            break;
            
        case ImageFormat::TGA:
            result = stbi_write_tga(output_path.c_str(), file.width, file.height,
                                   file.channels, file.data.data());
            break;
            
        default:
            set_error("Unsupported output format");
            return false;
    }
    
    if (!result) {
        set_error("Failed to write image: " + output_path);
        return false;
    }
    
    // Update statistics
    struct stat st;
    if (stat(output_path.c_str(), &st) == 0) {
        stats_.bytes_written += st.st_size;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.total_time_us += duration.count();
    
    return true;
}

bool MediaProcessor::convert(const MediaFile& input, MediaFile& output, 
                             const ConversionOptions& opts) {
    clear_error();
    
    if (!input.is_loaded()) {
        set_error("Input MediaFile not loaded");
        return false;
    }
    
    // Start with input data
    output = input;
    
    // Apply resizing if requested
    if (opts.target_width > 0 || opts.target_height > 0) {
        tbb32 new_width = opts.target_width > 0 ? opts.target_width : input.width;
        tbb32 new_height = opts.target_height > 0 ? opts.target_height : input.height;
        
        // Maintain aspect ratio if requested
        if (opts.maintain_aspect && opts.target_width > 0 && opts.target_height > 0) {
            float aspect = static_cast<float>(input.width) / static_cast<float>(input.height);
            float target_aspect = static_cast<float>(opts.target_width) / static_cast<float>(opts.target_height);
            
            if (aspect > target_aspect) {
                // Width is the limiting factor
                new_width = opts.target_width;
                new_height = static_cast<tbb32>(opts.target_width / aspect);
            } else {
                // Height is the limiting factor
                new_height = opts.target_height;
                new_width = static_cast<tbb32>(opts.target_height * aspect);
            }
        }
        
        if (!resize(input, output, new_width, new_height)) {
            return false;
        }
    }
    
    // Update format
    output.format = opts.output_format;
    
    return true;
}

bool MediaProcessor::resize(const MediaFile& input, MediaFile& output, 
                           tbb32 new_width, tbb32 new_height) {
    clear_error();
    
    if (!input.is_loaded()) {
        set_error("Input MediaFile not loaded");
        return false;
    }
    
    if (new_width == 0 || new_height == 0) {
        set_error("Invalid dimensions for resize");
        return false;
    }
    
    // Allocate output buffer
    size_t output_size = new_width * new_height * input.channels;
    std::vector<uint8_t> resized(output_size);
    
    // Perform bilinear resize
    resize_bilinear(input.data.data(), input.width, input.height, input.channels,
                    resized.data(), new_width, new_height);
    
    // Update output
    output = input;
    output.width = new_width;
    output.height = new_height;
    output.pixels = static_cast<tbb64>(new_width) * static_cast<tbb64>(new_height);
    output.data = std::move(resized);
    
    return true;
}

void MediaProcessor::resize_bilinear(const uint8_t* src, int src_w, int src_h, int channels,
                                     uint8_t* dst, int dst_w, int dst_h) {
    float x_ratio = static_cast<float>(src_w - 1) / static_cast<float>(dst_w);
    float y_ratio = static_cast<float>(src_h - 1) / static_cast<float>(dst_h);
    
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            float src_x = x * x_ratio;
            float src_y = y * y_ratio;
            
            int x_floor = static_cast<int>(src_x);
            int y_floor = static_cast<int>(src_y);
            int x_ceil = std::min(x_floor + 1, src_w - 1);
            int y_ceil = std::min(y_floor + 1, src_h - 1);
            
            float x_weight = src_x - x_floor;
            float y_weight = src_y - y_floor;
            
            for (int c = 0; c < channels; c++) {
                float tl = src[(y_floor * src_w + x_floor) * channels + c];
                float tr = src[(y_floor * src_w + x_ceil) * channels + c];
                float bl = src[(y_ceil * src_w + x_floor) * channels + c];
                float br = src[(y_ceil * src_w + x_ceil) * channels + c];
                
                float top = tl * (1 - x_weight) + tr * x_weight;
                float bottom = bl * (1 - x_weight) + br * x_weight;
                float value = top * (1 - y_weight) + bottom * y_weight;
                
                dst[(y * dst_w + x) * channels + c] = static_cast<uint8_t>(value);
            }
        }
    }
}

bool MediaProcessor::flip_vertical(MediaFile& file) {
    clear_error();
    
    if (!file.is_loaded()) {
        set_error("MediaFile not loaded");
        return false;
    }
    
    int row_size = file.width * file.channels;
    std::vector<uint8_t> temp_row(row_size);
    
    for (tbb32 y = 0; y < file.height / 2; y++) {
        uint8_t* top = file.data.data() + y * row_size;
        uint8_t* bottom = file.data.data() + (file.height - 1 - y) * row_size;
        
        std::memcpy(temp_row.data(), top, row_size);
        std::memcpy(top, bottom, row_size);
        std::memcpy(bottom, temp_row.data(), row_size);
    }
    
    return true;
}

bool MediaProcessor::flip_horizontal(MediaFile& file) {
    clear_error();
    
    if (!file.is_loaded()) {
        set_error("MediaFile not loaded");
        return false;
    }
    
    for (tbb32 y = 0; y < file.height; y++) {
        for (tbb32 x = 0; x < file.width / 2; x++) {
            int left_idx = (y * file.width + x) * file.channels;
            int right_idx = (y * file.width + (file.width - 1 - x)) * file.channels;
            
            for (tbb32 c = 0; c < file.channels; c++) {
                std::swap(file.data[left_idx + c], file.data[right_idx + c]);
            }
        }
    }
    
    return true;
}

bool MediaProcessor::extract_metadata(const std::string& filepath, MediaFile& metadata) {
    // Just use load_image - stb_image extracts metadata during load
    return load_image(filepath, metadata);
}

std::vector<std::string> MediaProcessor::batch_convert(
    const std::vector<std::string>& input_files,
    ImageFormat output_format,
    const std::string& output_dir,
    const ConversionOptions& opts) {
    
    std::vector<std::string> results;
    
    for (const auto& input_path : input_files) {
        MediaFile input, output;
        
        if (!load_image(input_path, input)) {
            results.push_back("FAILED: " + input_path + " (" + error_message() + ")");
            stats_.images_failed++;
            continue;
        }
        
        if (!convert(input, output, opts)) {
            results.push_back("FAILED: " + input_path + " (" + error_message() + ")");
            stats_.images_failed++;
            continue;
        }
        
        // Build output path
        size_t slash = input_path.find_last_of('/');
        std::string filename = (slash != std::string::npos) ? input_path.substr(slash + 1) : input_path;
        
        size_t dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            filename = filename.substr(0, dot);
        }
        
        std::string output_path = output_dir + "/" + filename + format_to_extension(output_format);
        
        if (!save_image(output, output_path, output_format, opts.jpeg_quality)) {
            results.push_back("FAILED: " + input_path + " (" + error_message() + ")");
            stats_.images_failed++;
            continue;
        }
        
        results.push_back("SUCCESS: " + input_path + " -> " + output_path);
    }
    
    return results;
}

// ============================================================================
// StreamProcessor Implementation
// ============================================================================

StreamProcessor::StreamProcessor(bool verbose)
    : verbose_(verbose)
{}

void StreamProcessor::emit_ui(const std::string& msg) {
    std::cout << msg << std::endl;
}

void StreamProcessor::emit_debug(const std::string& tag, const std::string& msg) {
    std::map<std::string, std::string> data;
    data["tag"] = tag;
    data["message"] = msg;
    streams::write_telemetry("debug", data);
}

void StreamProcessor::emit_progress(tbb32 current, tbb32 total, const std::string& filename) {
    if (!verbose_) return;
    
    int percent = (total > 0) ? (current * 100 / total) : 0;
    int bar_width = 40;
    int filled = (bar_width * current) / total;
    
    std::cout << "\r[";
    for (int i = 0; i < bar_width; i++) {
        if (i < filled) std::cout << "=";
        else if (i == filled) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << percent << "% " << filename << "          " << std::flush;
    
    if (current >= total) {
        std::cout << std::endl;
    }
}

void StreamProcessor::emit_image_binary(const MediaFile& file) {
    streams::write_image_binary(file);
}

bool StreamProcessor::process_file(const std::string& input_path,
                                   const std::string& output_path,
                                   const ConversionOptions& opts) {
    MediaFile input, output;
    
    if (verbose_) {
        emit_ui("Processing: " + input_path);
        emit_debug("process_file", "Loading " + input_path);
    }
    
    if (!processor_.load_image(input_path, input)) {
        emit_ui("ERROR: " + processor_.error_message());
        emit_debug("error", processor_.error_message());
        processor_stats_.images_failed++;
        return false;
    }
    
    if (verbose_) {
        emit_debug("metadata", std::to_string(input.width) + "x" + std::to_string(input.height));
    }
    
    if (!processor_.convert(input, output, opts)) {
        emit_ui("ERROR: " + processor_.error_message());
        emit_debug("error", processor_.error_message());
        processor_stats_.images_failed++;
        return false;
    }
    
    if (!processor_.save_image(output, output_path, opts.output_format, opts.jpeg_quality)) {
        emit_ui("ERROR: " + processor_.error_message());
        emit_debug("error", processor_.error_message());
        processor_stats_.images_failed++;
        return false;
    }
    
    if (verbose_) {
        emit_ui("SUCCESS: " + output_path);
        emit_debug("success", output_path);
    }
    
    // Emit binary data to FD 5
    emit_image_binary(output);
    
    processor_stats_.images_processed++;
    processor_stats_.bytes_read += input.file_size_bytes;
    
    return true;
}

std::vector<std::string> StreamProcessor::process_directory(
    const std::string& input_dir,
    const std::string& output_dir,
    const ConversionOptions& opts) {
    
    // This is a simplified version - in production would use filesystem traversal
    std::vector<std::string> results;
    results.push_back("Directory processing not implemented in this demo");
    return results;
}

void StreamProcessor::process_interactive() {
    emit_ui("=== Aria Media Processor (Interactive Mode) ===");
    emit_ui("Commands: convert <input> <output> <format>");
    emit_ui("          resize <input> <output> <width> <height>");
    emit_ui("          info <file>");
    emit_ui("          quit");
    emit_ui("");
    
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        
        if (line == "quit" || line == "exit") break;
        
        // Parse command (simplified parser)
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        
        if (cmd == "info") {
            std::string filepath;
            iss >> filepath;
            
            MediaFile meta;
            if (processor_.extract_metadata(filepath, meta)) {
                emit_ui("Format: " + processor_.format_to_string(meta.format));
                emit_ui("Dimensions: " + std::to_string(meta.width) + "x" + std::to_string(meta.height));
                emit_ui("Channels: " + std::to_string(meta.channels));
                emit_ui("File size: " + std::to_string(meta.file_size_bytes) + " bytes");
            } else {
                emit_ui("ERROR: " + processor_.error_message());
            }
        } else {
            emit_ui("Unknown command: " + cmd);
        }
    }
}

// ============================================================================
// Stream Writers
// ============================================================================

namespace streams {

void write_telemetry(const std::string& event, const std::map<std::string, std::string>& data) {
    std::ostringstream json;
    json << "{\"event\":\"" << event << "\"";
    for (const auto& kv : data) {
        json << ",\"" << kv.first << "\":\"" << kv.second << "\"";
    }
    json << "}\n";
    
    std::string output = json.str();
    ssize_t written = write(3, output.c_str(), output.length());
    (void)written;  // Silence unused warning
}

void write_image_binary(const MediaFile& file) {
    // Binary protocol: [magic:4][width:4][height:4][channels:4][format:4][data_size:8][pixels...]
    
    const char magic[4] = {'A', 'I', 'M', 'G'};
    write(5, magic, 4);
    
    write(5, &file.width, 4);
    write(5, &file.height, 4);
    write(5, &file.channels, 4);
    
    tbb32 format_code = static_cast<tbb32>(file.format);
    write(5, &format_code, 4);
    
    tbb64 data_size = file.data.size();
    write(5, &data_size, 8);
    
    write(5, file.data.data(), file.data.size());
}

void write_stats_binary(const ProcessStats& stats) {
    // Binary protocol: [magic:4][stats...]
    const char magic[4] = {'S', 'T', 'A', 'T'};
    write(5, magic, 4);
    
    write(5, &stats.images_processed, 8);
    write(5, &stats.images_failed, 8);
    write(5, &stats.bytes_read, 8);
    write(5, &stats.bytes_written, 8);
    write(5, &stats.total_time_us, 8);
}

} // namespace streams

} // namespace apic
} // namespace aria

// ============================================================================
// C FFI API
// ============================================================================

using namespace aria::apic;

static std::string g_ffi_error;
static std::string g_ffi_result;

int aria_media_convert(const char* input_path, const char* output_path, const char* format_str) {
    MediaProcessor proc;
    MediaFile input, output;
    
    if (!proc.load_image(input_path, input)) {
        g_ffi_error = proc.error_message();
        return -1;
    }
    
    ImageFormat fmt = ImageFormat::PNG;
    std::string fmt_s = format_str;
    std::transform(fmt_s.begin(), fmt_s.end(), fmt_s.begin(), ::tolower);
    
    if (fmt_s == "png") fmt = ImageFormat::PNG;
    else if (fmt_s == "jpeg" || fmt_s == "jpg") fmt = ImageFormat::JPEG;
    else if (fmt_s == "bmp") fmt = ImageFormat::BMP;
    else {
        g_ffi_error = "Unknown format: " + fmt_s;
        return -2;
    }
    
    ConversionOptions opts;
    opts.output_format = fmt;
    
    if (!proc.convert(input, output, opts)) {
        g_ffi_error = proc.error_message();
        return -3;
    }
    
    if (!proc.save_image(output, output_path, fmt, 90)) {
        g_ffi_error = proc.error_message();
        return -4;
    }
    
    return 0;
}

int aria_media_resize(const char* input_path, const char* output_path, int width, int height) {
    MediaProcessor proc;
    MediaFile input, output;
    
    if (!proc.load_image(input_path, input)) {
        g_ffi_error = proc.error_message();
        return -1;
    }
    
    if (!proc.resize(input, output, width, height)) {
        g_ffi_error = proc.error_message();
        return -2;
    }
    
    if (!proc.save_image(output, output_path, input.format, 90)) {
        g_ffi_error = proc.error_message();
        return -3;
    }
    
    return 0;
}

const char* aria_media_info(const char* filepath) {
    MediaProcessor proc;
    MediaFile meta;
    
    if (!proc.extract_metadata(filepath, meta)) {
        g_ffi_error = proc.error_message();
        return nullptr;
    }
    
    std::ostringstream info;
    info << "Format: " << proc.format_to_string(meta.format) << "\n";
    info << "Dimensions: " << meta.width << "x" << meta.height << "\n";
    info << "Channels: " << meta.channels << "\n";
    info << "File size: " << meta.file_size_bytes << " bytes\n";
    info << "Pixels: " << meta.pixels << "\n";
    
    g_ffi_result = info.str();
    return g_ffi_result.c_str();
}

void aria_media_free(const char* result) {
    // No-op - using static buffers
    (void)result;
}
