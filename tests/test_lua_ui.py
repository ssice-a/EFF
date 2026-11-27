"""Real Lua + ImGui CPU frames; no injection, game process or user Blender scene."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include "eiem_lua_ui.h"
#include <cstdio>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(false)
static bool frame(EiemLuaUi &ui, const EiemVariables &vars, EiemVariables &writes, const EiemVariables &defaults={},
                  const EiemLuaUi::ImageLoader* images=nullptr, const EiemLuaUi::Playback& playback={}) {
  ImGui::GetIO().DisplaySize={800,600}; ImGui::GetIO().DeltaTime=1.f/60;
  ImGui::NewFrame(); bool ok=ui.Draw(vars,writes,defaults,images,playback); ImGui::Render();
  if(!ok&&images)fprintf(stderr,"%s\n",ui.error.c_str());
  return ok;
}
int main() {
  ImGui::CreateContext();
  auto &io=ImGui::GetIO(); io.IniFilename=nullptr; io.Fonts->AddFontDefault();
  unsigned char *pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
  io.Fonts->SetTexID(1);
  EiemVariables vars{{"$size",.25},{"$other",0}}, writes;
  EiemLuaUi a("a/UI"), b("b/UI");
  EiemLuaUi reset("reset/UI");CHECK(reset.Load("return function() mod.set('$size',mod.default('$size')) end","reset.lua"));
  CHECK(frame(reset,vars,writes,{{"$size",.8}}) && writes.at("$size")==.8 && !writes.count("$other"));
  const char *source=R"lua(
    local count=0
    return function()
      count=count+1
      assert(io==nil and os==nil and package==nil and debug==nil and load==nil and pcall==nil)
      imgui.SetNextWindowSize(300,350)
      imgui.PushStyleColor(imgui.Col.Button, 0.3,0.2,0.4,1)
      if imgui.Begin("Same title") then
        imgui.PopStyleColor()
        imgui.PushStyleVar(imgui.StyleVar.FramePadding,4,3)
        imgui.Text("100% safe text")
        local changed,v=imgui.SliderFloat("Size",mod.get("$size"),0,1)
        imgui.DragFloat("Drag",v,.01)
        imgui.InputFloat("Input",v)
        imgui.Checkbox("Check",true)
        imgui.ColorEdit4("Color",1,.2,.3,1)
        if imgui.BeginChild("child",0,60,true) then imgui.Text("child") end
        imgui.EndChild()
        if imgui.BeginTable("table",2) then
          imgui.TableNextRow(); imgui.TableNextColumn(); imgui.Text("cell")
          imgui.EndTable()
        end
        imgui.PopStyleVar()
      else imgui.PopStyleColor() end
      imgui.End()
      mod.set("$size",mod.get("$size")+.25)
      mod.set("$other",count)
    end
  )lua";
  CHECK(a.Load(source,"a.lua") && b.Load(source,"b.lua"));
  CHECK(frame(a,vars,writes)); CHECK(writes.at("$size")==.5 && writes.at("$other")==1);
  CHECK(frame(a,vars,writes)); CHECK(writes.at("$other")==2);
  CHECK(frame(b,vars,writes)); CHECK(writes.at("$other")==1);
  CHECK(ImGui::FindWindowByName(a.WindowLabel("Same title").c_str()) !=
        ImGui::FindWindowByName(b.WindowLabel("Same title").c_str()));
  CHECK(a.Load(source,"a.lua")); CHECK(frame(a,vars,writes) && writes.at("$other")==1);
  EiemLuaUi playback("playback/UI");
  CHECK(playback.Load("return function() local state,time=mod.animation_state('clip'); "
                      "if state=='paused' then mod.set('$other',time) end end","playback.lua"));
  CHECK(frame(playback,vars,writes,{},nullptr,{{"clip",{true,true,false,.75}}}));
  CHECK(writes.at("$other")==.75);

  for (const char *bad : {
    "return function() mod.set('$size',1); error('intentional') end",
    "return function() mod.set('$missing',1) end",
    "return function() mod.set('$size',0/0) end",
    "return function() while true do end end",
    "return function() local t=string.rep('x',32*1024*1024) end",
    "return function() imgui.Begin('bad'); imgui.PushStyleColor(0,1,0,0,1) end",
    "return function() imgui.PopStyleColor() end",
    "return function() imgui.End() end",
    "return function() imgui.Begin('bad'); imgui.Begin('nested') end",
    "return function() imgui.Begin('bad'); imgui.PushID('x'); imgui.End() end",
    "return function() imgui.Begin('bad'); if imgui.BeginTabBar('tabs') and imgui.BeginTabItem('one') then error('tab') end end"
  }) {
    CHECK(a.Load(bad,"broken.lua"));
    CHECK(!frame(a,vars,writes) && !a.error.empty() && writes.empty());
    CHECK(frame(b,vars,writes)); // other VM/ImGui stacks survive every failure
  }
  CHECK(!a.Load("this is not Lua", "syntax.lua"));
  CHECK(a.error.find("syntax.lua")!=std::string::npos);
  CHECK(!a.Load("return 3", "not_function.lua"));
  CHECK(!a.Load("while true do end", "init_loop.lua"));
  CHECK(!a.LoadFile("mod.ini","../outside.lua"));

  // Drive the actual native slider with ImGui input events, not an unconditional
  // mod.set in the script. Measure its real rectangle via the public bindings.
  EiemLuaUi click("click/UI");
  EiemVariables inputVars{{"$size",0},{"$other",0},{"$x",0},{"$y",0},{"$right",0},{"$open",1}};
  CHECK(click.Load(R"lua(return function()
    if mod.get("$open") == 0 then return end
    imgui.SetNextWindowPos(40,40)
    imgui.SetNextWindowSize(300,180)
    local visible,open=imgui.Begin("Slider interaction",true)
    if not open then mod.set("$open",0) end
    if visible then
      imgui.SetNextItemWidth(200)
      local changed,value=imgui.SliderFloat("##size",mod.get("$size"),0,1)
      if changed then mod.set("$size",value) end
      local x,y=imgui.GetItemRectMin()
      local r,b=imgui.GetItemRectMax()
      mod.set("$x",x); mod.set("$y",(y+b)/2); mod.set("$right",r)
    end
    imgui.End()
  end)lua","click.lua"));
  CHECK(frame(click,inputVars,writes)); CHECK(frame(click,inputVars,writes));
  io.AddMousePosEvent((float)((writes.at("$x")+writes.at("$right"))/2),(float)writes.at("$y"));
  io.AddMouseButtonEvent(0,true);
  CHECK(frame(click,inputVars,writes));
  CHECK(writes.count("$size") && writes.at("$size")>.4 && writes.at("$size")<.6);
  io.AddMouseButtonEvent(0,false); CHECK(frame(click,inputVars,writes));
  auto *window=ImGui::FindWindowByName(click.WindowLabel("Slider interaction").c_str());
  CHECK(window);
  float closeX=window->Pos.x+window->Size.x-ImGui::GetStyle().FramePadding.x-ImGui::GetFontSize()/2;
  float closeY=window->Pos.y+ImGui::GetStyle().FramePadding.y+ImGui::GetFontSize()/2;
  io.AddMousePosEvent(closeX,closeY); io.AddMouseButtonEvent(0,true);
  CHECK(frame(click,inputVars,writes));
  io.AddMouseButtonEvent(0,false); CHECK(frame(click,inputVars,writes));
  CHECK(writes.at("$open")==0 && !writes.count("$size")); // Lua chooses close action; model untouched
  inputVars["$open"]=0;
  CHECK(frame(click,inputVars,writes) && writes.empty());
  inputVars["$open"]=1;
  CHECK(frame(click,inputVars,writes)); // script reopens; no DLL open flag gates it

  // One script freely composes multiple independent root windows, no key needed.
  EiemLuaUi multi("multi/UI");
  CHECK(multi.Load(R"lua(return function()
    imgui.SetNextWindowPos(20,20,imgui.Cond.Always)
    if imgui.Begin("First###stable") then imgui.Text("one") end
    imgui.End()
    imgui.SetNextWindowPos(350,20,imgui.Cond.Always)
    if imgui.Begin("Second",nil,imgui.WindowFlags.NoTitleBar) then imgui.Text("two") end
    imgui.End()
  end)lua","multi.lua"));
  CHECK(frame(multi,vars,writes) && writes.empty());
  auto *first=ImGui::FindWindowByName(multi.WindowLabel("First###stable").c_str());
  auto *second=ImGui::FindWindowByName(multi.WindowLabel("Second").c_str());
  CHECK(first && second && first!=second && !first->HasCloseButton && !second->HasCloseButton);
  CHECK(ImHashStr(multi.WindowLabel("First###stable").c_str()) ==
        ImHashStr(multi.WindowLabel("Changed###stable").c_str()));

  EiemLuaUi pictured("image/UI");
  CHECK(pictured.Load(R"lua(return function()
    imgui.SetNextWindowPos(20,340)
    imgui.SetNextWindowSize(300,180)
    if imgui.Begin("Images") then
      local x0,y0,x1,y1=imgui.GetWindowContentRect()
      imgui.DrawList.AddImage("background.dds",x0,y0,x1,y1,.2,0,.8,1,.5)
      if imgui.BeginTabBar("Pages") then
        if imgui.BeginTabItem("Look") then
          imgui.Image("panel.png",48,48,0,0,.5,1)
          imgui.SameLine()
          if imgui.ImageButton("select","panel.png",80,24,.5,0,1,1) then
            mod.set("$other",2); mod.invoke("ActionLook"); mod.trigger("KeySwitchLook"); mod.play("Look")
          end
          local x,y=imgui.GetItemRectMin()
          local r,b=imgui.GetItemRectMax()
          mod.set("$x",(x+r)/2); mod.set("$y",(y+b)/2)
          imgui.EndTabItem()
        end
        imgui.EndTabBar()
      end
    end
    imgui.End()
  end)lua","images.lua"));
  EiemLuaUi::ImageLoader images=[](const char* path,ImVec2 size) {
    if(std::string_view(path)=="background.dds"&&size.x>0&&size.y>0)
      return EiemLuaUi::ImageInfo{125,3840,2160};
    if(std::string_view(path)!="panel.png"||size.x<=0||size.y<=0)throw std::runtime_error("unexpected image");
    return EiemLuaUi::ImageInfo{123,2,2};
  };
  CHECK(frame(pictured,inputVars,writes,{},&images));
  CHECK(frame(pictured,inputVars,writes,{},&images));
  CHECK(writes.count("$x") && writes.count("$y"));
  bool leftUv=false,middleUv=false,rightUv=false,backgroundFirst=false,backgroundAlpha=false,backgroundCrop=false;
  auto* draw=ImGui::GetDrawData();
  for(int i=0;i<draw->CmdListsCount;++i) {
    const auto* list=draw->CmdLists[i];
    bool backgroundDrawn=false;
    for(const auto& cmd:list->CmdBuffer) {
      const auto id=cmd.TexRef.GetTexID();
      if(id!=123&&id!=125)continue;
      if(id==125)backgroundDrawn=true;
      else backgroundFirst|=backgroundDrawn;
      for(unsigned j=0;j<cmd.ElemCount;++j) {
        const auto& vertex=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[cmd.IdxOffset+j]];
        const auto& uv=vertex.uv;
        if(id==125) {
          backgroundAlpha|=(vertex.col>>IM_COL32_A_SHIFT)==128;
          backgroundCrop|=(uv.x>0&&uv.x<1)||(uv.y>0&&uv.y<1);
          continue;
        }
        leftUv|=uv.x==0;
        middleUv|=uv.x==.5f;
        rightUv|=uv.x==1;
      }
    }
  }
  CHECK(leftUv && middleUv && rightUv && backgroundFirst && backgroundAlpha && backgroundCrop);
  io.AddMousePosEvent(float(writes.at("$x")),float(writes.at("$y")));
  io.AddMouseButtonEvent(0,true);
  CHECK(frame(pictured,inputVars,writes,{},&images));
  io.AddMouseButtonEvent(0,false);
  CHECK(frame(pictured,inputVars,writes,{},&images));
  const auto requests=pictured.TakeRequests();
  using Op=EiemCustomGui::Operation;
  CHECK(requests.size()==6 && requests[0].operation==Op::Set && requests[0].value==2);
  CHECK(requests[1].operation==Op::Invoke && requests[2].operation==Op::Trigger && requests[3].operation==Op::Play);
  EiemLuaUi gallery("gallery/UI");
  CHECK(gallery.Load(R"lua(return function()
    imgui.SetNextWindowPos(430,20)
    imgui.SetNextWindowSize(240,160)
    if imgui.Begin("Gallery") then
      if imgui.ImageButton("placeholder","budget.png",96,24) then mod.set("$other",3) end
      local x,y=imgui.GetItemRectMin()
      local r,b=imgui.GetItemRectMax()
      mod.set("$x",(x+r)/2); mod.set("$y",(y+b)/2)
      for i=1,100 do imgui.ImageButton(tostring(i),"item"..i..".png",96,32) end
    end
    imgui.End()
  end)lua","gallery.lua"));
  int loaded=0;
  EiemLuaUi::ImageLoader galleryImages=[&](const char* path,ImVec2) {
    if(std::string_view(path)=="budget.png")return EiemLuaUi::ImageInfo{ImTextureID_Invalid,128,128};
    ++loaded;
    return EiemLuaUi::ImageInfo{124,128,128};
  };
  CHECK(frame(gallery,inputVars,writes,{},&galleryImages));
  loaded=0;
  CHECK(frame(gallery,inputVars,writes,{},&galleryImages));
  CHECK(loaded>0&&loaded<10);
  io.AddMousePosEvent(float(writes.at("$x")),float(writes.at("$y")));
  io.AddMouseButtonEvent(0,true); CHECK(frame(gallery,inputVars,writes,{},&galleryImages));
  io.AddMouseButtonEvent(0,false); CHECK(frame(gallery,inputVars,writes,{},&galleryImages));
  CHECK(writes.at("$other")==3);
  ImGui::DestroyContext();
  return 0;
}
'''

LIFECYCLE_SOURCE = SOURCE[:SOURCE.index('  EiemVariables vars')] + r'''
  EiemGuiState state;
  EiemVariables vars,writes;
  auto& context=*ImGui::GetCurrentContext();
  const char* script=R"lua(local frame=0
    return function()
      frame=frame+1
      imgui.SetNextWindowPos(10,10)
      imgui.SetNextWindowSize(360,400)
      if imgui.Begin("Dynamic panel") then
        if imgui.BeginChild("Child"..frame,320,320) then
          if imgui.BeginTabBar("Pages"..frame) then
            if imgui.BeginTabItem("Appearance") then
              if imgui.BeginTable("Choices"..frame,2) then
                imgui.TableNextColumn()
                imgui.Text("Current appearance")
                imgui.EndTable()
              end
              imgui.EndTabItem()
            end
            imgui.EndTabBar()
          end
        end
        imgui.EndChild()
      end
      imgui.End()
    end)lua";
  {
    EiemLuaUi panel("dynamic", &state, 1);
    CHECK(panel.Load(script,"dynamic.lua"));
    for(int i=0;i<2000;++i) {
      state.Collect({1});
      CHECK(frame(panel,vars,writes));
      CHECK(context.Windows.Size<=520 && context.WindowsById.Data.Size==context.Windows.Size);
      CHECK(context.TabBars.GetMapSize()<=520 && context.Tables.GetMapSize()<=520);
    }
    CHECK(context.TabBars.GetAliveCount()>0 && context.Tables.GetAliveCount()>0);
  }
  state.Collect({});
  CHECK(context.Windows.Size==1 && context.WindowsById.Data.Size==1);
  CHECK(context.TabBars.GetMapSize()==0 && context.TabBars.GetAliveCount()==0);
  CHECK(context.Tables.GetMapSize()==0 && context.Tables.GetAliveCount()==0);
  for(uint64_t session=2;session<522;++session) {
    {
      EiemLuaUi panel("profile/"+std::to_string(session),&state,session);
      CHECK(panel.Load(script,"profile.lua"));
      state.Collect({session});
      CHECK(frame(panel,vars,writes) && frame(panel,vars,writes));
    }
    state.Collect({});
    CHECK(context.Windows.Size==1 && context.WindowsById.Data.Size==1);
    CHECK(context.TabBars.GetMapSize()==0 && context.Tables.GetMapSize()==0);
  }
  CHECK(context.SettingsWindows.empty() && context.SettingsTables.empty());
  ImGui::DestroyContext();
  return 0;
}
'''


def build_harness(folder, source=SOURCE):
    cpp = folder / "lua_ui.cpp"
    cpp.write_text(source, encoding="utf-8")
    exe = folder / "lua_ui.exe"
    command = ["cl", "/nologo", "/MD", "/EHsc", "/std:c++17", "/utf-8",
               f"/I{ROOT / 'src'}", f"/I{ROOT / 'deps/imgui'}", f"/I{ROOT / 'deps/lua'}",
               str(cpp), str(ROOT / 'bin/imgui/eiem_imgui.lib'),
               str(ROOT / 'bin/imgui/eiem_lua.lib'), f"/Fe{exe}", "user32.lib", "imm32.lib"]
    build = subprocess.run(command, cwd=folder, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=120)
    if build.returncode:
        raise AssertionError(build.stdout + build.stderr)
    return exe


class LuaUiTests(unittest.TestCase):
    def run_harness(self, source):
        libraries = (ROOT / "bin/imgui" / name for name in ("eiem_imgui.lib", "eiem_lua.lib"))
        if not shutil.which("cl") or not all(path.is_file() for path in libraries):
            self.skipTest("Run tools/build_imgui.cmd, then tests from an MSVC environment")
        with tempfile.TemporaryDirectory(prefix="eiem-lua-ui-") as temp:
            folder = Path(temp)
            exe = build_harness(folder, source)
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_real_vm_frames_errors_isolation_and_reload(self):
        self.run_harness(SOURCE)

    def test_dynamic_ids_and_sequential_sessions_release_shared_state(self):
        self.run_harness(LIFECYCLE_SOURCE)


if __name__ == "__main__":
    unittest.main()
