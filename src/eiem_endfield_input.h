#pragma once
// Internal to eiem_native_input.h: Reader, Bytes and checked LZ4 are defined
// before this include. Release-format constants/algorithms are cross-checked
// with AnimeStudio VFSUtils.cs; no game objects, runtime addresses or setters.
namespace EndfieldInput {
inline uint32_t Rol32(uint32_t value, unsigned n) {
  return (value << n) | (value >> (32 - n));
}
inline uint64_t Rol64(uint64_t value, unsigned n) {
  return (value << n) | (value >> (64 - n));
}
inline uint16_t Rol16(uint16_t value, unsigned n) {
  return uint16_t((value << n) | (value >> (16 - n)));
}
inline bool Recognized(const Bytes &b) {
  if (b.size() < 8)
    return false;
  Reader r(b, 0, true);
  uint32_t a = uint32_t(r.Get(4)), value = uint32_t(r.Get(4));
  a ^= 0x4A92F0CD;
  return value == ((4 * a & 0xFFFF0000) ^ Rol32(a, 18) ^ 0xD8B1E637);
}
inline uint32_t Size32(uint16_t high, uint16_t low) {
  return Rol32((uint32_t(uint16_t(high ^ low ^ 0xA121)) << 16) | low, 14) ^
         0xF74324EE;
}
inline uint64_t Size64(uint32_t high, uint32_t low) {
  return Rol64((uint64_t(high ^ low ^ 0xDAD76848) << 32) | low, 14) ^
         0xA4F1A11747816520ULL;
}
struct Header {
  size_t bytes = 0, compressedInfo = 0, decodedInfo = 0, prefix = 0;
  uint32_t flags = 0, encFlags = 0;
};
inline Header ReadHeader(const Bytes &b, size_t maximum = 64ull * 1024 * 1024) {
  Require(b.size() >= 48 && Recognized(b),
          "unsupported Endfield release header");
  Reader r(b, 8, true);
  auto c2 = uint16_t(r.Get(2));
  auto f2 = uint32_t(r.Get(4)), enc = uint32_t(r.Get(4)),
       s2 = uint32_t(r.Get(4));
  auto f1 = uint32_t(r.Get(4));
  auto u1 = uint16_t(r.Get(2));
  r.Skip(4);
  auto u2 = uint16_t(r.Get(2));
  auto s1 = uint32_t(r.Get(4));
  auto c1 = uint16_t(r.Get(2));
  r.Skip(1);
  Header h;
  h.bytes = size_t(Size64(s1, s2));
  h.compressedInfo = Size32(c1, c2);
  h.decodedInfo = Size32(u1, u2);
  h.flags = f1 ^ f2 ^ 0xA7F49310;
  h.encFlags = enc ^ f2;
  // This revision is proven by observed disk ranges and independent member
  // decoding. Older CBT/header revisions are deliberately rejected.
  Require(h.encFlags == 8 && !(h.flags & ~0x2FFu),
          "unverified Endfield header revision/flags");
  h.prefix = 48;
  Require(maximum <= 512ull * 1024 * 1024 && h.bytes >= h.prefix && h.bytes <= maximum &&
              h.compressedInfo && h.compressedInfo <= 16ull * 1024 * 1024 &&
              h.decodedInfo && h.decodedInfo <= 16ull * 1024 * 1024,
          "Endfield input/header size exceeds bounds");
  return h;
}
// Uses an independent read handle at an observed loader offset. This produces
// a stable disk snapshot, not a claim to own the native loader's read buffer.
inline Bytes Snapshot(const std::filesystem::path &path, uint64_t offset) {
  struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() {
      if (value != INVALID_HANDLE_VALUE)
        CloseHandle(value);
    }
  } handle;
  handle.value =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  Require(handle.value != INVALID_HANDLE_VALUE,
          "Endfield snapshot file open failed");
  BY_HANDLE_FILE_INFORMATION before{}, after{};
  Require(GetFileInformationByHandle(handle.value, &before),
          "Endfield snapshot file info failed");
  uint64_t size = (uint64_t(before.nFileSizeHigh) << 32) | before.nFileSizeLow;
  Require(offset <= size && 48 <= size - offset && offset <= INT64_MAX,
          "Endfield snapshot offset exceeds file");
  LARGE_INTEGER position{};
  position.QuadPart = (LONGLONG)offset;
  Require(SetFilePointerEx(handle.value, position, nullptr, FILE_BEGIN),
          "Endfield snapshot seek failed");
  Bytes bytes(48);
  DWORD read = 0;
  Require(ReadFile(handle.value, bytes.data(), 48, &read, nullptr) &&
              read == 48,
          "Endfield snapshot header read failed");
  auto header = ReadHeader(bytes);
  Require(header.bytes <= size - offset,
          "Endfield snapshot input exceeds source file");
  bytes.resize(header.bytes);
  size_t p = 48;
  while (p < bytes.size()) {
    auto count = DWORD(std::min(bytes.size() - p, size_t(1024 * 1024)));
    Require(ReadFile(handle.value, bytes.data() + p, count, &read, nullptr) &&
                read == count,
            "Endfield snapshot data read failed");
    p += read;
  }
  Require(GetFileInformationByHandle(handle.value, &after) &&
              before.dwVolumeSerialNumber == after.dwVolumeSerialNumber &&
              before.nFileIndexHigh == after.nFileIndexHigh &&
              before.nFileIndexLow == after.nFileIndexLow &&
              before.nFileSizeHigh == after.nFileSizeHigh &&
              before.nFileSizeLow == after.nFileSizeLow &&
              before.ftLastWriteTime.dwHighDateTime ==
                  after.ftLastWriteTime.dwHighDateTime &&
              before.ftLastWriteTime.dwLowDateTime ==
                  after.ftLastWriteTime.dwLowDateTime,
          "Endfield snapshot source changed during read");
  return bytes;
}
// Filled from the independently used importer constants, not the game image.
inline constexpr unsigned char sbox[256] = {
    0xE4, 0xB9, 0x45, 0x07, 0x92, 0x82, 0x2F, 0x43, 0xF5, 0xC9, 0x22, 0x25,
    0xA9, 0x4F, 0x46, 0x6D, 0x4A, 0x71, 0x8B, 0x6C, 0x8C, 0xEB, 0xB2, 0xAC,
    0xCF, 0x0C, 0x9E, 0x01, 0x38, 0x32, 0xD3, 0x93, 0x98, 0x63, 0xDA, 0x96,
    0xE5, 0xC4, 0xC3, 0x6B, 0x7F, 0x26, 0x72, 0xD7, 0x97, 0xD5, 0x80, 0xBC,
    0x5D, 0xBB, 0x55, 0x67, 0x10, 0x73, 0xB3, 0x8D, 0xE2, 0x35, 0x29, 0x47,
    0xA8, 0x60, 0x3F, 0xC5, 0xEF, 0x68, 0xEC, 0xBE, 0xAB, 0xC6, 0xB8, 0x5C,
    0xD8, 0x15, 0x09, 0x54, 0xF3, 0x7A, 0x40, 0xA2, 0x30, 0x0A, 0xDC, 0x53,
    0xFA, 0xDB, 0xF1, 0x78, 0xDE, 0xAD, 0xF0, 0xB5, 0xC1, 0x81, 0x9F, 0x3E,
    0x83, 0x90, 0x31, 0xF2, 0xFB, 0x21, 0x28, 0x85, 0x06, 0xCA, 0xCD, 0x1E,
    0xD4, 0x3C, 0xA0, 0xC8, 0x23, 0x16, 0x6E, 0x89, 0x1D, 0xE7, 0xEE, 0x5E,
    0x42, 0xBD, 0xCB, 0x13, 0x50, 0xA6, 0x4E, 0x49, 0x58, 0xDF, 0x2C, 0x84,
    0x87, 0xB6, 0x91, 0x52, 0xDD, 0x19, 0xF9, 0x2B, 0x4D, 0x77, 0xBA, 0x04,
    0xA5, 0x41, 0xCE, 0x94, 0x3D, 0x5F, 0xFC, 0x9B, 0x79, 0x9A, 0x7E, 0x65,
    0x5A, 0xB1, 0x66, 0x34, 0x56, 0xA7, 0x1A, 0xBF, 0xEA, 0x7D, 0x27, 0x0B,
    0x59, 0x2E, 0xAE, 0x14, 0x33, 0xC0, 0x51, 0x39, 0xC7, 0x3A, 0x2A, 0x9D,
    0xF4, 0x7C, 0xCC, 0xD1, 0xD6, 0x70, 0x37, 0x0E, 0x75, 0x02, 0x1B, 0xE3,
    0xE9, 0x48, 0x0D, 0x24, 0x2D, 0xF7, 0xD2, 0xB7, 0xAF, 0xA3, 0xA1, 0x64,
    0x7B, 0xED, 0xF8, 0x05, 0x95, 0x3B, 0x74, 0xFD, 0x62, 0xD0, 0x0F, 0xFF,
    0x4B, 0xAA, 0x88, 0x5B, 0x03, 0xB4, 0xE8, 0x9C, 0xB0, 0x17, 0x1C, 0x76,
    0x57, 0xE0, 0xA4, 0x44, 0x20, 0xD9, 0x8E, 0x11, 0x86, 0x69, 0x36, 0xFE,
    0x4C, 0x6F, 0x61, 0x6A, 0x8F, 0xE1, 0x18, 0x8A, 0x12, 0x99, 0xE6, 0x1F,
    0x00, 0x08, 0xF6, 0xC2};
inline constexpr unsigned char key[16] = {0x3A, 0xF1, 0x8C, 0x47, 0xB2, 0x09,
                                          0x6D, 0xEE, 0x51, 0x24, 0x90, 0x7C,
                                          0x18, 0xD3, 0xA4, 0x62};
inline constexpr unsigned char iv[16] = {0xC7, 0x12, 0x5E, 0xA9, 0x04, 0xDB,
                                         0x33, 0x88, 0xF2, 0x0E, 0x77, 0x49,
                                         0x65, 0xBA, 0x1C, 0x93};
inline unsigned char Xtime(unsigned char value) {
  return (unsigned char)((value << 1) ^ ((value & 0x80) ? 0x1B : 0));
}
inline std::array<unsigned char, 176> RoundKeys() {
  std::array<unsigned char, 176> keys{};
  std::copy(key, key + 16, keys.begin());
  unsigned char rcon = 1;
  for (size_t p = 16; p < keys.size(); p += 4) {
    unsigned char word[4];
    std::copy(keys.begin() + p - 4, keys.begin() + p, word);
    if (p % 16 == 0) {
      auto first = word[0];
      for (unsigned j = 0; j < 3; ++j)
        word[j] = sbox[word[j + 1]];
      word[3] = sbox[first];
      word[0] ^= rcon;
      rcon = Xtime(rcon);
    }
    for (unsigned j = 0; j < 4; ++j)
      keys[p + j] = keys[p + j - 16] ^ word[j];
  }
  return keys;
}
inline void EncryptState(std::array<unsigned char, 16> &state) {
  static const auto keys = RoundKeys();
  for (unsigned round = 0; round <= 10; ++round) {
    if (round) {
      for (auto &byte : state)
        byte = sbox[byte];
      auto old = state;
      for (unsigned column = 0; column < 4; ++column)
        for (unsigned row = 1; row < 4; ++row)
          state[column * 4 + row] = old[((column + row) % 4) * 4 + row];
      if (round < 10)
        for (unsigned column = 0; column < 4; ++column) {
          auto a = state.data() + column * 4;
          auto t = (unsigned char)(a[0] ^ a[1] ^ a[2] ^ a[3]), first = a[0];
          a[0] ^= t ^ Xtime(a[0] ^ a[1]);
          a[1] ^= t ^ Xtime(a[1] ^ a[2]);
          a[2] ^= t ^ Xtime(a[2] ^ a[3]);
          a[3] ^= t ^ Xtime(a[3] ^ first);
        }
    }
    for (unsigned j = 0; j < 16; ++j)
      state[j] ^= keys[round * 16 + j];
  }
}
inline void XorStream(Bytes &bytes) {
  std::array<unsigned char, 16> state{};
  std::copy(iv, iv + 16, state.begin());
  for (size_t p = 0; p < bytes.size(); p += 16) {
    EncryptState(state);
    for (size_t j = 0; j < std::min(size_t(16), bytes.size() - p); ++j)
      bytes[p + j] ^= state[j];
    for (unsigned j = 0; j < 16; ++j) {
      auto temp = (unsigned char)(state[j] ^ (31 * j) ^
                                  (0xF19AB7752CDD0196ULL >> ((8 * j) & 0x38)));
      temp = (unsigned char)((temp >> 5) | (temp << 3));
      state[j] = sbox[temp];
    }
  }
}
inline void CryptBlock(Bytes &bytes) {
  if (bytes.size() <= 256) {
    XorStream(bytes);
    return;
  }
  auto blocks = bytes.size() / 16;
  auto step = blocks > 256 ? size_t(1) : 256 / blocks;
  auto count = std::min(blocks, size_t(256));
  Bytes gathered(256);
  for (size_t i = 0; i < count; ++i)
    std::copy_n(bytes.begin() + i * 16, step, gathered.begin() + i * step);
  XorStream(gathered);
  for (size_t i = 0; i < count; ++i)
    std::copy_n(gathered.begin() + i * step, step, bytes.begin() + i * 16);
}
struct Member {
  std::string name;
  uint32_t flags = 0;
  Bytes bytes;
};
struct Bundle {
  Header header;
  std::vector<Member> members;
};
inline uint32_t Count(Reader &r, uint32_t mask, uint32_t finalMask) {
  auto encoded = uint32_t(r.Get(4)) ^ mask;
  auto low = uint16_t(encoded), high = uint16_t(encoded >> 16);
  auto count =
      Rol32((uint32_t(uint16_t(low ^ high)) << 16) | low, 14) ^ finalMask;
  Require(count <= 1000000, "Endfield directory/block count exceeds limit");
  return count;
}
inline Bundle Decode(const Bytes &b, size_t maximum = 64ull * 1024 * 1024) {
  Bundle bundle;
  auto &h = bundle.header;
  h = ReadHeader(b, maximum);
  Require(h.bytes == b.size(), "Endfield input incomplete");
  Require(h.compressedInfo <= b.size() - h.prefix, "Endfield directory range");
  size_t infoOffset = h.flags & 0x80 ? b.size() - h.compressedInfo : h.prefix;
  Bytes info(b.begin() + infoOffset, b.begin() + infoOffset + h.compressedInfo);
  auto mode = h.flags & 63;
  if (mode) {
    Require(mode == 2 || mode == 3,
            "unsupported Endfield directory compression");
    CryptBlock(info);
    info = Lz4(info.data(), info.size(), h.decodedInfo);
  } else
    Require(info.size() == h.decodedInfo, "Endfield directory size mismatch");
  Reader r(info, 0, true);
  auto blockCount = Count(r, 0x23F77B8A, 0x91CE0A4F);
  struct Chunk {
    size_t decoded, encoded;
    uint16_t flags;
  };
  std::vector<Chunk> chunks;
  size_t total = 0;
  for (size_t i = 0; i < blockCount; ++i) {
    auto a = uint16_t(r.Get(2)), b2 = uint16_t(r.Get(2)),
         c = uint16_t(r.Get(2));
    auto ef = uint16_t(r.Get(2) ^ 0x9CD6), d = uint16_t(r.Get(2));
    auto lo = uint8_t(ef), hi = uint8_t(ef >> 8);
    Chunk chunk{Size32(a, c), Size32(b2, d),
                uint16_t(c ^
                         Rol16(uint16_t((uint16_t(lo ^ hi) << 8) | lo), 14) ^
                         0x523F)};
    Require((chunk.flags == 0 || chunk.flags == 5) && chunk.decoded &&
                chunk.encoded && chunk.decoded <= 512ull * 1024 * 1024 - total,
            "unsupported Endfield block flags/range");
    total += chunk.decoded;
    chunks.push_back(chunk);
  }
  auto memberCount = Count(r, 0x6B0AE55D, 0xE4C1D9F2);
  struct Entry {
    uint64_t offset, size;
    uint32_t flags;
    std::string name;
  };
  std::vector<Entry> entries;
  for (size_t i = 0; i < memberCount; ++i) {
    auto a = uint32_t(r.Get(4)) ^ 0x8E06A9F8;
    auto b2 = uint32_t(r.Get(4)), c = uint32_t(r.Get(4)),
         d = uint32_t(r.Get(4));
    std::string name;
    for (;;) {
      auto byte = (unsigned char)r.Get(1);
      if (!byte)
        break;
      Require(name.size() < 64, "Endfield member name too long");
      byte ^= (unsigned char)(name.size() ^ 0x97);
      Require(byte && byte < 128, "unsupported Endfield member name encoding");
      name += char(byte);
    }
    auto e = uint32_t(r.Get(4));
    auto lo = uint16_t(a), hi = uint16_t(a >> 16);
    Entry entry{Size64(d, c), Size64(b2, e),
                Rol32((uint32_t(uint16_t(hi ^ lo)) << 16) | lo, 14) ^
                    0xF13927C4 ^ b2,
                name};
    Require(!name.empty() && entry.offset <= total &&
                entry.size <= total - entry.offset,
            "Endfield member range/name invalid");
    entries.push_back(std::move(entry));
  }
  Require(r.p == info.size(), "Endfield directory suffix");
  size_t dataOffset = h.prefix;
  if (!(h.flags & 0x80))
    dataOffset += h.flags & 0x200 ? (h.compressedInfo + 15) & ~size_t(15)
                                  : h.compressedInfo;
  Bytes data;
  data.reserve(total);
  for (const auto &chunk : chunks) {
    auto end = h.flags & 0x80 ? infoOffset : b.size();
    Require(dataOffset <= end && chunk.encoded <= end - dataOffset,
            "Endfield data block exceeds range");
    Bytes bytes(b.begin() + dataOffset, b.begin() + dataOffset + chunk.encoded);
    if (chunk.flags == 5) {
      CryptBlock(bytes);
      bytes = Lz4(bytes.data(), bytes.size(), chunk.decoded, true);
    } else
      Require(bytes.size() == chunk.decoded,
              "Endfield uncompressed block size");
    data.insert(data.end(), bytes.begin(), bytes.end());
    dataOffset += chunk.encoded;
  }
  Require(dataOffset == (h.flags & 0x80 ? infoOffset : b.size()),
          "Endfield data suffix");
  for (const auto &entry : entries) {
    Member member{entry.name, entry.flags,
                  Bytes(data.begin() + size_t(entry.offset),
                        data.begin() + size_t(entry.offset + entry.size))};
    bundle.members.push_back(std::move(member));
  }
  return bundle;
}
inline void Put(Bytes &bytes, size_t pos, uint64_t value, size_t count) {
  Require(count <= 8 && pos <= bytes.size() && count <= bytes.size() - pos,
          "Endfield encode range");
  for (size_t i = 0; i < count; ++i)
    bytes[pos + i] = (unsigned char)(value >> (8 * (count - 1 - i)));
}
inline void Append(Bytes &bytes, uint64_t value, size_t count) {
  auto pos = bytes.size();
  bytes.resize(pos + count);
  Put(bytes, pos, value, count);
}
inline std::pair<uint16_t, uint16_t> Encode32(uint32_t size) {
  auto encoded = Rol32(size ^ 0xF74324EE, 18);
  auto low = uint16_t(encoded);
  return {uint16_t((encoded >> 16) ^ low ^ 0xA121), low};
}
inline std::pair<uint32_t, uint32_t> Encode64(uint64_t size) {
  auto encoded = Rol64(size ^ 0xA4F1A11747816520ULL, 50);
  auto low = uint32_t(encoded);
  return {uint32_t((encoded >> 32) ^ low ^ 0xDAD76848), low};
}
inline uint32_t EncodeCount(uint32_t count, uint32_t mask, uint32_t finalMask) {
  auto value = Rol32(count ^ finalMask, 18);
  auto low = uint16_t(value), high = uint16_t((value >> 16) ^ low);
  return ((uint32_t(high) << 16) | low) ^ mask;
}
// Offline/native-input writer: retains member identities and native format.
// Uncompressed block mode is supported by the format reader; acceptance of a
// rebuilt file by the actual game's loader is a separate required check.
inline Bytes Encode(const Bytes &source, const std::vector<Member> &members,
                    size_t maximum = 64ull * 1024 * 1024) {
  // Hot-reload candidates may be prepared native inputs larger than 64 MiB.
  // Honor the same explicitly bounded contract for the source and the output.
  ReadHeader(source, maximum);
  Require(maximum <= 512ull * 1024 * 1024, "Endfield output limit exceeds decoded limit");
  Require(!members.empty() && members.size() <= 1000000,
          "Endfield output member count");
  Bytes data;
  std::vector<size_t> offsets;
  for (const auto &member : members) {
    Require(!member.name.empty() && member.name.size() <= 64 &&
                data.size() <= maximum && member.bytes.size() <= maximum - data.size(),
            "Endfield output member/byte limit");
    data.resize((data.size() + 15) & ~size_t(15));
    Require(data.size() <= maximum && member.bytes.size() <= maximum - data.size(),
            "Endfield output alignment exceeds limit");
    offsets.push_back(data.size());
    data.insert(data.end(), member.bytes.begin(), member.bytes.end());
  }
  Require(!data.empty(), "Endfield output data is empty");
  Bytes info;
  Append(info, EncodeCount(1, 0x23F77B8A, 0x91CE0A4F), 4);
  auto block = Encode32(uint32_t(data.size()));
  auto flagsValue = Rol16(uint16_t(block.second ^ 0x523F), 2);
  auto lo = uint8_t(flagsValue), hi = uint8_t((flagsValue >> 8) ^ lo);
  Append(info, block.first, 2);
  Append(info, block.first, 2);
  Append(info, block.second, 2);
  Append(info, ((uint16_t(hi) << 8) | lo) ^ 0x9CD6, 2);
  Append(info, block.second, 2);
  Append(info, EncodeCount(uint32_t(members.size()), 0x6B0AE55D, 0xE4C1D9F2),
         4);
  for (size_t i = 0; i < members.size(); ++i) {
    const auto &member = members[i];
    auto size = Encode64(member.bytes.size()), offset = Encode64(offsets[i]);
    auto flags = Rol32(member.flags ^ 0xF13927C4 ^ size.first, 18);
    auto low = uint16_t(flags), high = uint16_t((flags >> 16) ^ low);
    Append(info, ((uint32_t(high) << 16) | low) ^ 0x8E06A9F8, 4);
    Append(info, size.first, 4);
    Append(info, offset.second, 4);
    Append(info, offset.first, 4);
    for (size_t j = 0; j < member.name.size(); ++j) {
      auto byte = (unsigned char)member.name[j];
      Require(byte && byte < 128, "unsupported Endfield output member name");
      Append(info, byte ^ (unsigned char)(j ^ 0x97), 1);
    }
    Append(info, 0, 1);
    Append(info, size.second, 4);
  }
  size_t paddedInfo = (info.size() + 15) & ~size_t(15);
  Require(paddedInfo <= 16ull * 1024 * 1024 &&
              48 + paddedInfo + data.size() <= maximum,
          "Endfield output exceeds input limit");
  Bytes output(source.begin(), source.begin() + 48);
  output.insert(output.end(), info.begin(), info.end());
  output.resize(48 + paddedInfo);
  output.insert(output.end(), data.begin(), data.end());
  auto infoSize = Encode32(uint32_t(info.size()));
  auto fileSize = Encode64(output.size());
  Reader reader(source, 10, true);
  auto f2 = uint32_t(reader.Get(4));
  Put(output, 8, infoSize.second, 2);
  Put(output, 14, 8 ^ f2, 4);
  Put(output, 18, fileSize.second, 4);
  Put(output, 22, 0x240 ^ f2 ^ 0xA7F49310, 4);
  Put(output, 26, infoSize.first, 2);
  Put(output, 32, infoSize.second, 2);
  Put(output, 34, fileSize.first, 4);
  Put(output, 38, infoSize.first, 2);
  return output;
}
} // namespace EndfieldInput
