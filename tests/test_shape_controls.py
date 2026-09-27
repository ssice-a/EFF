"""Production parser/publication and per-renderer shape ownership tests."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include <windows.h>
#include <sstream>
#include <cstdio>
static void Log(const char *, ...) {}
#include "eiem_mods.h"
#include "eiem_shape_state.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(false)
static const char *ini = R"ini([Constants]
$size=0
[UISize]
path=ui.lua
[KeySize]
key=F6
type=cycle
$size=0,1
[RenderMain]
asset=Body
shape.Inflate=$size
shape_speed.Inflate=0.5
)ini";
int main() {
  EiemModProgram p; std::string error;
  std::istringstream input(ini), other(ini);
  CHECK(EiemModParseStream(input,"a/mod.ini",p,&error));
  CHECK(EiemModParseStream(other,"b/mod.ini",p,&error));
  CHECK(p.states[0].uis.size()==1 && p.rules[0].shapeCount==1);
  CHECK(std::string(p.rules[0].shapeNames[0])=="Inflate");
  CHECK(p.rules[0].shapeSpeedCount==1 &&
        std::string(p.rules[0].shapeSpeedNames[0])=="Inflate" &&
        p.rules[0].shapeSpeeds[0]==.5f);
  EiemPublishModState(p); s_eiemModGeneration=7;
  CHECK(EiemSelectControlledMod("a/mod.ini"));
  EiemModInputEvent slider{{},7,"a/mod.ini","UISize",{{"$size",.6}}};
  EiemModInputEvent keyA{{VK_F6,0},7,"a/mod.ini"};
  EiemModInputEvent keyB{{VK_F6,0},7,"b/mod.ini"};
  EiemModProgram next; std::vector<std::string> affected; bool shapesOnly=false;
  CHECK(EiemPrepareInputUpdate({slider},&next,&affected,&shapesOnly));
  CHECK(shapesOnly && affected.size()==1 && next.rules[0].shapeWeights[0]==.6f);
  CHECK(next.rules[1].shapeWeights[0]==0 && s_eiemModProgram.rules[0].shapeWeights[0]==0);
  CHECK(EiemPrepareInputUpdate({slider,keyA},&next,&affected));
  CHECK(next.rules[0].shapeWeights[0]==0); // unmatched cycle value starts at 0
  CHECK(EiemPrepareInputUpdate({keyB,slider},&next,&affected));
  CHECK(next.rules[0].shapeWeights[0]==.6f && next.rules[1].shapeWeights[0]==0);
  CHECK(EiemSelectControlledMod("b/mod.ini"));
  CHECK(EiemPrepareInputUpdate({keyB,slider},&next,&affected));
  CHECK(next.rules[0].shapeWeights[0]==.6f && next.rules[1].shapeWeights[0]==1);
  slider.generation=6;
  CHECK(!EiemPrepareInputUpdate({slider},&next,&affected));
  slider.generation=7; slider.values["$size"]=1e38;
  CHECK(!EiemPrepareInputUpdate({slider},&next,&affected));
  slider.values={{"$size",.4},{"$unknown",1}};
  CHECK(!EiemPrepareInputUpdate({slider},&next,&affected));
  CHECK(next.states[0].variables.at("$size")==0); // no partial transaction
  EiemModProgram shapeOnly;
  std::istringstream shapeOnlyIni(
    "[Constants]\npersist $wide=0.4\n"
    "[ShapeControlWidth]\nvariable=$wide\nlabel=Width\nmin=0.2\nmax=0.8\n"
    "[RenderShape]\nasset=Cloth\nshape.Width=$wide\n");
  CHECK(EiemModParseStream(shapeOnlyIni,"shape/mod.ini",shapeOnly,&error));
  EiemPublishModState(shapeOnly); s_eiemModGeneration=8;
  LONG shapeGeneration=0,controlGeneration=0;
  auto shapeControls=EiemGetModControls(&shapeGeneration,&controlGeneration);
  CHECK(shapeControls.size()==1 && shapeControls[0].keys.empty() &&
        shapeControls[0].shapeControls.size()==1 && shapeControls[0].selected);
  EiemModInputEvent direct;
  direct.generation=8; direct.modPath="shape/mod.ini";
  direct.values={{"$wide",0.7}}; direct.directValues=true;
  CHECK(EiemGetSelectedModPath()=="shape/mod.ini");
  CHECK(EiemPrepareInputUpdate({direct},&next,&affected));
  CHECK(next.rules[0].shapeWeights[0]==.7f);
  direct.values={{"$wide",0.9}};
  CHECK(!EiemPrepareInputUpdate({direct},&next,&affected)); // above authored max
  for (const char *bad : {
    "[RenderA]\nasset=A\nshape.X=1e38\n",
    "[Constants]\n$x=0\n[KeyA]\nkey=F6\ntype=cycle\n$x=0,1e38\n[RenderA]\nasset=A\nshape.X=$x\n",
    "[RenderA]\nasset=A\nshape.X=$missing\n",
    "[RenderA]\nasset=A\nif false\nshape.X=$missing\nendif\n",
    "[RenderA]\nasset=A\nshape.=0\n",
    "[RenderA]\nasset=A\nshape.X=nan\n",
    "[RenderA]\nasset=A\nshape.X=1\nshape_speed.X=0\n",
    "[RenderA]\nasset=A\nshape.X=1\nshape_speed.=1\n",
    "[UIA]\npath=../out.lua\nkey=F8\n",
    "[UIA]\npath=C:/out.lua\nkey=F8\n",
    "[UIA]\npath=/out.lua\nkey=F8\n",
    "[UIA]\npath=ui.lua\nkey=INSERT\n",
    "[UIA]\npath=ui.lua\nkey=F8\ncondition=$missing\n",
    "[UIA]\npath=ui.lua\nkey=F8\nkey=F9\n",
    "[UIA]\n",
    "[UIA]\npath=ui.lua\npath=other.lua\n",
    "[Constants]\n$x=0\n[KeyX]\nkey=F8\ntype=cycle\nscope=invalid\n$x=0,1\n",
    "[Constants]\n$x=0\n[KeyX]\nkey=F8\ntype=cycle\nscope=ui\nscope=both\n$x=0,1\n",
    "[Constants]\n$x=0\n[KeyX]\nkey=F8\ntype=cycle\n$x=0,1\n[UIX]\npath=ui.lua\nkey=F8\n",
    "[Constants]\n$x=0\n[SliderA]\nvariable=$x\nmin=1\nmax=0\n",
    "[Constants]\n$x=2\n[SliderA]\nvariable=$x\n",
    "[SliderA]\nvariable=$missing\n",
    "[Constants]\n$x=0\n[SliderA]\nvariable=$x\nvariable=$x\n"}) {
    EiemModProgram invalid; std::istringstream stream(bad);
    CHECK(!EiemModParseStream(stream,"bad/mod.ini",invalid,&error));
    CHECK(invalid.states.empty());
  }
  for (const char *bad : {
    "[Constants]\n$x=0\n[ShapeControlA]\nvariable=$x\nlabel=X\nmin=1\nmax=0\n[RenderA]\nasset=A\nshape.X=$x\n",
    "[Constants]\n$x=0\n[ShapeControlA]\nvariable=$x\nlabel=X\nmin=0\nmax=1\n[ShapeControlB]\nvariable=$x\nlabel=Y\nmin=0\nmax=1\n[RenderA]\nasset=A\nshape.X=$x\n"}) {
    EiemModProgram invalid; std::istringstream stream(bad);
    CHECK(!EiemModParseStream(stream,"bad/mod.ini",invalid,&error));
  }
  std::istringstream stale(
    "[Constants]\n$x=0\n$old=0\n"
    "[ShapeControlA]\nvariable=$x\nlabel=X\nmin=0\nmax=1\n"
    "[ShapeControlOld]\nvariable=$old\nlabel=Old\nmin=0\nmax=1\n"
    "[RenderA]\nasset=A\nshape.X=$x\n");
  EiemModProgram staleProgram;
  CHECK(EiemModParseStream(stale, "stale/mod.ini", staleProgram, &error));
  CHECK(staleProgram.states[0].shapeControls.size() == 1);
  CHECK(staleProgram.states[0].shapeControls[0].variable == "$x");
  std::istringstream conditional(R"ini([Constants]
$x=0
[UIX]
path=ui.lua
[RenderA]
asset=A
shape.X=$x
if $x > 0.5
shape.X=
handling=skip
endif
)ini");
  EiemModProgram q;
  CHECK(EiemModParseStream(conditional,"c/mod.ini",q,&error));
  // UI declarations need no key. Any key can update a variable; script decides
  // how to use it. Focus scope applies to ordinary keys, not special UI actions.
  std::istringstream conditions("[Constants]\n$show=0\n[UIA]\npath=ui.lua\n"
    "[KeyShow]\nkey=F9\ntype=cycle\nscope=both\n$show=0,1\n");
  EiemModProgram conditionDoc;
  CHECK(EiemModParseStream(conditions,"condition.ini",conditionDoc,&error));
  CHECK(EiemCycleModKey(conditionDoc,{VK_F9,0},true).size()==1);
  CHECK(conditionDoc.states[0].variables.at("$show")==1);
  conditionDoc.states[0].keys[0].scope=EiemKeyScope::Game;
  CHECK(EiemCycleModKey(conditionDoc,{VK_F9,0},true).empty());
  CHECK(EiemCycleModKey(conditionDoc,{VK_F9,0},false).size()==1);
  EiemPublishModState(q);
  s_eiemModGeneration=7;
  EiemSelectControlledMod("c/mod.ini");
  CHECK(EiemPrepareInputUpdate({{{},7,"c/mod.ini","UIX",{{"$x",1}}}},&next,&affected,&shapesOnly));
  CHECK(!shapesOnly && next.rules[0].shapeCount==0);
  // Tiny values must not be rounded away by string formatting.
  std::istringstream tiny("[RenderA]\nasset=A\nshape.X=0.00000001\n");
  EiemModProgram precise;
  CHECK(EiemModParseStream(tiny,"tiny.ini",precise,&error));
  CHECK(precise.rules[0].shapeWeights[0] == 1e-8f);
  struct Mesh { std::vector<std::string> names; } m1{{"Other","Inflate"}}, m2{{"Inflate","Other"}};
  struct Renderer { std::vector<float> weights; unsigned writes=0; bool fail=false; } a{{30,20}}, b{{40,70}};
  struct Backend {
    int Index(void *mesh, const std::string &name) {
      auto &names=((Mesh*)mesh)->names;
      auto it=std::find(names.begin(),names.end(),name);
      return it==names.end() ? -1 : (int)(it-names.begin());
    }
    bool Read(void *r,int index,float &value) { value=((Renderer*)r)->weights.at(index); return true; }
    bool Write(void *r,int index,float value) {
      auto &renderer=*(Renderer*)r;
      if(renderer.fail) return false;
      renderer.weights.at(index)=value; ++renderer.writes; return true;
    }
  } backend;
  EiemShapeState sa,sb; EiemModRule rule{};
  CHECK(EiemSetRenderField(rule,"shape.Inflate",".5",error));
  CHECK(EiemApplyShapeWeights(&a,&m1,rule,sa,backend,error));
  CHECK(EiemApplyShapeWeights(&b,&m1,rule,sb,backend,error));
  CHECK(a.weights[0]==30 && b.weights[0]==40 && a.weights[1]==50 && b.weights[1]==50);
  CHECK(EiemApplyShapeWeights(&a,&m1,rule,sa,backend,error) && a.writes==1);
  EiemModRule empty{};
  CHECK(EiemApplyShapeWeights(&a,&m1,empty,sa,backend,error));
  CHECK(EiemApplyShapeWeights(&b,&m1,empty,sb,backend,error));
  CHECK(a.weights[1]==20 && b.weights[1]==70); // independent instance baselines
  CHECK(EiemApplyShapeWeights(&a,&m2,rule,sa,backend,error));
  CHECK(a.weights[0]==50 && a.weights[1]==20); // remap by name on new Mesh
  CHECK(EiemApplyShapeWeights(&a,&m2,empty,sa,backend,error) && a.weights[0]==30);
  CHECK(EiemSetRenderField(rule,"shape.Missing","1",error));
  auto writes=a.writes;
  CHECK(!EiemApplyShapeWeights(&a,&m2,rule,sa,backend,error) && a.writes==writes);
  CHECK(EiemSetRenderField(rule,"shape.Missing","",error));
  a.fail=true;
  CHECK(!EiemApplyShapeWeights(&a,&m2,rule,sa,backend,error));
  a.fail=false;
  CHECK(EiemApplyShapeWeights(&a,&m2,empty,sa,backend,error));
  EiemModRule animated{};
  CHECK(EiemSetRenderField(animated,"shape.Inflate","1",error));
  CHECK(EiemSetRenderField(animated,"shape_speed.Inflate",".5",error));
  a.weights[0]=0;
  CHECK(EiemApplyShapeWeights(&a,&m2,animated,sa,backend,error));
  CHECK(a.weights[0]==0 && EiemShapeStateAnimating(sa));
  CHECK(EiemApplyShapeWeights(&a,&m2,animated,sa,backend,error,.5f));
  CHECK(a.weights[0]==25 && EiemShapeStateAnimating(sa));
  a.fail=true;
  CHECK(!EiemApplyShapeWeights(&a,&m2,animated,sa,backend,error,1.5f));
  CHECK(a.weights[0]==25 && EiemShapeStateAnimating(sa));
  a.fail=false;
  CHECK(EiemApplyShapeWeights(&a,&m2,animated,sa,backend,error,1.5f));
  CHECK(a.weights[0]==100 && !EiemShapeStateAnimating(sa));
  return 0;
}
'''


class ShapeControlsTests(unittest.TestCase):
    def test_parser_and_ordered_input(self):
        if not shutil.which("cl"):
            self.skipTest("Requires MSVC developer environment")
        with tempfile.TemporaryDirectory(prefix="eiem-shapes-") as folder:
            folder = Path(folder)
            source = folder / "test.cpp"
            source.write_text(SOURCE, encoding="utf-8")
            exe = folder / "test.exe"
            result = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                     f"/I{ROOT / 'src'}", str(source), f"/Fe{exe}"],
                                    cwd=folder, capture_output=True, text=True, encoding="utf-8", errors="replace")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__": unittest.main()
