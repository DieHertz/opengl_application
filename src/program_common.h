#ifndef program_common_h
#define program_common_h

#include "gl/gl_include.h"

const auto MAX_LIGHTS = 8;
const auto FIRST_SM_TIU = GL_TEXTURE10;

const GLuint transf_binding_point = 1;
const GLuint lights_binding_point = 2;
const GLuint mtl_binding_point = 3;

#endif /* program_common_h */
