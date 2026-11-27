#pragma once
#include "eiem_sampled_animation.h"
#include "eiem_expression.h"
#include "eiem_custom_gui.h"
#include "eiem_gui_state.h"
#include <cstring>
#include <cfloat>
#include <map>
#include <vector>
#include <stdexcept>
#include "../deps/imgui/imgui.h"
#include "../deps/imgui/imgui_internal.h"
#include "../deps/lua/lua.h"
#include "../deps/lua/lauxlib.h"
#include "../deps/lua/lualib.h"
#include <filesystem>
#include <functional>

// Each instance owns one script. No Unity objects or application callbacks are
// exported to Lua. The caller publishes successful frame writes as one event.
class EiemLuaUi {
public:
  struct ImageInfo { ImTextureID id=ImTextureID_Invalid;float width=0,height=0;const char* error=nullptr; };
  using ImageLoader=std::function<ImageInfo(const char*,ImVec2)>;
  using Playback=std::map<std::string,EiemSampledAnimation::Playback>;
private:
  lua_State *vm = nullptr;
  size_t memory = 0;
  int budget = 0, drawRef = LUA_NOREF;
  unsigned calls=0;
  bool drawing = false;
  int colors = 0, styles = 0;
  std::vector<char> scopes;
  const EiemVariables *variables=nullptr, *defaults=nullptr;
  EiemVariables pending;
  EiemCustomGui::Requests requests;
  const ImageLoader* images=nullptr;
  const Playback* playback=nullptr;
  EiemGuiState* guiState;
  uint64_t session;
  std::string identity, sourceName;
  enum Op { Begin, End, BeginChild, EndChild, Text, Button, Checkbox, SliderFloat,
    DragFloat, InputFloat, ColorEdit4, SameLine, Separator, Spacing, NewLine, Dummy,
    SetNextWindowSize, SetNextWindowPos, SetNextWindowBgAlpha, SetNextItemWidth,
    PushStyleColor, PopStyleColor, PushStyleVar, PopStyleVar, PushID, PopID,
    BeginDisabled, EndDisabled, BeginGroup, EndGroup, BeginTable, EndTable,
    TableNextRow, TableNextColumn, CollapsingHeader, RadioButton,
    GetItemRectMin, GetItemRectMax, IsItemHovered, SetTooltip,
    BeginTabBar, EndTabBar, BeginTabItem, EndTabItem, BeginCombo, EndCombo, Selectable,
    ProgressBar, Image, ImageButton, DrawListAddImage, GetWindowContentRect, TextWrapped, GetContentRegionAvail,
    Get, Set, Default, Invoke, Trigger, Play, Pause, Stop, AnimationState };
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
  void PushScope(char type) {
    if(scopes.size()>=64)luaL_error(vm,"ImGui nesting exceeds 64");
    scopes.push_back(type);
  }
  void Queue(EiemCustomGui::Operation operation,const std::string& name,double value=0) {
    if(requests.size()>=64)luaL_error(vm,"Too many Mod operations in one GUI frame");
    requests.push_back({operation,name,value});
  }
  static const char* ImagePath(lua_State* L,int index) {
    size_t length=0;
    const char* path=luaL_checklstring(L,index,&length);
    if(!length||length>512||strlen(path)!=length)luaL_error(L,"Invalid GUI image path");
    return path;
  }
  ImageInfo LoadImage(const char* path,ImVec2 sampleSize) {
    try{return (*images)(path,sampleSize);}
    catch(const std::exception& e){error=e.what();return {};}
  }
  static float SampleScale(float extent,float uvExtent) {
    // Coordinates outside the largest supported texture do not need more detail.
    return ImMin(4096.f,extent/std::abs(uvExtent));
  }
  void Unwind() {
    while(!scopes.empty()) {
      char scope=scopes.back();scopes.pop_back();
      switch(scope) {
      case 'w': ImGui::End();break;
      case 'c': ImGui::EndChild();break;
      case 'i': ImGui::PopID();break;
      case 'd': ImGui::EndDisabled();break;
      case 'g': ImGui::EndGroup();break;
      case 't': ImGui::EndTable();break;
      case 'b': ImGui::EndTabBar();break;
      case 'a': ImGui::EndTabItem();break;
      case 'o': ImGui::PopID();ImGui::EndCombo();break;
      }
    }
  }
  static int Dispatch(lua_State *L) {
    auto &s = Self(L);
    Op op = (Op)lua_tointeger(L, lua_upvalueindex(1));
    if (!s.drawing) return luaL_error(L, "UI APIs are only available inside the draw function");
    if (++s.calls>1024) return luaL_error(L,"GUI call budget exceeded");
    auto boolean = [&](bool value) { lua_pushboolean(L, value); return 1; };
    auto changed = [&](bool yes, float value) { lua_pushboolean(L, yes); lua_pushnumber(L, value); return 2; };
    if (op == Get || op == Set || op == Default) {
      const std::string name = luaL_checkstring(L, 1);
      if (!s.variables->count(name)) return luaL_error(L, "Undeclared Mod variable: %s", name.c_str());
      if(op==Default) {
        if(!s.defaults->count(name))return luaL_error(L,"Author default unavailable: %s",name.c_str());
        lua_pushnumber(L,s.defaults->at(name));return 1;
      }
      if (op == Get) {
        lua_pushnumber(L, s.pending.count(name) ? s.pending.at(name) : s.variables->at(name)); return 1;
      }
      double value = luaL_checknumber(L, 2);
      if (!std::isfinite(value)) return luaL_error(L, "Non-finite Mod variable");
      s.Queue(EiemCustomGui::Operation::Set,name,value);
      s.pending[name] = value; return 0;
    }
    if(op>=Invoke&&op<=Stop) {
      using Request=EiemCustomGui::Operation;
      const Request ops[]={Request::Invoke,Request::Trigger,Request::Play,Request::Pause,Request::Stop};
      size_t size=0;const char* name=luaL_checklstring(L,1,&size);
      if(!size||size>256||strlen(name)!=size)return luaL_error(L,"Invalid Mod operation target");
      s.Queue(ops[op-Invoke],name);
      return 0;
    }
    if(op==AnimationState) {
      const char* name=luaL_checkstring(L,1);
      auto clip=s.playback->find(name);
      if(clip==s.playback->end())return luaL_error(L,"Unknown Mod animation: %s",name);
      lua_pushstring(L,!clip->second.playing?"stopped":clip->second.paused?"paused":"playing");
      lua_pushnumber(L,clip->second.time);
      return 2;
    }
    const bool setup = op == Begin || op == SetNextWindowSize || op == SetNextWindowPos ||
      op == SetNextWindowBgAlpha || op == PushStyleColor || op == PopStyleColor ||
      op == PushStyleVar || op == PopStyleVar;
    if (!setup && s.scopes.empty()) return luaL_error(L, "Control needs imgui.Begin");
    switch (op) {
    case Begin: {
      if(!s.scopes.empty())return luaL_error(L,"Nested root window");
      std::string title = s.WindowLabel(luaL_checkstring(L, 1));
      bool hasOpen = !lua_isnoneornil(L,2), open = true;
      if (hasOpen) { luaL_checktype(L,2,LUA_TBOOLEAN); open = lua_toboolean(L,2) != 0; }
      int flags = (int)luaL_optinteger(L, 3, 0);
      // Internal child/popup flags cannot be supplied by a public root window.
      if (flags & ~((1 << 19) - 1)) return luaL_error(L, "Unsupported window flags");
      s.PushScope('w');
      lua_pushboolean(L,ImGui::Begin(title.c_str(),hasOpen?&open:nullptr,flags|ImGuiWindowFlags_NoSavedSettings));
      if(s.guiState)s.guiState->Window(ImGui::GetCurrentWindowRead(),s.session);
      if(hasOpen&&!open)s.closeRequested=true;
      if (hasOpen) lua_pushboolean(L,open); else lua_pushnil(L);
      return 2;
    }
    case End: s.PopScope('w'); ImGui::End(); break;
    case BeginChild: {
      const char *id = luaL_checkstring(L, 1);
      float w = Number(L, 2), h = Number(L, 3);
      bool border = lua_toboolean(L, 4) != 0;
      s.PushScope('c');
      const bool visible=ImGui::BeginChild(id,{w,h},border?ImGuiChildFlags_Borders:0);
      if(s.guiState)s.guiState->Window(ImGui::GetCurrentWindowRead(),s.session);
      return boolean(visible);
    }
    case EndChild: s.PopScope('c'); ImGui::EndChild(); break;
    case Text: ImGui::TextUnformatted(luaL_checkstring(L, 1)); break;
    case TextWrapped: ImGui::TextWrapped("%s",luaL_checkstring(L,1));break;
    case Button: return boolean(ImGui::Button(luaL_checkstring(L, 1), {Number(L, 2), Number(L, 3)}));
    case RadioButton: return boolean(ImGui::RadioButton(luaL_checkstring(L, 1), lua_toboolean(L, 2) != 0));
    case Checkbox: {
      const char *label = luaL_checkstring(L, 1); bool value = lua_toboolean(L, 2) != 0;
      bool yes = ImGui::Checkbox(label, &value); lua_pushboolean(L, yes); lua_pushboolean(L, value); return 2;
    }
    case SliderFloat: {
      const char *label = luaL_checkstring(L, 1); float value = Number(L, 2), lo = Number(L, 3), hi = Number(L, 4, 1);
      if (!(lo < hi) || lo < -FLT_MAX/2 || hi > FLT_MAX/2) return luaL_error(L, "Invalid slider range");
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
    case PushID: { const char* id=luaL_checkstring(L,1);s.PushScope('i');ImGui::PushID(id);break; }
    case PopID: s.PopScope('i'); ImGui::PopID(); break;
    case BeginDisabled: s.PushScope('d'); ImGui::BeginDisabled(lua_gettop(L) == 0 || lua_toboolean(L,1)); break;
    case EndDisabled: s.PopScope('d'); ImGui::EndDisabled(); break;
    case BeginGroup: s.PushScope('g'); ImGui::BeginGroup(); break;
    case EndGroup: s.PopScope('g'); ImGui::EndGroup(); break;
    case BeginTable: {
      const char *id = luaL_checkstring(L,1); int columns = (int)luaL_checkinteger(L,2);
      if (columns < 1 || columns > 64) return luaL_error(L,"Table columns must be 1..64");
      s.PushScope('t');
      if(!ImGui::BeginTable(id,columns,ImGuiTableFlags_NoSavedSettings)) {
        s.scopes.pop_back();
        return boolean(false);
      }
      if(s.guiState)s.guiState->Table(ImGui::GetCurrentTable()->ID,s.session);
      return boolean(true);
    }
    case EndTable: s.PopScope('t'); ImGui::EndTable(); break;
    case TableNextRow: case TableNextColumn:
      if (!ImGui::GetCurrentContext()->CurrentTable) return luaL_error(L,"No active table");
      if (op == TableNextRow) ImGui::TableNextRow(); else return boolean(ImGui::TableNextColumn()); break;
    case CollapsingHeader: {
      const char* label=luaL_checkstring(L,1);
      const bool expanded=ImGui::CollapsingHeader(label);
      auto* window=ImGui::GetCurrentWindowRead();
      if(s.guiState)s.guiState->CollapsingHeader(window,window->GetID(label),s.session);
      return boolean(expanded);
    }
    case GetItemRectMin: case GetItemRectMax: {
      ImVec2 pos = op == GetItemRectMin ? ImGui::GetItemRectMin() : ImGui::GetItemRectMax();
      lua_pushnumber(L,pos.x); lua_pushnumber(L,pos.y); return 2;
    }
    case GetContentRegionAvail: {
      ImVec2 size=ImGui::GetContentRegionAvail();
      lua_pushnumber(L,size.x);lua_pushnumber(L,size.y);return 2;
    }
    case IsItemHovered: return boolean(ImGui::IsItemHovered());
    case SetTooltip: ImGui::SetTooltip("%s",luaL_checkstring(L,1)); break;
    case BeginTabBar: case BeginTabItem: case BeginCombo: {
      const char* label=luaL_checkstring(L,1);
      const char* preview=op==BeginCombo?luaL_checkstring(L,2):nullptr;
      if(op==BeginTabItem&&(s.scopes.empty()||s.scopes.back()!='b'))return luaL_error(L,"Tab item needs a tab bar");
      const ImGuiID combo=op==BeginCombo?ImGui::GetID(label):0;
      s.PushScope(op==BeginTabBar?'b':op==BeginTabItem?'a':'o');
      const bool visible=op==BeginTabBar?ImGui::BeginTabBar(label):
        op==BeginTabItem?ImGui::BeginTabItem(label):ImGui::BeginCombo(label,preview);
      if(!visible)s.scopes.pop_back();
      else if(op==BeginTabBar&&s.guiState)s.guiState->TabBar(ImGui::GetCurrentTabBar()->ID,s.session);
      // ImGui recycles combo windows by depth. Scope their contents to the
      // actual combo so identical labels in different panels stay independent.
      else if(op==BeginCombo)ImGui::PushOverrideID(combo);
      return boolean(visible);
    }
    case EndTabBar:s.PopScope('b');ImGui::EndTabBar();break;
    case EndTabItem:s.PopScope('a');ImGui::EndTabItem();break;
    case EndCombo:s.PopScope('o');ImGui::PopID();ImGui::EndCombo();break;
    case Selectable:return boolean(ImGui::Selectable(luaL_checkstring(L,1),lua_toboolean(L,2)!=0));
    case ProgressBar:ImGui::ProgressBar(Number(L,1),{Number(L,2,-FLT_MIN),Number(L,3)},luaL_optstring(L,4,nullptr));break;
    case GetWindowContentRect: {
      const auto& rect=ImGui::GetCurrentWindowRead()->InnerRect;
      lua_pushnumber(L,rect.Min.x);lua_pushnumber(L,rect.Min.y);
      lua_pushnumber(L,rect.Max.x);lua_pushnumber(L,rect.Max.y);
      return 4;
    }
    case Image: case ImageButton: case DrawListAddImage: {
      const bool button=op==ImageButton;
      const bool drawOnly=op==DrawListAddImage;
      const char* id=button?luaL_checkstring(L,1):nullptr;
      const int arg=button?2:1;
      const char* path=ImagePath(L,arg);
      if(!s.images)return luaL_error(L,"GUI image loader unavailable");
      const int uvArg=drawOnly?6:arg+3;
      ImVec2 uv0={Number(L,uvArg),Number(L,uvArg+1)},uv1={Number(L,uvArg+2,1),Number(L,uvArg+3,1)};
      auto normalized=[](float value){return value>=0&&value<=1;};
      if(!normalized(uv0.x)||!normalized(uv0.y)||!normalized(uv1.x)||!normalized(uv1.y)||
         uv0.x==uv1.x||uv0.y==uv1.y)return luaL_error(L,"Invalid GUI image UV rectangle");
      auto request=[&](ImVec2 size){
        return s.LoadImage(path,{SampleScale(size.x,uv1.x-uv0.x),SampleScale(size.y,uv1.y-uv0.y)});
      };
      if(drawOnly) {
        ImRect rect(Number(L,2),Number(L,3),Number(L,4),Number(L,5));
        const float alpha=Number(L,10,1);
        if(!normalized(alpha))return luaL_error(L,"Invalid GUI image opacity");
        if(alpha==0||rect.GetWidth()<=0||rect.GetHeight()<=0||ImGui::GetCurrentWindowRead()->SkipItems||
           !ImGui::IsRectVisible(rect.Min,rect.Max))return boolean(false);
        const auto image=request(rect.GetSize());
        if(!s.error.empty())return luaL_error(L,"%s",s.error.c_str());
        if(image.id==ImTextureID_Invalid) {
          lua_pushboolean(L,false);
          if(image.error&&*image.error) {lua_pushstring(L,image.error);return 2;}
          return 1;
        }
        ImGui::GetWindowDrawList()->AddImage(image.id,rect.Min,rect.Max,uv0,uv1,
                                            ImGui::GetColorU32(ImVec4(1,1,1,alpha)));
        return boolean(true);
      }
      ImVec2 size={Number(L,arg+1),Number(L,arg+2)};
      if(size.x<0||size.y<0||size.x>8192||size.y>8192)return luaL_error(L,"Invalid GUI image display size");
      const auto& style=ImGui::GetStyle();
      const ImVec2 padding=button?style.FramePadding:ImVec2(style.ImageBorderSize,style.ImageBorderSize);
      auto* window=ImGui::GetCurrentWindow();
      if(window->SkipItems)return button?boolean(false):0;
      const bool clipped=size.x&&size.y&&!ImGui::IsRectVisible({size.x+padding.x*2,size.y+padding.y*2});
      ImageInfo image;
      if(!clipped){
        image=request(size);
        if(!s.error.empty())return luaL_error(L,"%s",s.error.c_str());
      }
      if(size.x==0||size.y==0) {
        const float width=(image.width>0?image.width:96.f)*std::abs(uv1.x-uv0.x);
        const float height=(image.height>0?image.height:96.f)*std::abs(uv1.y-uv0.y);
        if(size.x==0&&size.y==0)size={width,height};
        else if(size.x==0)size.x=size.y*width/height;
        else size.y=size.x*height/width;
      }
      if(!(size.x>0&&size.y>0&&size.x<=8192&&size.y<=8192))return luaL_error(L,"Invalid GUI image display size");
      bool clicked=false;
      if(image.id==ImTextureID_Invalid){
        // Preserve the native widget's ID, layout and interaction when clipped
        // or when this frame has exhausted the texture budget.
        auto atlas=ImGui::GetIO().Fonts->TexRef;
        const auto white=ImGui::GetFontTexUvWhitePixel();
        const ImVec4 transparent={0,0,0,0};
        if(button)clicked=ImGui::ImageButton(id,atlas,size,white,white,transparent,transparent);
        else ImGui::ImageWithBg(atlas,size,white,white,style.Colors[ImGuiCol_FrameBg],transparent);
      }else if(button)clicked=ImGui::ImageButton(id,image.id,size,uv0,uv1);
      else ImGui::Image(image.id,size,uv0,uv1);
      if(image.error&&*image.error&&ImGui::IsItemHovered())ImGui::SetTooltip("%s: %s",path,image.error);
      if(button)return boolean(clicked);
      break;
    }
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
    for (const char *name : {"dofile","loadfile","load","collectgarbage","pcall","xpcall","print","setmetatable","getmetatable","warn"}) {
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
      EIEM_UI_API(GetItemRectMax), EIEM_UI_API(IsItemHovered), EIEM_UI_API(SetTooltip),
      EIEM_UI_API(BeginTabBar), EIEM_UI_API(EndTabBar), EIEM_UI_API(BeginTabItem), EIEM_UI_API(EndTabItem),
      EIEM_UI_API(BeginCombo), EIEM_UI_API(EndCombo), EIEM_UI_API(Selectable),
      EIEM_UI_API(ProgressBar), EIEM_UI_API(Image), EIEM_UI_API(ImageButton), EIEM_UI_API(GetWindowContentRect),
      EIEM_UI_API(TextWrapped), EIEM_UI_API(GetContentRegionAvail)
#undef EIEM_UI_API
    };
    lua_newtable(L);
    for (const auto &f : functions) { lua_pushinteger(L,f.op); lua_pushcclosure(L,Dispatch,1); lua_setfield(L,-2,f.name); }
    lua_newtable(L);
    lua_pushinteger(L,DrawListAddImage);lua_pushcclosure(L,Dispatch,1);lua_setfield(L,-2,"AddImage");
    lua_setfield(L,-2,"DrawList");
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
    for(const auto& f: {std::make_pair("invoke",Invoke),{"trigger",Trigger},{"play",Play},{"pause",Pause},{"stop",Stop},
                       {"animation_state",AnimationState}}){
      lua_pushinteger(L,f.second);lua_pushcclosure(L,Dispatch,1);lua_setfield(L,-2,f.first);
    }
    lua_pushinteger(L,1);lua_setfield(L,-2,"api_version");
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
  bool closeRequested=false;
  EiemCustomGui::Requests TakeRequests() {return std::move(requests);}
  explicit EiemLuaUi(std::string id,EiemGuiState* state=nullptr,uint64_t serial=0)
    :guiState(state),session(serial),identity(std::move(id)){}
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
      auto root=std::filesystem::absolute(std::filesystem::u8path(modIni)).parent_path();
      auto bytes=EiemCustomGui::ReadFile(root,relative,256*1024);
      return Load(std::string(bytes.begin(),bytes.end()),relative);
    } catch (const std::exception &e) { error = relative + ": " + e.what(); return false; }
  }
  bool Draw(const EiemVariables &current, EiemVariables &writes, const EiemVariables &authorDefaults = {},
            const ImageLoader* imageLoader=nullptr,const Playback& animationPlayback={}) {
    writes.clear();requests.clear();closeRequested=false;
    if (!error.empty() || drawRef == LUA_NOREF) return false;
    variables=&current; defaults=&authorDefaults;images=imageLoader;playback=&animationPlayback;
    pending.clear();scopes.clear();colors=styles=0;calls=0;
    ImGuiErrorRecoveryState base{}, after{};
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
    Unwind();
    ImGui::ErrorRecoveryStoreState(&after);
    if(after.SizeOfColorStack>base.SizeOfColorStack)ImGui::PopStyleColor(after.SizeOfColorStack-base.SizeOfColorStack);
    if(after.SizeOfStyleVarStack>base.SizeOfStyleVarStack)ImGui::PopStyleVar(after.SizeOfStyleVarStack-base.SizeOfStyleVarStack);
    // SetNextWindow* data must not leak to another Mod, even after an error.
    context->NextWindowData.ClearFlags(); context->NextItemData.ClearFlags();
    context->ErrorCallback = oldCallback; context->ErrorCallbackUserData = oldData;
    io.ConfigErrorRecoveryEnableAssert = oldAssert; io.ConfigErrorRecoveryEnableTooltip = oldTooltip;
    if (ok && error.empty()) { writes.swap(pending); return true; }
    if (!error.empty() && error.find(sourceName) == std::string::npos) error = sourceName + ": " + error;
    pending.clear();requests.clear();return false;
  }
};
