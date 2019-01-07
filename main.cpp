#include <json/json.hpp>
using json = nlohmann::json;
#include <glm/glm.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "src/camera.h"
#include "src/image.h"
#include "src/material/dielectric.h"
#include "src/material/phong.h"
#include "src/light/photonMap.h"
#include "src/light/pointLight.h"
#include "src/object/mesh.h"
#include "src/object/sphere.h"
#include "src/render.h"
#include "src/window.h"

Camera camera;
Image image;
std::vector<Light*> lights;
std::vector<Material*> materials;
std::vector<Object*> objects;

glm::vec3 getVec3(json node) {
  return node.empty() ?
    glm::vec3(0) :
    glm::vec3(node["x"], node["y"], node["z"]);
}

float getFloat(json node) {
  return node.empty() ? 0.0f : node.get<float>();
}

void setupCamera(json node) {
  camera.direction = getVec3(node["direction"]);
  camera.fieldOfView = node["fieldOfView"];
  camera.position = getVec3(node["position"]);
  camera.up = getVec3(node["up"]);
}

void setupImage(json node) {
  image.height = node["height"];
  image.width = node["width"];
  image.render = node["render"];
  // TODO: Extract cleaner structure.
  PhotonMap::photonCount = node["photonCount"];
  PhotonMap::photonSearchDistanceSquared = node["photonSearchDistanceSquared"];
  image.init();
}

void setupLights(json node) {
  for (auto& light : node) {
    if (light["type"] == "point") {
      PointLight* l = new PointLight();
      l->intensity = getVec3(light["intensity"]);
      l->position = getVec3(light["position"]);
      l->power = getVec3(light["power"]);
      lights.push_back(l);
    }
  }
}

void setupMaterials(json node) {
  for (auto& material : node) {
    if (material["type"] == "phong") {
      Phong* m = new Phong();
      m->key = material["key"];
      m->diffuse = getVec3(material["diffuse"]);
      m->specular = getVec3(material["specular"]);
      m->lobe = getFloat(material["lobe"]);
      materials.push_back(m);
    } else if (material["type"] == "dielectric") {
      Dielectric* m = new Dielectric();
      m->key = material["key"];
      m->specular = getVec3(material["specular"]);
      m->refractive = getVec3(material["refractive"]);
      m->refractiveIndex = getFloat(material["refractiveIndex"]);
      materials.push_back(m);
    }
  }
}

Material* find(std::string material) {
  for (int i = 0; i < materials.size(); i++) {
    if (materials.at(i)->key == material) {
      return materials.at(i);
    }
  }
  return NULL;
}

void setupObjects(json node) {
  for (auto& object : node) {
    Object* o;
    if (object["type"] == "sphere") {
      o = new Sphere();
    } else if (object["type"] == "obj") {
      o = new Mesh();
      std::string file = object["file"];
      ((Mesh*) o)->init(file.c_str());
    }
    o->key = object["key"];
    o->material = find(object["material"]);
    json rotate = object["rotate"];
    o->translate(getVec3(object["translate"]));
    o->scale(getVec3(object["scale"]));
    o->rotate(rotate["angle"], getVec3(rotate["axis"]));
    objects.push_back(o);
  }
}

void setup() {
  std::ifstream i("scene.json");
  nlohmann::json scene;
  i >> scene;
  i.close();
  
  setupCamera(scene["camera"]);
  setupImage(scene["image"]);
  setupLights(scene["lights"]);
  setupMaterials(scene["materials"]);
  setupObjects(scene["objects"]);

  std::cout << "setup complete" << std::endl;
}

int main() {
  std::cout << "illumin8r" << std::endl;
  setup();
  Render(camera, image, lights, objects);
  Show();
  return 0;
}
