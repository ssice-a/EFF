#pragma once
#include "eiem_expression.h"
#include "eiem_keys.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <cstdint>

// Shared by the DLL and author tools. No Unity, window, filesystem, or input
// sampling is performed here. INI remains the only persisted program.
namespace EiemControlProgram {
using Expr=std::shared_ptr<EiemExpression>;
inline void Check(bool ok,const std::string& error){if(!ok)throw std::runtime_error(error);}
inline std::string Trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");return a==s.npos?std::string{}:s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
inline bool Starts(const std::string& s,const char* prefix){return s.rfind(prefix,0)==0;}
inline std::set<std::string> Dependencies(const Expr& e){std::set<std::string> result;std::vector<const EiemExpression*> todo;if(e)todo.push_back(e.get());while(!todo.empty()){auto n=todo.back();todo.pop_back();if(n->op==EiemExpression::Variable)result.insert(n->variable);if(n->left)todo.push_back(n->left.get());if(n->right)todo.push_back(n->right.get());}return result;}
inline Expr Expression(const std::string& text,const EiemVariables& declared,const std::string& where){std::string error;auto e=EiemExpressionParser(text,error).Parse();Check(e&&e->Validate(declared,error),where+": "+error);return e;}
enum class Type { Float, Integer, Boolean };
struct Parameter {
  std::string id,variable,label,description,group;
  Type type=Type::Float;
  double initial=0,minimum=-std::numeric_limits<double>::max(),maximum=std::numeric_limits<double>::max(),softMin=0,softMax=1;
  bool exposed=false,persistent=false,retain=true;
  Expr computed;
  void Validate(double value)const{Check(std::isfinite(value)&&value>=minimum&&value<=maximum,"parameter outside domain: "+variable);Check(type==Type::Float||value==std::trunc(value),"parameter requires integral value: "+variable);Check(type!=Type::Boolean||value==0||value==1,"parameter requires boolean value: "+variable);}
};
struct Instruction {
  enum Op { Assign, Call, Play, Pause, Stop, If, Jump } op=Assign;
  std::string target,where;Expr expression;size_t jump=0;
};
struct Action {std::string id,label;bool exposed=false;std::vector<Instruction> code;};
struct Event {std::string id,action,trigger="press";EiemKeyChord key;};
struct Binding {std::string id,target,part,material,property,channel,variable;Expr value;};
struct Ui {std::string id,type,parent,label,variable,action,description;int order=0;bool expanded=false;Expr visible,enabled;};
struct Program {
  std::map<std::string,Parameter> parameters;
  std::map<std::string,Action> actions;
  std::vector<Event> events;
  std::vector<Binding> bindings;
  std::map<std::string,Ui> ui;
  std::map<std::string,std::vector<std::string>> children;
  std::vector<std::string> derivedOrder;
  std::map<std::string,std::string> rawSections;
};
inline bool Boolean(const std::string& s){Check(s=="1"||s=="0"||s=="true"||s=="false","expected true/false: "+s);return s=="1"||s=="true";}
inline double Number(const std::string& s){double n=0;Check(EiemNumber(s,&n),"expected finite number: "+s);return n;}
template<class Section>std::shared_ptr<Program> Declare(const std::vector<Section>& sections,EiemVariables& defaults,std::set<std::string>& persistent){
  auto p=std::make_shared<Program>();
  for(const auto& section:sections)if(Starts(section.id,"Parameter")){
    try{const auto& v=section.values;Parameter d;d.id=section.id;d.variable=v.at("variable");Check(EiemVariableName(d.variable)&&d.variable!="$dt","invalid parameter variable");
      auto get=[&](const char* k,const std::string& fallback){auto f=v.find(k);return f==v.end()?fallback:f->second;};
      auto type=get("type","float");Check(type=="float"||type=="int"||type=="bool","unsupported parameter type: "+type);d.type=type=="bool"?Type::Boolean:type=="int"?Type::Integer:Type::Float;
      d.label=get("label",d.variable);d.description=get("description","");d.group=get("group","");d.exposed=Boolean(get("public","false"));d.persistent=Boolean(get("persist",persistent.count(d.variable)?"true":"false"));
      Check(!persistent.count(d.variable)||d.persistent,"parameter persistence disagrees with Constants: "+d.variable);
      auto reload=get("reload","preserve");Check(reload=="preserve"||reload=="reset","invalid parameter reload policy");d.retain=reload=="preserve";
      if(d.type==Type::Boolean){d.minimum=0;d.maximum=1;}if(v.count("min"))d.minimum=Number(v.at("min"));if(v.count("max"))d.maximum=Number(v.at("max"));Check(d.minimum<=d.maximum,"invalid parameter range");
      d.initial=Number(get("default",defaults.count(d.variable)?std::to_string(defaults.at(d.variable)):"0"));d.Validate(d.initial);
      d.softMin=Number(get("ui.min",std::to_string(v.count("min")?d.minimum:0.0)));d.softMax=Number(get("ui.max",std::to_string(v.count("max")?d.maximum:std::max(1.0,d.initial))));Check(d.softMin<=d.softMax&&d.softMin>=d.minimum&&d.softMax<=d.maximum,"invalid UI soft range");
      Check(!defaults.count(d.variable)||defaults.at(d.variable)==d.initial,"parameter default disagrees with Constants: "+d.variable);defaults[d.variable]=d.initial;
      if(d.persistent)persistent.insert(d.variable);Check(p->parameters.emplace(d.variable,std::move(d)).second,"duplicate parameter variable");
    }catch(const std::exception& e){throw std::runtime_error("["+section.id+"] line "+std::to_string(section.line)+": "+e.what());}
  }return p;
}
inline void PrepareDerived(Program& p){
  std::map<std::string,size_t> count;std::map<std::string,std::vector<std::string>> users;
  for(const auto& row:p.parameters)if(row.second.computed){count[row.first]=0;for(const auto& dep:Dependencies(row.second.computed))if(p.parameters.count(dep)&&p.parameters.at(dep).computed){++count[row.first];users[dep].push_back(row.first);}}
  std::vector<std::string> ready;for(const auto& row:count)if(!row.second)ready.push_back(row.first);
  for(size_t i=0;i<ready.size();++i){p.derivedOrder.push_back(ready[i]);for(const auto& user:users[ready[i]])if(--count[user]==0)ready.push_back(user);}
  Check(ready.size()==count.size(),"computed parameter dependency cycle");
}
inline EiemVariables Evaluate(const Program& p,EiemVariables values){for(const auto& name:p.derivedOrder){const auto& d=p.parameters.at(name);auto value=d.computed->Evaluate(values);d.Validate(value);values[name]=value;}return values;}
template<class Section>void Compile(Program& p,const std::vector<Section>& sections,EiemVariables& defaults,const std::set<std::string>& clips){
  auto declared=defaults;declared["$dt"]=0;
  for(const auto& section:sections)try{
    const auto& v=section.values;auto get=[&](const char* k,const std::string& fallback=std::string{}){auto f=v.find(k);return f==v.end()?fallback:f->second;};
    if(Starts(section.id,"Parameter")){auto& d=p.parameters.at(v.at("variable"));if(v.count("value")){Check(!d.persistent,"computed parameter cannot be persisted");d.computed=Expression(v.at("value"),defaults,section.id);}}
    else if(Starts(section.id,"Action")){
      Action a;a.id=section.id;a.label=get("label",a.id);a.exposed=Boolean(get("public","false"));std::istringstream input(v.at("code"));std::string line;size_t lineNumber=0;
      struct Branch{size_t condition;size_t otherwise=SIZE_MAX;};std::vector<Branch> branches;
      while(std::getline(input,line)){++lineNumber;line=Trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;Instruction n;n.where="["+a.id+"] code line "+std::to_string(lineNumber);
        if(Starts(line,"if ")){n.op=Instruction::If;n.expression=Expression(Trim(line.substr(3)),declared,n.where);branches.push_back({a.code.size()});}
        else if(line=="else"){Check(!branches.empty()&&branches.back().otherwise==SIZE_MAX,n.where+": unexpected else");n.op=Instruction::Jump;auto& b=branches.back();b.otherwise=a.code.size();a.code[b.condition].jump=a.code.size()+1;}
        else if(line=="endif"){Check(!branches.empty(),n.where+": unexpected endif");auto b=branches.back();branches.pop_back();a.code[b.otherwise==SIZE_MAX?b.condition:b.otherwise].jump=a.code.size();continue;}
        else if(Starts(line,"call ")||Starts(line,"play ")||Starts(line,"pause ")||Starts(line,"stop ")){auto at=line.find(' ');auto op=line.substr(0,at);n.target=Trim(line.substr(at+1));n.op=op=="call"?Instruction::Call:op=="play"?Instruction::Play:op=="pause"?Instruction::Pause:Instruction::Stop;if(n.op!=Instruction::Call)Check(clips.count(n.target),n.where+": animation absent: "+n.target);}
        else{auto at=line.find('=');Check(at!=line.npos,n.where+": expected assignment or command");n.target=Trim(line.substr(0,at));Check(defaults.count(n.target)&&n.target!="$dt",n.where+": undeclared assignment: "+n.target);n.expression=Expression(Trim(line.substr(at+1)),declared,n.where);}
        a.code.push_back(std::move(n));
      }Check(branches.empty(),a.id+": missing endif");Check(!a.code.empty(),a.id+": empty action");p.actions.emplace(a.id,std::move(a));
    }else if(Starts(section.id,"Event")){Event e;e.id=section.id;e.action=v.at("action");e.trigger=get("trigger","press");Check(e.trigger=="press"||e.trigger=="release"||e.trigger=="hold","invalid event trigger");Check(EiemParseKeyChord(v.at("key"),&e.key),"invalid event key");p.events.push_back(std::move(e));}
    else if(Starts(section.id,"Binding")){Binding b;b.id=section.id;b.target=v.at("target");b.part=get("part");b.material=get("material");b.property=get("property");b.channel=get("channel");
      Check(b.target=="material.float"||b.target=="part.visible"||b.target=="shape.weight","unsupported binding target: "+b.target);b.value=Expression(v.at("value"),defaults,section.id);b.variable="$binding_"+section.id;
      Check(!defaults.count(b.variable),"generated binding variable collision");defaults[b.variable]=0;Parameter d;d.variable=b.variable;d.computed=b.value;p.parameters.emplace(b.variable,std::move(d));p.bindings.push_back(std::move(b));
    }else if(Starts(section.id,"UI")){Ui u;u.id=section.id;u.type=v.at("type");u.parent=get("parent");u.label=get("label",u.id);u.variable=get("variable");u.action=get("action");u.description=get("description");auto order=Number(get("order","0"));Check(order==std::trunc(order)&&order>=std::numeric_limits<int>::min()&&order<=std::numeric_limits<int>::max(),"UI order requires a 32-bit integer");u.order=int(order);u.expanded=Boolean(get("expanded","false"));
      Check(u.type=="group"||u.type=="row"||u.type=="tabs"||u.type=="tab"||u.type=="slider"||u.type=="number"||u.type=="checkbox"||u.type=="button"||u.type=="text","unsupported UI type: "+u.type);
      if(v.count("visible"))u.visible=Expression(v.at("visible"),defaults,section.id);if(v.count("enabled"))u.enabled=Expression(v.at("enabled"),defaults,section.id);p.ui.emplace(u.id,std::move(u));
    }
    if(Starts(section.id,"Parameter")||Starts(section.id,"Action")||Starts(section.id,"Event")||Starts(section.id,"Binding")||Starts(section.id,"UI"))p.rawSections[section.id]=section.source;
  }catch(const std::exception& e){throw std::runtime_error("["+section.id+"] line "+std::to_string(section.line)+": "+e.what());}
  PrepareDerived(p);
  for(const auto& a:p.actions)for(const auto& n:a.second.code){if(n.op==Instruction::Call)Check(p.actions.count(n.target),n.where+": action absent: "+n.target);if(n.op==Instruction::Assign&&p.parameters.count(n.target))Check(!p.parameters.at(n.target).computed,n.where+": computed parameter is read only");}
  // Validate call and UI-parent graphs before any execution or drawing.
  auto acyclic=[](const std::map<std::string,std::vector<std::string>>& edges){std::map<std::string,size_t> degree;for(const auto& row:edges){degree.emplace(row.first,0);for(const auto& to:row.second)++degree[to];}std::vector<std::string> ready;for(const auto& row:degree)if(!row.second)ready.push_back(row.first);for(size_t i=0;i<ready.size();++i){auto f=edges.find(ready[i]);if(f!=edges.end())for(const auto& to:f->second)if(--degree[to]==0)ready.push_back(to);}return ready.size()==degree.size();};
  std::map<std::string,std::vector<std::string>> calls;for(const auto& a:p.actions){calls[a.first];for(const auto& n:a.second.code)if(n.op==Instruction::Call)calls[a.first].push_back(n.target);}Check(acyclic(calls),"action call cycle");
  for(const auto& e:p.events)Check(p.actions.count(e.action),e.id+": action absent");
  std::map<std::string,std::vector<std::string>> parents;
  for(const auto& row:p.ui){const auto& u=row.second;
    if(!u.parent.empty()){Check(p.ui.count(u.parent),u.id+": UI parent absent");const auto& type=p.ui.at(u.parent).type;Check(type=="group"||type=="row"||type=="tabs"||type=="tab",u.id+": parent is not a layout");Check((type=="tabs")== (u.type=="tab"),u.id+": tab must belong to tabs");}
    else Check(u.type!="tab",u.id+": tab requires tabs parent");
    if(u.type=="slider"||u.type=="number"||u.type=="checkbox"){Check(p.parameters.count(u.variable)&&p.parameters.at(u.variable).exposed,u.id+": parameter must be public");Check(u.type!="checkbox"||p.parameters.at(u.variable).type==Type::Boolean,u.id+": checkbox requires bool");}
    if(u.type=="button")Check(p.actions.count(u.action)&&p.actions.at(u.action).exposed,u.id+": action must be public");
    if(u.type=="text"&&!u.variable.empty())Check(defaults.count(u.variable),u.id+": display variable absent");
    parents[u.id];if(!u.parent.empty())parents[u.parent].push_back(u.id);p.children[u.parent].push_back(u.id);
  }Check(acyclic(parents),"UI parent cycle");
  for(const auto& row:p.ui){size_t depth=0;auto node=&row.second;while(!node->parent.empty()){Check(++depth<=64,"UI nesting exceeds 64 levels");node=&p.ui.at(node->parent);}if(row.second.type=="row")Check(p.children[row.first].size()<=64,"UI row exceeds 64 columns");}
  for(auto& row:p.children)std::stable_sort(row.second.begin(),row.second.end(),[&](const auto& a,const auto& b){return std::tie(p.ui.at(a).order,a)<std::tie(p.ui.at(b).order,b);});
  auto initial=Evaluate(p,defaults);for(const auto& row:p.parameters)row.second.Validate(initial.at(row.first));
}
struct Result {EiemVariables values;std::set<std::string> written;std::vector<std::pair<Instruction::Op,std::string>> commands;};
inline Result Execute(const Program& p,const std::string& action,const EiemVariables& base,double elapsed,const EiemVariables* effective=nullptr){
  Check(std::isfinite(elapsed)&&elapsed>=0,"invalid event duration");Result out;out.values=base;auto evaluated=Evaluate(p,effective?*effective:base);evaluated["$dt"]=elapsed;
  struct Frame{const Action* action;size_t pc=0;};std::vector<Frame> stack{{&p.actions.at(action)}};
  size_t budget=65536;
  while(!stack.empty()){Check(budget--!=0,"action instruction budget exceeded");auto& frame=stack.back();if(frame.pc==frame.action->code.size()){stack.pop_back();continue;}const auto& n=frame.action->code[frame.pc++];
    if(n.op==Instruction::If){if(!n.expression->Evaluate(evaluated))frame.pc=n.jump;}
    else if(n.op==Instruction::Jump)frame.pc=n.jump;
    else if(n.op==Instruction::Call)stack.push_back({&p.actions.at(n.target)});
    else if(n.op==Instruction::Assign){auto value=n.expression->Evaluate(evaluated);auto d=p.parameters.find(n.target);if(d!=p.parameters.end())d->second.Validate(value);out.values.at(n.target)=value;out.written.insert(n.target);evaluated.at(n.target)=value;evaluated=Evaluate(p,std::move(evaluated));evaluated["$dt"]=elapsed;}
    else out.commands.emplace_back(n.op,n.target);
  }return out;
}
} // namespace EiemControlProgram
