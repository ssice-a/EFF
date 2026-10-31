#include "../../src/eiem_author_resource_file.h"
#include <iostream>

using namespace EiemDirectory;
using namespace EiemAuthorResourceFile;

int main() {
  try {
    Bytes metadata{'m', 'e', 's', 'h', '-', 'v', '3'};
    Bytes payload{0, 1, 2, 3, 0xFE, 0xFF};
    auto file = Encode(1, metadata, payload);
    auto read = [&](size_t at, unsigned char* output, size_t size) {
      Require(at <= file.size() && size <= file.size() - at,
              "smoke read range outside author resource");
      memcpy(output, file.data() + at, size);
    };
    auto layout = Parse(file.size(), read);
    Require(layout.kind == 1 && layout.metadataBytes == metadata.size() &&
                layout.payloadBytes == payload.size() && ReadMetadata(layout, read) == metadata,
            "author resource fields differ");
    Bytes payloadRead(layout.payloadBytes);
    read(layout.payloadOffset, payloadRead.data(), payloadRead.size());
    Require(payloadRead == payload, "author resource payload range differs");

    auto rejects = [&](Bytes candidate, size_t declaredBytes) {
      bool rejected = false;
      try {
        Parse(declaredBytes, [&](size_t at, unsigned char* output, size_t size) {
          Require(at <= candidate.size() && size <= candidate.size() - at,
                  "smoke read exceeds candidate");
          memcpy(output, candidate.data() + at, size);
        });
      } catch (const std::exception&) {
        rejected = true;
      }
      Require(rejected, "invalid author resource was accepted");
    };

    auto badChecksum = file;
    badChecksum[40] ^= 0x80;
    rejects(std::move(badChecksum), file.size());
    auto truncated = file;
    truncated.pop_back();
    rejects(std::move(truncated), file.size() - 1);
    auto wrongKind = file;
    Put(wrongKind.data() + 20, 8, 4);
    rejects(std::move(wrongKind), file.size());
    auto suffix = file;
    suffix.push_back(0);
    rejects(std::move(suffix), file.size() + 1);

    std::cout << "PASS self-describing per-resource header, metadata integrity, native payload span and malformed extents\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
