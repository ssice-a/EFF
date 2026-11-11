#include "../../src/eiem_author_controls_ini.h"
#include <iostream>
using namespace EiemDirectory;
namespace P=EiemControlProgram;
std::vector<Section> Ini(const std::string& text){return ParseIni(Bytes(text.begin(),text.end()));}
std::shared_ptr<P::Program> Build(const std::string& text,EiemVariables& values){auto ini=Ini(text);std::set<std::string> saved;auto p=P::Declare(ini,values,saved);P::Compile(*p,ini,values,{"Spin"});return p;}
void Reject(const std::string& text){EiemVariables values;bool failed=false;try{Build(text,values);}catch(const std::exception&){failed=true;}Require(failed,"invalid program accepted");}
int main(){try{
 const std::string source=R"ini([ParameterEnabled]
variable=$enabled
type=bool
default=true
public=true
[ParameterValue]
variable=$value
default=2
min=0
max=10
public=true
persist=true
[ParameterEffective]
variable=$result
value=$enabled * ($value + 1)
[ActionToggle]
label=Toggle
public=true
code=
    if $enabled
        $enabled = false
    else
        $enabled = true
        if $value < 4
            $value = $value + 2 * 1.5
        endif
        play Spin
    endif
[ActionStep]
code=
    $value = $value + $dt
    call ActionToggle
[EventToggle]
key=F7
action=ActionToggle
[UIRoot]
type=group
label=Controls
[UISwitch]
type=checkbox
parent=UIRoot
variable=$enabled
[UIButton]
type=button
parent=UIRoot
action=ActionToggle
)ini";
 EiemVariables values;auto p=Build(source,values);Require(P::Evaluate(*p,values).at("$result")==3,"derived evaluation");
 auto off=P::Execute(*p,"ActionToggle",values,0);Require(off.values.at("$enabled")==0&&off.commands.empty(),"if branch");
 auto on=P::Execute(*p,"ActionToggle",off.values,0);Require(on.values.at("$enabled")==1&&on.values.at("$value")==5&&on.commands.size()==1,"else/nested branch");
 Require(values.at("$value")==2&&values.at("$enabled")==1,"execution mutated source");
 Require(p->rawSections.at("ActionToggle").find("$enabled = true")!=std::string::npos,"source lost");
 std::string error;auto math=P::Expression("-2 * -3 + 8 / 2 - 5 % 2",{},"test");Require(math->Evaluate({})==9,"arithmetic precedence");
 Require(P::Expression("true || 1 / 0",{},"test")->Evaluate({})==1,"short circuit");
 auto step=P::Execute(*p,"ActionStep",values,.5);Require(step.values.at("$value")==2.5,"duration/call");
 Reject("[ParameterX]\nvariable=$x\ntype=bool\ndefault=2\n");
 Reject("[ParameterX]\nvariable=$x\nvalue=$y\n[ParameterY]\nvariable=$y\nvalue=$x\n");
 Reject("[ActionA]\ncode=call ActionB\n[ActionB]\ncode=call ActionA\n");
 Reject("[UIA]\ntype=group\nparent=UIB\n[UIB]\ntype=group\nparent=UIA\n");
 Reject("[ActionBad]\ncode=if true\n    $missing = 1\n");
 Reject("[ParameterX]\nvariable=$x\nvalue=1\n[ActionBad]\ncode=$x=2\n");
 Reject("[UIX]\ntype=group\norder=0.5\n");
 std::string expansion=source+"[ActionRepeat0]\ncode=$enabled = false\n";
 for(size_t i=1;i<=17;++i)expansion+="[ActionRepeat"+std::to_string(i)+"]\ncode=\n    call ActionRepeat"+std::to_string(i-1)+"\n    call ActionRepeat"+std::to_string(i-1)+"\n";
 EiemVariables expansionValues;auto expansionProgram=Build(expansion,expansionValues);bool limited=false;try{P::Execute(*expansionProgram,"ActionRepeat17",expansionValues,0);}catch(const std::exception& e){limited=std::string(e.what()).find("budget")!=std::string::npos;}
 Require(limited&&expansionValues.at("$enabled")==1,"action expansion exceeded budget or partially committed");
 const auto material=source+"[MaterialExample]\npath=example.effres\n[BindingValue]\ntarget=material.float\nmaterial=MaterialExample\nproperty=_Example\nvalue=$result\n";
 auto materialSource=material;auto play=materialSource.find("        play Spin");materialSource.erase(play,std::string("        play Spin\n").size());
 auto state=EiemAuthorControlsIni::Read(Ini(materialSource),[](const auto&){return EiemAuthorControlsIni::Samples{};},[](const auto&){return EiemAuthorResourceMetadata::Effect{};});
 Require(state.program&&state.materialTracks.size()==1&&state.materialTracks.begin()->second.property=="_Example","author adapter");
 bool failed=false;try{auto bad=P::Expression("1 / 0",{},"test");bad->Evaluate({});}catch(const std::exception&){failed=true;}Require(failed,"division zero accepted");
 std::cout<<"PASS control program: ordered actions, arithmetic, typed parameters, UI, cycles, material binding, source preservation\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
