#include "Mesh.h"
#define TINYOBJLOADER_DISABLE_FAST_FLOAT 1
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
#include <ufbx/ufbx.h>

#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include <cstring>

namespace Elysian
{
    bool Mesh::LoadFromFile(const std::string& filename)
    {
        std::string ext = filename.substr(filename.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == "emsh") return LoadEMSH(filename);

        std::string compiledFilename = filename.substr(0, filename.find_last_of('.')) + ".emsh";
        std::ifstream f(compiledFilename.c_str());
        if (f.good())
        {
            f.close();
            std::cout << "[Asset Pipeline] Found compiled asset, loading: " << compiledFilename << std::endl;
            return LoadEMSH(compiledFilename);
        }

        std::cout << "[Asset Pipeline] Compiling raw " << ext << " to .emsh format..." << std::endl;
        bool imported = false;

        if (ext == "obj") imported = ImportOBJ(filename);
        else if (ext == "fbx") imported = ImportFBX(filename);
        else if (ext == "gltf" || ext == "glb") imported = ImportGLTF(filename);
        else
        {
            std::cerr << "[Asset Pipeline] Unsupported file extension: " << ext << std::endl;
            return false;
        }

        if (imported)
        {
            // --- NEW: UNIVERSAL TANGENT GENERATOR ---
            for (auto& v : m_Vertices) v.tangent = glm::vec3(0.0f);

            for (size_t i = 0; i < m_Indices.size(); i += 3)
            {
                Vertex& v0 = m_Vertices[m_Indices[i + 0]];
                Vertex& v1 = m_Vertices[m_Indices[i + 1]];
                Vertex& v2 = m_Vertices[m_Indices[i + 2]];

                glm::vec3 edge1 = v1.pos - v0.pos;
                glm::vec3 edge2 = v2.pos - v0.pos;
                glm::vec2 deltaUV1 = v1.uv - v0.uv;
                glm::vec2 deltaUV2 = v2.uv - v0.uv;

                float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
                glm::vec3 tangent(1.0f, 0.0f, 0.0f);

                if (!std::isinf(f) && !std::isnan(f))
                {
                    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                }
                v0.tangent += tangent;
                v1.tangent += tangent;
                v2.tangent += tangent;
            }

            for (auto& v : m_Vertices)
            {
                // Gram-Schmidt orthogonalization to keep angles strictly at 90 degrees
                v.tangent = glm::normalize(v.tangent - glm::dot(v.tangent, v.normal) * v.normal);

                // Safety catch for broken geometry
                if (std::isnan(v.tangent.x) || std::isnan(v.tangent.y) || std::isnan(v.tangent.z))
                {
                    v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                }
            }

            SaveEMSH(compiledFilename);
            std::cout << "[Asset Pipeline] Successfully compiled to " << compiledFilename << std::endl;
            return true;
        }

        return false;
    }

    bool Mesh::SaveEMSH(const std::string& filename)
    {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        VMSHHeader header;
        header.magic[0] = 'E';
        header.magic[1] = 'M';
        header.magic[2] = 'S';
        header.magic[3] = 'H';
        header.version = 1;
        header.vertexCount = static_cast<uint32_t>(m_Vertices.size());
        header.indexCount = static_cast<uint32_t>(m_Indices.size());

        file.write(reinterpret_cast<char*>(&header), sizeof(VMSHHeader));
        file.write(reinterpret_cast<char*>(m_Vertices.data()), m_Vertices.size() * sizeof(Vertex));
        file.write(reinterpret_cast<char*>(m_Indices.data()), m_Indices.size() * sizeof(uint32_t));

        return true;
    }

    bool Mesh::LoadEMSH(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        VMSHHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(VMSHHeader));

        if (strncmp(header.magic, "EMSH", 4) != 0) return false;

        if (header.version != 1)
        {
            // <--- REQUIRES VERSION 3
            std::cout << "[Asset Pipeline] Outdated EMSH version detected. Recompiling..." << std::endl;
            return false;
        }

        m_Vertices.resize(header.vertexCount);
        m_Indices.resize(header.indexCount);

        file.read(reinterpret_cast<char*>(m_Vertices.data()), m_Vertices.size() * sizeof(Vertex));
        file.read(reinterpret_cast<char*>(m_Indices.data()), m_Indices.size() * sizeof(uint32_t));

        std::cout << "[Asset Pipeline] Loaded MESH: " << m_Vertices.size() << " vertices." << std::endl;
        return true;
    }

    bool Mesh::ImportOBJ(const std::string& filename)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str());
        if (!warn.empty()) std::cout << "TinyObj Warning: " << warn << std::endl;
        if (!err.empty() || !ret) return false;

        std::map<std::tuple<int, int, int>, uint32_t> indexMap;
        std::vector<Vertex> tempVertices;
        std::vector<uint32_t> tempIndices;

        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                int vi = index.vertex_index, ni = index.normal_index, ti = index.texcoord_index;
                auto key = std::make_tuple(vi, ni, ti);

                if (indexMap.find(key) == indexMap.end())
                {
                    Vertex v;
                    v.pos = glm::vec3(attrib.vertices[3 * vi + 0], attrib.vertices[3 * vi + 1],
                                      attrib.vertices[3 * vi + 2]);

                    // FIX: Pure white default color
                    v.color = glm::vec3(1.0f, 1.0f, 1.0f);

                    if (ni >= 0 && ni * 3 + 2 < attrib.normals.size())
                    {
                        v.normal = glm::vec3(attrib.normals[3 * ni + 0], attrib.normals[3 * ni + 1],
                                             attrib.normals[3 * ni + 2]);
                    }
                    else
                    {
                        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    }

                    if (ti >= 0 && ti * 2 + 1 < attrib.texcoords.size())
                    {
                        v.uv = glm::vec2(attrib.texcoords[2 * ti + 0], 1.0f - attrib.texcoords[2 * ti + 1]);
                    }
                    else
                    {
                        v.uv = glm::vec2(0.0f, 0.0f);
                    }

                    uint32_t newIndex = static_cast<uint32_t>(tempVertices.size());
                    tempVertices.push_back(v);
                    indexMap[key] = newIndex;
                    tempIndices.push_back(newIndex);
                }
                else
                {
                    tempIndices.push_back(indexMap[key]);
                }
            }
        }
        m_Vertices = std::move(tempVertices);
        m_Indices = std::move(tempIndices);
        return true;
    }

    bool Mesh::ImportFBX(const std::string& filename)
    {
        ufbx_load_opts opts = {0};
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.target_unit_meters = 1.0f;

        ufbx_error error;
        ufbx_scene* scene = ufbx_load_file(filename.c_str(), &opts, &error);
        if (!scene)
        {
            std::cerr << "UFBX Error: " << error.description.data << std::endl;
            return false;
        }

        std::map<std::tuple<uint32_t, uint32_t, uint32_t>, uint32_t> indexMap;
        std::vector<Vertex> tempVertices;
        std::vector<uint32_t> tempIndices;

        for (size_t mi = 0; mi < scene->meshes.count; mi++)
        {
            ufbx_mesh* mesh = scene->meshes.data[mi];
            std::vector<uint32_t> tri_indices(mesh->max_face_triangles * 3);

            for (size_t fi = 0; fi < mesh->faces.count; fi++)
            {
                ufbx_face face = mesh->faces.data[fi];
                uint32_t num_tris = ufbx_triangulate_face(tri_indices.data(), tri_indices.size(), mesh, face);

                for (uint32_t i = 0; i < num_tris * 3; i++)
                {
                    uint32_t ix = tri_indices[i];
                    uint32_t pos_idx = mesh->vertex_indices.data[ix];
                    uint32_t norm_idx = mesh->vertex_normal.exists ? mesh->vertex_normal.indices.data[ix] : 0;
                    uint32_t uv_idx = mesh->vertex_uv.exists ? mesh->vertex_uv.indices.data[ix] : 0;

                    auto key = std::make_tuple(pos_idx, norm_idx, uv_idx);
                    if (indexMap.find(key) == indexMap.end())
                    {
                        Vertex v;
                        v.pos = glm::vec3(mesh->vertices.data[pos_idx].x, mesh->vertices.data[pos_idx].y,
                                          mesh->vertices.data[pos_idx].z);

                        // FIX: Pure white default color to stop the rainbow effect!
                        v.color = glm::vec3(1.0f, 1.0f, 1.0f);

                        if (mesh->vertex_normal.exists)
                        {
                            v.normal = glm::vec3(mesh->vertex_normal.values.data[norm_idx].x,
                                                 mesh->vertex_normal.values.data[norm_idx].y,
                                                 mesh->vertex_normal.values.data[norm_idx].z);
                        }
                        else
                        {
                            v.normal = glm::vec3(0, 1, 0);
                        }

                        if (mesh->vertex_uv.exists)
                        {
                            v.uv = glm::vec2(mesh->vertex_uv.values.data[uv_idx].x,
                                             1.0f - mesh->vertex_uv.values.data[uv_idx].y);
                        }
                        else
                        {
                            v.uv = glm::vec2(0.0f, 0.0f);
                        }

                        uint32_t newIndex = static_cast<uint32_t>(tempVertices.size());
                        tempVertices.push_back(v);
                        indexMap[key] = newIndex;
                        tempIndices.push_back(newIndex);
                    }
                    else
                    {
                        tempIndices.push_back(indexMap[key]);
                    }
                }
            }
        }
        ufbx_free_scene(scene);
        m_Vertices = std::move(tempVertices);
        m_Indices = std::move(tempIndices);
        return true;
    }

    bool Mesh::ImportGLTF(const std::string& filename)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        std::string ext = filename.substr(filename.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        bool ret = false;
        if (ext == "glb")
        {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
        }
        else
        {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
        }

        if (!warn.empty()) std::cout << "TinyGLTF Warn: " << warn << std::endl;
        if (!err.empty()) std::cerr << "TinyGLTF Error: " << err << std::endl;
        if (!ret) return false;

        if (model.meshes.empty() || model.meshes[0].primitives.empty()) return false;
        auto& primitive = model.meshes[0].primitives[0];

        if (primitive.attributes.find("POSITION") == primitive.attributes.end()) return false;
        auto& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
        auto& posView = model.bufferViews[posAccessor.bufferView];
        auto& posBuffer = model.buffers[posView.buffer];
        const uint8_t* posData = &posBuffer.data[posView.byteOffset + posAccessor.byteOffset];
        int posStride = posAccessor.ByteStride(posView);

        const uint8_t* normData = nullptr;
        int normStride = 0;
        if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
        {
            auto& normAccessor = model.accessors[primitive.attributes.at("NORMAL")];
            auto& normView = model.bufferViews[normAccessor.bufferView];
            auto& normBuffer = model.buffers[normView.buffer];
            normData = &normBuffer.data[normView.byteOffset + normAccessor.byteOffset];
            normStride = normAccessor.ByteStride(normView);
        }

        // --- FIXED: Safe UV Extraction with Component Type ---
        const uint8_t* uvData = nullptr;
        int uvStride = 0;
        int uvComponentType = TINYGLTF_COMPONENT_TYPE_FLOAT; // Default
        if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
        {
            auto& uvAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            auto& uvView = model.bufferViews[uvAccessor.bufferView];
            auto& uvBuffer = model.buffers[uvView.buffer];
            uvData = &uvBuffer.data[uvView.byteOffset + uvAccessor.byteOffset];
            uvStride = uvAccessor.ByteStride(uvView);
            uvComponentType = uvAccessor.componentType; // Save the type!
        }

        std::vector<Vertex> tempVertices(posAccessor.count);
        for (size_t i = 0; i < posAccessor.count; ++i)
        {
            const float* p = reinterpret_cast<const float*>(posData + (i * posStride));
            tempVertices[i].pos = glm::vec3(p[0], p[1], p[2]);
            tempVertices[i].color = glm::vec3(1.0f, 1.0f, 1.0f);

            if (normData)
            {
                const float* n = reinterpret_cast<const float*>(normData + (i * normStride));
                tempVertices[i].normal = glm::vec3(n[0], n[1], n[2]);
            }
            else
            {
                tempVertices[i].normal = glm::vec3(0, 1, 0);
            }

            // --- FIXED: Properly Decode Short/Byte UVs to prevent NaN vanishing ---
            if (uvData)
            {
                float u = 0.0f, v = 0.0f;
                if (uvComponentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                {
                    const float* uv = reinterpret_cast<const float*>(uvData + (i * uvStride));
                    u = uv[0];
                    v = uv[1];
                }
                else if (uvComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const uint16_t* uv = reinterpret_cast<const uint16_t*>(uvData + (i * uvStride));
                    u = uv[0] / 65535.0f;
                    v = uv[1] / 65535.0f;
                }
                else if (uvComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    const uint8_t* uv = reinterpret_cast<const uint8_t*>(uvData + (i * uvStride));
                    u = uv[0] / 255.0f;
                    v = uv[1] / 255.0f;
                }
                tempVertices[i].uv = glm::vec2(u, 1.0f - v);
            }
            else
            {
                tempVertices[i].uv = glm::vec2(0.0f, 0.0f);
            }
        }

        std::vector<uint32_t> tempIndices;
        if (primitive.indices >= 0)
        {
            auto& indAccessor = model.accessors[primitive.indices];
            auto& indView = model.bufferViews[indAccessor.bufferView];
            auto& indBuffer = model.buffers[indView.buffer];
            const uint8_t* indData = &indBuffer.data[indView.byteOffset + indAccessor.byteOffset];
            int indStride = indAccessor.ByteStride(indView);

            tempIndices.resize(indAccessor.count);
            for (size_t i = 0; i < indAccessor.count; ++i)
            {
                if (indAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    tempIndices[i] = *reinterpret_cast<const uint16_t*>(indData + (i * indStride));
                }
                else if (indAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    tempIndices[i] = *reinterpret_cast<const uint32_t*>(indData + (i * indStride));
                }
                else if (indAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    tempIndices[i] = *reinterpret_cast<const uint8_t*>(indData + (i * indStride));
                }
            }
        }
        else
        {
            tempIndices.resize(posAccessor.count);
            for (uint32_t i = 0; i < posAccessor.count; ++i)
            {
                tempIndices[i] = i;
            }
        }

        m_Vertices = std::move(tempVertices);
        m_Indices = std::move(tempIndices);
        return true;
    }
}
