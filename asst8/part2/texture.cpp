#include <vector>

#include "asstcommon.h"
#include "glsupport.h"
#include "ppm.h"
#include "texture.h"


using namespace std;

ImageTexture::ImageTexture(const char *ppmFileName, bool srgb) {
    int width, height;
    vector<PackedPixel> pixData;
    ppmRead(ppmFileName, width, height, pixData);

    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0,

                 (!srgb) ? GL_RGB : GL_SRGB, width, height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, &pixData[0]);


        glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    checkGlErrors();
}
