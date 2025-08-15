# BMP
A header only portable C++ BMP/DIB parser that supports **all documented and semi-documented** BMP specifications, including Windows (INFO/V2/V3/V4/V5) and OS/2 v1 (CORE) and OS/2 v2 variants. Designed for full compatibility with historical and modern BMP files, including unusual bitfield layouts, palettes, and advanced color information.

> **Note:** We intentionally avoid proprietary or niche BMP flavors (e.g., Adobe BMP, GIMP BMP). It focuses on official and widely-used platform variations, including some unofficial but de facto Windows and macOS BMP behaviors.

## Features

* **Header Support**:

  * Windows BITMAPINFOHEADER (V3), BITMAPV4HEADER, BITMAPV5HEADER
  * Windows BITMAPCOREHEADER (OS/2 v1) and OS/2 v2 variants
  * All documented header sizes recognized and parsed

* **Pixel Format Support**:

  * 1, 2, 4, 8, 16, 24, 32 bits per pixel
  * Indexed color with palettes
  * Top-down and bottom-up bitmaps
  * Arbitrary bitfields (including alpha channel)

* **Compression Support**:

  * `BI_RGB` (uncompressed)
  * `BI_RLE8` and `BI_RLE4`
  * `BI_BITFIELDS` / `BI_ALPHABITFIELDS`
  * Embedded `BI_JPEG` / `BI_PNG` streams (exposed as raw data)

* **Advanced Color Features**:

  * V4/V5 color space fields (sRGB, calibrated RGB)
  * Gamma, color endpoints, rendering intent
  * Embedded ICC profiles (raw data preserved)

* **Other**:

  * Recognizes CMYK and CMYK RLE modes (rare)
  * Preserves unknown/extra header data for round-tripping

## Build Instructions

1. **Clone this repository**:

   ```bash
   git clone https://github.com/sushant-k-ray/BMP.git
   cd BMP
   ```

2. **Install dependencies**:

   * **Linux** (Debian/Ubuntu):

     ```bash
     sudo apt install libglfw3-dev
     ```
   * **Windows**:
     Use vcpkg or manually download GLFW + GLAD.

3. **Compile**:

   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

4. **Run**:

   ```bash
   ./BMP
   ```

## Usage

```cpp
#include "bmp.hpp"
#include <fstream>
#include <iostream>

int main() {
    std::ifstream file("image.bmp", std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    BMPImage image;
    if (!parseBMP(file, image)) {
        std::cerr << "Failed to parse BMP\n";
        return 1;
    }

    std::cout << "Width: " << image.width << " Height: " << image.height << "\n";
}
```

## Limitations

* No conversion for CMYK modes — data is preserved but not converted.
* Embedded JPEG/PNG are preserved as blobs; decoding requires external libraries.
* BMP does not natively define HDR/float formats — high bit depth is supported via masks.

## License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.


