#define EIEM_PANEL_TEST
#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
#include <thread>
#include <future>
#include <wct.h>
#pragma comment(lib,"advapi32.lib")

namespace TestPanel {
using namespace Entry;
using namespace RuntimeControls;
using EiemControlPanel::Action;
UINT physical=0;
bool Sample(const EiemKeyChord& chord) { return physical==chord.vk; }
Package Make(const std::string& id) {
  Package p;p.id=id;p.root=std::filesystem::temp_directory_path()/"eff-panel-fixture"/id;
  p.defaults={{"$switch",0},{"$shape",0}};p.controls["$shape"]={u8"体型",0,1};
  p.bindings={{"衣装切换","$switch","cycle",{VK_NUMPAD1,0},{0,1,2},1},
              {"increase","$shape","hold",{VK_UP,0},{1},1},
              {"set","$switch","press",{'P',0},{2},1}};
  p.targetIndex[Identity{"assets/characters/actor_"+id+"/body","body"}]="body";
  return p;
}
std::shared_ptr<const std::vector<Package>> Graph(std::vector<Package> p) {
  return std::make_shared<const std::vector<Package>>(std::move(p));
}
void Install(std::vector<Package> packages,bool merge=false) {
  auto candidate=Prepare(Graph(std::move(packages)),EiemGlobalConfig{});
  if(merge) MergeLive(candidate,Sample);Swap(candidate);
}
void Queue(Action action,const std::string& mod,const std::string& name={},double value=0,
           std::shared_ptr<const EiemControlPanel::Snapshot> view={}) {
  if(!view) view=panel.Read();
  Require(panel.Submit({view->revision,view->scope,action,mod,name,value}),"panel queue full");
}
void Frame(bool focus=true,double delta=.25) {PanelCommands(focus,Sample);PollControls(focus,delta,Sample);}
void AnimationChecks(){
 auto p=Make("Anim");ShapeAnimation clip;clip.id="wing";clip.label="wing";clip.key={VK_F6,0};clip.duration=1;clip.speedVariable="$speed";clip.tracks["$shape"]={0,.5f,1};p.shapeAnimations.emplace(clip.id,clip);p.defaults["$speed"]=1;
 {auto valid=p;valid.persistent.insert("$speed");ValidateShapeAnimations(valid);
  auto bad=valid;bad.shapeAnimations.at("wing").speedVariable="$shape";bool rejected=false;try{ValidateShapeAnimations(bad);}catch(const std::exception&){rejected=true;}Require(rejected,"animation speed aliases shape");
  bad=valid;auto duplicate=clip;duplicate.id="other";bad.shapeAnimations.emplace(duplicate.id,duplicate);rejected=false;try{ValidateShapeAnimations(bad);}catch(const std::exception&){rejected=true;}Require(rejected,"animation speed aliases another group");}
 physical=0;panel.open=false;LoadControls(Graph({p}));physical=VK_F6;Frame(true,0);AdvanceAnimations(.25);Require(std::abs(ShapeValue("Anim","$shape")-.25)<1e-6,"animation play sample");Require(variables.at("Anim").at("$shape")==0,"animated output changed manual preferences");
 auto immutable=panel.Read();panel.open=true;Queue(Action::AnimationSpeed,"Anim","wing",2);Frame(true,0);Require(animations.at("Anim").clips.at("wing").time==.25,"speed reset phase");AdvanceAnimations(.125);Require(std::abs(ShapeValue("Anim","$shape")-.5)<1e-6,"speed rate differs");
 Queue(Action::AnimationPause,"Anim","wing");Frame(true,0);AdvanceAnimations(.5);Require(ShapeValue("Anim","$shape")==.5,"paused clip advances");
 Queue(Action::AnimationPlay,"Anim","wing");Frame(true,0);AdvanceAnimations(.25);Require(std::abs(ShapeValue("Anim","$shape"))<1e-6,"loop wrap");
 Queue(Action::Value,"Anim","$shape",.7);Frame(true,0);Require(!animations.at("Anim").clips.at("wing").playing&&ShapeValue("Anim","$shape")==.7,"manual slider handoff");
 Queue(Action::AnimationSpeed,"Anim","wing",0);Frame(true,0);Require(variables.at("Anim").at("$speed")==2,"zero speed accepted");
 Queue(Action::AnimationReset,"Anim","wing");Frame(true,0);Require(variables.at("Anim").at("$speed")==1,"default speed reset");
 p.persistent.insert("$speed");auto fixtureRoot=std::filesystem::temp_directory_path()/std::filesystem::path("eff-animation-speed-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(fixtureRoot);auto speedPackage=p;speedPackage.root=fixtureRoot;
 EiemVariables saved=p.defaults;saved["$speed"]=1.75;EiemEntryPreferences::Write(fixtureRoot/"state.ini",{{"$speed",1.75}},p.id);EiemEntryPreferences fresh;Require(fresh.Preview(speedPackage).at("$speed")==1.75,"animation speed restart persistence");std::filesystem::remove(fixtureRoot/"state.ini");std::filesystem::remove(fixtureRoot);
 auto stale=panel.Read();Install({p},true);Queue(Action::AnimationPlay,"Anim","wing",0,stale);Frame(true,0);Require(!animations.at("Anim").clips.at("wing").playing,"stale animation command survived reload");
 Require(!immutable->animations.at("Anim").at("wing").paused,"animation snapshot mutated");
 panel.open=false;physical=0;Frame(true,0);physical=VK_F6;Frame(true,0);Require(animations.at("Anim").clips.at("wing").playing,"animation restart");physical=VK_UP;Frame(true,.1);Require(!animations.at("Anim").clips.at("wing").playing,"manual key handoff");
 p.shapeAnimations.at("wing").loop=false;physical=0;LoadControls(Graph({p}));animations.at("Anim").Play(p.shapeAnimations.at("wing"));AdvanceAnimations(1.5);Require(!animations.at("Anim").clips.at("wing").playing,"once clip did not stop");
 Install({},true);panel.open=false;physical=0;
}
void Checks() {
  AnimationChecks();
  EiemControlPanel::Shortcut shortcut;
  EiemKeyChord insert{VK_INSERT,0};
  Require(!shortcut.Press(insert,true,true),"startup held Insert synthesized toggle");
  shortcut.Press(insert,false,true);
  Require(shortcut.Press(insert,true,true) && !shortcut.Press(insert,true,true),"Insert repeats while held");
  shortcut.Press(insert,false,false);Require(!shortcut.Press(insert,true,false) && !shortcut.Press(insert,true,true),"focus synthesized Insert toggle");
  Require(!shortcut.Press({VK_HOME,0},true,true),"new GUI binding synthesized toggle");
  LoadControls(Graph({Make("A"),Make("B")}));Require(selected=="A","default selection differs from old manager");
  auto immutable=panel.Read();
  physical=VK_NUMPAD1;Frame();Frame();
  Require(variables.at("A").at("$switch")==1 && variables.at("B").at("$switch")==1,"same key did not reach both enabled Mods or repeated");
  Require(immutable->values.at("A").at("$switch")==0,"published UI snapshot mutated");
  panel.open=true;Queue(Action::Select,"B");Frame();
  panel.open=false;Frame();Require(selected=="B" && variables.at("B").at("$switch")==1,"selection manufactured held edge");
  physical=0;Frame();physical=VK_NUMPAD1;Frame();
  Require(variables.at("B").at("$switch")==2 && variables.at("A").at("$switch")==2,"panel selection limited physical keys to one Mod");
  panel.open=true;physical=0;Frame();physical=VK_NUMPAD1;Frame();
  Require(variables.at("B").at("$switch")==2,"panel physical shortcut not paused");
  Queue(Action::Invoke,"A",u8"衣装切换");Queue(Action::Invoke,"B",u8"衣装切换");Frame();
  Require(variables.at("A").at("$switch")==2 && variables.at("B").at("$switch")==0,"panel invocation escaped selection");
  Queue(Action::Invoke,"B","set");Frame();Require(variables.at("B").at("$switch")==2,"press button");
  auto oldScope=panel.Read();Queue(Action::Select,"A");Queue(Action::Value,"B","$shape",.9,oldScope);Frame();
  Require(selected=="A" && variables.at("B").at("$shape")==0,"queued old selection command applied");
  Queue(Action::Value,"A","$shape",.7);Frame();Require(variables.at("A").at("$shape")==.7,"shape slider command");
  Queue(Action::Value,"A","$shape",2);Queue(Action::Value,"A","$switch",1);Frame();
  Require(variables.at("A").at("$shape")==.7,"invalid slider range accepted");
  Queue(Action::Value,"A","$shape",0);Frame();
  Queue(Action::Hold,"A","increase",1);physical=VK_UP;
  Frame();Frame();Require(variables.at("A").at("$shape")==.5,"GUI hold not using Unity delta or double-counted physical hold");
  Queue(Action::Hold,"A","increase",0);Frame();Require(variables.at("A").at("$shape")==.5,"released GUI hold stuck");
  Queue(Action::Hold,"A","increase",1);Frame(false);Frame();
  Require(variables.at("A").at("$shape")==.5,"lost focus applied queued hold");
  Queue(Action::Hold,"A","increase",1);Frame();panel.open=false;physical=0;Frame();panel.open=true;Frame();
  Require(variables.at("A").at("$shape")==.75 && uiHold.empty(),"closed/reopened panel resumed GUI hold");
  Queue(Action::Keyboard,"A",{},0);Frame();panel.open=false;physical=0;Frame();physical=VK_NUMPAD1;Frame();
  Require(variables.at("A").at("$switch")==2 && variables.at("B").at("$switch")==0,"per-Mod disable also stopped the other enabled Mod");
  panel.open=true;Queue(Action::Invoke,"A",u8"衣装切换");Frame();
  Require(variables.at("A").at("$switch")==0,"keyboard disable incorrectly disabled panel controls");
  Queue(Action::Keyboard,"A",{},1);Frame();
  auto beforeReload=panel.Read();Queue(Action::Value,"A","$shape",.9,beforeReload);
  auto a=Make("A");a.persistent={"$shape"};
  auto b=Make("B");Install({a,b},true);Frame();
  Require(selected=="A" && keySwitchEnabled.at("A") && variables.at("A").at("$shape")==0,"reload accepted stale command");
  Queue(Action::Value,"A","$shape",.6);Frame();Install({a,b},true);
  Require(selected=="A" && variables.at("A").at("$shape")==.6,"reload lost valid persistent shape/selection");
  auto oldAuthor=panel.Read();Queue(Action::Value,"A","$shape",.8,oldAuthor);
  auto otherAuthor=a;otherAuthor.root/=L"other-author";Install({otherAuthor,b},true);Frame();
  Require(variables.at("A").at("$shape")==0,"reused Mod ID inherited another author's values/commands");
  auto stale=panel.Read();Install({b},true);Queue(Action::Invoke,"A",u8"衣装切换",0,stale);Frame();
  Require(selected=="B" && variables.at("B").at("$switch")==0,"removed selected Mod not reconciled");
  Install({},true);Frame();Require(selected.empty(),"empty graph kept removed selection");
  Install({Make("A")},true);Frame();Require(selected=="A","adding first character Mod left no keyboard target");
  Install({Make("A"),Make("B")},true);Frame();Require(selected=="A","adding another character Mod changed existing keyboard target");
  Install({Make("B")},true);Frame();Require(selected=="B","deleting current character Mod did not select remaining Mod");
  Install({},true);Frame();Require(selected.empty()&&keys.empty()&&specs.empty()&&panel.Read()->graph->empty(),"deleting last Mod left live controls");
  Install({Make("B")},true);Frame();Require(selected=="B","readding character Mod after empty set failed default selection");
  auto reserved=Make("reserved");reserved.bindings.front().key={VK_INSERT,MOD_CONTROL};
  auto accepted=Prepare(Graph({reserved}),EiemGlobalConfig{});Require(accepted.keys.at(reserved.id).front().key==reserved.bindings.front().key,"user-selected overlapping shortcut was rejected");
  auto view=panel.Read();panel.open=true;Queue(Action::Keyboard,"B",{},0,view);Frame();
  Require(!keySwitchEnabled.at("B"),"checkbox did not disable its Mod");
  Install({Make("C"),Make("D")});view=panel.Read();
  Queue(Action::Keyboard,"C",{},0,view);Queue(Action::Keyboard,"D",{},0,view);Frame();
  Require(!keySwitchEnabled.at("C")&&!keySwitchEnabled.at("D"),"one checkbox invalidated another queued checkbox");
  Queue(Action::Keyboard,"C",{},1);Queue(Action::Keyboard,"D",{},1);Frame();
  Require(keySwitchEnabled.at("C")&&keySwitchEnabled.at("D"),"multiple Mods could not be enabled together");
  auto disabled=Make("FromIni");disabled.keySwitchEnabled=false;Install({disabled,Make("DefaultEnabled")});
  panel.open=false;physical=0;Frame();physical=VK_NUMPAD1;Frame();
  Require(variables.at("FromIni").at("$switch")==0&&variables.at("DefaultEnabled").at("$switch")==1,"INI flag/default did not control independent key routing");
  disabled.keySwitchEnabled=true;Install({disabled,Make("DefaultEnabled")},true);Frame();
  Require(keySwitchEnabled.at("FromIni")&&variables.at("FromIni").at("$switch")==0,"F10 ignored edited INI or synthesized a held press");
  // Store publication and UI submissions use only immutable data across threads.
  EiemControlPanel::Store bridge;std::atomic_bool done=false;
  std::thread producer([&]{for(int i=0;i<2000;++i){auto s=std::make_shared<EiemControlPanel::Snapshot>();s->revision=i;bridge.Publish(s);}done=true;});
  while(!done) {auto s=bridge.Read();if(s) bridge.Submit({s->revision,0,Action::Select,"A"});bridge.Drain();}
  producer.join();Require(bridge.Read()->revision==1999,"cross-thread snapshot publication");
  panel.open=false;physical=0;
  std::cout<<"PASS per-Mod INI/UI keyboard enable, simultaneous identical keys, independent checkbox queues, default/reload/removal, held edges, immutable snapshots, stale graph/scope rejection, GUI pause/buttons/sliders/hold, focus/close, persistence, overlapping user keys, concurrent publication\n";
}
LRESULT CALLBACK GameProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {return DefWindowProcW(hwnd,msg,wp,lp);}
void CrossThreadWindowChecks(){
  EiemControlPanel::Store bridge;auto snapshot=std::make_shared<EiemControlPanel::Snapshot>();snapshot->graph=Graph({});bridge.Publish(snapshot);
  auto resume=CreateEventW(nullptr,TRUE,FALSE,nullptr);Require(resume!=nullptr,"owner resume event");
  std::atomic_bool stall=false,paused=false,stop=false;std::promise<HWND> ready;
  std::thread ownerThread([&]{
    WNDCLASSW wc{};wc.lpfnWndProc=GameProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"EFF_PanelCrossThreadOwner";RegisterClassW(&wc);
    auto owner=CreateWindowW(wc.lpszClassName,L"EFF panel cross-thread test",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,400,300,nullptr,nullptr,wc.hInstance,nullptr);
    ShowWindow(owner,SW_SHOW);SetForegroundWindow(owner);ready.set_value(owner);
    MSG message{};while(!stop){if(stall){paused=true;WaitForSingleObject(resume,INFINITE);stall=false;}while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(1);}DestroyWindow(owner);
  });
  struct Cleanup{HANDLE resume;std::atomic_bool& stop;std::thread& thread;~Cleanup(){SetEvent(resume);stop=true;thread.join();CloseHandle(resume);}}cleanup{resume,stop,ownerThread};
  auto owner=ready.get_future().get();Require(owner!=nullptr,"cross-thread owner creation");bridge.gameWindow=owner;bridge.open=true;
  double closeMs=0;
  {EiemControlPanel::Window window(bridge);Require(window.Create(),"cross-thread panel creation");window.Tick(owner);SetForegroundWindow(window.Handle());
    Require(IsWindowVisible(window.Handle()),"cross-thread panel not visible");
    stall=true;while(!paused)Sleep(1);auto panelTid=GetCurrentThreadId();
    std::thread watchdog([&]{if(WaitForSingleObject(resume,500)==WAIT_TIMEOUT){
      auto session=OpenThreadWaitChainSession(0,nullptr);if(session){WAITCHAIN_NODE_INFO nodes[WCT_MAX_NODE_COUNT]{};DWORD count=WCT_MAX_NODE_COUNT;BOOL cycle=FALSE;
        if(GetThreadWaitChain(session,0,WCTP_GETINFO_ALL_FLAGS,panelTid,&count,nodes,&cycle)){std::cout<<"CLOSE-WAIT cycle="<<int(cycle);for(DWORD i=0;i<count;++i){std::cout<<" type="<<int(nodes[i].ObjectType);if(nodes[i].ObjectType==WctThreadType)std::cout<<" tid="<<nodes[i].ThreadObject.ThreadId;}std::cout<<std::endl;}CloseThreadWaitChainSession(session);}
      SetEvent(resume);
    }});
    auto start=std::chrono::steady_clock::now();SendMessageW(window.Handle(),WM_CLOSE,0,0);closeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();SetEvent(resume);watchdog.join();
    Require(!bridge.open&&!IsWindowVisible(window.Handle()),"cross-thread close left panel open");
  }
  std::cout<<"CROSS-THREAD closeMs="<<closeMs<<" ownerPumpPaused=1"<<std::endl;
  Require(closeMs<250,"panel close waited for the game window thread");
  std::cout<<"PASS cross-thread close remains responsive while game window does not pump messages\n";
}
void SavedPreferencesChecks() {
  auto parent=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
  auto folder=parent/("eff-preference-smoke-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
  Require(std::filesystem::create_directory(folder),"preference fixture already exists");
  struct Cleanup {std::filesystem::path folder,parent;~Cleanup(){if(folder.parent_path()==parent)std::filesystem::remove_all(folder);}} cleanup{folder,parent};
  auto p=Make("Saved");p.root=folder/"Saved";p.persistent={"$shape","$switch"};std::filesystem::create_directory(p.root);
  auto author=[&](const std::string& id){auto text="[Mod]\nformat=2\nid="+id+"\n";EiemResourceReloadPlan::Save(p.root/"mod.ini",Bytes(text.begin(),text.end()));};author(p.id);
  // Existing untagged [Values] files remain readable.
  EiemEntryPreferences::Write(p.root/"state.ini",{{"$shape",.6},{"$switch",2}});
  auto graph=Graph({p});auto candidate=Prepare(graph,EiemGlobalConfig{});
  auto compiled=FullResourceReload::CompiledControls(graph,EiemResourceReloadPlan::Plan{},EiemGlobalConfig{});
  Require(candidate.variables.at(p.id)==compiled.variables.at(p.id)&&candidate.variables.at(p.id).at("$shape")==.6&&candidate.panelSnapshot->values==candidate.variables,"new Mod did not restore disk values in both control preparation paths/UI");
  Install({},true);MergeLive(candidate,Sample);Swap(candidate);
  Require(variables.at(p.id).at("$shape")==.6,"hot addition discarded saved preferences");
  variables.at(p.id).at("$shape")=.9;preferences.Capture(p,variables.at(p.id));
  auto changed=p;changed.controls.at("$shape").maximum=.5;
  auto invalid=Prepare(Graph({changed}),EiemGlobalConfig{});MergeLive(invalid,Sample);
  Require(invalid.variables.at(p.id).at("$shape")==0&&preferences.Preview(p).at("$shape")==.9,"rejected/new-domain candidate changed live preference cache or retained invalid value");
  Install({},true);Install({p},true);
  Require(variables.at(p.id).at("$shape")==.9,"delete/readd lost queued preferences");preferences.Flush(Log,true);
  EiemEntryPreferences local;Require(local.Load(p).at("$shape")==.9,"saved runtime values not readable");
  local.Capture(p,{{"$shape",.7},{"$switch",2}});
  auto reused=p;reused.id="Replacement";author(reused.id);
  auto before=EiemDirectory::Read(p.root/"state.ini");local.Flush(Log,true);
  Require(EiemDirectory::Read(p.root/"state.ini")==before&&local.Preview(p).at("$shape")==.7,"old pending owner overwrote replacement author's state");
  auto replacement=Prepare(Graph({reused}),EiemGlobalConfig{});
  Require(replacement.variables.at(reused.id)==reused.defaults,"different Mod ID inherited queued/disk preferences");
  EiemEntryPreferences::Write(p.root/"state.ini",{{"$shape",.6},{"$switch",2}});
  auto previous=std::atomic_load(&published);std::atomic_store(&published,graph);
  auto legacyReplacement=Prepare(Graph({reused}),EiemGlobalConfig{});std::atomic_store(&published,previous);
  Require(legacyReplacement.variables.at(reused.id)==reused.defaults,"runtime ID change inherited untagged legacy values");
  local.Capture(reused,{{"$shape",.4},{"$switch",1}});local.Flush(Log,true);
  EiemEntryPreferences fresh;Require(fresh.Preview(reused).at("$shape")==.4,"replacement owner failed to save its own values");
  local.Capture(reused,{{"$shape",.5},{"$switch",1}});std::filesystem::remove_all(p.root);local.Flush(Log,true);
  Require(!std::filesystem::exists(p.root),"saving deleted Mod recreated its directory");
  Install({},true);
  std::cout<<"PASS hot addition/readdition saved values, legacy state, compiled controls/UI values, rejected candidate cache isolation, changed domains, reused directory ownership, deleted Mod save refusal\n";
}
void KeySettingsChecks(){
  auto folder=std::filesystem::current_path()/"bin"/("control-key-settings-smoke-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
  Require(std::filesystem::create_directory(folder),"key settings fixture already exists");
  struct Cleanup {std::filesystem::path folder;~Cleanup(){std::filesystem::remove_all(folder);}}cleanup{folder};
  auto p=Make("Keys");p.root=folder;const std::string original="\xEF\xBB\xBF; author comment\r\n[Mod]\r\nformat=2\r\nid=Keys\r\n\r\n[Constants]\r\npersist $outfit=0\r\n";
  EiemResourceReloadPlan::Save(folder/"mod.ini",Bytes(original.begin(),original.end()));
  const std::string compiled="unchanged compiled resource bytes";EiemResourceReloadPlan::Save(folder/"compiled.bin",Bytes(compiled.begin(),compiled.end()));
  EiemModKeySettings saves;Require(saves.Preview(p),"old INI default did not enable keys");saves.Capture(p,false);
  Require(!saves.Preview(p),"pending checkbox lost before disk flush");saves.Flush(Log,true);
  auto bytes=EiemDirectory::Read(folder/"mod.ini");auto off=EiemModKeySettings::Rewrite(bytes,p.id,false);
  Require(off==std::string(bytes.begin(),bytes.end())&&off==std::string(original).insert(original.find("[Mod]\r\n")+7,"key_switch_enabled=0\r\n"),"UI save rewrote comments/line endings or changed other declarations");
  auto on=EiemModKeySettings::Rewrite(bytes,p.id,true);Require(on==std::regex_replace(off,std::regex("key_switch_enabled=0"),"key_switch_enabled=1"),"UI enable changed more than the INI flag");
  Require(EiemDirectory::Read(folder/"compiled.bin")==Bytes(compiled.begin(),compiled.end()),"key checkbox rewrote compiled resources");
  p.keySwitchEnabled=false;Require(!saves.Preview(p),"INI disable not restored without cached UI state");
  auto prepared=Prepare(Graph({p}),EiemGlobalConfig{});auto fromCompiled=FullResourceReload::CompiledControls(Graph({p}),EiemResourceReloadPlan::Plan{},EiemGlobalConfig{});
  Require(!prepared.keySwitchEnabled.at(p.id)&&prepared.keySwitchEnabled==fromCompiled.keySwitchEnabled,"compiled and configuration controls disagree on the INI flag");
  saves.Capture(p,true);std::filesystem::remove(folder/"mod.ini");saves.Flush(Log,true);Require(!std::filesystem::exists(folder/"mod.ini"),"UI save recreated a removed Mod INI");
  bool rejected=false;try{EiemDirectory::KeySwitchEnabled("2");}catch(const std::exception&){rejected=true;}Require(rejected,"invalid INI key flag accepted");
  Require(EiemDirectory::KeySwitchEnabled("TRUE")&&!EiemDirectory::KeySwitchEnabled("0"),"INI key boolean parsing differs");
  std::cout<<"PASS per-Mod checkbox INI save, comments/BOM/CRLF preservation, restart/compiled control defaults, no resource rewrite, removed Mod save refusal\n";
}
void WindowChecks(bool demo) {
  Install({Make("lizhiyan"),Make("other")});
  WNDCLASSW wc{};wc.lpfnWndProc=GameProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"EFF_PanelTestOwner";
  RegisterClassW(&wc);HWND owner=CreateWindowW(wc.lpszClassName,L"EFF control panel test",WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,CW_USEDEFAULT,800,760,nullptr,nullptr,wc.hInstance,nullptr);
  Require(owner!=nullptr,"test owner creation");ShowWindow(owner,SW_SHOW);SetForegroundWindow(owner);
  panel.gameWindow=owner;panel.open=true;EiemControlPanel::Window window(panel);
  Require(window.Create(),"native control panel creation");window.Tick(owner);
  // Desktop foreground activation may be refused for a background test process.
  // Inject only the focus observation; the HWNDs/widgets/messages are real.
  auto Pump=[&]{MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Frame();window.Tick(window.Handle());};
  for(int i=0;i<10;++i) {Pump();Sleep(20);}
  Require(IsWindowVisible(window.Handle()),"panel not shown");
  window.Tick(nullptr);Require(!IsWindowVisible(window.Handle()),"external focus did not hide panel");window.Tick(owner);
  auto Click=[&](RECT rect){int x=(rect.left+rect.right)/2,y=(rect.top+rect.bottom)/2;window.TestMouse(x,y,false);Pump();window.TestMouse(x,y,true);Pump();window.TestMouse(x,y,false);Pump();Pump();};
  Click(window.TestRect(Action::Keyboard,"lizhiyan"));Click(window.TestRect(Action::Keyboard,"other"));
  Require(!keySwitchEnabled.at("lizhiyan")&&!keySwitchEnabled.at("other"),"ImGui checkboxes were limited by panel selection");
  Click(window.TestRect(Action::Keyboard,"lizhiyan"));Click(window.TestRect(Action::Keyboard,"other"));
  Require(keySwitchEnabled.at("lizhiyan")&&keySwitchEnabled.at("other"),"ImGui could not enable multiple Mods");
  Click(window.TestRect(Action::Select,""));Click(window.TestRect(Action::Select,"other"));
  Require(selected=="other","ImGui selector not wired to runtime");
  Click(window.TestRect(Action::Invoke,"other",u8"衣装切换"));
  Require(variables.at("other").at("$switch")==1&&variables.at("lizhiyan").at("$switch")==0,"ImGui cycle button not isolated");
  auto before=variables.at("other").at("$shape");Click(window.TestRect(Action::Value,"other","$shape"));
  auto value=variables.at("other").at("$shape");Require(value>=0&&value<=1&&value!=before,"ImGui shape slider not wired");
  std::cout<<"Panel backdrop acrylic="<<window.TestAcrylic()<<"\n";
  if(demo) {auto until=GetTickCount64()+60000;while(GetTickCount64()<until && panel.open){Pump();Sleep(16);}}
  SendMessageW(window.Handle(),WM_CLOSE,0,0);Pump();
  Require(!panel.open && !IsWindowVisible(window.Handle()),"close did not hide control panel");
  DestroyWindow(owner);panel.gameWindow=nullptr;
  std::cout<<"PASS real ImGui/D3D composition, independent checkboxes, Mod dropdown, cycle button, shape slider and close; injected foreground/mouse observation\n";
}
}
int main(int argc,char** argv) {
  using EiemDirectory::Require;
  std::cout.setf(std::ios::unitbuf);
  if(argc==3&&std::string(argv[1])=="--animation-package")try{auto p=EiemDirectory::Load(std::filesystem::u8path(argv[2]));Require(p.shapeAnimations.size()==1,"Python animation package lost clips");const auto& clip=p.shapeAnimations.begin()->second;Require(clip.label==u8"翅膀运动"&&clip.key.vk==VK_F6&&clip.duration==1&&clip.tracks.size()==1,"Python/C++ animation fields differ");EiemOfflinePackage::Writer writer;EiemOfflinePackage::WriteAuthor(writer,p);EiemOfflinePackage::Reader reader{writer.b};auto roundtrip=EiemOfflinePackage::ReadAuthor(reader,p.root.parent_path());Require(reader.p==writer.b.size()&&roundtrip.shapeAnimations.size()==1&&roundtrip.shapeAnimations.begin()->second.tracks==clip.tracks,"compiled author animation roundtrip");std::cout<<"PASS Python/C++ animation package and offline author roundtrip\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
  try {if(argc>1&&std::string(argv[1])=="--cross-thread-window"){TestPanel::CrossThreadWindowChecks();return 0;}TestPanel::Checks();TestPanel::SavedPreferencesChecks();TestPanel::KeySettingsChecks();if(argc>1) TestPanel::WindowChecks(std::string(argv[1])=="--demo");return 0;}
  catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
