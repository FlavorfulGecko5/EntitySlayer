// idMapFileEditor 0.1 by proteh
// -- edited by Scorp0rX0r 09/09/2020 - Remove file operations and work with streams only.
// -- Further edited by FlavorfulGecko5 to integrate into .entities parser

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Oodle 
{
    typedef unsigned char byte;
    typedef unsigned char uint8;
    typedef unsigned int uint32;
    typedef unsigned __int64 uint64;
    typedef signed __int64 int64;
    typedef signed int int32;
    typedef unsigned short uint16;
    typedef signed short int16;
    typedef unsigned int uint;

    typedef int WINAPI OodLZ_CompressFunc(
        int codec, uint8* src_buf, size_t src_len, uint8* dst_buf, int level,
        void* opts, size_t offs, size_t unused, void* scratch, size_t scratch_size);

    typedef int WINAPI OodLZ_DecompressFunc(uint8* src_buf, int src_len, uint8* dst, size_t dst_size,
        int fuzz, int crc, int verbose,
        uint8* dst_base, size_t e, void* cb, void* cb_ctx, void* scratch, size_t scratch_size, int threadPhase);

    inline HMODULE oodle;
    inline OodLZ_CompressFunc* OodLZ_Compress;
    inline OodLZ_DecompressFunc* OodLZ_Decompress;
    inline bool initializedSuccessfully = false;

    inline bool init()
    {
        initializedSuccessfully = false;

        oodle = LoadLibraryA("./oo2core_8_win64.dll");
        if (oodle == nullptr) // Could not find oodle binary
            return false;

        OodLZ_Decompress = (OodLZ_DecompressFunc*)GetProcAddress(oodle, "OodleLZ_Decompress");
        OodLZ_Compress = (OodLZ_CompressFunc*)GetProcAddress(oodle, "OodleLZ_Compress");

        if (OodLZ_Decompress == nullptr || OodLZ_Compress == nullptr) 
        { // Couldn't find the function(s)
            FreeLibrary(oodle);
            oodle = nullptr;
            OodLZ_Decompress = nullptr;
            OodLZ_Compress = nullptr;
            return false;
        }

        initializedSuccessfully = true;
        return true;
    }

    inline bool DecompressBuffer(char* inputBuffer, size_t inputSize, char* outputBuffer, size_t outputSize)
    {
        if(!initializedSuccessfully && !init())
            return false;

        int result = OodLZ_Decompress((byte*)inputBuffer, (int)inputSize, (byte*)outputBuffer, outputSize,
            1, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0);

        if((size_t) result != outputSize) // Decompression failed
            return false;
        return true;
    }

    inline bool CompressBuffer(char* inputBuffer, size_t inputSize, char* outputBuffer, size_t &outputSize)
    {
        if(!initializedSuccessfully && !init())
            return false;

        int compressedSize = OodLZ_Compress(13, (byte*)inputBuffer, inputSize, (byte*)outputBuffer,
            4, 0, 0, 0, 0, 0);
        if(compressedSize < 0) // Compression failed
            return false;

        outputSize = (size_t)compressedSize;
        return true;
    }
}