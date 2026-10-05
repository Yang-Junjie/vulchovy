#include "io/scene_io.h"

#include <array>
#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "io/gltf.h"
#include "io/obj.h"

namespace vulchovy {

namespace {

using json = nlohmann::json;

json to_json(const vec3& value) {
    return json::array({value.x, value.y, value.z});
}

vec3 vec3_from_json(const json& value, const vec3& fallback) {
    if (!value.is_array() || value.size() != 3)
        return fallback;
    return vec3{value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

json to_json(const mat4& value) {
    json array = json::array();
    const float* data = &value[0][0];
    for (int i = 0; i < 16; ++i)
        array.push_back(data[i]);
    return array;
}

mat4 mat4_from_json(const json& value) {
    if (!value.is_array() || value.size() != 16)
        return mat4{1.0f};
    mat4 result;
    float* data = &result[0][0];
    for (int i = 0; i < 16; ++i)
        data[i] = value[i].get<float>();
    return result;
}

} // namespace

SceneDescription load_scene_description(const std::filesystem::path& path) {
    SceneDescription description;

    std::ifstream file(path);
    if (!file) {
        std::fprintf(stderr, "vulchovy: could not open scene '%s'\n", path.string().c_str());
        return description;
    }

    json root;
    try {
        file >> root;
    } catch (const json::exception& error) {
        std::fprintf(stderr, "vulchovy: could not parse scene '%s': %s\n", path.string().c_str(),
                     error.what());
        return description;
    }

    if (const auto it = root.find("environment"); it != root.end()) {
        description.environment.path = it->value("path", std::string{});
        description.environment.intensity = it->value("intensity", 1.0f);
        description.environment.rotation = it->value("rotation", 0.0f);
    }
    if (const auto it = root.find("camera"); it != root.end()) {
        description.camera.position = vec3_from_json(it->value("position", json{}), description.camera.position);
        description.camera.target = vec3_from_json(it->value("target", json{}), description.camera.target);
        description.camera.up = vec3_from_json(it->value("up", json{}), description.camera.up);
        description.camera.vertical_fov_degrees = it->value("fov", 40.0f);
    }
    if (const auto it = root.find("models"); it != root.end() && it->is_array()) {
        for (const json& entry : *it) {
            ModelPlacement placement;
            placement.path = entry.value("path", std::string{});
            placement.name = entry.value("name", std::string{});
            if (const auto transform = entry.find("transform"); transform != entry.end())
                placement.transform = mat4_from_json(*transform);
            if (!placement.path.empty())
                description.models.push_back(std::move(placement));
        }
    }
    if (const auto it = root.find("lights"); it != root.end() && it->is_array()) {
        for (const json& entry : *it) {
            Light light;
            light.position = vec3_from_json(entry.value("position", json{}), light.position);
            light.emission = vec3_from_json(entry.value("emission", json{}), light.emission);
            light.radius = entry.value("radius", 0.0f);
            description.lights.push_back(light);
        }
    }

    return description;
}

bool save_scene_description(const SceneDescription& description,
                            const std::filesystem::path& path) {
    json root;
    root["environment"] = {
        {"path", description.environment.path},
        {"intensity", description.environment.intensity},
        {"rotation", description.environment.rotation},
    };
    root["camera"] = {
        {"position", to_json(description.camera.position)},
        {"target", to_json(description.camera.target)},
        {"up", to_json(description.camera.up)},
        {"fov", description.camera.vertical_fov_degrees},
    };
    root["models"] = json::array();
    for (const ModelPlacement& placement : description.models) {
        root["models"].push_back({
            {"path", placement.path},
            {"name", placement.name},
            {"transform", to_json(placement.transform)},
        });
    }
    root["lights"] = json::array();
    for (const Light& light : description.lights) {
        root["lights"].push_back({
            {"position", to_json(light.position)},
            {"emission", to_json(light.emission)},
            {"radius", light.radius},
        });
    }

    std::ofstream file(path);
    if (!file) {
        std::fprintf(stderr, "vulchovy: could not write scene '%s'\n", path.string().c_str());
        return false;
    }
    file << root.dump(2) << '\n';
    return true;
}

Scene build_scene(const SceneDescription& description) {
    Scene scene;

    for (const ModelPlacement& placement : description.models) {
        const std::string extension = std::filesystem::path(placement.path).extension().string();
        if (extension == ".gltf" || extension == ".glb") {
            add_gltf(scene, placement.path, placement.transform);
            continue;
        }
        ObjModel model = load_obj(placement.path);
        if (model.meshes.empty())
            continue;
        const std::string name =
            placement.name.empty() ? placement.path : placement.name;
        add_model(scene, model, name, placement.transform);
    }

    scene.lights = description.lights;

    if (!description.environment.path.empty())
        scene.environment = load_environment(description.environment.path);
    if (!scene.environment.valid())
        scene.environment = make_procedural_sky();
    scene.environment.intensity = description.environment.intensity;
    scene.environment.rotation = description.environment.rotation;

    return scene;
}

SceneDescription default_scene_description() {
    SceneDescription description;
    description.environment.path =
        (std::filesystem::path{VULCHOVY_ASSET_DIR} / "modern_buildings_2_2k.hdr").string();
    description.environment.intensity = 1.0f;
    description.environment.rotation = 0.0f;
    description.lights.push_back(Light{
        .position = vec3{0.0f, 3.6f, 0.0f},
        .emission = vec3{6.0f, 6.0f, 6.0f},
        .radius = 0.7f,
    });
    return description;
}

} // namespace vulchovy
