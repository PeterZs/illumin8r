#pragma once

#include "material.h"

class Phong : public Material {
  public:
    int lobe;

    virtual Sample sampleDiffuse(const glm::vec3 &ki, const Hit &hit) const;
    virtual Sample sampleSpecular(const glm::vec3 &ki, const Hit &hit) const;
    virtual Sample sampleRefractive(const glm::vec3 &ki, const Hit &hit) const;
};
