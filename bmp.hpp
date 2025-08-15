/*    Copyright (c) 2025 Sushant kr. Ray
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a copy
 *    of this software and associated documentation files (the "Software"), to deal
 *    in the Software without restriction, including without limitation the rights
 *    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the Software is
 *    furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in all
 *    copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *    SOFTWARE.
 */

#ifndef BMP_H
#define BMP_H

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <limits>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace bmp {
struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

static inline bool add_overflow(size_t a, size_t b, size_t& out){
    if (a > std::numeric_limits<size_t>::max() - b)
        return true;

    out = a + b;
    return false;
}

static inline uint16_t le16(const uint8_t* p){
    return uint16_t(p[0] | (uint16_t(p[1])<<8));
}

static inline uint32_t le32(const uint8_t* p){
    return uint32_t(p[0] | (uint32_t(p[1])<<8) |
                    (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24));
}

static inline uint64_t le64(const uint8_t* p){
    return uint64_t(le32(p)) | (uint64_t(le32(p+4))<<32);
}

enum class DIBType { CORE_OS2_V1, OS2_V2, INFO, V2, V3, V4, V5 };

// Compression values from Windows headers (wingdi.h) and OS/2 docs
enum class Compression : uint32_t {
    BI_RGB            = 0,
    BI_RLE8           = 1,
    BI_RLE4           = 2,
    BI_BITFIELDS      = 3,
    BI_JPEG           = 4,
    BI_PNG            = 5,
    BI_ALPHABITFIELDS = 6,
    BI_CMYK           = 11,
    BI_CMYKRLE8       = 12,
    BI_CMYKRLE4       = 13
};

// Color space type codes (V4/V5)
enum class ColorSpaceType : uint32_t {
    LCS_CALIBRATED_RGB = 0x00000000,
    LCS_sRGB           = 0x73524742,
    LCS_WINDOWS_COLOR_SPACE = 0x57696E20,
    PROFILE_LINKED     = 0x4C494E4B,
    PROFILE_EMBEDDED   = 0x4D424544
};

// Rendering intent (V5)
enum class RenderingIntent : uint32_t {
    LCS_GM_ABS_COLORIMETRIC = 8,
    LCS_GM_BUSINESS = 1,
    LCS_GM_GRAPHICS = 2,
    LCS_GM_IMAGES = 4
};


enum class PixelFormat {
    RGBA8,
    BGRA8,
    BGR8,
    Gray8,

    // If the source uses bitfields beyond simple 5:6:5 or 8:8:8:8 and
    // cannot be losslessly mapped to 8-bit, we expose raw packed
    // data + masks instead of forcing truncation.
    RawBitfields
};

struct Bitmasks { uint32_t r = 0, g = 0, b = 0, a = 0; };
struct CIEXYZ { int32_t x = 0, y = 0, z = 0; }; // 16.16 fixed from BMP V4/V5
struct CIEXYZTRIPLE { CIEXYZ r, g, b; };

struct Metadata {
    DIBType dib_type;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 1;
    uint16_t bpp = 0;
    Compression compression = Compression::BI_RGB;
    uint32_t image_size = 0;
    uint32_t ppm_x = 0;
    uint32_t ppm_y = 0;
    uint32_t color_used = 0;
    uint32_t color_important = 0;

    // Bitfields (INFO/V2/V3/V4/V5)
    bool has_masks = false;
    Bitmasks masks{};

    // V4/V5 fields
    ColorSpaceType cstype = ColorSpaceType::LCS_sRGB;
    CIEXYZTRIPLE endpoints{};
    uint32_t gamma_red = 0;
    uint32_t gamma_green = 0;
    uint32_t gamma_blue = 0;

    // V5 extras
    RenderingIntent intent = RenderingIntent::LCS_GM_IMAGES;
    std::vector<uint8_t> embedded_profile;

    uint32_t file_offset_pixels = 0;
    uint32_t header_size = 0;
    uint32_t file_size = 0;
    bool top_down() const {
        return height < 0;
    }

    uint32_t abs_height() const {
        return height < 0 ? uint32_t(-int64_t(height)) : uint32_t(height);
    }
};

struct PaletteEntry { uint8_t b, g, r, a; };
struct Image {
    Metadata meta;
    PixelFormat format = PixelFormat::RGBA8;
    std::vector<uint8_t> pixels;
    std::vector<PaletteEntry> palette;
    Bitmasks raw_masks{};
    uint8_t raw_bits_per_pixel = 0;
};

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : p_(data), n_(size) {}

    static Image from_file(const std::string& path){
        std::ifstream f(path, std::ios::binary);
        if(!f)
            throw ParseError("Cannot open file");

        f.seekg(0, std::ios::end);
        auto len = f.tellg();
        f.seekg(0);
        if(len <= 0)
            throw ParseError("Empty file");

        std::vector<uint8_t> buf = std::vector<uint8_t>(size_t(len));
        f.read(reinterpret_cast<char*>(buf.data()), len);
        if(!f)
            throw ParseError("Failed to read file");

        Reader r(buf.data(), buf.size());
        return r.parse();
    }

    Image parse(){
        // BMP file header (14 bytes)
        if(n_ < 14)
            throw ParseError("Truncated BMP header");

        if(p_[0] != 'B' || p_[1] != 'M')
            throw ParseError("Not a BMP (missing 'BM')");

        uint32_t file_size = le32(p_ + 2);
        uint32_t reserved = le32(p_ + 6);
        uint32_t off_bits = le32(p_ + 10);

        if(off_bits > n_)
            throw ParseError("Pixel data offset beyond file size");

        // DIB header dispatch
        if(n_ < 18)
            throw ParseError("Missing DIB header size");

        const uint8_t* dib = p_ + 14;
        uint32_t dib_size = le32(dib);

        if (dib_size + 14u > n_)
            throw ParseError("Truncated DIB header");

        Metadata m{};
        m.file_offset_pixels = off_bits;
        m.header_size = dib_size;
        m.file_size = file_size;

        if(dib_size == 12){ // BITMAPCOREHEADER (OS/2 v1)
            m.dib_type = DIBType::CORE_OS2_V1;
            m.width = int16_t(le16(dib + 4));
            m.height = int16_t(le16(dib + 6));
            m.planes = le16(dib + 8);
            m.bpp = le16(dib + 10);
        } else if(dib_size == 16 || dib_size == 64){
            if (dib_size == 64){
                m.dib_type = DIBType::OS2_V2;
                m.width = int32_t(le32(dib + 4));
                m.height = int32_t(le32(dib + 8));
                m.planes = le16(dib + 12);
                m.bpp = le16(dib + 14);
                m.compression = Compression(le32(dib + 16));
                m.image_size = le32(dib + 20);
                m.ppm_x = le32(dib + 24);
                m.ppm_y = le32(dib + 28);
                m.color_used = le32(dib + 32);
                m.color_important = le32(dib + 36);
            } else {
                m.dib_type = DIBType::OS2_V2;
                m.width = int32_t(le32(dib + 4));
                m.height = int32_t(le32(dib + 8));
                m.planes = le16(dib + 12);
                m.bpp = le16(dib + 14);
            }
        } else if(dib_size == 40 || dib_size == 52 || dib_size == 56 ||
                   dib_size == 108 || dib_size == 124){
            // Windows INFO/V2/V3/V4/V5 family
            m.width = int32_t(le32(dib + 4));
            m.height = int32_t(le32(dib + 8));
            m.planes = le16(dib + 12);
            m.bpp = le16(dib + 14);
            m.compression = Compression(le32(dib + 16));
            m.image_size = le32(dib + 20);
            m.ppm_x = le32(dib + 24);
            m.ppm_y = le32(dib + 28);
            m.color_used = le32(dib + 32);
            m.color_important = le32(dib + 36);
            if(dib_size == 40)
                m.dib_type = DIBType::INFO;

            else if(dib_size == 52)
                m.dib_type = DIBType::V2;

            else if(dib_size == 56)
                m.dib_type = DIBType::V3;

            else if(dib_size == 108)
                m.dib_type = DIBType::V4;

            else if(dib_size == 124)
                m.dib_type = DIBType::V5;

            const uint8_t* p_masks = nullptr;
            if (m.compression == Compression::BI_BITFIELDS ||
                m.compression == Compression::BI_ALPHABITFIELDS) {
                if(dib_size >= 52){
                    p_masks = dib + 40;
                    m.masks.r = le32(p_masks + 0);
                    m.masks.g = le32(p_masks + 4);
                    m.masks.b = le32(p_masks + 8);
                    m.has_masks = true;
                    if(dib_size >= 56)
                        m.masks.a = le32(p_masks+12);
                } else {
                    if (14 + dib_size + 12 <= n_) {
                        p_masks = dib + 40;
                        m.masks.r = le32(p_masks + 0);
                        m.masks.g = le32(p_masks + 4);
                        m.masks.b = le32(p_masks + 8);
                        m.has_masks = true;
                        if (m.compression == Compression::BI_ALPHABITFIELDS &&
                            14 + dib_size + 16 <= n_)
                            m.masks.a = le32(p_masks + 12);
                    }
                }
            }

            if (dib_size >= 108) {
                // V4 color space
                m.cstype = ColorSpaceType(le32(dib + 40));
                m.endpoints.r.x = int32_t(le32(dib + 44));
                m.endpoints.r.y = int32_t(le32(dib + 48));
                m.endpoints.r.z = int32_t(le32(dib + 52));

                m.endpoints.g.x = int32_t(le32(dib + 56));
                m.endpoints.g.y = int32_t(le32(dib + 60));
                m.endpoints.g.z = int32_t(le32(dib + 64));

                m.endpoints.b.x = int32_t(le32(dib + 68));
                m.endpoints.b.y = int32_t(le32(dib + 72));
                m.endpoints.b.z = int32_t(le32(dib + 76));

                m.gamma_red   = le32(dib + 80);
                m.gamma_green = le32(dib + 84);
                m.gamma_blue  = le32(dib + 88);
            }

            if (dib_size >= 124) {
                // V5 extras
                uint32_t intent = le32(dib + 92);
                uint32_t profile_data = le32(dib + 112);
                uint32_t profile_size = le32(dib + 116);

                if (m.cstype == ColorSpaceType::PROFILE_EMBEDDED &&
                    profile_size > 0) {
                    size_t start = size_t((dib - p_)) + profile_data;
                    size_t end = start + profile_size;
                    if (start <= n_ && end <= n_ && end >= start)
                        m.embedded_profile.assign(p_ + start, p_ + end);
                }

                switch(intent){
                case 1: m.intent = RenderingIntent::LCS_GM_BUSINESS; break;
                case 2: m.intent = RenderingIntent::LCS_GM_GRAPHICS; break;
                case 4: m.intent = RenderingIntent::LCS_GM_IMAGES; break;
                case 8: m.intent = RenderingIntent::LCS_GM_ABS_COLORIMETRIC;
                    break;
                default: m.intent = RenderingIntent::LCS_GM_IMAGES; break;
                }
            }
        } else {
            throw ParseError("Unsupported or corrupt DIB header size");
        }

        if (m.planes == 0)
            throw ParseError("Invalid planes");

        if (m.bpp == 0)
            throw ParseError("Invalid bits-per-pixel");

        if (m.width == 0 || m.height == 0)
            throw ParseError("Zero dimensions");

        size_t palette_offset = 14 + m.header_size;
        std::vector<PaletteEntry> palette;
        uint32_t palette_entries = 0;

        auto default_palette_entries = [&](){
            if (m.dib_type == DIBType::CORE_OS2_V1) {
                if (m.bpp <= 8)
                    return (uint32_t(1) << m.bpp);
                else
                    return 0u;
            } else {
                if (m.bpp <= 8)
                    return m.color_used? m.color_used : (uint32_t(1) << m.bpp);

                return m.color_used;
            }
        };

        palette_entries = default_palette_entries();

        size_t bytes_available_for_palette =
            (m.file_offset_pixels > palette_offset ?
                 (m.file_offset_pixels - palette_offset) : 0);

        if (palette_entries > 0 && bytes_available_for_palette > 0){
            if (m.dib_type == DIBType::CORE_OS2_V1) {
                size_t need = size_t(palette_entries) * 3;
                palette_entries = (bytes_available_for_palette / 3u);
                palette_entries = std::min<uint32_t>(palette_entries,
                                                     default_palette_entries());
                palette.reserve(palette_entries);
                const uint8_t* q = p_ + palette_offset;
                for (uint32_t i = 0; i < palette_entries; i++){
                    PaletteEntry e{ q[0], q[1], q[2], 0 };
                    palette.push_back(e);
                    q+=3;
                }
            } else {
                size_t need = size_t(palette_entries) * 4;
                palette_entries = (bytes_available_for_palette / 4u);
                palette_entries = std::min<uint32_t>(palette_entries,
                                                     default_palette_entries());
                palette.reserve(palette_entries);
                const uint8_t* q = p_ + palette_offset;
                for (uint32_t i = 0; i < palette_entries; i++){
                    PaletteEntry e{ q[0], q[1], q[2], q[3] };
                    palette.push_back(e);
                    q+=4;
                }
            }
        }

        if (m.file_offset_pixels > n_)
            throw ParseError("Pixel array offset beyond data");

        const uint8_t* pix = p_ + m.file_offset_pixels;
        size_t pix_size = n_ - m.file_offset_pixels;
        if (m.image_size && m.image_size <= pix_size)
            pix_size = m.image_size;

        Image img{};
        img.meta = m;
        img.palette = std::move(palette);

        switch(m.bpp){
        case 1:
        case 2:
        case 4:
        case 8:
            if (m.compression == Compression::BI_RGB)
                decode_indexed_uncompressed(pix, pix_size, img);
            else if (m.compression == Compression::BI_RLE8 && m.bpp == 8)
                decode_rle8(pix, pix_size, img);
            else if (m.compression == Compression::BI_RLE4 && (m.bpp == 4))
                decode_rle4(pix, pix_size, img);
            else if (m.compression == Compression::BI_PNG ||
                     m.compression == Compression::BI_JPEG)
                expose_embedded_stream(pix, pix_size, img);
            else
                throw ParseError("Unsupported compression for indexed BMP");

            break;
        case 16:
            if (m.compression == Compression::BI_RGB) {
                img.meta.has_masks = true;
                img.meta.masks = {0x7C00u, 0x03E0u, 0x001Fu, 0u};
                decode_bitfields(pix, pix_size, img, 2);
            } else if (m.compression == Compression::BI_BITFIELDS ||
                       m.compression == Compression::BI_ALPHABITFIELDS)
                decode_bitfields(pix, pix_size, img, 2);
            else if (m.compression == Compression::BI_PNG ||
                     m.compression == Compression::BI_JPEG)
                expose_embedded_stream(pix, pix_size, img);
            else
                throw ParseError("Unsupported compression for 16bpp");

            break;
        case 24:
            if (m.compression == Compression::BI_RGB)
                decode_bgr24(pix, pix_size, img);
            else if (m.compression == Compression::BI_PNG ||
                     m.compression == Compression::BI_JPEG)
                expose_embedded_stream(pix, pix_size, img);
            else
                throw ParseError("Unsupported compression for 24bpp");

            break;
        case 32:
            if (m.compression == Compression::BI_RGB) {
                img.meta.has_masks = true;
                img.meta.masks = {0x00FF0000u, 0x0000FF00u,
                                  0x000000FFu, 0xFF000000u};
                decode_bitfields(pix, pix_size, img, 4);
            } else if (m.compression == Compression::BI_BITFIELDS ||
                       m.compression == Compression::BI_ALPHABITFIELDS)
                decode_bitfields(pix, pix_size, img, 4);
            else if (m.compression == Compression::BI_PNG ||
                     m.compression == Compression::BI_JPEG)
                expose_embedded_stream(pix, pix_size, img);
            else
                throw ParseError("Unsupported compression for 32bpp");

            break;
        default:
            throw ParseError("Unsupported bits-per-pixel");
        }

        return img;
    }

private:
    const uint8_t* p_ = nullptr;
    size_t n_ = 0;

    static uint32_t row_stride_aligned(uint32_t width, uint16_t bpp){
        uint64_t bits = uint64_t(width) * bpp;
        uint32_t bytes = uint32_t((bits + 7) / 8);
        return (bytes + 3u) & ~3u;
    }

    static void decode_indexed_uncompressed(const uint8_t* pix,
                                            size_t pix_size, Image& img){
        const auto& m = img.meta;
        if (img.palette.empty())
            throw ParseError("Missing palette for indexed BMP");

        uint32_t W = m.width < 0 ? uint32_t(-m.width) : uint32_t(m.width);
        uint32_t H = m.abs_height();
        uint32_t stride = row_stride_aligned(W, m.bpp);
        size_t needed = size_t(stride) * H;

        if (needed > pix_size)
            throw ParseError("Pixel data truncated");

        img.format = PixelFormat::BGRA8;
        img.pixels.resize(size_t(W) * H * 4u);

        auto put_px = [&](uint32_t x, uint32_t y, uint8_t idx){
            if (idx >= img.palette.size())
                idx = 0;

            const auto& pe = img.palette[idx];
            size_t off = (size_t(y) * W + x) * 4u;
            img.pixels[off + 0] = pe.b;
            img.pixels[off + 1] = pe.g;
            img.pixels[off + 2] = pe.r;
            img.pixels[off + 3] = pe.a;
        };

        for (uint32_t row = 0; row < H; row++){
            const uint8_t* src = pix + size_t(row) * stride;
            uint32_t y = img.meta.top_down() ? row : (H - 1 - row);
            if (m.bpp == 8){
                for (uint32_t x = 0; x < W; x++)
                    put_px(x,y, src[x]);
            } else if (m.bpp == 4){
                for (uint32_t x = 0; x < W; x += 2){
                    uint8_t byte = src[x / 2];
                    uint8_t hi = (byte >> 4) & 0xF, lo = byte & 0xF;
                    put_px(x, y, hi);
                    if (x + 1 < W)
                        put_px(x + 1, y, lo);
                }
            } else if (m.bpp == 2){
                for (uint32_t x = 0; x < W; x += 4){
                    uint8_t byte = src[x / 4];
                    uint8_t i0 = (byte >> 6) & 3;
                    uint8_t i1 = (byte >> 4) & 3;
                    uint8_t i2 = (byte >> 2) & 3;
                    uint8_t i3 = byte & 3;
                    put_px(x, y, i0);
                    if(x + 1 < W)
                        put_px(x + 1, y, i1);

                    if(x + 2 < W)
                        put_px(x + 2, y, i2);

                    if(x + 3 < W)
                        put_px(x + 3, y, i3);
                }
            } else if (m.bpp == 1){
                for (uint32_t x = 0; x < W; x += 8){
                    uint8_t byte = src[x/8];
                    for (int b = 7;b >= 0; b--){
                        uint32_t xx = x + (7 - b);
                        if (xx < W)
                            put_px(xx, y, (byte >> b) & 1);
                    }
                }
            }
        }
    }

    static void decode_rle8(const uint8_t* pix, size_t pix_size, Image& img){
        const auto& m = img.meta;
        if (img.palette.empty())
            throw ParseError("Missing palette for RLE8");

        uint32_t W = m.width < 0 ? uint32_t(-m.width) : uint32_t(m.width);
        uint32_t H = m.abs_height();
        img.format = PixelFormat::BGRA8;
        img.pixels.assign(size_t(W) * H * 4u, 0);
        auto put = [&](uint32_t x, uint32_t y, uint8_t idx){
            if (x >= W || y >= H)
                return;

            if (idx >= img.palette.size())
                idx=0;

            const auto& pe = img.palette[idx];
            size_t off = (size_t(y) * W + x) * 4u;
            img.pixels[off + 0] = pe.b;
            img.pixels[off + 1] = pe.g;
            img.pixels[off + 2] = pe.r;
            img.pixels[off + 3] = pe.a;
        };

        uint32_t x = 0, y = 0;
        auto adv_y = [&](){
            y++;
            x = 0;
        };

        size_t i = 0;
        while(i < pix_size && y < H){
            if (i + 1 > pix_size)
                break;

            uint8_t count = pix[i++];
            if (count){
                if (i >= pix_size)
                    break;

                uint8_t val = pix[i++];
                for (uint8_t k = 0; k < count; k++) {
                    uint32_t yy = m.top_down()? y : (H - 1 - y);
                    put(x++, yy, val);
                    if (x >= W){
                        adv_y();
                        if(y >= H)
                            break;
                    }
                }
            } else {
                if (i >= pix_size)
                    break;

                uint8_t cmd = pix[i++];
                if (cmd == 0)
                    adv_y();
                else if (cmd == 1)
                    break;
                else if (cmd == 2){
                    if (i+1>=pix_size)
                        break;

                    uint8_t dx = pix[i++], dy = pix[i++];
                    x += dx;
                    y += dy;
                    if (x >= W)
                        x = W;

                    if (y >= H)
                        y = H;
                } else {
                    uint8_t n = cmd;
                    if (i + n > pix_size)
                        break;

                    for (uint8_t k = 0; k < n; k++){
                        uint32_t yy = m.top_down()? y : (H - 1 - y);
                        put(x++, yy, pix[i++]);
                        if (x >= W){
                            adv_y();
                            if(y >= H)
                                break;
                        }
                    }

                    if ((n & 1) && (i < pix_size))
                        i++;
                }
            }
        }
    }

    static void decode_rle4(const uint8_t* pix, size_t pix_size, Image& img){
        const auto& m = img.meta;
        if (img.palette.empty())
            throw ParseError("Missing palette for RLE4");

        uint32_t W = m.width < 0 ? uint32_t(-m.width) : uint32_t(m.width);
        uint32_t H = m.abs_height();

        img.format = PixelFormat::BGRA8;
        img.pixels.assign(size_t(W) * H * 4u, 0);

        auto put = [&](uint32_t x, uint32_t y, uint8_t idx){
            if (x >= W || y >= H)
                return;

            if (idx >= img.palette.size())
                idx=0;

            const auto& pe = img.palette[idx];
            size_t off = (size_t(y) * W + x) * 4u;
            img.pixels[off] = pe.b;
            img.pixels[off + 1] = pe.g;
            img.pixels[off + 2] = pe.r;
            img.pixels[off + 3] = pe.a;
        };

        uint32_t x = 0, y = 0;
        size_t i = 0;
        while(i < pix_size && y < H){
            uint8_t count = pix[i++];
            if (count){
                if (i >= pix_size)
                    break;

                uint8_t byte = pix[i++];
                uint8_t hi = (byte >> 4) & 0xF, lo = byte & 0xF;
                for (uint8_t k = 0; k < count; k++){
                    uint8_t idx = (k & 1) ? lo : hi;
                    uint32_t yy = m.top_down()? y : (H - 1 - y);
                    put(x++, yy, idx);
                    if (x >= W){
                        y++;
                        x=0;
                        if(y>=H)
                            break;
                    }
                }
            } else {
                if (i >= pix_size)
                    break;

                uint8_t cmd = pix[i++];
                if (cmd == 0){
                    y++;
                    x=0;
                }

                else if (cmd == 1){
                    break;
                }

                else if (cmd == 2){
                    if (i + 1 >= pix_size)
                        break;

                    x += pix[i++];
                    y += pix[i++];
                }

                else {
                    uint8_t n = cmd;
                    size_t bytes = (n + 1) / 2;
                    if (i + bytes > pix_size)
                        break;

                    for (uint8_t k = 0; k < n; k++){
                        uint8_t nib = (k & 1)? (pix[i + k/2] & 0xF) :
                                          ((pix[i + k/2] >> 4) & 0xF);
                        uint32_t yy = m.top_down()? y : (H - 1 - y);
                        put(x++, yy, nib);
                        if (x >= W){
                            y++;
                            x=0;
                            if(y >= H)
                                break;
                        }
                    }

                    i += bytes;
                    if ((bytes & 1) && i < pix_size)
                        i++;
                }
            }
        }
    }

    static void decode_bgr24(const uint8_t* pix, size_t pix_size, Image& img){
        const auto& m = img.meta;
        uint32_t W = m.width < 0 ? uint32_t(-m.width) : uint32_t(m.width);
        uint32_t H = m.abs_height();
        uint32_t stride = row_stride_aligned(W, 24);
        size_t needed = size_t(stride)*H;

        if (needed > pix_size)
            throw ParseError("Pixel data truncated");

        img.format = PixelFormat::BGR8;
        img.pixels.resize(size_t(W) * H * 3u);

        for (uint32_t row = 0; row < H; row++){
            const uint8_t* src = pix + size_t(row) * stride;
            uint32_t y = m.top_down()? row : (H - 1 - row);
            std::memcpy(&img.pixels[size_t(y) * W * 3u], src, size_t(W) * 3u);
        }
    }

    static uint8_t bit_extract_norm(uint32_t v, uint32_t mask){
        if (!mask)
            return 0;

        int shift = 0;
        while(((mask >> shift) & 1u) == 0u)
            ++shift;

        uint32_t width = 0;
        uint32_t m = mask >> shift;
        while(m & 1u){
            ++width;
            m >>= 1;
        }

        uint32_t comp = (v & mask) >> shift;
        if (width >= 8)
            return uint8_t(comp >> (width - 8));

        else if (width == 0)
            return 0;

        else {
            uint32_t x = comp;
            while (width < 8){
                x = (x << width) | comp;
                width <<= 1;
            }

            return uint8_t(x & 0xFF);
        }
    }

    static void decode_bitfields(const uint8_t* pix, size_t pix_size,
                                 Image& img, uint32_t bytes_per_pixel){
        const auto& m = img.meta;
        uint32_t W = m.width < 0 ? uint32_t(-m.width) : uint32_t(m.width);
        uint32_t H = m.abs_height();
        uint32_t stride = row_stride_aligned(W, uint16_t(bytes_per_pixel * 8));
        size_t needed = size_t(stride) * H;
        if (needed > pix_size)
            throw ParseError("Pixel data truncated");

        auto masks = m.masks;
        if (!m.has_masks)
            masks = {0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u};

        bool looks8= ((masks.r | masks.g | masks.b | masks.a) <= 0xFFFFFFFFu)&&
                      (((masks.r == 0x00FF0000u &&
                         masks.g == 0x0000FF00u &&
                         masks.b == 0x000000FFu) ||
                        (masks.r == 0x000000FFu &&
                         masks.g == 0x0000FF00u &&
                         masks.b == 0x00FF0000u)));

        if (looks8){
            img.format = PixelFormat::BGRA8;
            img.pixels.resize(size_t(W) * H * 4u);
            for (uint32_t row = 0; row < H; row++){
                const uint8_t* src = pix + size_t(row) * stride;
                uint32_t y = m.top_down()? row : (H - 1 - row);
                for (uint32_t x = 0; x < W; x++){
                    uint32_t v = (bytes_per_pixel == 2) ?
                                     uint32_t(le16(src + x * 2u)) :
                                     le32(src + x * 4u);

                    uint8_t B = bit_extract_norm(v, masks.b);
                    uint8_t G = bit_extract_norm(v, masks.g);
                    uint8_t R = bit_extract_norm(v, masks.r);
                    uint8_t A = masks.a ? bit_extract_norm(v, masks.a) : 255;

                    size_t off = (size_t(y) * W + x) * 4u;
                    img.pixels[off] = B;
                    img.pixels[off + 1] = G;
                    img.pixels[off + 2] = R;
                    img.pixels[off + 3] = A;
                }
            }
        } else {
            img.format = PixelFormat::RawBitfields;
            img.raw_masks = masks;
            img.raw_bits_per_pixel = uint8_t(bytes_per_pixel * 8);
            img.pixels.resize(size_t(W) * H * bytes_per_pixel);

            for (uint32_t row = 0; row < H; row++){
                const uint8_t* src = pix + size_t(row) * stride;
                uint32_t y = m.top_down()? row : (H - 1 - row);
                std::memcpy(&img.pixels[size_t(y) * W * bytes_per_pixel],
                            src, size_t(W) * bytes_per_pixel);
            }
        }
    }

    static void expose_embedded_stream(const uint8_t* pix, size_t pix_size,
                                       Image& img){
        img.format = PixelFormat::RawBitfields;
        img.pixels.assign(pix, pix + pix_size);
        img.raw_masks = {0, 0, 0, 0};
        img.raw_bits_per_pixel = 0;
    }
};

inline Image load_from_memory(const uint8_t* data, size_t size){
    Reader r(data,size); return r.parse();
}

inline Image load_file(const std::string& path){
    return Reader::from_file(path);
}
}

#endif // BMP_H
