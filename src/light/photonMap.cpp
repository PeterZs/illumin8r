#include <glm/ext.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vector_query.hpp>
#include <iostream>
#include "../render.h"
#include "photonMap.h"

int PhotonMap::photonCount = 100;
float PhotonMap::photonSearchDistanceSquared = 4;

void PhotonMap::emitPhoton(
  Ray &ray,
  const std::vector<Object*> &objects,
  Photon* photon,
  bool requiresSpecularHit
) {
  Hit hit = cast(ray, objects);
  if (hit.isEmpty) {
    return;
  }
  if (!requiresSpecularHit && !hit.material->isSpecular()) {
    Photon* p = new Photon();
    p->position = hit.position;
    p->power = photon->power;
    photons.push_back(p);
  }
  float random = glm::linearRand(0.0f, 1.0f);
  float Pmax = std::max({photon->power.x, photon->power.y, photon->power.z});
  float Pd = std::max({
      hit.material->diffuse.x * photon->power.x,
      hit.material->diffuse.y * photon->power.y,
      hit.material->diffuse.z * photon->power.z}) / Pmax;
  float Ps = std::max({
      hit.material->specular.x * photon->power.x,
      hit.material->specular.y * photon->power.y,
      hit.material->specular.z * photon->power.z}) / Pmax;
  if (random < Pd) {
    // Diffuse reflection.
    Material::Sample sample = hit.material->sampleDiffuse(-ray.direction, hit.normal);
    assert(glm::dot(hit.normal, sample.direction) >= 0);
    photon->power = photon->power *
      glm::dot(sample.direction, hit.normal) *
      sample.brdf /
      sample.pdf /
      Pd;
    ray.direction = sample.direction;
  } else if (random < Pd + Ps) {
    // Specular reflection.
    // TODO: refraction.
    requiresSpecularHit = false;
    Material::Sample sample = hit.material->sampleSpecular(-ray.direction, hit.normal);
    photon->power = photon->power *
      glm::dot(sample.direction, hit.normal) *
      sample.brdf /
      sample.pdf /
      Ps;
    ray.direction = sample.direction;
    if (glm::dot(hit.normal, sample.direction) < 0) {
      return;
    }
  } else  {
    // Absorption.
    return;
  }
  assert(glm::isNormalized(ray.direction, 0.1f));
  assert(glm::dot(ray.direction, hit.normal) > 0);
  // TODO: Parameterize bias.
  float bias = 0.0001f;
  ray.position = hit.position + bias * ray.direction;
  emitPhoton(ray, objects, photon, requiresSpecularHit);
}

// TODO: Reconsider api.
PhotonMap::PhotonNode::PhotonNode(std::vector<Photon*> &photons) {
  if (photons.size() == 1) {
    photon = photons.at(0);
    photon->axis = -1;
    left = NULL;
    right = NULL;
    return;
  }

  // Find the cube surrounding the points.
  glm::vec3 min = glm::vec3(FLT_MAX), max = glm::vec3(-FLT_MAX);
  int photonsSize = photons.size();
  for (int i = 0; i < photonsSize; i++) {
    if (photons.at(i)->position.x < min.x) min.x = photons.at(i)->position.x;
    if (photons.at(i)->position.y < min.y) min.y = photons.at(i)->position.y;
    if (photons.at(i)->position.z < min.z) min.z = photons.at(i)->position.z;
    if (photons.at(i)->position.x > max.x) max.x = photons.at(i)->position.x;
    if (photons.at(i)->position.y > max.y) max.y = photons.at(i)->position.y;
    if (photons.at(i)->position.z > max.z) max.z = photons.at(i)->position.z;
  }
  
  // Select the dimension which is largest.
  float a[3] = {abs(max.x - min.x), abs(max.y - min.y), abs(max.z - min.z)};
  int n = sizeof(a) / sizeof(float);
  int axis = std::distance(a, std::max_element(a, a + n));
  
  // Find the median of the points along the dimension.
  std::sort(photons.begin(), photons.end(), [axis](const Photon* p1, const Photon* p2) {
    return p1->position[axis] < p2->position[axis];
  });
  int median = photons.size() / 2;
  photon = photons.at(median);
  photon->axis = axis;

  // Partition the points by the median.
  std::vector<Photon*> lower(photons.begin(), photons.begin() + median);
  std::vector<Photon*> higher(photons.begin() + median + 1, photons.end());
  left = lower.size() == 0 ? NULL : new PhotonNode(lower);
  right = higher.size() == 0 ? NULL : new PhotonNode(higher);
}

void PhotonMap::getNearest(
  std::vector<Photon*> &photons,
  PhotonNode* photonNode,
  glm::vec3 position
) {
  if (!photonNode) {
    return;
  }

  int axis = photonNode->photon->axis;
  if (axis != -1) {
    float splitDistance = position[axis] - photonNode->photon->position[axis];
    float splitDistanceSquared = splitDistance * splitDistance;
    if (splitDistance < 0) {
      PhotonMap::getNearest(photons, photonNode->left, position);
      if (splitDistanceSquared < photonSearchDistanceSquared) {
        PhotonMap::getNearest(photons, photonNode->right, position);
      }
    } else {
      PhotonMap::getNearest(photons, photonNode->right, position);
      if (splitDistanceSquared < photonSearchDistanceSquared) {
        PhotonMap::getNearest(photons, photonNode->left, position);
      }
    }
  }

  float trueDistanceSquared = glm::distance2(position, photonNode->photon->position);
  if (trueDistanceSquared < photonSearchDistanceSquared) {
    photons.push_back(photonNode->photon);
  }
}

void PhotonMap::init(
  const std::vector<Light*> &lights,
  const std::vector<Object*> &objects,
  bool requiresSpecularHit
) {
  for (int lightIndex = 0; lightIndex < lights.size(); lightIndex++) {
    for (int i = 0; i < photonCount / lights.size(); i++) {
      Ray ray = lights.at(lightIndex)->sampleDirection();
      Photon p;
      p.power = lights.at(lightIndex)->intensity;
      emitPhoton(ray, objects, &p, requiresSpecularHit);
    }
  }
  std::cout << "photon emission complete: " << photons.size() << " / " << photonCount << std::endl;

  photonNode = new PhotonNode(photons);
  std::cout << "photon map complete: " << photons.size() << " / " << photonCount << std::endl;
}