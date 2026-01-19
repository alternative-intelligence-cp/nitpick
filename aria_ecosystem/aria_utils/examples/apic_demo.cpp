/**
 * @file apic_demo.cpp
 * @brief Comprehensive demonstrations of apic (Aria picture processor)
 */

#include "../src/apic/media_processor.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <sys/stat.h>

using namespace aria::apic;

// Demo helper: Create a test image in memory
void create_test_image(MediaFile& file, tbb32 width, tbb32 height, tbb32 channels) {
    file.width = width;
    file.height = height;
    file.channels = channels;
    file.pixels = static_cast<tbb64>(width) * static_cast<tbb64>(height);
    file.format = ImageFormat::PNG;
    file.colorspace = (channels == 3) ? ColorSpace::RGB : ColorSpace::RGBA;
    
    size_t data_size = width * height * channels;
    file.data.resize(data_size);
    
    // Create gradient pattern
    for (tbb32 y = 0; y < height; y++) {
        for (tbb32 x = 0; x < width; x++) {
            size_t idx = (y * width + x) * channels;
            file.data[idx + 0] = static_cast<uint8_t>((x * 255) / width);      // R
            file.data[idx + 1] = static_cast<uint8_t>((y * 255) / height);     // G
            file.data[idx + 2] = static_cast<uint8_t>(128);                     // B
            if (channels == 4) {
                file.data[idx + 3] = 255;  // Alpha
            }
        }
    }
}

bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

void demo_1_basic_save_load() {
    std::cout << "\n=== Demo 1: Basic Image Save/Load ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile test_img;
    
    // Create test image
    create_test_image(test_img, 128, 128, 3);
    
    // Save as PNG
    std::string png_path = "/tmp/test_apic_demo1.png";
    bool saved = proc.save_image(test_img, png_path, ImageFormat::PNG, 90);
    assert(saved && "Should save PNG");
    assert(file_exists(png_path) && "PNG file should exist");
    
    // Load it back
    MediaFile loaded;
    bool loaded_ok = proc.load_image(png_path, loaded);
    assert(loaded_ok && "Should load PNG");
    assert(loaded.width == 128 && "Width should match");
    assert(loaded.height == 128 && "Height should match");
    assert(loaded.channels == 3 && "Channels should match");
    assert(loaded.format == ImageFormat::PNG && "Format should be PNG");
    
    std::cout << "✓ Created 128x128 test image" << std::endl;
    std::cout << "✓ Saved to PNG: " << png_path << std::endl;
    std::cout << "✓ Loaded back: " << loaded.width << "x" << loaded.height << std::endl;
}

void demo_2_png_to_jpeg() {
    std::cout << "\n=== Demo 2: PNG → JPEG Conversion ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile png_img, jpeg_img;
    
    // Create and save PNG
    create_test_image(png_img, 256, 256, 3);
    std::string png_path = "/tmp/test_apic_demo2.png";
    proc.save_image(png_img, png_path, ImageFormat::PNG, 90);
    
    // Load PNG
    proc.load_image(png_path, png_img);
    
    // Convert to JPEG
    ConversionOptions opts;
    opts.output_format = ImageFormat::JPEG;
    opts.jpeg_quality = 85;
    
    bool converted = proc.convert(png_img, jpeg_img, opts);
    assert(converted && "Conversion should succeed");
    
    // Save JPEG
    std::string jpeg_path = "/tmp/test_apic_demo2.jpg";
    proc.save_image(jpeg_img, jpeg_path, ImageFormat::JPEG, opts.jpeg_quality);
    assert(file_exists(jpeg_path) && "JPEG file should exist");
    
    std::cout << "✓ Converted PNG → JPEG (quality " << opts.jpeg_quality << ")" << std::endl;
    std::cout << "✓ Output: " << jpeg_path << std::endl;
}

void demo_3_png_to_bmp() {
    std::cout << "\n=== Demo 3: PNG → BMP Conversion ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile png_img, bmp_img;
    
    create_test_image(png_img, 64, 64, 3);
    
    ConversionOptions opts;
    opts.output_format = ImageFormat::BMP;
    
    bool converted = proc.convert(png_img, bmp_img, opts);
    assert(converted && "Conversion should succeed");
    
    std::string bmp_path = "/tmp/test_apic_demo3.bmp";
    proc.save_image(bmp_img, bmp_path, ImageFormat::BMP, 90);
    assert(file_exists(bmp_path) && "BMP file should exist");
    
    std::cout << "✓ Converted PNG → BMP" << std::endl;
    std::cout << "✓ Output: " << bmp_path << std::endl;
}

void demo_4_resize() {
    std::cout << "\n=== Demo 4: Image Resizing ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile original, resized;
    
    // Create 256x256 image
    create_test_image(original, 256, 256, 3);
    
    // Resize to 128x128
    bool resized_ok = proc.resize(original, resized, 128, 128);
    assert(resized_ok && "Resize should succeed");
    assert(resized.width == 128 && "Width should be 128");
    assert(resized.height == 128 && "Height should be 128");
    assert(resized.data.size() == 128 * 128 * 3 && "Data size should match");
    
    std::cout << "✓ Resized from 256x256 → 128x128" << std::endl;
    std::cout << "✓ Data size: " << original.data.size() << " → " << resized.data.size() << " bytes" << std::endl;
    
    // Save resized
    std::string resize_path = "/tmp/test_apic_demo4_resized.png";
    proc.save_image(resized, resize_path, ImageFormat::PNG, 90);
    std::cout << "✓ Saved resized image: " << resize_path << std::endl;
}

void demo_5_aspect_ratio() {
    std::cout << "\n=== Demo 5: Aspect Ratio Preservation ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile original, converted;
    
    // Create 400x200 image (2:1 aspect ratio)
    create_test_image(original, 400, 200, 3);
    
    ConversionOptions opts;
    opts.output_format = ImageFormat::PNG;
    opts.target_width = 200;
    opts.target_height = 200;  // Request square
    opts.maintain_aspect = true;  // But maintain aspect
    
    bool converted_ok = proc.convert(original, converted, opts);
    assert(converted_ok && "Conversion should succeed");
    
    // Should be 200x100 (maintaining 2:1 aspect ratio)
    assert(converted.width == 200 && "Width should be 200");
    assert(converted.height == 100 && "Height should be 100");
    
    std::cout << "✓ Original: 400x200 (2:1 aspect)" << std::endl;
    std::cout << "✓ Requested: 200x200" << std::endl;
    std::cout << "✓ Result: " << converted.width << "x" << converted.height << " (aspect preserved)" << std::endl;
}

void demo_6_flip_operations() {
    std::cout << "\n=== Demo 6: Flip Operations ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile img;
    
    // Create 100x100 test pattern
    create_test_image(img, 100, 100, 3);
    
    // Store original first pixel
    uint8_t orig_r = img.data[0];
    uint8_t orig_g = img.data[1];
    uint8_t orig_b = img.data[2];
    
    // Flip vertical
    bool flipped_v = proc.flip_vertical(img);
    assert(flipped_v && "Vertical flip should succeed");
    
    std::cout << "✓ Flipped vertically" << std::endl;
    
    // Flip horizontal
    bool flipped_h = proc.flip_horizontal(img);
    assert(flipped_h && "Horizontal flip should succeed");
    
    std::cout << "✓ Flipped horizontally" << std::endl;
    
    // Save flipped image
    std::string flip_path = "/tmp/test_apic_demo6_flipped.png";
    proc.save_image(img, flip_path, ImageFormat::PNG, 90);
    std::cout << "✓ Saved flipped image: " << flip_path << std::endl;
}

void demo_7_metadata_extraction() {
    std::cout << "\n=== Demo 7: Metadata Extraction ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile test_img;
    
    // Create and save test image
    create_test_image(test_img, 320, 240, 4);  // RGBA
    std::string png_path = "/tmp/test_apic_demo7.png";
    proc.save_image(test_img, png_path, ImageFormat::PNG, 90);
    
    // Extract metadata
    MediaFile metadata;
    bool extracted = proc.extract_metadata(png_path, metadata);
    assert(extracted && "Metadata extraction should succeed");
    
    std::cout << "✓ Format: " << static_cast<int>(metadata.format) << " (PNG)" << std::endl;
    std::cout << "✓ Dimensions: " << metadata.width << "x" << metadata.height << std::endl;
    std::cout << "✓ Channels: " << metadata.channels << std::endl;
    std::cout << "✓ Pixels: " << metadata.pixels << std::endl;
    std::cout << "✓ File size: " << metadata.file_size_bytes << " bytes" << std::endl;
}

void demo_8_batch_conversion() {
    std::cout << "\n=== Demo 8: Batch Conversion ===" << std::endl;
    
    MediaProcessor proc;
    
    // Create test images
    std::vector<std::string> input_files;
    for (int i = 0; i < 3; i++) {
        MediaFile test;
        create_test_image(test, 64 + i * 32, 64 + i * 32, 3);
        std::string path = "/tmp/test_apic_batch_" + std::to_string(i) + ".png";
        proc.save_image(test, path, ImageFormat::PNG, 90);
        input_files.push_back(path);
    }
    
    // Batch convert to JPEG
    ConversionOptions opts;
    opts.output_format = ImageFormat::JPEG;
    opts.jpeg_quality = 80;
    
    std::vector<std::string> results = proc.batch_convert(
        input_files, ImageFormat::JPEG, "/tmp", opts
    );
    
    assert(results.size() == 3 && "Should process 3 files");
    
    for (const auto& result : results) {
        std::cout << "  " << result << std::endl;
    }
    
    const ProcessStats& stats = proc.get_stats();
    std::cout << "✓ Batch processed " << stats.images_processed << " images" << std::endl;
}

void demo_9_quality_comparison() {
    std::cout << "\n=== Demo 9: JPEG Quality Comparison ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile test_img;
    
    create_test_image(test_img, 512, 512, 3);
    
    int qualities[] = {10, 50, 90};
    for (int quality : qualities) {
        std::string jpeg_path = "/tmp/test_apic_quality_" + std::to_string(quality) + ".jpg";
        proc.save_image(test_img, jpeg_path, ImageFormat::JPEG, quality);
        
        struct stat st;
        stat(jpeg_path.c_str(), &st);
        
        std::cout << "✓ Quality " << quality << ": " << st.st_size << " bytes" << std::endl;
    }
}

void demo_10_error_handling() {
    std::cout << "\n=== Demo 10: Error Handling ===" << std::endl;
    
    MediaProcessor proc;
    MediaFile file;
    
    // Try to load nonexistent file
    bool loaded = proc.load_image("/tmp/nonexistent_image.png", file);
    assert(!loaded && "Should fail to load");
    assert(proc.has_error() && "Should have error");
    
    std::cout << "✓ Detected missing file error" << std::endl;
    std::cout << "  Error: " << proc.error_message() << std::endl;
    
    // Try to resize unloaded file
    MediaFile unloaded, resized;
    bool resized_ok = proc.resize(unloaded, resized, 100, 100);
    assert(!resized_ok && "Should fail to resize unloaded file");
    assert(proc.has_error() && "Should have error");
    
    std::cout << "✓ Detected unloaded file error" << std::endl;
    std::cout << "  Error: " << proc.error_message() << std::endl;
}

void demo_11_six_stream_output() {
    std::cout << "\n=== Demo 11: Six-Stream Output ===" << std::endl;
    
    StreamProcessor stream_proc(true);  // verbose mode
    
    // Create test image
    MediaFile test;
    create_test_image(test, 200, 150, 3);
    std::string input_path = "/tmp/test_apic_stream_input.png";
    
    MediaProcessor proc;
    proc.save_image(test, input_path, ImageFormat::PNG, 90);
    
    // Process with six-stream output
    ConversionOptions opts;
    opts.output_format = ImageFormat::JPEG;
    opts.jpeg_quality = 85;
    opts.verbose = true;
    
    std::string output_path = "/tmp/test_apic_stream_output.jpg";
    bool processed = stream_proc.process_file(input_path, output_path, opts);
    
    assert(processed && "Processing should succeed");
    
    std::cout << "✓ Processed with six-stream output" << std::endl;
    std::cout << "  - stdout: UI messages" << std::endl;
    std::cout << "  - FD 3: Telemetry (stddbg)" << std::endl;
    std::cout << "  - FD 5: Binary image data (stddato)" << std::endl;
    
    const ProcessStats& stats = stream_proc.stats();
    std::cout << "✓ Images processed: " << stats.images_processed << std::endl;
}

void demo_12_ffi_api() {
    std::cout << "\n=== Demo 12: FFI C API ===" << std::endl;
    
    // Create test image
    MediaFile test;
    create_test_image(test, 150, 100, 3);
    std::string png_path = "/tmp/test_apic_ffi.png";
    
    MediaProcessor proc;
    proc.save_image(test, png_path, ImageFormat::PNG, 90);
    
    // Test convert via FFI
    std::string jpeg_path = "/tmp/test_apic_ffi.jpg";
    int result = aria_media_convert(png_path.c_str(), jpeg_path.c_str(), "jpeg");
    assert(result == 0 && "FFI convert should succeed");
    assert(file_exists(jpeg_path) && "JPEG should exist");
    
    std::cout << "✓ FFI convert: PNG → JPEG" << std::endl;
    
    // Test resize via FFI
    std::string resized_path = "/tmp/test_apic_ffi_resized.png";
    result = aria_media_resize(png_path.c_str(), resized_path.c_str(), 75, 50);
    assert(result == 0 && "FFI resize should succeed");
    assert(file_exists(resized_path) && "Resized should exist");
    
    std::cout << "✓ FFI resize: 150x100 → 75x50" << std::endl;
    
    // Test info via FFI
    const char* info = aria_media_info(png_path.c_str());
    assert(info != nullptr && "FFI info should succeed");
    
    std::cout << "✓ FFI info:" << std::endl;
    std::cout << info;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Aria Picture Processor (apic) - Demo Suite" << std::endl;
    std::cout << "  12 Comprehensive Demonstrations" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    try {
        demo_1_basic_save_load();
        demo_2_png_to_jpeg();
        demo_3_png_to_bmp();
        demo_4_resize();
        demo_5_aspect_ratio();
        demo_6_flip_operations();
        demo_7_metadata_extraction();
        demo_8_batch_conversion();
        demo_9_quality_comparison();
        demo_10_error_handling();
        demo_11_six_stream_output();
        demo_12_ffi_api();
        
        std::cout << "\n==================================================" << std::endl;
        std::cout << "  ✓ All 12 demos completed successfully!" << std::endl;
        std::cout << "==================================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
