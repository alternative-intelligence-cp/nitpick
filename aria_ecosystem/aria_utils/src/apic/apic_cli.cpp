// ============================================================================
// APIC - Image Processing CLI with Six-Stream Topology
// ============================================================================
// Purpose: Image conversion and manipulation utility
//
// Six-Stream Architecture:
//   FD 0 (stdin):   Image data input (future)
//   FD 1 (stdout):  Progress messages
//   FD 2 (stderr):  Error messages
//   FD 3 (stddbg):  Debug telemetry (NDJSON)
//   FD 4 (stddati): Not used
//   FD 5 (stddato): Binary image data (future)
//
// Usage:
//   apic [OPTIONS] INPUT OUTPUT
//
// Options:
//   -f FORMAT      Output format (png, jpeg, bmp)
//   -q QUALITY     JPEG quality (1-100)
//   -w WIDTH       Resize width
//   -h HEIGHT      Resize height
//   -a, --aspect   Maintain aspect ratio
//   -v, --verbose  Verbose output
//   --debug        Enable debug telemetry on FD 3
//   --help         Show this help message
// ============================================================================

#include "media_processor.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <chrono>

// FD constants
#ifndef STDDBG_FILENO
#define STDDBG_FILENO 3
#endif

using namespace aria::apic;

// ============================================================================
// Options
// ============================================================================
struct Options {
    std::string input_file;
    std::string output_file;
    ImageFormat output_format = ImageFormat::AUTO;
    uint32_t target_width = 0;
    uint32_t target_height = 0;
    uint32_t jpeg_quality = 90;
    bool maintain_aspect = false;
    bool verbose = false;
    bool debug = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

ImageFormat parse_format(const std::string& format_str) {
    if (format_str == "png") return ImageFormat::PNG;
    if (format_str == "jpeg" || format_str == "jpg") return ImageFormat::JPEG;
    if (format_str == "bmp") return ImageFormat::BMP;
    if (format_str == "tga") return ImageFormat::TGA;
    return ImageFormat::AUTO;
}

std::string format_to_string(ImageFormat format) {
    switch (format) {
        case ImageFormat::PNG: return "PNG";
        case ImageFormat::JPEG: return "JPEG";
        case ImageFormat::BMP: return "BMP";
        case ImageFormat::TGA: return "TGA";
        default: return "UNKNOWN";
    }
}

std::string colorspace_to_string(ColorSpace cs) {
    switch (cs) {
        case ColorSpace::RGB: return "RGB";
        case ColorSpace::RGBA: return "RGBA";
        case ColorSpace::GRAYSCALE: return "Grayscale";
        case ColorSpace::GRAYSCALE_ALPHA: return "Grayscale+Alpha";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Main Processing
// ============================================================================

int process_image(const Options& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"apic\","
            "\"message\":\"Processing: %s -> %s\"}\n",
            opts.input_file.c_str(),
            opts.output_file.c_str()
        );
    }
    
    MediaProcessor processor;
    MediaFile input_file, output_file;
    
    // Load input image
    if (!processor.load_image(opts.input_file, input_file)) {
        std::cerr << "apic: cannot load image '" << opts.input_file << "'\n";
        return 1;
    }
    
    if (opts.verbose) {
        std::cout << "Loaded: " << opts.input_file << "\n";
        std::cout << "  Format: " << format_to_string(input_file.format) << "\n";
        std::cout << "  Dimensions: " << input_file.width << "x" << input_file.height << "\n";
        std::cout << "  Color: " << colorspace_to_string(input_file.colorspace) << "\n";
        std::cout << "  Channels: " << input_file.channels << "\n";
    }
    
    // Resize if requested
    if (opts.target_width > 0 || opts.target_height > 0) {
        uint32_t new_width = opts.target_width > 0 ? opts.target_width : input_file.width;
        uint32_t new_height = opts.target_height > 0 ? opts.target_height : input_file.height;
        
        // Maintain aspect ratio if requested
        if (opts.maintain_aspect && opts.target_width > 0 && opts.target_height == 0) {
            double aspect = static_cast<double>(input_file.height) / input_file.width;
            new_height = static_cast<uint32_t>(new_width * aspect);
        } else if (opts.maintain_aspect && opts.target_height > 0 && opts.target_width == 0) {
            double aspect = static_cast<double>(input_file.width) / input_file.height;
            new_width = static_cast<uint32_t>(new_height * aspect);
        }
        
        if (opts.verbose) {
            std::cout << "Resizing to: " << new_width << "x" << new_height << "\n";
        }
        
        MediaFile resized;
        if (!processor.resize(input_file, resized, new_width, new_height)) {
            std::cerr << "apic: resize failed\n";
            return 1;
        }
        
        output_file = resized;
    } else {
        output_file = input_file;
    }
    
    // Determine output format
    ImageFormat out_format = opts.output_format;
    if (out_format == ImageFormat::AUTO) {
        // Detect from extension
        std::string ext = opts.output_file.substr(opts.output_file.find_last_of('.') + 1);
        out_format = parse_format(ext);
        if (out_format == ImageFormat::AUTO) {
            out_format = ImageFormat::PNG;  // Default
        }
    }
    
    // Save output image
    if (!processor.save_image(output_file, opts.output_file, out_format, opts.jpeg_quality)) {
        std::cerr << "apic: cannot save image '" << opts.output_file << "'\n";
        return 1;
    }
    
    if (opts.verbose) {
        std::cout << "Saved: " << opts.output_file << "\n";
        std::cout << "  Format: " << format_to_string(out_format) << "\n";
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"apic\","
            "\"input_format\":\"%s\",\"output_format\":\"%s\","
            "\"input_width\":%u,\"input_height\":%u,"
            "\"output_width\":%u,\"output_height\":%u,"
            "\"elapsed_ms\":%.6f}\n",
            format_to_string(input_file.format).c_str(),
            format_to_string(out_format).c_str(),
            input_file.width, input_file.height,
            output_file.width, output_file.height,
            elapsed_ms
        );
    }
    
    return 0;
}

// ============================================================================
// Help and Main
// ============================================================================

void print_help(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] INPUT OUTPUT\n\n"
              << "Image processing utility\n\n"
              << "Options:\n"
              << "  -f FORMAT      Output format (png, jpeg, bmp)\n"
              << "  -q QUALITY     JPEG quality (1-100, default: 90)\n"
              << "  -w WIDTH       Resize width\n"
              << "  -h HEIGHT      Resize height\n"
              << "  -a, --aspect   Maintain aspect ratio\n"
              << "  -v, --verbose  Verbose output\n"
              << "  --debug        Enable debug telemetry on FD 3\n"
              << "  --help         Show this help message\n\n"
              << "Examples:\n"
              << "  apic input.png output.jpg\n"
              << "  apic -f png -w 800 input.jpg resized.png\n"
              << "  apic -w 1920 -a image.png scaled.png\n";
}

int main(int argc, char* argv[]) {
    Options opts;
    
    // Filter long options
    std::vector<char*> filtered_argv;
    filtered_argv.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            opts.debug = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--aspect") == 0) {
            opts.maintain_aspect = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts.verbose = true;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    
    argc = filtered_argv.size();
    argv = filtered_argv.data();
    
    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "f:q:w:h:av")) != -1) {
        switch (opt) {
            case 'f':
                opts.output_format = parse_format(optarg);
                break;
            case 'q':
                opts.jpeg_quality = std::atoi(optarg);
                if (opts.jpeg_quality < 1 || opts.jpeg_quality > 100) {
                    std::cerr << "apic: quality must be 1-100\n";
                    return 1;
                }
                break;
            case 'w':
                opts.target_width = std::atoi(optarg);
                break;
            case 'h':
                opts.target_height = std::atoi(optarg);
                break;
            case 'a':
                opts.maintain_aspect = true;
                break;
            case 'v':
                opts.verbose = true;
                break;
            default:
                print_help(filtered_argv[0]);
                return 1;
        }
    }
    
    // Get input and output files
    if (optind + 1 >= argc) {
        std::cerr << "apic: missing input or output file\n";
        print_help(filtered_argv[0]);
        return 1;
    }
    
    opts.input_file = argv[optind];
    opts.output_file = argv[optind + 1];
    
    return process_image(opts);
}
