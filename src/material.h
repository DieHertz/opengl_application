#ifndef material_h
#define material_h

#include <glm/vec4.hpp>

struct material {
    glm::vec4 diffuse;
    glm::vec4 specular;
    float shininess;
    float reflectance;
};

#endif /* material_h */
