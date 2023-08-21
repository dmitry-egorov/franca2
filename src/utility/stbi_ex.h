#ifndef FRANCA2_WEB_STBI_EX_H
#define FRANCA2_WEB_STBI_EX_H
#define STBI_ONLY_PNG
#include "../../third_party/stb/stb_image.h"

#include "syntax.h"
namespace stbi_ex {
    struct image_data {
        stbi_uc* data;
        usize2 size;
        uint channels;
    };

    inline void release(image_data& sd) {
        if (sd.data != nullptr); else return;
        stbi_image_free(sd.data);
        sd.data = nullptr;
    }

    image_data load(char const *filename, int desired_channels) {
        var result = image_data {};
        int w, h, channels;
        result.data = stbi_load(filename, &w, &h, &channels, desired_channels);
        result.size = { .w = (uint)w, .h = (uint)h };
        result.channels = (uint)channels;
        return result;
    }
}


#endif //FRANCA2_WEB_STBI_EX_H
