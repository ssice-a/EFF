#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
#include <thread>

namespace TestPanel {
using namespace Entry;
using namespace RuntimeControls;
using EiemControlPanel::Action;
UINT physical=0;
bool Sample(const EiemKeyChord& chord) { return physical==chord.vk; }
Package Make(const std::string& id) {
  Package p;p.id=id;p.root=std::filesystem::path("E:/EIEM_Workspace/panel-fixture")/id;
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
void Checks() {
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
  Require(variables.at("A").at("$switch")==1 && variables.at("B").at("$switch")==0,"same key changed unselected Mod or repeated");
  Require(immutable->values.at("A").at("$switch")==0,"published UI snapshot mutated");
  panel.open=true;Queue(Action::Select,"B");Frame();
  panel.open=false;Frame();Require(selected=="B" && variables.at("B").at("$switch")==0,"selection manufactured held edge");
  physical=0;Frame();physical=VK_NUMPAD1;Frame();
  Require(variables.at("B").at("$switch")==1 && variables.at("A").at("$switch")==1,"selected Mod key isolation");
  panel.open=true;physical=0;Frame();physical=VK_NUMPAD1;Frame();
  Require(variables.at("B").at("$switch")==1,"panel physical shortcut not paused");
  Queue(Action::Invoke,"A",u8"衣装切换");Queue(Action::Invoke,"B",u8"衣装切换");Frame();
  Require(variables.at("A").at("$switch")==1 && variables.at("B").at("$switch")==2,"panel invocation escaped selection");
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
  Require(variables.at("A").at("$switch")==1,"disabled keyboard changed variables");
  panel.open=true;Queue(Action::Invoke,"A",u8"衣装切换");Frame();
  Require(variables.at("A").at("$switch")==2,"keyboard disable incorrectly disabled panel controls");
  Queue(Action::Keyboard,"A",{},1);Frame();
  auto beforeReload=panel.Read();Queue(Action::Value,"A","$shape",.9,beforeReload);
  auto a=Make("A");a.persistent={"$shape"};
  auto b=Make("B");Install({a,b},true);Frame();
  Require(selected=="A" && keyboard && variables.at("A").at("$shape")==0,"reload accepted stale command");
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
  bool rejected=false;try{Prepare(Graph({reserved}),EiemGlobalConfig{});}catch(const std::exception&){rejected=true;}
  Require(rejected,"modifier chord overlaps reserved Insert");
  // Store publication and UI submissions use only immutable data across threads.
  EiemControlPanel::Store bridge;std::atomic_bool done=false;
  std::thread producer([&]{for(int i=0;i<2000;++i){auto s=std::make_shared<EiemControlPanel::Snapshot>();s->revision=i;bridge.Publish(s);}done=true;});
  while(!done) {auto s=bridge.Read();if(s) bridge.Submit({s->revision,0,Action::Select,"A"});bridge.Drain();}
  producer.join();Require(bridge.Read()->revision==1999,"cross-thread snapshot publication");
  panel.open=false;physical=0;
  std::cout<<"PASS selected Mod input isolation, default/reload/removal selection, held edges, immutable snapshots, stale graph/scope rejection, GUI pause/buttons/sliders/hold, focus/close, keyboard disable, persistence, reserved keys, concurrent publication\n";
}
LRESULT CALLBACK GameProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {return DefWindowProcW(hwnd,msg,wp,lp);}
void SavedPreferencesChecks() {
  auto folder=std::filesystem::path("E:/")/("eff-preference-smoke-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
  Require(std::filesystem::create_directory(folder),"preference fixture already exists");
  struct Cleanup {std::filesystem::path folder;~Cleanup(){std::filesystem::remove_all(folder);}} cleanup{folder};
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
  auto combo=GetDlgItem(window.Handle(),100);Require(SendMessageW(combo,CB_GETCOUNT,0,0)==2,"native Mod selector missing entries");
  SendMessageW(combo,CB_SETCURSEL,1,0);SendMessageW(window.Handle(),WM_COMMAND,MAKEWPARAM(100,CBN_SELCHANGE),reinterpret_cast<LPARAM>(combo));Pump();
  Require(selected=="other","native selector not wired to runtime");
  SendMessageW(GetDlgItem(window.Handle(),200),BM_CLICK,0,0);Pump();
  Require(variables.at("other").at("$switch")==1 && variables.at("lizhiyan").at("$switch")==0,"native cycle button not isolated");
  auto slider=GetDlgItem(window.Handle(),203);Require(slider!=nullptr,"native shape slider missing");
  SendMessageW(slider,TBM_SETPOS,TRUE,3500);SendMessageW(window.Handle(),WM_HSCROLL,TB_THUMBPOSITION,reinterpret_cast<LPARAM>(slider));Pump();
  Require(std::abs(variables.at("other").at("$shape")-.35)<.00001,"native slider not wired");
  if(demo) {auto until=GetTickCount64()+60000;while(GetTickCount64()<until && panel.open){Pump();Sleep(16);}}
  SendMessageW(window.Handle(),WM_CLOSE,0,0);Pump();
  Require(!panel.open && !IsWindowVisible(window.Handle()),"close did not hide control panel");
  DestroyWindow(owner);panel.gameWindow=nullptr;
  std::cout<<"PASS real Win32 panel creation, Mod dropdown, cycle button, shape trackbar and close; injected foreground observation\n";
}
}
int main(int argc,char** argv) {
  try {TestPanel::Checks();TestPanel::SavedPreferencesChecks();if(argc>1) TestPanel::WindowChecks(std::string(argv[1])=="--demo");return 0;}
  catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
