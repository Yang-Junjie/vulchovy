#include "io/gltf.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "io/image.h"

namespace vulchovy {

namespace {

float clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

mat4 mat4_from_column_major(const cgltf_float m[16]) {
    mat4 result;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            result[column][row] = m[column * 4 + row];
    return result;
}

/// @brief Per-load cache mapping (image, colours space) to a scene texture.
struct TextureCache {
    std::unordered_map<uint64_t, int32_t> entries;
};

int32_t texture_for(Scene& scene, TextureCache& cache, const cgltf_image* image,
                    const std::filesystem::path& base_dir, bool srgb) {
    if (image == nullptr)
        return -1;

    const uint64_t key = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(image)) << 1) |
                         (srgb ? 1u : 0u);
    if (const auto found = cache.entries.find(key); found != cache.entries.end())
        return found->second;

    ImageData decoded;
    if (image->buffer_view != nullptr) {
        const uint8_t* data = static_cast<const uint8_t*>(image->buffer_view->buffer->data) +
                              image->buffer_view->offset;
        decoded = load_image_from_memory(data, image->buffer_view->size, 4);
    } else if (image->uri != nullptr && std::strncmp(image->uri, "data:", 5) != 0) {
        decoded = load_image(base_dir / image->uri, 4);
    }

    if (!decoded.valid()) {
        cache.entries.emplace(key, -1);
        return -1;
    }

    TextureData texture;
    texture.width = decoded.width;
    texture.height = decoded.height;
    texture.srgb = srgb;
    if (decoded.hdr) {
        texture.pixels.resize(static_cast<size_t>(decoded.width) * decoded.height * 4);
        for (size_t i = 0; i < texture.pixels.size(); ++i)
            texture.pixels[i] = static_cast<uint8_t>(clamp01(decoded.floats[i]) * 255.0f + 0.5f);
    } else {
        texture.pixels = std::move(decoded.bytes);
    }

    const int32_t index = static_cast<int32_t>(scene.textures.size());
    scene.textures.push_back(std::move(texture));
    cache.entries.emplace(key, index);
    return index;
}

Material convert_material(const cgltf_material* source, Scene& scene, TextureCache& cache,
                          const std::filesystem::path& base_dir) {
    Material material;
    if (source == nullptr)
        return material;

    const cgltf_pbr_metallic_roughness& pbr = source->pbr_metallic_roughness;
    material.base_color = vec3{pbr.base_color_factor[0], pbr.base_color_factor[1],
                               pbr.base_color_factor[2]};
    material.metallic = clamp01(pbr.metallic_factor);
    material.roughness = clamp01(pbr.roughness_factor);
    material.base_color_texture = texture_for(
        scene, cache, pbr.base_color_texture.texture ? pbr.base_color_texture.texture->image : nullptr,
        base_dir, /*srgb=*/true);
    material.metallic_roughness_texture = texture_for(
        scene, cache,
        pbr.metallic_roughness_texture.texture ? pbr.metallic_roughness_texture.texture->image
                                               : nullptr,
        base_dir, /*srgb=*/false);
    material.normal_texture = texture_for(
        scene, cache,
        source->normal_texture.texture ? source->normal_texture.texture->image : nullptr, base_dir,
        /*srgb=*/false);
    material.emissive_texture = texture_for(
        scene, cache,
        source->emissive_texture.texture ? source->emissive_texture.texture->image : nullptr,
        base_dir, /*srgb=*/true);
    const float strength =
        source->has_emissive_strength ? source->emissive_strength.emissive_strength : 1.0f;
    material.emission = vec3{source->emissive_factor[0], source->emissive_factor[1],
                             source->emissive_factor[2]} *
                        strength;

    // KHR_materials_transmission / _volume / _ior.
    if (source->has_transmission)
        material.transmission = clamp01(source->transmission.transmission_factor);
    if (source->has_ior && source->ior.ior > 0.0f)
        material.ior = source->ior.ior;
    if (source->has_volume) {
        material.attenuation = vec3{source->volume.attenuation_color[0],
                                    source->volume.attenuation_color[1],
                                    source->volume.attenuation_color[2]};
    }
    return material;
}

bool build_mesh(const cgltf_primitive* primitive, Mesh& mesh) {
    if (primitive->type != cgltf_primitive_type_triangles)
        return false;

    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* normals = nullptr;
    const cgltf_accessor* texcoords = nullptr;
    const cgltf_accessor* tangents = nullptr;
    for (cgltf_size i = 0; i < primitive->attributes_count; ++i) {
        const cgltf_attribute& attribute = primitive->attributes[i];
        switch (attribute.type) {
        case cgltf_attribute_type_position:
            positions = attribute.data;
            break;
        case cgltf_attribute_type_normal:
            normals = attribute.data;
            break;
        case cgltf_attribute_type_texcoord:
            if (attribute.index == 0)
                texcoords = attribute.data;
            break;
        case cgltf_attribute_type_tangent:
            tangents = attribute.data;
            break;
        default:
            break;
        }
    }
    if (positions == nullptr || positions->count == 0)
        return false;

    const cgltf_size vertex_count = positions->count;
    mesh.vertices.resize(vertex_count);
    std::vector<float> buffer(vertex_count * 4);

    cgltf_accessor_unpack_floats(positions, buffer.data(), vertex_count * 3);
    for (cgltf_size i = 0; i < vertex_count; ++i)
        mesh.vertices[i].position = vec3{buffer[i * 3 + 0], buffer[i * 3 + 1], buffer[i * 3 + 2]};

    if (normals != nullptr) {
        cgltf_accessor_unpack_floats(normals, buffer.data(), vertex_count * 3);
        for (cgltf_size i = 0; i < vertex_count; ++i)
            mesh.vertices[i].normal = vec3{buffer[i * 3 + 0], buffer[i * 3 + 1], buffer[i * 3 + 2]};
    }

    if (texcoords != nullptr) {
        cgltf_accessor_unpack_floats(texcoords, buffer.data(), vertex_count * 2);
        for (cgltf_size i = 0; i < vertex_count; ++i)
            mesh.vertices[i].uv = vec2{buffer[i * 2 + 0], buffer[i * 2 + 1]};
    }

    if (tangents != nullptr) {
        cgltf_accessor_unpack_floats(tangents, buffer.data(), vertex_count * 4);
        for (cgltf_size i = 0; i < vertex_count; ++i) {
            const vec3 t{buffer[i * 4 + 0], buffer[i * 4 + 1], buffer[i * 4 + 2]};
            const float len = length(t);
            mesh.vertices[i].tangent = len > 1e-8f ? t / len : vec3{1.0f, 0.0f, 0.0f};
        }
    }

    if (primitive->indices != nullptr) {
        const cgltf_size index_count = primitive->indices->count;
        mesh.indices.resize(index_count);
        for (cgltf_size i = 0; i < index_count; ++i)
            mesh.indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(primitive->indices, i));
    } else {
        mesh.indices.resize(vertex_count);
        for (cgltf_size i = 0; i < vertex_count; ++i)
            mesh.indices[i] = static_cast<uint32_t>(i);
    }

    if (normals == nullptr)
        compute_smooth_normals(mesh);
    if (tangents == nullptr)
        compute_tangents(mesh);
    return true;
}

} // namespace

bool add_gltf(Scene& scene, const std::filesystem::path& path, const mat4& root_transform) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.string().c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "vulchovy: could not parse glTF '%s'\n", path.string().c_str());
        return false;
    }
    if (cgltf_load_buffers(&options, data, path.string().c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "vulchovy: could not load glTF buffers '%s'\n", path.string().c_str());
        cgltf_free(data);
        return false;
    }

    const std::filesystem::path base_dir = path.parent_path();
    TextureCache texture_cache;

    // Materials.
    std::vector<uint32_t> material_indices(data->materials_count);
    for (cgltf_size i = 0; i < data->materials_count; ++i) {
        material_indices[i] = static_cast<uint32_t>(scene.materials.size());
        scene.materials.push_back(convert_material(&data->materials[i], scene, texture_cache, base_dir));
    }
    // A default material covers primitives that reference none.
    const uint32_t default_material = static_cast<uint32_t>(scene.materials.size());
    scene.materials.push_back(Material{});

    // Meshes: one Mesh per primitive, remembering its material.
    std::vector<std::vector<Primitive>> mesh_primitives(data->meshes_count);
    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive& primitive = mesh.primitives[p];
            Mesh imported;
            if (!build_mesh(&primitive, imported))
                continue;
            const uint32_t mesh_index = static_cast<uint32_t>(scene.meshes.size());
            scene.meshes.push_back(std::move(imported));
            const uint32_t material =
                primitive.material ? material_indices[primitive.material - data->materials]
                                   : default_material;
            mesh_primitives[m].push_back(Primitive{.mesh = mesh_index, .material = material});
        }
    }

    // Nodes: build the hierarchy, applying the root transform to the roots.
    std::function<uint32_t(const cgltf_node*, int32_t, bool)> add_node =
        [&](const cgltf_node* source, int32_t parent, bool is_root) -> uint32_t {
        Node node;
        node.name = source->name != nullptr ? source->name : "node";
        cgltf_float matrix[16];
        cgltf_node_transform_local(source, matrix);
        node.local_transform = mat4_from_column_major(matrix);
        if (is_root)
            node.local_transform = root_transform * node.local_transform;
        node.parent = parent;
        if (source->mesh != nullptr)
            node.primitives = mesh_primitives[source->mesh - data->meshes];

        const uint32_t index = static_cast<uint32_t>(scene.nodes.size());
        scene.nodes.push_back(std::move(node));
        for (cgltf_size c = 0; c < source->children_count; ++c) {
            const uint32_t child = add_node(source->children[c], static_cast<int32_t>(index), false);
            scene.nodes[index].children.push_back(child);
        }
        return index;
    };

    bool added = false;
    if (data->scene != nullptr) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
            add_node(data->scene->nodes[i], -1, true);
            added = true;
        }
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i) {
            if (data->nodes[i].parent == nullptr) {
                add_node(&data->nodes[i], -1, true);
                added = true;
            }
        }
    }

    cgltf_free(data);
    return added;
}

} // namespace vulchovy
