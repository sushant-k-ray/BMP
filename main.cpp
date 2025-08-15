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

#include "bmp.hpp"
#include <cstring>

#include "glimview.hpp"

#include <iostream>

int main(){
    try{
        auto img = bmp::load_file("test.bmp");
        std::cout << "Loaded BMP: "
                  << img.meta.width << "x" << img.meta.abs_height()
                  << " bpp=" << img.meta.bpp << "\n";

        std::cout << "Format: " <<
                     (img.format == bmp::PixelFormat::BGRA8 ? "BGRA8" :
                      img.format == bmp::PixelFormat::BGR8 ? "BGR8" :
                      img.format == bmp::PixelFormat::RGBA8 ? "RGBA8" :
                      img.format == bmp::PixelFormat::Gray8 ? "Gray8" :
                      "RawBitfields")
                  << "\n";

        // RGBA imgdata for openGL imview
        unsigned char* imgdata = (unsigned char*)malloc(sizeof(unsigned char) *
                                 img.meta.height * img.meta.width * 4);

        for(int i = 0; i < img.meta.height; i++){
            // imview image is rotated by 180 degrees
            int imindex = img.meta.width * (img.meta.height - i);
            int bufindex = img.meta.width * i;

            for(int j = 0; j < img.meta.width; j++){
                int pix = (imindex + j);
                int bufpix = (bufindex + j) * 4;

                if(img.format == bmp::PixelFormat::BGRA8) {
                    pix *= 4;
                    imgdata[bufpix] = img.pixels.data()[pix + 2];
                    imgdata[bufpix + 1] = img.pixels.data()[pix + 1];
                    imgdata[bufpix + 2] = img.pixels.data()[pix];
                    imgdata[bufpix + 3] = img.pixels.data()[pix + 3];
                } else if(img.format == bmp::PixelFormat::BGR8) {
                    pix *= 3;
                    imgdata[bufpix] = img.pixels.data()[pix + 2];
                    imgdata[bufpix + 1] = img.pixels.data()[pix + 1];
                    imgdata[bufpix + 2] = img.pixels.data()[pix];
                    imgdata[bufpix + 3] = 255;
                } else if(img.format == bmp::PixelFormat::RGBA8) {
                    pix *= 4;
                    imgdata[bufpix] = img.pixels.data()[pix];
                    imgdata[bufpix + 1] = img.pixels.data()[pix + 1];
                    imgdata[bufpix + 2] = img.pixels.data()[pix + 2];
                    imgdata[bufpix + 3] = img.pixels.data()[pix + 3];
                } else if(img.format == bmp::PixelFormat::Gray8) {
                    imgdata[bufpix] = img.pixels.data()[pix];
                    imgdata[bufpix + 1] = img.pixels.data()[pix];
                    imgdata[bufpix + 2] = img.pixels.data()[pix];
                    imgdata[bufpix + 3] = 255;
                } else {
                    free(imgdata);
                    return 0;
                }
            }
        }

        glimviewUpdateImage(imgdata, img.meta.width, img.meta.height);
        showGlimview();
        free(imgdata);
    } catch(const std::exception& e){
        std::cerr << "Error: " << e.what() << "\n";
    }
}
