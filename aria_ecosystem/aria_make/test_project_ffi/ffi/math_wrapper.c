#include <math.h>

// Simple wrapper around sqrt for FFI testing
double aria_sqrt(double x) {
    return sqrt(x);
}

// Calculate distance between two points
double aria_distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx*dx + dy*dy);
}
// Modified
