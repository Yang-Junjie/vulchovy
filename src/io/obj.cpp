#include "io/obj.h"

#include <cmath>
#include <cstdio>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "io/image.h"

namespace vulchovy {

namespace {

constexpr int32_t kNoTexture = -1;

float clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

int default_material_index(const std::vector<tinyobj::material_t>& materials, int id) {
    if (id >= 0 && static_cast<size_t>(id) < materials.size())
        return id;
    return -1; // resolved to the fallback material by the caller
}

Material convert_material(const tinyobj::material_t& source) {
    Material material;
    material.base_color = vec3{clamp01(source.diffuse[0]), clamp01(source.diffuse[1]),
                               clamp01(source.diffuse[2])};
    // Blinn-Phong shininess -> GGX roughness (alpha = roughness^2 = sqrt(2/(Ns+2))).
    const float shininess = source.shininess > 0.0f ? source.shininess : 0.0f;
    material.roughness = glm::clamp(std::sqrt(std::sqrt(2.0f / (shininess + 2.0f))), 0.04f, 1.0f);
    material.metallic = 0.0f;
    material.emission = vec3{source.emission[0], source.emission[1], source.emission[2]};
    if (source.ior > 0.0f)
        material.ior = source.ior;
    return material;
}

/// @brief Loads (once) an RGBA8 texture and returns its index in the model.
int32_t load_texture(ObjModel& model, std::unordered_map<std::string, int32_t>& cache,
                     const std::filesystem::path& base_dir, const std::string& name, bool srgb) {
    if (name.empty())
        return kNoTexture;

    const std::filesystem::path texture_path = base_dir / name;
    const std::string key = texture_path.string();
    if (const auto found = cache.find(key); found != cache.end())
        return found->second;

    ImageData image = load_image(texture_path, 4);
    if (!image.valid())
        return kNoTexture;

    TextureData texture;
    texture.width = image.width;
    texture.height = image.height;
    texture.srgb = srgb;
    if (image.hdr) {
        texture.pixels.resize(static_cast<size_t>(image.width) * image.height * 4);
        for (size_t i = 0; i < texture.pixels.size(); ++i)
            texture.pixels[i] =
                static_cast<uint8_t>(clamp01(image.floats[i]) * 255.0f + 0.5f);
    } else {
        texture.pixels = std::move(image.bytes);
    }

    const int32_t index = static_cast<int32_t>(model.textures.size());
    model.textures.push_back(std::move(texture));
    cache.emplace(key, index);
    return index;
}

} // namespace

ObjModel load_obj(const std::filesystem::path& path) {
    ObjModel model;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> source_materials;
    std::string warn;
    std::string error;

    const std::string base_dir = path.parent_path().string();
    const std::string mtl_dir = base_dir.empty() ? std::string{} : base_dir + "/";
    const bool loaded = tinyobj::LoadObj(&attrib, &shapes, &source_materials, &warn, &error,
                                         path.string().c_str(), mtl_dir.c_str(),
                                         /*triangulate=*/true);
    if (!loaded) {
        std::fprintf(stderr, "vulchovy: could not load OBJ '%s': %s\n", path.string().c_str(),
                     error.empty() ? "unknown error" : error.c_str());
        return model;
    }
    if (!warn.empty())
        std::fprintf(stderr, "vulchovy: OBJ warning for '%s': %s\n", path.string().c_str(),
                     warn.c_str());

    std::unordered_map<std::string, int32_t> texture_cache;
    const std::filesystem::path texture_dir = path.parent_path();
    for (const tinyobj::material_t& source : source_materials) {
        Material material = convert_material(source);
        material.base_color_texture =
            load_texture(model, texture_cache, texture_dir, source.diffuse_texname, /*srgb=*/true);
        material.emissive_texture = load_texture(model, texture_cache, texture_dir,
                                                 source.emissive_texname, /*srgb=*/true);
        model.materials.push_back(material);
    }
    if (model.materials.empty())
        model.materials.push_back(Material{}); // fallback

    const size_t vertex_count = attrib.vertices.size() / 3;
    const bool has_normals = !attrib.normals.empty();

    // Area-weighted smooth normals per position, computed once so that they are
    // shared across material groups.
    std::vector<vec3> position_normals;
    if (!has_normals) {
        position_normals.assign(vertex_count, vec3{0.0f});
        for (const tinyobj::shape_t& shape : shapes) {
            const std::vector<tinyobj::index_t>& indices = shape.mesh.indices;
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                const tinyobj::index_t a = indices[i + 0];
                const tinyobj::index_t b = indices[i + 1];
                const tinyobj::index_t c = indices[i + 2];
                if (a.vertex_index < 0 || b.vertex_index < 0 || c.vertex_index < 0)
                    continue;
                const vec3 pa{attrib.vertices[a.vertex_index * 3 + 0],
                              attrib.vertices[a.vertex_index * 3 + 1],
                              attrib.vertices[a.vertex_index * 3 + 2]};
                const vec3 pb{attrib.vertices[b.vertex_index * 3 + 0],
                              attrib.vertices[b.vertex_index * 3 + 1],
                              attrib.vertices[b.vertex_index * 3 + 2]};
                const vec3 pc{attrib.vertices[c.vertex_index * 3 + 0],
                              attrib.vertices[c.vertex_index * 3 + 1],
                              attrib.vertices[c.vertex_index * 3 + 2]};
                const vec3 face = cross(pb - pa, pc - pa);
                position_normals[a.vertex_index] += face;
                position_normals[b.vertex_index] += face;
                position_normals[c.vertex_index] += face;
            }
        }
        for (vec3& normal : position_normals) {
            const float len = length(normal);
            normal = len > 1e-16f ? normal / len : vec3{0.0f, 1.0f, 0.0f};
        }
    }

    auto resolve_material = [&](int id) -> uint32_t {
        const int resolved = default_material_index(source_materials, id);
        return resolved < 0 ? 0u : static_cast<uint32_t>(resolved);
    };

    // One output mesh per (shape, material) pair so each primitive has a single
    // material. Vertices are deduplicated by (position, normal) so hard edges are
    // preserved when the OBJ provides normals.
    for (const tinyobj::shape_t& shape : shapes) {
        std::unordered_map<uint32_t, uint32_t> mesh_by_material; // source material -> mesh slot
        std::unordered_map<uint64_t, uint32_t> unique_vertices;

        const std::vector<tinyobj::index_t>& indices = shape.mesh.indices;
        const std::vector<int>& material_ids = shape.mesh.material_ids;

        for (size_t face = 0; face * 3 + 2 < indices.size(); ++face) {
            const int source_id = face < material_ids.size() ? material_ids[face] : -1;
            const uint32_t material = resolve_material(source_id);

            auto [slot_it, inserted] =
                mesh_by_material.try_emplace(material, static_cast<uint32_t>(model.meshes.size()));
            if (inserted) {
                model.meshes.push_back(Mesh{});
                model.mesh_material.push_back(material);
            }
            Mesh& mesh = model.meshes[slot_it->second];

            for (int corner = 0; corner < 3; ++corner) {
                const tinyobj::index_t& index = indices[face * 3 + corner];
                if (index.vertex_index < 0)
                    continue;
                const int normal_index = has_normals ? index.normal_index : -1;
                const uint64_t key = (static_cast<uint64_t>(index.vertex_index) << 32) |
                                     static_cast<uint32_t>(normal_index);

                uint32_t local;
                const auto found = unique_vertices.find(key);
                if (found != unique_vertices.end()) {
                    local = found->second;
                } else {
                    Vertex vertex;
                    vertex.position = vec3{attrib.vertices[index.vertex_index * 3 + 0],
                                           attrib.vertices[index.vertex_index * 3 + 1],
                                           attrib.vertices[index.vertex_index * 3 + 2]};
                    if (has_normals && index.normal_index >= 0) {
                        vertex.normal = vec3{attrib.normals[index.normal_index * 3 + 0],
                                             attrib.normals[index.normal_index * 3 + 1],
                                             attrib.normals[index.normal_index * 3 + 2]};
                    } else {
                        vertex.normal = position_normals[index.vertex_index];
                    }
                    if (index.texcoord_index >= 0) {
                        vertex.uv = vec2{attrib.texcoords[index.texcoord_index * 2 + 0],
                                         1.0f - attrib.texcoords[index.texcoord_index * 2 + 1]};
                    }
                    local = static_cast<uint32_t>(mesh.vertices.size());
                    mesh.vertices.push_back(vertex);
                    unique_vertices.emplace(key, local);
                }
                mesh.indices.push_back(local);
            }
        }
    }

    return model;
}

uint32_t add_model(Scene& scene, const ObjModel& model, std::string name, const mat4& transform) {
    const int32_t texture_base = static_cast<int32_t>(scene.textures.size());
    scene.textures.insert(scene.textures.end(), model.textures.begin(), model.textures.end());

    const uint32_t material_base = static_cast<uint32_t>(scene.materials.size());
    for (Material material : model.materials) {
        if (material.base_color_texture >= 0)
            material.base_color_texture += texture_base;
        if (material.metallic_roughness_texture >= 0)
            material.metallic_roughness_texture += texture_base;
        if (material.normal_texture >= 0)
            material.normal_texture += texture_base;
        if (material.emissive_texture >= 0)
            material.emissive_texture += texture_base;
        scene.materials.push_back(material);
    }

    Node node;
    node.name = std::move(name);
    node.local_transform = transform;
    for (size_t i = 0; i < model.meshes.size(); ++i) {
        const uint32_t mesh_index = static_cast<uint32_t>(scene.meshes.size());
        scene.meshes.push_back(model.meshes[i]);
        const uint32_t material = material_base + model.mesh_material[i];
        node.primitives.push_back(Primitive{.mesh = mesh_index, .material = material});
    }
    scene.nodes.push_back(std::move(node));
    return static_cast<uint32_t>(scene.nodes.size() - 1);
}

} // namespace vulchovy
