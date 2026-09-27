#pragma once

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cerrno>
#include <climits>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include "eiem_expression.h"
#include "eiem_keys.h"
#include "eiem_physics_asset.h"
#include "eiem_file_io.h"

// Authoring syntax -> typed resource declarations and Render actions. No
// published configuration, Unity objects, hotkeys or lifecycle state live here.
struct EiemModRule {
  char modPath[MAX_PATH] = {};
  char section[96] = {};
  char path[768] = {};
  char handling[32] = {};
  char mesh[192] = {};
  char asset[192] = {};
  int32_t matchVertices = -1;
  int32_t matchIndices = -1;
  int32_t matchSubMeshes = -1;
  char skeleton[192] = {};
  char physics[192] = {};
  bool hasMesh = false;
  bool hasSkeleton = false;
  bool hasPhysics = false;
  char materials[16][192] = {};
  int32_t materialSlots[16] = {};
  uint32_t materialCount = 0;
  int32_t submeshSlots[32] = {};
  uint32_t submeshCount = 0;
  // A hidden submesh keeps its slot and material, but receives an empty index
  // buffer in the generated Mesh variant. The Renderer and its skinning stay
  // unchanged, so visibility changes do not create or destroy Unity objects.
  uint32_t hiddenSubmeshMask = 0;
  char shapeNames[64][192] = {};
  float shapeWeights[64] = {}; // authoring units: 1 == Unity 100
  uint32_t shapeCount = 0;
  char shapeSpeedNames[64][192] = {};
  float shapeSpeeds[64] = {}; // authoring units per second
  uint32_t shapeSpeedCount = 0;
};

// A Prefab declaration groups the Render actions for one logical model. Each
// supported lifecycle owner resolves that PFB identity first, then invokes the
// same Render executor on its completed model hierarchy.
struct EiemModPrefab {
  char modPath[MAX_PATH] = {};
  char section[96] = {};
  char path[768] = {};
  char renders[64][96] = {};
  uint32_t renderCount = 0;
};

struct EiemModResource {
  char modPath[MAX_PATH] = {};
  char section[96] = {};
  char path[768] = {};
  char kind[24] = {};
  char source[768] = {};
  // Optional logical identity of the original game resource represented by
  // this editable payload. Render rules use that identity for authoring and
  // diagnostics; resource declarations never execute replacement logic.
  char targetPath[768] = {};
  char targetAsset[192] = {};
  bool textureLinear = false;
  bool textureMipmaps = true;
  int32_t textureFilter = 1;
  int32_t textureWrap = 0;
  int32_t textureAniso = 1;
  float textureMipBias = 0.0f;
  std::shared_ptr<const EiemPhysicsAsset> physicsAsset;
};

static void EiemModInitRule(EiemModRule *rule) {
  if (!rule) return;
  memset(rule, 0, sizeof(*rule));
  for (auto &slot : rule->materialSlots) slot = -1;
  for (auto &slot : rule->submeshSlots) slot = -1;
  rule->matchVertices = -1;
  rule->matchIndices = -1;
  rule->matchSubMeshes = -1;
}

static inline void EiemModTrim(std::string &value) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
}

static inline bool EiemModEquals(const char *a, const char *b) {
  return a && b && _stricmp(a, b) == 0;
}

static bool EiemModSameLogicalPath(const char *a, const char *b) {
  if (!a || !b || !a[0] || !b[0]) return false;
  while (*a && *b) {
    const unsigned char ca = (unsigned char)(*a == '\\' ? '/' : *a);
    const unsigned char cb = (unsigned char)(*b == '\\' ? '/' : *b);
    if (tolower(ca) != tolower(cb)) return false;
    ++a;
    ++b;
  }
  return !*a && !*b;
}

static inline void EiemModCopy(char *out, size_t capacity, const std::string &value) {
  if (!out || capacity == 0) return;
  strncpy_s(out, capacity, value.c_str(), _TRUNCATE);
}

static bool EiemModInteger(const std::string &text, int32_t *out,
                            int32_t minimum = 0, int32_t maximum = INT32_MAX) {
  if (text.empty()) return false;
  char *end = nullptr;
  errno = 0;
  const long long value = strtoll(text.c_str(), &end, 10);
  if (errno || end == text.c_str() || *end || value < minimum || value > maximum)
    return false;
  *out = (int32_t)value;
  return true;
}

// A Render has immutable selectors and an ordered body. Evaluated rules remain
// plain data, so existing hooks never need to interpret conditions.
struct EiemModStatement {
  size_t line = 0;
  std::string key, value;
  std::shared_ptr<EiemExpression> condition;
  std::shared_ptr<EiemExpression> number;
  std::vector<EiemModStatement> yes, no;
};
struct EiemRenderProgram {
  EiemModRule selector;
  size_t stateIndex = 0;
  std::vector<EiemModStatement> statements;
};
struct EiemKeyAssignment {
  std::string variable;
  std::vector<double> values;
};
enum class EiemModKeyBehavior { Cycle, Hold };
enum class EiemKeyScope { Game, Ui, Both };
static bool EiemKeyInScope(EiemKeyScope scope, bool uiFocus) {
  return scope == EiemKeyScope::Both || scope == (uiFocus ? EiemKeyScope::Ui : EiemKeyScope::Game);
}
struct EiemModKey {
  std::string section;
  EiemKeyChord chord;
  size_t line = 0;
  std::vector<EiemKeyAssignment> assignments;
  EiemModKeyBehavior behavior = EiemModKeyBehavior::Cycle;
  double speed = 0.0; // Hold target movement in variable units per second.
  EiemKeyScope scope = EiemKeyScope::Game;
};
struct EiemModUi {
  std::string section, path;
};
struct EiemModShapeControl {
  std::string section, variable, label;
  double minimum = NAN, maximum = NAN;
};
struct EiemModState {
  std::string path;
  EiemVariables defaults, variables;
  std::unordered_set<std::string> persistent;
  std::vector<EiemModKey> keys;
  std::vector<EiemModUi> uis;
  std::vector<EiemModShapeControl> shapeControls;
};
struct EiemModProgram {
  std::vector<EiemModPrefab> prefabs;
  std::vector<EiemModResource> resources;
  std::vector<EiemRenderProgram> definitions;
  std::vector<EiemModState> states;
  std::vector<EiemModRule> rules;
  std::vector<size_t> standaloneRules;
};

static bool EiemRenderSelectorKey(const std::string &key) {
  return key == "path" || key == "asset" || key == "match.vertices" ||
         key == "match.indices" || key == "match.submeshes";
}

static bool EiemSetRenderField(EiemModRule &rule, const std::string &key,
                               const std::string &value, std::string &error,
                               const double *number = nullptr) {
  auto fail = [&] { error = "Invalid Render field: " + key + "=" + value; return false; };
  auto copy = [&](auto &field) {
    if (value.size() >= sizeof(field)) return false;
    EiemModCopy(field, sizeof(field), value);
    return true;
  };
  if (key == "path") return copy(rule.path) || fail();
  if (key == "asset") return copy(rule.asset) || fail();
  if (key == "match.vertices") return EiemModInteger(value, &rule.matchVertices) || fail();
  if (key == "match.indices") return EiemModInteger(value, &rule.matchIndices) || fail();
  if (key == "match.submeshes") return EiemModInteger(value, &rule.matchSubMeshes) || fail();
  if (key == "handling") {
    if (!value.empty() && !EiemModEquals(value.c_str(), "skip")) return fail();
    return copy(rule.handling) || fail();
  }
  if (key == "mesh") { rule.hasMesh = !value.empty(); return copy(rule.mesh) || fail(); }
  if (key == "skeleton") { rule.hasSkeleton = !value.empty(); return copy(rule.skeleton) || fail(); }
  if (key == "physics") { rule.hasPhysics = !value.empty(); return copy(rule.physics) || fail(); }
  if (key.compare(0, 6, "shape.") == 0) {
    const std::string name = key.substr(6);
    if (name.empty() || name.size() >= sizeof(rule.shapeNames[0])) return fail();
    uint32_t slot = 0;
    while (slot < rule.shapeCount && name != rule.shapeNames[slot]) ++slot;
    if (value.empty() && !number) {
      if (slot < rule.shapeCount) {
        for (uint32_t i = slot + 1; i < rule.shapeCount; ++i) {
          memcpy(rule.shapeNames[i - 1], rule.shapeNames[i], sizeof(rule.shapeNames[i]));
          rule.shapeWeights[i - 1] = rule.shapeWeights[i];
        }
        --rule.shapeCount;
      }
      return true;
    }
    double weight = 0;
    if (number) weight = *number;
    if (slot >= _countof(rule.shapeNames) || (!number && !EiemNumber(value, &weight)) ||
        !std::isfinite((float)(weight * 100))) return fail();
    EiemModCopy(rule.shapeNames[slot], sizeof(rule.shapeNames[slot]), name);
    rule.shapeWeights[slot] = (float)weight;
    if (slot == rule.shapeCount) ++rule.shapeCount;
    return true;
  }
  if (key.compare(0, 12, "shape_speed.") == 0) {
    const std::string name = key.substr(12);
    if (name.empty() || name.size() >= sizeof(rule.shapeSpeedNames[0])) return fail();
    uint32_t slot = 0;
    while (slot < rule.shapeSpeedCount && name != rule.shapeSpeedNames[slot]) ++slot;
    if (value.empty()) {
      if (slot < rule.shapeSpeedCount) {
        for (uint32_t i = slot + 1; i < rule.shapeSpeedCount; ++i) {
          memcpy(rule.shapeSpeedNames[i - 1], rule.shapeSpeedNames[i],
                 sizeof(rule.shapeSpeedNames[i]));
          rule.shapeSpeeds[i - 1] = rule.shapeSpeeds[i];
        }
        --rule.shapeSpeedCount;
      }
      return true;
    }
    double speed = 0;
    if (slot >= _countof(rule.shapeSpeedNames) || !EiemNumber(value, &speed) ||
        !std::isfinite((float)speed) || speed <= 0) return fail();
    EiemModCopy(rule.shapeSpeedNames[slot], sizeof(rule.shapeSpeedNames[slot]), name);
    rule.shapeSpeeds[slot] = (float)speed;
    if (slot == rule.shapeSpeedCount) ++rule.shapeSpeedCount;
    return true;
  }
  int32_t index = 0;
  if (key.compare(0, 9, "material.") == 0) {
    if (!EiemModInteger(key.substr(9), &index, 0, _countof(rule.materialSlots) - 1)) return fail();
    uint32_t slot = 0;
    while (slot < rule.materialCount && rule.materialSlots[slot] != index) ++slot;
    if (value.empty()) {
      if (slot < rule.materialCount) {
        for (uint32_t i = slot + 1; i < rule.materialCount; ++i) {
          rule.materialSlots[i - 1] = rule.materialSlots[i];
          memcpy(rule.materials[i - 1], rule.materials[i], sizeof(rule.materials[i]));
        }
        --rule.materialCount;
      }
      return true;
    }
    if (!copy(rule.materials[slot])) return fail();
    if (slot == rule.materialCount) ++rule.materialCount;
    rule.materialSlots[slot] = index;
    return true;
  }
  if (key.compare(0, 8, "submesh.") == 0) {
    int32_t slot = -1;
    if (!EiemModInteger(key.substr(8), &index, 0, _countof(rule.submeshSlots) - 1) ||
        (!value.empty() && !EiemModInteger(value, &slot, 0, _countof(rule.materialSlots) - 1))) return fail();
    rule.submeshSlots[index] = slot;
    rule.submeshCount = (std::max)(rule.submeshCount, (uint32_t)index + 1);
    return true;
  }
  if (key.compare(0, 16, "submesh_visible.") == 0) {
    if (!EiemModInteger(key.substr(16), &index, 0, 31)) return fail();
    if (value.empty()) {
      rule.hiddenSubmeshMask &= ~(1u << (uint32_t)index);
      return true;
    }
    const bool visible = EiemModEquals(value.c_str(), "true") || value == "1";
    const bool hidden = EiemModEquals(value.c_str(), "false") || value == "0";
    if (!visible && !hidden) return fail();
    if (hidden) rule.hiddenSubmeshMask |= 1u << (uint32_t)index;
    else rule.hiddenSubmeshMask &= ~(1u << (uint32_t)index);
    return true;
  }
  return fail();
}

static void EiemEvaluateStatements(const std::vector<EiemModStatement> &statements,
                                    const EiemVariables &variables, EiemModRule &rule) {
  for (const auto &statement : statements) {
    if (statement.condition)
      EiemEvaluateStatements(statement.condition->Evaluate(variables) ? statement.yes : statement.no, variables, rule);
    else {
      std::string unused; // fields were validated before publication
      const double number = statement.number ? statement.number->Evaluate(variables) : 0;
      EiemSetRenderField(rule, statement.key, statement.value, unused,
                         statement.number ? &number : nullptr);
    }
  }
}

static void EiemEvaluateModProgram(EiemModProgram &program) {
  program.rules.clear();
  for (const auto &definition : program.definitions) {
    EiemModRule rule = definition.selector;
    EiemEvaluateStatements(definition.statements, program.states[definition.stateIndex].variables, rule);
    program.rules.push_back(rule);
  }
}

template <typename Visitor>
static void EiemVisitStatements(const std::vector<EiemModStatement> &statements, Visitor visit) {
  for (const auto &statement : statements) {
    visit(statement);
    EiemVisitStatements(statement.yes, visit);
    EiemVisitStatements(statement.no, visit);
  }
}

static std::string EiemModIdentifier(const char *file, const char *section) {
  std::string result = std::string(file) + '\n' + section;
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  return result;
}

static void EiemCompileModProgram(EiemModProgram &program) {
  program.standaloneRules.clear();
  // Prefab declarations group resources but do not scope Render selectors.
  // Every Render that identifies a source Mesh is compiled as a direct action.
  for (size_t i = 0; i < program.definitions.size(); ++i) {
    const auto &rule = program.definitions[i].selector;
    if (rule.path[0] || rule.asset[0])
      program.standaloneRules.push_back(i);
  }
  // Derive default 0..1 sliders once per parsed generation. Explicit
  // ShapeControl sections carry the author's label and range; key-free Mesh
  // shapes still become selectable without asking the Blender exporter to
  // create a Lua UI or a shortcut.
  for (const auto &definition : program.definitions) {
    auto &state = program.states[definition.stateIndex];
    EiemVisitStatements(definition.statements, [&](const EiemModStatement &statement) {
      if (statement.key.compare(0, 6, "shape.") != 0 || !statement.number ||
          statement.number->op != EiemExpression::Variable) return;
      const std::string &variable = statement.number->variable;
      if (!state.defaults.count(variable) ||
          std::any_of(state.shapeControls.begin(), state.shapeControls.end(),
                      [&](const auto &control) {
                        return control.variable == variable;
                      })) return;
      EiemModShapeControl control;
      control.variable = variable;
      control.label = statement.key.substr(6);
      control.minimum = (std::min)(0.0, state.defaults.at(variable));
      control.maximum = (std::max)(1.0, state.defaults.at(variable));
      for (const auto &key : state.keys)
        for (const auto &assignment : key.assignments)
          if (assignment.variable == variable)
            for (double value : assignment.values) {
              control.minimum = (std::min)(control.minimum, value);
              control.maximum = (std::max)(control.maximum, value);
            }
      state.shapeControls.push_back(std::move(control));
    });
  }
  EiemEvaluateModProgram(program);
}

// A file is validated in isolation before appending. References cannot silently
// reach another mod; typoed references in inactive branches are errors too.
static bool EiemValidShapeExpression(const EiemExpression &expression, const EiemModState &state) {
  auto finiteWeight = [](double value) { return std::isfinite((float)(value * 100.0)); };
  if (expression.op == EiemExpression::Number) return finiteWeight(expression.number);
  if (expression.op == EiemExpression::Negative) return EiemValidShapeExpression(*expression.left, state);
  if (expression.op != EiemExpression::Variable) return true; // comparisons/logical results are 0 or 1
  if (!finiteWeight(state.defaults.at(expression.variable))) return false;
  for (const auto &key : state.keys) for (const auto &assignment : key.assignments)
    if (assignment.variable == expression.variable)
      for (double value : assignment.values) if (!finiteWeight(value)) return false;
  return true;
}

// Validate a complete UI transaction before publishing anything. Lua cannot add
// variables or publish a value that overflows a Render's native shape weights.
static bool EiemValidateUiValues(const EiemModProgram &program, size_t stateIndex,
                                 const EiemVariables &values, EiemVariables &proposed,
                                 std::string &error) {
  proposed = program.states[stateIndex].variables;
  for (const auto &item : values) {
    if (!proposed.count(item.first) || !std::isfinite(item.second)) {
      error = "Invalid UI variable/value: " + item.first; return false;
    }
    proposed[item.first] = item.second;
  }
  bool valid = true;
  for (const auto &definition : program.definitions) if (definition.stateIndex == stateIndex)
    EiemVisitStatements(definition.statements, [&](const EiemModStatement &s) {
      if (s.number && !std::isfinite((float)(s.number->Evaluate(proposed) * 100.0))) {
        valid = false; error = "UI value overflows shape weight: " + s.key;
      }
    });
  return valid;
}

static bool EiemApplyUiValues(EiemModProgram &program, size_t stateIndex,
                              const EiemVariables &values, std::string &error) {
  EiemVariables proposed;
  if (!EiemValidateUiValues(program,stateIndex,values,proposed,error)) return false;
  program.states[stateIndex].variables = std::move(proposed); return true;
}

static bool EiemModRelativeFilePath(const std::string &value) {
  if (value.empty() || value.size() >= 768 || value.front() == '/' || value.front() == '\\' ||
      value.find(':') != std::string::npos || value.find('\0') != std::string::npos) return false;
  std::string normalized = value;
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  std::istringstream parts(normalized);
  for (std::string part; std::getline(parts, part, '/');) if (part == "..") return false;
  return true;
}

static bool EiemValidateModDocument(EiemModProgram &doc, std::string &error) {
  auto &state = doc.states.front();
  for (const auto &resource : doc.resources) {
    if (!EiemModRelativeFilePath(resource.path)) {
      error = "Resource needs a relative file path: " + std::string(resource.section);
      return false;
    }
  }
  std::unordered_map<std::string, std::string> resources;
  std::unordered_map<std::string, size_t> renders;
  for (const auto &r : doc.resources) resources[EiemModIdentifier("", r.section)] = r.kind;
  for (size_t i = 0; i < doc.definitions.size(); ++i)
    renders[EiemModIdentifier("", doc.definitions[i].selector.section)] = i;
  for (size_t i = 0; i < doc.definitions.size(); ++i) {
    bool valid = true;
    std::unordered_set<std::string> shapes, shapeSpeeds;
    EiemVisitStatements(doc.definitions[i].statements, [&](const EiemModStatement &s) {
      if (!valid) return;
      std::string detail;
      if (s.condition && !s.condition->Validate(state.defaults, detail)) valid = false;
      if (s.number && !s.number->Validate(state.defaults, detail)) valid = false;
      if (valid && s.number && !EiemValidShapeExpression(*s.number, state)) {
        valid = false; detail = "Shape weight exceeds finite Unity float range";
      }
      if (s.key.compare(0, 6, "shape.") == 0 && shapes.insert(s.key).second && shapes.size() > 64) {
        valid = false; detail = "Render exceeds 64 shape channels";
      }
      if (s.key.compare(0, 12, "shape_speed.") == 0 &&
          shapeSpeeds.insert(s.key).second && shapeSpeeds.size() > 64) {
        valid = false; detail = "Render exceeds 64 shape transition channels";
      }
      if (!s.condition && !s.value.empty()) {
        std::string kind;
        if (s.key == "mesh") kind = "Mesh";
        else if (s.key == "skeleton") kind = "Skeleton";
        else if (s.key == "physics") kind = "Physics";
        else if (s.key.compare(0, 9, "material.") == 0) kind = "Material";
        if (!kind.empty()) {
          auto it = resources.find(EiemModIdentifier("", s.value.c_str()));
          if (it == resources.end() || it->second != kind) {
            valid = false; detail = "Missing " + kind + " declaration: " + s.value;
          }
        }
      }
      if (!valid) error = std::to_string(s.line) + ": " + detail;
    });
    if (!valid) return false;
  }
  for (const auto &prefab : doc.prefabs)
    for (uint32_t i = 0; i < prefab.renderCount; ++i)
      if (prefab.renders[i][0] && !renders.count(EiemModIdentifier("", prefab.renders[i]))) {
        error = "Prefab references missing Render: " + std::string(prefab.renders[i]); return false;
      }
  for (const auto &key : state.keys) {
    if (!key.chord.vk || key.assignments.empty()) {
      error = std::to_string(key.line) + ": Key needs key=, type and variable values"; return false;
    }
    if (key.behavior == EiemModKeyBehavior::Hold) {
      if (!std::isfinite(key.speed) || key.speed <= 0) {
        error = std::to_string(key.line) + ": Hold Key needs speed=positive"; return false;
      }
      for (const auto &assignment : key.assignments) {
        if (!state.defaults.count(assignment.variable) || assignment.values.size() != 1) {
          error = std::to_string(key.line) + ": Hold Key needs one target value: " + assignment.variable; return false;
        }
      }
    } else {
      const size_t length = key.assignments.front().values.size();
      for (const auto &assignment : key.assignments) {
        if (!state.defaults.count(assignment.variable) || length < 2 || assignment.values.size() != length) {
          error = std::to_string(key.line) + ": Invalid cycle variable/list: " + assignment.variable; return false;
        }
      }
    }
  }
  for (const auto &ui : state.uis) {
    if (!EiemModRelativeFilePath(ui.path)) {
      error = "UI needs a relative script path: " + ui.section; return false;
    }
  }
  std::unordered_set<std::string> shapeVariables;
  for (const auto &definition : doc.definitions)
    EiemVisitStatements(definition.statements, [&](const EiemModStatement &statement) {
      if (statement.key.compare(0, 6, "shape.") == 0 && statement.number &&
          statement.number->op == EiemExpression::Variable)
        shapeVariables.insert(statement.number->variable);
    });
  std::unordered_set<std::string> declaredControls;
  std::vector<EiemModShapeControl> activeControls;
  activeControls.reserve(state.shapeControls.size());
  for (const auto &control : state.shapeControls) {
    // A stale authoring entry can survive after a Shape Key is removed or
    // after sibling parts are merged. It cannot affect a Renderer when no
    // shape.* statement references its variable, so discard that entry while
    // keeping the rest of the Mod usable. Referenced controls still undergo
    // the complete declaration validation below.
    if (!shapeVariables.count(control.variable)) continue;
    if (!state.defaults.count(control.variable) || control.label.empty() ||
        !std::isfinite((float)control.minimum) ||
        !std::isfinite((float)control.maximum) ||
        control.minimum >= control.maximum ||
        state.defaults.at(control.variable) < control.minimum ||
        state.defaults.at(control.variable) > control.maximum ||
        !declaredControls.insert(control.variable).second) {
      error = "Invalid ShapeControl declaration: " + control.section;
      return false;
    }
    activeControls.push_back(control);
  }
  state.shapeControls = std::move(activeControls);
  state.variables = state.defaults;
  return true;
}

static void EiemAppendModDocument(EiemModProgram &output, EiemModProgram doc) {
  const size_t stateIndex = output.states.size();
  for (auto &definition : doc.definitions) definition.stateIndex += stateIndex;
  output.states.insert(output.states.end(), doc.states.begin(), doc.states.end());
  output.definitions.insert(output.definitions.end(), doc.definitions.begin(), doc.definitions.end());
  output.prefabs.insert(output.prefabs.end(), doc.prefabs.begin(), doc.prefabs.end());
  output.resources.insert(output.resources.end(), doc.resources.begin(), doc.resources.end());
  EiemCompileModProgram(output);
}

static bool EiemModParseStream(std::istream &input, const char *path,
                                EiemModProgram &output, std::string *error = nullptr) {
  EiemModProgram doc;
  doc.states.push_back({});
  doc.states[0].path = path;
  auto &state = doc.states[0];
  size_t lineNumber = 0;
  if (error) error->clear();
  auto fail = [&](const std::string &message) {
    if (error) *error = std::to_string(lineNumber) + ": " + message;
    return false;
  };
  if (strlen(path) >= sizeof(EiemModRule::modPath))
    return fail("Mod INI path exceeds UTF-8 byte limit");
  enum Section { None, Constants, Key, UI, ShapeControl, Render, Prefab, Resource } section = None;
  std::unordered_set<std::string> uiFields;
  std::unordered_set<std::string> shapeControlFields;
  std::unordered_set<std::string> sections;
  struct Frame {
    std::vector<EiemModStatement> *parent;
    EiemModStatement *branch;
    bool hadElse = false;
  };
  std::vector<Frame> stack;
  std::vector<EiemModStatement> *body = nullptr;
  bool keyTypeSeen = false, keySpeedSeen = false, keyScopeSeen = false;
  size_t conditionNodes = 0;
  auto finishSection = [&]() {
    if (!stack.empty()) return fail("Missing endif before section end");
    if (section == Key && !keyTypeSeen) return fail("Key needs type=cycle or type=hold");
    if (section == Resource && !doc.resources.back().path[0]) return fail("Resource needs path");
    if (section == Prefab && !doc.prefabs.back().path[0]) return fail("Prefab needs path");
    return true;
  };
  std::string line;
  while (std::getline(input, line)) {
    ++lineNumber;
    if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
    EiemModTrim(line);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
    if (line.front() == '[' && line.back() == ']') {
      if (!finishSection()) return false;
      std::string name = line.substr(1, line.size() - 2);
      EiemModTrim(name);
      if (name.empty() || name.size() >= 96 || !sections.insert(EiemModIdentifier("", name.c_str())).second)
        return fail("Empty, too long or duplicate section: " + name);
      body = nullptr;
      if (EiemModEquals(name.c_str(), "Constants")) section = Constants;
      else if (_strnicmp(name.c_str(), "Slider", 6) == 0) {
        return fail("Slider sections have been removed; re-export as UI + Lua");
      }
      else if (_strnicmp(name.c_str(), "UI", 2) == 0) {
        section = UI; uiFields.clear(); state.uis.push_back({});
        state.uis.back().section = name;
      }
      else if (_strnicmp(name.c_str(), "ShapeControl", 12) == 0) {
        section = ShapeControl; shapeControlFields.clear();
        state.shapeControls.push_back({});
        state.shapeControls.back().section = name;
      }
      else if (_strnicmp(name.c_str(), "Key", 3) == 0) {
        section = Key; keyTypeSeen = keySpeedSeen = keyScopeSeen = false;
        state.keys.push_back({});
        state.keys.back().section = name; state.keys.back().line = lineNumber;
      } else if (_strnicmp(name.c_str(), "Render", 6) == 0) {
        section = Render;
        conditionNodes = 0;
        doc.definitions.push_back({});
        auto &definition = doc.definitions.back();
        EiemModInitRule(&definition.selector);
        EiemModCopy(definition.selector.modPath, sizeof(definition.selector.modPath), path);
        EiemModCopy(definition.selector.section, sizeof(definition.selector.section), name);
        body = &definition.statements;
      } else if (_strnicmp(name.c_str(), "Prefab", 6) == 0) {
        section = Prefab; doc.prefabs.push_back({});
        auto &p = doc.prefabs.back();
        EiemModCopy(p.modPath, sizeof(p.modPath), path);
        EiemModCopy(p.section, sizeof(p.section), name);
      } else {
        const char *kind = _strnicmp(name.c_str(), "Mesh", 4) == 0 ? "Mesh" :
          _strnicmp(name.c_str(), "Material", 8) == 0 ? "Material" :
          _strnicmp(name.c_str(), "Texture", 7) == 0 ? "Texture" :
          _strnicmp(name.c_str(), "Skeleton", 8) == 0 ? "Skeleton" :
          _strnicmp(name.c_str(), "Physics", 7) == 0 ? "Physics" : nullptr;
        if (!kind) return fail("Unknown section: " + name);
        section = Resource; doc.resources.push_back({});
        auto &r = doc.resources.back();
        EiemModCopy(r.modPath, sizeof(r.modPath), path);
        EiemModCopy(r.section, sizeof(r.section), name);
        EiemModCopy(r.kind, sizeof(r.kind), kind);
      }
      continue;
    }
    std::string command = line.substr(0, line.find_first_of(" \t("));
    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (command == "if" || command == "else" || command == "endif") {
      if (section != Render) return fail("Conditions are only allowed inside Render");
      std::string expressionText;
      if (command == "if") {
        if (++conditionNodes > 128) return fail("Render exceeds 128 conditional nodes");
        expressionText = line.substr(2); EiemModTrim(expressionText);
        body->push_back({}); auto &s = body->back(); s.line = lineNumber;
        std::string detail;
        s.condition = EiemExpressionParser(expressionText, detail).Parse();
        if (!s.condition) return fail(detail);
        stack.push_back({body, &s, false}); body = &s.yes;
      } else if (command == "else") {
        if (stack.empty() || stack.back().hadElse) return fail("Unexpected/duplicate else");
        auto &frame = stack.back();
        std::string rest = line.substr(4); EiemModTrim(rest);
        if (rest.empty()) { frame.hadElse = true; body = &frame.branch->no; }
        else {
          if (++conditionNodes > 128) return fail("Render exceeds 128 conditional nodes");
          if (rest.size() <= 2 || _strnicmp(rest.c_str(), "if", 2) != 0 ||
              !(std::isspace((unsigned char)rest[2]) || rest[2] == '(')) return fail("Expected else or else if");
          frame.branch->no.push_back({});
          auto &s = frame.branch->no.back(); s.line = lineNumber;
          expressionText = rest.substr(2); EiemModTrim(expressionText);
          std::string detail;
          s.condition = EiemExpressionParser(expressionText, detail).Parse();
          if (!s.condition) return fail(detail);
          frame.branch = &s; body = &s.yes;
        }
      } else {
        if (line.size() != 5 || stack.empty()) return fail("Unexpected endif");
        body = stack.back().parent; stack.pop_back();
      }
      continue;
    }
    const size_t equals = line.find('=');
    if (equals == std::string::npos || section == None) return fail("Expected section and key=value");
    std::string key = line.substr(0, equals), value = line.substr(equals + 1);
    EiemModTrim(key); EiemModTrim(value);
    if (section == Constants) {
      double number = 0;
      const bool persist=key.compare(0,8,"persist ")==0;
      if(persist){key.erase(0,8);EiemModTrim(key);}
      if (!EiemVariableName(key) || !EiemNumber(value, &number) || !state.defaults.emplace(key, number).second)
        return fail("Invalid/duplicate variable: " + key);
      if(persist)state.persistent.insert(key);
      continue;
    }
    if (section == Key && EiemVariableName(key)) {
      auto &assignments = state.keys.back().assignments;
      for (const auto &a : assignments) if (a.variable == key) return fail("Duplicate key assignment: " + key);
      EiemKeyAssignment a; a.variable = key;
      size_t start = 0;
      for (;;) {
        size_t end = value.find(',', start);
        std::string token = value.substr(start, end == std::string::npos ? end : end - start);
        EiemModTrim(token); double number = 0;
        if (!EiemNumber(token, &number)) return fail("Invalid Key value: " + token);
        a.values.push_back(number);
        if (end == std::string::npos) break;
        start = end + 1;
      }
      assignments.push_back(std::move(a));
      continue;
    }
    const std::string authoredKey = key;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (section == Render && key.compare(0, 6, "shape.") == 0)
      key = "shape." + authoredKey.substr(6);
    if (section == Render && key.compare(0, 12, "shape_speed.") == 0)
      key = "shape_speed." + authoredKey.substr(12);
    if (section == UI) {
      if (!uiFields.insert(key).second) return fail("Duplicate UI field: " + key);
      auto &ui = state.uis.back();
      if (key == "path" && EiemModRelativeFilePath(value)) ui.path = value;
      else return fail("Invalid UI field: " + key);
      continue;
    }
    if (section == ShapeControl) {
      if (!shapeControlFields.insert(key).second)
        return fail("Duplicate ShapeControl field: " + key);
      auto &control = state.shapeControls.back();
      if (key == "variable" && EiemVariableName(value)) control.variable = value;
      else if (key == "label" && !value.empty() && value.size() <= 192)
        control.label = value;
      else if (key == "min" && EiemNumber(value, &control.minimum)) {}
      else if (key == "max" && EiemNumber(value, &control.maximum)) {}
      else return fail("Invalid ShapeControl field: " + key);
      continue;
    }
    if (section == Key) {
      if (key == "key") {
        if (state.keys.back().chord.vk || !EiemParseKeyChord(value, &state.keys.back().chord)) return fail("Invalid/duplicate key chord");
      } else if (key == "type" && !keyTypeSeen) {
        if (EiemModEquals(value.c_str(), "cycle"))
          state.keys.back().behavior = EiemModKeyBehavior::Cycle;
        else if (EiemModEquals(value.c_str(), "hold"))
          state.keys.back().behavior = EiemModKeyBehavior::Hold;
        else
          return fail("Key type must be cycle or hold");
        keyTypeSeen = true;
      } else if (key == "speed" && !keySpeedSeen) {
        double speed = 0;
        if (!EiemNumber(value, &speed) || !std::isfinite(speed) || speed <= 0)
          return fail("Key speed must be positive");
        state.keys.back().speed = speed;
        keySpeedSeen = true;
      }
      else if (key == "scope" && !keyScopeSeen) {
        keyScopeSeen = true;
        if (EiemModEquals(value.c_str(),"game")) state.keys.back().scope = EiemKeyScope::Game;
        else if (EiemModEquals(value.c_str(),"ui")) state.keys.back().scope = EiemKeyScope::Ui;
        else if (EiemModEquals(value.c_str(),"both")) state.keys.back().scope = EiemKeyScope::Both;
        else return fail("Key scope must be game, ui or both");
      }
      else return fail("Unsupported Key field: " + key);
      continue;
    }
    if (section == Render) {
      std::string detail;
      EiemModRule check; EiemModInitRule(&check);
      std::shared_ptr<EiemExpression> number;
      if (key.compare(0, 6, "shape.") == 0 && !value.empty()) {
        number = EiemExpressionParser(value, detail).Parse();
        if (!number) return fail(detail);
      }
      if (!EiemSetRenderField(check, key, number ? "0" : value, detail)) return fail(detail);
      if (EiemRenderSelectorKey(key)) {
        if (!stack.empty()) return fail("Render selectors must be unconditional");
        EiemSetRenderField(doc.definitions.back().selector, key, value, detail);
      } else {
        EiemModStatement s; s.line = lineNumber; s.key = key; s.value = value;
        s.number = number;
        body->push_back(std::move(s));
      }
      continue;
    }
    if (section == Prefab) {
      auto &p = doc.prefabs.back();
      if (key == "path") {
        if (value.size() >= sizeof(p.path)) return fail("Prefab path too long");
        EiemModCopy(p.path, sizeof(p.path), value);
      } else if (key.compare(0, 7, "render.") == 0) {
        int32_t index = 0;
        if (!EiemModInteger(key.substr(7), &index, 0, _countof(p.renders) - 1) || value.size() >= sizeof(p.renders[0]))
          return fail("Invalid Render reference");
        EiemModCopy(p.renders[index], sizeof(p.renders[0]), value);
        p.renderCount = (std::max)(p.renderCount, (uint32_t)index + 1);
      } else return fail("Unknown Prefab field: " + key);
      continue;
    }
    auto &r = doc.resources.back();
    auto copy = [&](auto &field) {
      if (value.size() >= sizeof(field)) return false;
      EiemModCopy(field, sizeof(field), value); return true;
    };
    if (key == "path") { if (!copy(r.path)) return fail("Resource path too long"); }
    else if (key == "source") { if (!copy(r.source)) return fail("Resource source too long"); }
    else if (key == "target.path") { if (!copy(r.targetPath)) return fail("Target path too long"); }
    else if (key == "target.asset") { if (!copy(r.targetAsset)) return fail("Target asset too long"); }
    else if (key == "linear" || key == "mipmaps") {
      bool yes = EiemModEquals(value.c_str(), "true") || value == "1";
      bool no = EiemModEquals(value.c_str(), "false") || value == "0";
      if (!yes && !no) return fail("Invalid boolean: " + key);
      if (key == "linear") r.textureLinear = yes; else r.textureMipmaps = yes;
    } else if (key == "filter") {
      if (!EiemModInteger(value, &r.textureFilter, 0, 2)) return fail("Invalid filter");
    } else if (key == "wrap") {
      if (!EiemModInteger(value, &r.textureWrap, 0, 3)) return fail("Invalid wrap");
    } else if (key == "aniso") {
      if (!EiemModInteger(value, &r.textureAniso, 0, 16)) return fail("Invalid aniso");
    } else if (key == "mip_bias") {
      double number = 0;
      if (!EiemNumber(value, &number) || !std::isfinite((float)number)) return fail("Invalid mip_bias");
      r.textureMipBias = (float)number;
    }
    // Resource-only offline metadata remains inert, never interpreted as commands.
  }
  if (input.bad()) return fail("Input read failed");
  if (!finishSection()) return false;
  std::string detail;
  if (!EiemValidateModDocument(doc, detail)) { if (error) *error = detail; return false; }
  EiemAppendModDocument(output, std::move(doc));
  return true;
}

static bool EiemPrepareModPhysics(EiemModProgram &doc,std::string &error) {
  std::unordered_map<std::string,std::shared_ptr<const EiemPhysicsAsset>> snapshots;
  for (auto &resource:doc.resources) if (EiemModEquals(resource.kind,"Physics")) {
    std::filesystem::path path;
    auto base=std::filesystem::u8path(resource.modPath).parent_path();
    if (base.empty()) base=".";
    std::shared_ptr<const EiemPhysicsAsset> asset;
    if (!EiemPhysicsResolveFile(base,resource.path,path,error)) return false;
    const auto key=EiemModIdentifier("",path.u8string().c_str());
    const auto found=snapshots.find(key);
    if (found!=snapshots.end()) asset=found->second;
    else if (!EiemLoadPhysicsAsset(path,asset,error)) {
      error=std::string(resource.section)+": "+error; return false;
    }
    snapshots[key]=asset; resource.physicsAsset=std::move(asset);
  }
  // A Render's physical rig remains stable across its conditional branches.
  // Validate inactive references too, before a later key can select them.
  for (const auto &definition:doc.definitions) {
    std::vector<const EiemModResource *> physics, skeletons;
    EiemVisitStatements(definition.statements,[&](const EiemModStatement &s) {
      if (s.value.empty() || (s.key!="physics" && s.key!="skeleton")) return;
      for (const auto &r:doc.resources) if (EiemModEquals(r.section,s.value.c_str())) {
        (s.key=="physics" ? physics:skeletons).push_back(&r); break;
      }
    });
    if (physics.empty()) continue;
    const auto &expected=physics.front()->physicsAsset->skeleton;
    for (const auto *resource:physics)
      if (!EiemSamePhysicsSkeleton(expected,resource->physicsAsset->skeleton)) {
        error=std::string(definition.selector.section)+": Physics branches require one shared Skeleton"; return false;
      }
    for (const auto *resource:skeletons) {
      std::filesystem::path path; EiemSkeletonDocument skeleton;
      auto base=std::filesystem::u8path(resource->modPath).parent_path();
      if (base.empty()) base=".";
      if (!EiemPhysicsResolveFile(base,resource->path,path,error) || !EiemLoadPhysicsSkeleton(path,skeleton,error)) return false;
      if (!EiemSamePhysicsSkeleton(expected,skeleton)) {
        error=std::string(definition.selector.section)+": Render and Physics Skeleton data differ"; return false;
      }
    }
  }
  error.clear(); return true;
}

static bool EiemModParseFile(const char *path, EiemModProgram &output, std::string *error = nullptr) {
  if (error) error->clear();
  auto input = EiemOpenUtf8Input(path);
  if (!input) {
    // Missing mod.ini is optional. A present but unreadable path must reject
    // the candidate; otherwise F10 silently unloads that Mod.
    try {
      std::error_code diskError;
      const auto status = path && path[0]
          ? std::filesystem::status(std::filesystem::u8path(path), diskError)
          : std::filesystem::file_status{};
      if (error && ((diskError && diskError != std::errc::no_such_file_or_directory) ||
                    (!diskError && std::filesystem::exists(status))))
        *error = "mod.ini exists but cannot be read";
    } catch (const std::exception &) {
      if (error) *error = "mod.ini path cannot be inspected";
    }
    return false;
  }
  EiemModProgram doc; std::string detail;
  if (!EiemModParseStream(input,path,doc,error)) return false;
  if (!EiemPrepareModPhysics(doc,detail)) { if (error) *error=detail; return false; }
  EiemAppendModDocument(output,std::move(doc)); return true;
}

// Apply one key event. Cycle keys advance once; hold keys move each assigned
// variable toward its single target. The caller supplies elapsed wall time for
// hold ticks, while an edge event receives one default 20 ms step.
static std::vector<std::string> EiemApplyModKey(
    EiemModProgram &program, EiemKeyChord chord, bool uiFocus = false,
    const char *modPath = nullptr, const char *keySection = nullptr,
    bool holdTick = false, double holdSeconds = 0.02) {
  std::vector<std::string> changed;
  if (!std::isfinite(holdSeconds) || holdSeconds <= 0) holdSeconds = 0.02;
  holdSeconds = (std::min)(holdSeconds, 0.25);
  for (auto &state : program.states) {
    if (modPath && (!modPath[0] || !EiemModEquals(state.path.c_str(), modPath)))
      continue;
    bool dirty = false;
    for (const auto &key : state.keys) {
      if (keySection && (!keySection[0] || key.section != keySection))
        continue;
      if (!(key.chord == chord) || !EiemKeyInScope(key.scope,uiFocus)) continue;
      if (key.behavior == EiemModKeyBehavior::Hold) {
        // A hold edge and every subsequent poll use the same target movement.
        // The target may be either side of the current value, so increase and
        // decrease keys need no separate direction flag.
        for (const auto &a : key.assignments) {
          double &value = state.variables.at(a.variable);
          const double target = a.values.front();
          const double distance = key.speed * holdSeconds;
          const double next = value < target
              ? (std::min)(value + distance, target)
              : (std::max)(value - distance, target);
          dirty = dirty || value != next;
          value = next;
        }
      } else if (!holdTick) {
        size_t count = key.assignments.front().values.size(), selected = 0;
        for (size_t i = 0; i < count; ++i) {
          bool matches = true;
          for (const auto &a : key.assignments)
            if (state.variables.at(a.variable) != a.values[i]) { matches = false; break; }
          if (matches) { selected = (i + 1) % count; break; }
        }
        for (const auto &a : key.assignments) {
          double &value = state.variables.at(a.variable);
          dirty = dirty || value != a.values[selected];
          value = a.values[selected];
        }
      }
    }
    if (dirty) changed.push_back(state.path);
  }
  if (!changed.empty()) EiemEvaluateModProgram(program);
  return changed;
}

// Compatibility wrapper for callers/tests that explicitly request a cycle.
static std::vector<std::string> EiemCycleModKey(
    EiemModProgram &program, EiemKeyChord chord, bool uiFocus = false,
    const char *modPath = nullptr, const char *keySection = nullptr) {
  return EiemApplyModKey(program, chord, uiFocus, modPath, keySection,
                         false, 0.02);
}
