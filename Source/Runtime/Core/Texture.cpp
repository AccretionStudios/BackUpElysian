#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace Elysian
{
    Texture::~Texture()
    {
        FreeMemory();
    }

    void Texture::FreeMemory()
    {
        if (m_Pixels)
        {
            delete[] m_Pixels;
            m_Pixels = nullptr;
        }
    }

    bool Texture::LoadFromFile(const std::string& filename)
    {
        std::string ext = filename.substr(filename.find_last_of('.') + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "etex")
        {
            return LoadETEX(filename);
        }
        
        std::string compiledFilename = filename.substr(0, filename.find_last_of('.')) + ".etex";
        std::ifstream f(compiledFilename.c_str());
        if (f.good())
        {
            f.close();
            std::cout << "[Asset Pipeline] Found compiled texture, loading: " << compiledFilename << std::endl;
            return LoadETEX(compiledFilename);
        }
        
        std::cout << "[Asset Pipeline] Compiling raw " << ext << " to .etex format..." << std::endl;

        if (ImportRawImage(filename))
        {
            SaveETEX(compiledFilename);
            std::cout << "[Asset Pipeline] Successfully compiled to " << compiledFilename << std::endl;
            return true;
        }

        return false;
    }

    bool Texture::ImportRawImage(const std::string& filename)
    {
        // We force STBI_rgb_alpha (4 channels) so Vulkan always gets a consistent RGBA format
        stbi_set_flip_vertically_on_load(false); // Vulkan 0,0 is top-left, but many 3D formats expect bottom-left

        unsigned char* stbiPixels = stbi_load(filename.c_str(), &m_Width, &m_Height, &m_Channels, STBI_rgb_alpha);

        if (!stbiPixels)
        {
            std::cerr << "[Asset Pipeline] STB failed to load image: " << filename << std::endl;
            return false;
        }

        m_Channels = 4; // Because we forced STBI_rgb_alpha
        uint32_t dataSize = m_Width * m_Height * m_Channels;

        // Copy STB memory into our own managed memory so we can safely free STB
        FreeMemory();
        m_Pixels = new unsigned char[dataSize];
        memcpy(m_Pixels, stbiPixels, dataSize);

        stbi_image_free(stbiPixels);
        return true;
    }

    bool Texture::SaveETEX(const std::string& filename)
    {
        if (!m_Pixels) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        VTEXHeader header;
        header.magic[0] = 'E';
        header.magic[1] = 'T';
        header.magic[2] = 'E';
        header.magic[3] = 'X';
        header.version = 1;
        header.width = m_Width;
        header.height = m_Height;
        header.channels = m_Channels;
        header.dataSize = m_Width * m_Height * m_Channels;

        file.write(reinterpret_cast<char*>(&header), sizeof(VTEXHeader));
        file.write(reinterpret_cast<char*>(m_Pixels), header.dataSize);

        return true;
    }

    bool Texture::LoadETEX(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;

        VTEXHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(VTEXHeader));

        // Security Check: Is this actually a Elysian Texture?
        if (strncmp(header.magic, "ETEX", 4) != 0)
        {
            std::cerr << "[Asset Pipeline] Error: Invalid magic number. Not a ETEX file!" << std::endl;
            return false;
        }

        m_Width = header.width;
        m_Height = header.height;
        m_Channels = header.channels;

        FreeMemory();
        m_Pixels = new unsigned char[header.dataSize];
        file.read(reinterpret_cast<char*>(m_Pixels), header.dataSize);

        std::cout << "[Asset Pipeline] Loaded ETEX: " << m_Width << "x" << m_Height << " (" << m_Channels <<
            " channels)" << std::endl;
        return true;
    }
}
