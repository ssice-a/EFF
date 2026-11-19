#include <string>
#include "../../src/eiem_physics_component_identity.h"
#include <iostream>

int main() {
  using namespace EiemPhysicsComponentIdentity;
  const std::string authorId = "19c8f6f506834281a28d8ea787d4d056";
  const std::string expected =
      "__EFF_EFF_COMPOSED_1_M40342DE038E2809845AF515550DBF4124F6A2EF40753F07F1CC69DEAC49BAC1B_"
      "19c8f6f506834281a28d8ea787d4d056";
  const auto composedId = "M40342DE038E2809845AF515550DBF4124F6A2EF40753F07F1CC69DEAC49BAC1B_" + authorId;
  const auto generated = Name("EFF_COMPOSED_1", composedId);
  if (generated != expected || !IsGeneratedName(generated)) return 1;
  if (!IsGeneratedName(Name("lvt", authorId))) return 2;
  if (IsGeneratedName("MC_Laevat_Skirt") || IsGeneratedName(Name("package", "not-a-group-id"))) return 3;
  if (IsGeneratedName("prefix" + generated) || IsGeneratedName(Name("", authorId))) return 4;
  std::cout << "PASS generated Physics component identity and native cloth name rejection\n";
  return 0;
}
