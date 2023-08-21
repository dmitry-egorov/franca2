//
// Created by degor on 12.08.2023.
//

#ifndef FRANCA2_WEB_WGPU_PNG_H
#define FRANCA2_WEB_WGPU_PNG_H

#include "syntax.h"
#include "results.h"
#include "wgpu_ex.h"
#include "stbi_ex.h"

namespace wgpu_ex::png {
    inline ret2<WGPUTexture, WGPUTextureView> load_8bit_texture_from_png_file(WGPUDevice device, const char *path) {
        tmp(sd, stbi_ex::load(path, STBI_grey));

        //TODO: support multiple formats
        if(sd.data && sd.channels == 1); else return ret2_fail;

        return make_texture_2d(device, sd.size, WGPUTextureFormat_R8Unorm, sd.data);
    }
}

#endif //FRANCA2_WEB_WGPU_PNG_H
