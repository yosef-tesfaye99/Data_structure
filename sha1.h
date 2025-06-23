// sha1.h
#ifndef SHA1_H
#define SHA1_H

#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>

class SHA1 {
public:
    SHA1() { reset(); }

    void update(const std::string &s) {
        update(reinterpret_cast<const uint8_t*>(s.c_str()), s.size());
    }

    void update(const uint8_t *data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            dataBuffer[bufferIndex++] = data[i];
            messageLength += 8;

            if (bufferIndex == 64) {
                processBlock(dataBuffer);
                bufferIndex = 0;
            }
        }
    }

    std::string final() {
        dataBuffer[bufferIndex++] = 0x80;
        if (bufferIndex > 56) {
            while (bufferIndex < 64) dataBuffer[bufferIndex++] = 0x00;
            processBlock(dataBuffer);
            bufferIndex = 0;
        }
        while (bufferIndex < 56) dataBuffer[bufferIndex++] = 0x00;

        for (int i = 7; i >= 0; --i) dataBuffer[bufferIndex++] = (messageLength >> (i * 8)) & 0xFF;
        processBlock(dataBuffer);

        std::ostringstream result;
        for (int i = 0; i < 5; ++i)
            result << std::hex << std::setw(8) << std::setfill('0') << digest[i];
        return result.str();
    }

private:
    uint32_t digest[5];
    uint8_t dataBuffer[64];
    size_t bufferIndex = 0;
    uint64_t messageLength = 0;

    void reset() {
        digest[0] = 0x67452301;
        digest[1] = 0xEFCDAB89;
        digest[2] = 0x98BADCFE;
        digest[3] = 0x10325476;
        digest[4] = 0xC3D2E1F0;
        bufferIndex = 0;
        messageLength = 0;
    }

    void processBlock(const uint8_t *block) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) | (block[i * 4 + 2] << 8) | block[i * 4 + 3];
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        }

        uint32_t a = digest[0];
        uint32_t b = digest[1];
        uint32_t c = digest[2];
        uint32_t d = digest[3];
        uint32_t e = digest[4];

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = temp;
        }

        digest[0] += a;
        digest[1] += b;
        digest[2] += c;
        digest[3] += d;
        digest[4] += e;
    }

    uint32_t rol(uint32_t value, int bits) {
        return (value << bits) | (value >> (32 - bits));
    }
};

inline std::string sha1(const std::string &s) {
    SHA1 sha;
    sha.update(s);
    return sha.final();
}

#endif // SHA1_H
