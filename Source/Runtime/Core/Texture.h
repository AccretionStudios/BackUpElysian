#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Elysian
{

#pragma pack(push, 1)
    struct VTEXHeader
    {
        char magic[4];
        uint32_t version;
        uint32_t width;
        uint32_t height;
        uint32_t channels;
        uint32_t dataSize; // Size of the raw pixel array
    };
#pragma pack(pop)

    class Texture
    {
    public:
        ~Texture();

        bool LoadFromFile(const std::string& filename);

        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
        int GetChannels() const { return m_Channels; }
        const unsigned char* GetPixels() const { return m_Pixels; }

        bool IsLoaded() const { return m_Pixels != nullptr; }

    private:
        int m_Width = 0;
        int m_Height = 0;
        int m_Channels = 0;
        unsigned char* m_Pixels = nullptr;

        // Internal Asset Pipeline
        bool LoadETEX(const std::string& filename);
        bool SaveETEX(const std::string& filename);
        bool ImportRawImage(const std::string& filename);

        void FreeMemory();
    };
}
