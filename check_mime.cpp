#include <iostream>
#include <vector>
#include <string>
#include "include/c2pa.hpp"

int main() {
    try {
        auto supported_types = c2pa::Builder::supported_mime_types();
        std::cout << "Supported MIME types:" << std::endl;
        for (const auto& type : supported_types) {
            std::cout << "  " << type << std::endl;
        }
        
        bool video_mp4_supported = std::find(supported_types.begin(), supported_types.end(), "video/mp4") != supported_types.end();
        std::cout << "
video/mp4 supported: " << (video_mp4_supported ? "YES" : "NO") << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
