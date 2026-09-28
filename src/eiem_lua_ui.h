#pragma once
#include "eiem_mod_document.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <filesystem>
#include <functional>

// Each instance owns one script. No Unity objects or application callbacks are
// exported to Lua. The caller publishes successful frame writes as one event.
class EiemLuaUi {
  lua_State *vm = nullptr;
  size_t memory = 0;
  int budget = 0, drawRef = LUA_NOREF;
  bool drawing = false;
  int colors = 0, styles = 0;
  std::vector<char> scopes;
  EiemVariables variables, pending, defaults;
  std::string identity, sourceName;
  enum Op { Begin, End, BeginChild, EndChild, Text, Button, Checkbox, SliderFloat,
    DragFloat, InputFloat, ColorEdit4, SameLine, Separator, Spacing, NewLine, Dummy,
    SetNextWindowSize, SetNextWindowPos, SetNextWindowBgAlpha, SetNextItemWidth,
    PushStyleColor, PopStyleColor, PushStyleVar, PopStyleVar, PushID, PopID,
    BeginDisabled, EndDisabled, BeginGroup, EndGroup, BeginTable, EndTable,
    TableNextRow, TableNextColumn, CollapsingHeader, RadioButton,
    GetItemRectMin, GetItemRectMax, IsItemHovered, SetTooltip, Get, Set, Default };
  static void *Allocate(void *ud, void *ptr, size_t oldSize, size_t newSize) {
    auto &self = *static_cast<EiemLuaUi *>(ud);
    if (!ptr) oldSize = 0;
    if (!newSize) { self.memory -= oldSize; free(ptr); return nullptr; }
    constexpr size_t limit = 16 * 1024 * 1024;
    if (newSize > limit || self.memory - oldSize > limit - newSize) return nullptr;
    void *next = realloc(ptr, newSize);
    if (next) self.memory = self.memory - oldSize + newSize;
    return next;
  }
  static EiemLuaUi &Self(lua_State *L) { return **static_cast<EiemLuaUi **>(lua_getextraspace(L)); }
  static void InstructionHook(lua_State *L, lua_Debug *) {
    if (--Self(L).budget <= 0) luaL_error(L, "UI instruction budget exceeded");
  }
  static int Traceback(lua_State *L) {
    luaL_traceback(L, L, lua_tostring(L, 1), 1); return 1;
  }
  void PopScope(char expected) {
    if (scopes.empty() || scopes.back() != expected) luaL_error(vm, "Unbalanced ImGui scope");
    scopes.pop_back();
  }
  static float Number(lua_State *L, int index, float fallback = 0) {
    const double value = luaL_optnumber(L, index, fallback);
    if (!std::isfinite((float)value)) luaL_error(L, "Expected finite float");
    return (float)value;
  }
  static int Dispatch(lua_State *L) {
    auto &s = Self(L);
    Op op = (Op)lua_tointeger(L, lua_upvalueindex(1));
    if (!s.drawing) return luaL_error(L, "UI APIs are only available inside the draw function");
    auto boolean = [&](bool value) { lua_pushboolean(L, value); return 1; };
    auto changed = [&](bool yes, float value) { lua_pushboolean(L, yes); lua_pushnumber(L, value); return 2; };
    if (op == Get || op == Set || op == Default) {
      const std::string name = luaL_checkstring(L, 1);
      if (!s.variables.count(name)) return luaL_error(L, "Undeclared Mod variable: %s", name.c_str());
      if(op==Default) {
        if(!s.defaults.count(name))return luaL_error(L,"Author default unavailable: %s",name.c_str());
        lua_pushnumber(L,s.defaults.at(name));return 1;
      }
      if (op == Get) {
        lua_pushnumber(L, s.pending.count(name) ? s.pending.at(name) : s.variables.at(name)); return 1;
      }
      double value = luaL_checknumber(L, 2);
      if (!std::isfinite(value)) return luaL_error(L, "Non-finite Mod variable");
      s.pending[name] = value; return 0;
    }
    const bool setup = op == Begin || op == SetNextWindowSize || op == SetNextWindowPos ||
      op == SetNextWindowBgAlpha || op == PushStyleColor || op == PopStyleColor ||
      op == PushStyleVar || op == PopStyleVar;
    if (!setup && s.scopes.empty()) return luaL_error(L, "Control needs imgui.Begin");
    switch (op) {
    case Begin: {
      std::string title = s.WindowLabel(luaL_checkstring(L, 1));
      bool hasOpen = !lua_isnoneornil(L,2), open = true;
      if (hasOpen) { luaL_checktype(L,2,LUA_TBOOLEAN); open = lua_toboolean(L,2) != 0; }
      int flags = (int)luaL_optinteger(L, 3, 0);
      // Internal child/popup flags cannot be supplied by a public root window.
      if (flags & ~((1 << 19) - 1)) return luaL_error(L, "Unsupported window flags");
      s.scopes.push_back('w');
      lua_pushboolean(L,ImGui::Begin(title.c_str(), hasOpen ? &open : nullptr, flags));
      if (hasOpen) lua_pushboolean(L,open); else lua_pushnil(L);
      return 2;
    }
    case End: s.PopScope('w'); ImGui::End(); break;
    case BeginChild: {
      const char *id = luaL_checkstring(L, 1);
      float w = Number(L, 2), h = Number(L, 3);
      bool border = lua_toboolean(L, 4) != 0;
      s.scopes.push_back('c');
      return boolean(ImGui::BeginChild(id, {w,h}, border ? ImGuiChildFlags_Borders : 0));
    }
    case EndChild: s.PopScope('c'); ImGui::EndChild(); break;
    case Text: ImGui::TextUnformatted(luaL_checkstring(L, 1)); break;
    case Button: return boolean(ImGui::Button(luaL_checkstring(L, 1), {Number(L, 2), Number(L, 3)}));
    case RadioButton: return boolean(ImGui::RadioButton(luaL_checkstring(L, 1), lua_toboolean(L, 2) != 0));
    case Checkbox: {
      const char *label = luaL_checkstring(L, 1); bool value = lua_toboolean(L, 2) != 0;
      bool yes = ImGui::Checkbox(label, &value); lua_pushboolean(L, yes); lua_pushboolean(L, value); return 2;
    }
    case SliderFloat: {
      const char *label = luaL_checkstring(L, 1); float value = Number(L, 2), lo = Number(L, 3), hi = Number(L, 4, 1);
      if (!(lo < hi)) return luaL_error(L, "Slider minimum must be less than maximum");
      bool yes = ImGui::SliderFloat(label, &value, lo, hi, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      return changed(yes, value);
    }
    case DragFloat: {
      const char *label = luaL_checkstring(L, 1); float value = Number(L, 2);
      bool yes = ImGui::DragFloat(label, &value, Number(L, 3, .01f), Number(L, 4), Number(L, 5));
      return changed(yes, value);
    }
    case InputFloat: {
      const char *label = luaL_checkstring(L, 1); float value = Number(L, 2);
      bool yes = ImGui::InputFloat(label, &value, Number(L, 3)); return changed(yes, value);
    }
    case ColorEdit4: {
      const char *label = luaL_checkstring(L, 1);
      float color[4] = {Number(L,2), Number(L,3), Number(L,4), Number(L,5,1)};
      lua_pushboolean(L, ImGui::ColorEdit4(label, color));
      for (float value : color) lua_pushnumber(L, value); return 5;
    }
    case SameLine: ImGui::SameLine(Number(L,1), Number(L,2,-1)); break;
    case Separator: ImGui::Separator(); break;
    case Spacing: ImGui::Spacing(); break;
    case NewLine: ImGui::NewLine(); break;
    case Dummy: ImGui::Dummy({Number(L,1),Number(L,2)}); break;
    case SetNextWindowSize: case SetNextWindowPos: {
      float x = Number(L,1), y = Number(L,2); int cond = (int)luaL_optinteger(L,3,0);
      if (cond != 0 && cond != ImGuiCond_Always && cond != ImGuiCond_Once &&
          cond != ImGuiCond_FirstUseEver && cond != ImGuiCond_Appearing) return luaL_error(L,"Invalid ImGui condition");
      if (op == SetNextWindowSize) ImGui::SetNextWindowSize({x,y},cond);
      else ImGui::SetNextWindowPos({x,y},cond);
      break;
    }
    case SetNextWindowBgAlpha: ImGui::SetNextWindowBgAlpha(Number(L,1,1)); break;
    case SetNextItemWidth: ImGui::SetNextItemWidth(Number(L,1)); break;
    case PushStyleColor: {
      int index = (int)luaL_checkinteger(L,1);
      if (index < 0 || index >= ImGuiCol_COUNT) return luaL_error(L,"Invalid style color");
      ImGui::PushStyleColor(index, {Number(L,2),Number(L,3),Number(L,4),Number(L,5,1)}); ++s.colors; break;
    }
    case PopStyleColor: {
      int n = (int)luaL_optinteger(L,1,1);
      if (n < 0 || n > s.colors) return luaL_error(L,"Unbalanced style color");
      ImGui::PopStyleColor(n); s.colors -= n; break;
    }
    case PushStyleVar: {
      int index = (int)luaL_checkinteger(L,1);
      if (index < 0 || index >= ImGuiStyleVar_COUNT) return luaL_error(L,"Invalid style variable");
      const auto *info = ImGui::GetStyleVarInfo(index);
      if (info->Count == 2) ImGui::PushStyleVar(index, {Number(L,2),Number(L,3)});
      else ImGui::PushStyleVar(index, Number(L,2));
      ++s.styles; break;
    }
    case PopStyleVar: {
      int n = (int)luaL_optinteger(L,1,1);
      if (n < 0 || n > s.styles) return luaL_error(L,"Unbalanced style variable");
      ImGui::PopStyleVar(n); s.styles -= n; break;
    }
    case PushID: ImGui::PushID(luaL_checkstring(L,1)); s.scopes.push_back('i'); break;
    case PopID: s.PopScope('i'); ImGui::PopID(); break;
    case BeginDisabled: ImGui::BeginDisabled(lua_gettop(L) == 0 || lua_toboolean(L,1)); s.scopes.push_back('d'); break;
    case EndDisabled: s.PopScope('d'); ImGui::EndDisabled(); break;
    case BeginGroup: ImGui::BeginGroup(); s.scopes.push_back('g'); break;
    case EndGroup: s.PopScope('g'); ImGui::EndGroup(); break;
    case BeginTable: {
      const char *id = luaL_checkstring(L,1); int columns = (int)luaL_checkinteger(L,2);
      if (columns < 1 || columns > 64) return luaL_error(L,"Table columns must be 1..64");
      bool visible = ImGui::BeginTable(id, columns);
      if (visible) s.scopes.push_back('t'); return boolean(visible);
    }
    case EndTable: s.PopScope('t'); ImGui::EndTable(); break;
    case TableNextRow: case TableNextColumn:
      if (!ImGui::GetCurrentContext()->CurrentTable) return luaL_error(L,"No active table");
      if (op == TableNextRow) ImGui::TableNextRow(); else return boolean(ImGui::TableNextColumn()); break;
    case CollapsingHeader: return boolean(ImGui::CollapsingHeader(luaL_checkstring(L,1)));
    case GetItemRectMin: case GetItemRectMax: {
      ImVec2 pos = op == GetItemRectMin ? ImGui::GetItemRectMin() : ImGui::GetItemRectMax();
      lua_pushnumber(L,pos.x); lua_pushnumber(L,pos.y); return 2;
    }
    case IsItemHovered: return boolean(ImGui::IsItemHovered());
    case SetTooltip: ImGui::SetTooltip("%s",luaL_checkstring(L,1)); break;
    default: return luaL_error(L,"Unknown UI API");
    }
    return 0;
  }
  static int Initialize(lua_State *L) {
    const struct { const char *name; lua_CFunction function; } libraries[] = {
      {LUA_GNAME,luaopen_base}, {LUA_MATHLIBNAME,luaopen_math}, {LUA_STRLIBNAME,luaopen_string},
      {LUA_TABLIBNAME,luaopen_table}, {LUA_UTF8LIBNAME,luaopen_utf8}
    };
    for (const auto &lib : libraries) { luaL_requiref(L, lib.name, lib.function, 1); lua_pop(L,1); }
    // Don't expose Lua-side pcall/coroutines: they could repeatedly catch the
    // instruction hook error. Only the host owns protected execution.
    for (const char *name : {"dofile","loadfile","load","collectgarbage","pcall","xpcall","print"}) {
      lua_pushnil(L); lua_setglobal(L,name);
    }
    const struct { const char *name; Op op; } functions[] = {
#define EIEM_UI_API(name) {#name,name}
      EIEM_UI_API(Begin), EIEM_UI_API(End), EIEM_UI_API(BeginChild), EIEM_UI_API(EndChild),
      EIEM_UI_API(Text), EIEM_UI_API(Button), EIEM_UI_API(Checkbox), EIEM_UI_API(SliderFloat),
      EIEM_UI_API(DragFloat), EIEM_UI_API(InputFloat), EIEM_UI_API(ColorEdit4), EIEM_UI_API(SameLine),
      EIEM_UI_API(Separator), EIEM_UI_API(Spacing), EIEM_UI_API(NewLine), EIEM_UI_API(Dummy),
      EIEM_UI_API(SetNextWindowSize), EIEM_UI_API(SetNextWindowPos), EIEM_UI_API(SetNextWindowBgAlpha),
      EIEM_UI_API(SetNextItemWidth), EIEM_UI_API(PushStyleColor), EIEM_UI_API(PopStyleColor),
      EIEM_UI_API(PushStyleVar), EIEM_UI_API(PopStyleVar), EIEM_UI_API(PushID), EIEM_UI_API(PopID),
      EIEM_UI_API(BeginDisabled), EIEM_UI_API(EndDisabled), EIEM_UI_API(BeginGroup), EIEM_UI_API(EndGroup),
      EIEM_UI_API(BeginTable), EIEM_UI_API(EndTable), EIEM_UI_API(TableNextRow), EIEM_UI_API(TableNextColumn),
      EIEM_UI_API(CollapsingHeader), EIEM_UI_API(RadioButton), EIEM_UI_API(GetItemRectMin),
      EIEM_UI_API(GetItemRectMax), EIEM_UI_API(IsItemHovered), EIEM_UI_API(SetTooltip)
#undef EIEM_UI_API
    };
    lua_newtable(L);
    for (const auto &f : functions) { lua_pushinteger(L,f.op); lua_pushcclosure(L,Dispatch,1); lua_setfield(L,-2,f.name); }
    auto constants = [&](const char *table, std::initializer_list<std::pair<const char *,int>> fields) {
      lua_newtable(L);
      for (const auto &f : fields) { lua_pushinteger(L,f.second); lua_setfield(L,-2,f.first); }
      lua_setfield(L,-2,table);
    };
    constants("Col", {{"Text",ImGuiCol_Text},{"WindowBg",ImGuiCol_WindowBg},{"Button",ImGuiCol_Button},
      {"ButtonHovered",ImGuiCol_ButtonHovered},{"ButtonActive",ImGuiCol_ButtonActive},{"FrameBg",ImGuiCol_FrameBg}});
    constants("StyleVar", {{"Alpha",ImGuiStyleVar_Alpha},{"WindowPadding",ImGuiStyleVar_WindowPadding},
      {"WindowRounding",ImGuiStyleVar_WindowRounding},{"FrameRounding",ImGuiStyleVar_FrameRounding},
      {"FramePadding",ImGuiStyleVar_FramePadding},{"ItemSpacing",ImGuiStyleVar_ItemSpacing}});
    constants("WindowFlags", {{"NoTitleBar",ImGuiWindowFlags_NoTitleBar},{"NoResize",ImGuiWindowFlags_NoResize},
      {"NoMove",ImGuiWindowFlags_NoMove},{"NoBackground",ImGuiWindowFlags_NoBackground},
      {"AlwaysAutoResize",ImGuiWindowFlags_AlwaysAutoResize}});
    constants("Cond", {{"Always",ImGuiCond_Always},{"Once",ImGuiCond_Once},
      {"FirstUseEver",ImGuiCond_FirstUseEver},{"Appearing",ImGuiCond_Appearing}});
    lua_setglobal(L,"imgui"); lua_newtable(L);
    lua_pushinteger(L,Get); lua_pushcclosure(L,Dispatch,1); lua_setfield(L,-2,"get");
    lua_pushinteger(L,Set); lua_pushcclosure(L,Dispatch,1); lua_setfield(L,-2,"set");
    lua_pushinteger(L,Default); lua_pushcclosure(L,Dispatch,1); lua_setfield(L,-2,"default");
    lua_setglobal(L,"mod"); return 0;
  }
  bool Call(int arguments, int results) {
    budget = 200;
    const int function = lua_gettop(vm) - arguments;
    lua_pushcfunction(vm, Traceback); lua_insert(vm,function);
    int result = lua_pcall(vm, arguments, results, function);
    if (result != LUA_OK) { const char *message = lua_tostring(vm,-1); error = message ? message : "Lua error"; lua_pop(vm,1); }
    lua_remove(vm,function); return result == LUA_OK;
  }
public:
  std::string error;
  explicit EiemLuaUi(std::string id) : identity(std::move(id)) {}
  EiemLuaUi(const EiemLuaUi &) = delete;
  EiemLuaUi &operator=(const EiemLuaUi &) = delete;
  ~EiemLuaUi() { if (vm) lua_close(vm); }
  std::string WindowLabel(const std::string &title) const {
    // Keep native ### stable-title semantics without letting it discard the
    // Mod namespace. Multiple windows per script have independent native IDs.
    char id[16]; snprintf(id,sizeof(id),"%08X",ImHashStr(title.c_str(),0,ImHashStr(identity.c_str())));
    return title.substr(0,title.find("##")) + "###EFF-" + id;
  }
  bool Load(const std::string &source, const std::string &filename) {
    if (vm) { lua_close(vm); vm = nullptr; }
    memory = 0; error.clear(); drawRef = LUA_NOREF; sourceName = filename;
    if (source.size() > 256 * 1024) { error = "UI script exceeds 256 KiB"; return false; }
    vm = lua_newstate(Allocate,this);
    if (!vm) { error = "Cannot allocate Lua state"; return false; }
    *static_cast<EiemLuaUi **>(lua_getextraspace(vm)) = this;
    lua_sethook(vm,InstructionHook,LUA_MASKCOUNT,1000);
    lua_pushcfunction(vm,Initialize);
    if (!Call(0,0)) return false;
    if (luaL_loadbufferx(vm,source.data(),source.size(),("@"+filename).c_str(),"t") != LUA_OK) {
      error = lua_tostring(vm,-1); lua_pop(vm,1); return false;
    }
    if (!Call(0,1)) return false;
    if (!lua_isfunction(vm,-1)) { error = "UI script must return a draw function"; lua_pop(vm,1); return false; }
    drawRef = luaL_ref(vm,LUA_REGISTRYINDEX); return true;
  }
  bool LoadFile(const std::string &modIni, const std::string &relative) {
    try {
      namespace fs = std::filesystem;
      if (!EiemModRelativeFilePath(relative)) throw std::runtime_error("Invalid UI relative path");
      auto root = fs::canonical(fs::absolute(fs::u8path(modIni)).parent_path());
      auto path = fs::canonical(root / fs::u8path(relative));
      auto part = path.begin();
      for (const auto &segment : root) {
        if (part == path.end() || _wcsicmp(segment.c_str(),part->c_str()))
          throw std::runtime_error("UI path escapes Mod directory");
        ++part;
      }
      if (!fs::is_regular_file(path) || fs::file_size(path) > 256 * 1024)
        throw std::runtime_error("UI script is not a regular file <= 256 KiB");
      std::ifstream input(path,std::ios::binary);
      if (!input) throw std::runtime_error("Cannot open UI script");
      std::string source((std::istreambuf_iterator<char>(input)),{});
      return Load(source,path.u8string());
    } catch (const std::exception &e) { error = relative + ": " + e.what(); return false; }
  }
  bool Draw(const EiemVariables &current, EiemVariables &writes, const EiemVariables &authorDefaults = {}) {
    writes.clear(); if (!error.empty() || drawRef == LUA_NOREF) return false;
    variables = current; defaults=authorDefaults;pending.clear(); scopes.clear(); colors = styles = 0;
    ImGuiErrorRecoveryState base, after;
    ImGui::ErrorRecoveryStoreState(&base);
    auto *context = ImGui::GetCurrentContext();
    auto oldCallback = context->ErrorCallback; auto oldData = context->ErrorCallbackUserData;
    context->ErrorCallbackUserData = this;
    context->ErrorCallback = [](ImGuiContext *, void *ud, const char *message) {
      auto &ui = *static_cast<EiemLuaUi *>(ud); if (ui.error.empty()) ui.error = message;
    };
    auto &io = ImGui::GetIO(); bool oldAssert = io.ConfigErrorRecoveryEnableAssert;
    bool oldTooltip = io.ConfigErrorRecoveryEnableTooltip;
    io.ConfigErrorRecoveryEnableAssert = false; io.ConfigErrorRecoveryEnableTooltip = false;
    drawing = true; lua_rawgeti(vm,LUA_REGISTRYINDEX,drawRef);
    bool ok = Call(0,0); drawing = false;
    ImGui::ErrorRecoveryStoreState(&after);
    if (!scopes.empty() || colors || styles || memcmp(&base,&after,sizeof(base))) {
      if (error.empty()) error = "Unbalanced ImGui stacks at end of UI frame";
      ok = false;
    }
    ImGui::ErrorRecoveryTryToRecoverState(&base);
    // SetNextWindow* data must not leak to another Mod, even after an error.
    context->NextWindowData.ClearFlags(); context->NextItemData.ClearFlags();
    context->ErrorCallback = oldCallback; context->ErrorCallbackUserData = oldData;
    io.ConfigErrorRecoveryEnableAssert = oldAssert; io.ConfigErrorRecoveryEnableTooltip = oldTooltip;
    if (ok && error.empty()) { writes.swap(pending); return true; }
    if (!error.empty() && error.find(sourceName) == std::string::npos) error = sourceName + ": " + error;
    pending.clear(); return false;
  }
};
