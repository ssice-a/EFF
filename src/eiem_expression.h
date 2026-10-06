#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <vector>

using EiemVariables = std::unordered_map<std::string, double>;

struct EiemExpression {
  enum Op { Number, Variable, Not, Negative, Equal, NotEqual, Less, LessEqual,
            Greater, GreaterEqual, And, Or } op = Number;
  double number = 0;
  std::string variable;
  std::shared_ptr<EiemExpression> left, right;

  // Last-reference releases drain an intrusive queue. Long expressions never
  // recurse through shared_ptr destruction and need no extra release allocation.
  static std::shared_ptr<EiemExpression> Make() {
    return std::shared_ptr<EiemExpression>(new EiemExpression, Release);
  }

 private:
  EiemExpression* releaseNext_ = nullptr;
  static void Release(EiemExpression* node) noexcept {
    thread_local EiemExpression* pending = nullptr;
    thread_local bool draining = false;
    node->releaseNext_ = pending; pending = node;
    if (draining) return;
    draining = true;
    while (pending) {
      auto current = pending; pending = current->releaseNext_;
      delete current;
    }
    draining = false;
  }

 public:

  bool Validate(const EiemVariables &variables, std::string &error) const {
    std::vector<const EiemExpression*> pending{this};
    while(!pending.empty()) {
      auto node=pending.back();pending.pop_back();
      if(node->op==Variable&&!variables.count(node->variable)){
        error="Undeclared variable: "+node->variable;return false;
      }
      if(node->right)pending.push_back(node->right.get());
      if(node->left)pending.push_back(node->left.get());
    }
    return true;
  }
  double Evaluate(const EiemVariables &variables) const {
    struct Frame {const EiemExpression* node;unsigned phase=0;double lhs=0;};
    thread_local std::vector<Frame> stack;stack.clear();stack.push_back({this});double value=0;
    while(!stack.empty()) {
      auto& frame=stack.back();auto node=frame.node;
      if(node->op==Number||node->op==Variable){value=node->op==Number?node->number:variables.at(node->variable);stack.pop_back();continue;}
      if(frame.phase==0){frame.phase=1;stack.push_back({node->left.get()});continue;}
      if(frame.phase==1){
        if(node->op==Not||node->op==Negative){value=node->op==Not?double(!value):-value;stack.pop_back();continue;}
        if((node->op==And&&!value)||(node->op==Or&&value)){value=node->op==Or;stack.pop_back();continue;}
        frame.lhs=value;frame.phase=2;stack.push_back({node->right.get()});continue;
      }
      auto lhs=frame.lhs;
      switch(node->op){
        case And:case Or:value=bool(value);break;
        case Equal:value=lhs==value;break;case NotEqual:value=lhs!=value;break;
        case Less:value=lhs<value;break;case LessEqual:value=lhs<=value;break;
        case Greater:value=lhs>value;break;case GreaterEqual:value=lhs>=value;break;
        default:value=0;break;
      }stack.pop_back();
    }
    return value;
  }
};

static bool EiemVariableName(const std::string &name) {
  if (name.size() < 2 || name[0] != '$' ||
      !(std::isalpha((unsigned char)name[1]) || name[1] == '_')) return false;
  for (size_t i = 2; i < name.size(); ++i)
    if (!(std::isalnum((unsigned char)name[i]) || name[i] == '_')) return false;
  return true;
}

static bool EiemNumber(const std::string &text, double *out) {
  if (text == "true") { *out = 1; return true; }
  if (text == "false") { *out = 0; return true; }
  char *end = nullptr;
  errno = 0;
  double n = std::strtod(text.c_str(), &end);
  if (errno || end == text.c_str() || *end || !std::isfinite(n)) return false;
  *out = n;
  return true;
}

// Small precedence parser. Syntax is parsed once; evaluating it never calls Unity.
class EiemExpressionParser {
  using Node = std::shared_ptr<EiemExpression>;
  struct Operator {const char* text;int precedence;EiemExpression::Op op;bool unary=false;};
  const std::string &text_;size_t pos_=0;std::string &error_;
  std::vector<Node> values_;std::vector<Operator> operators_;
  void Space(){while(pos_<text_.size()&&std::isspace((unsigned char)text_[pos_]))++pos_;}
  Node Fail(){error_="Invalid expression at column "+std::to_string(pos_+1);return nullptr;}
  bool Reduce(){
    auto op=operators_.back();operators_.pop_back();if(values_.size()<(op.unary?1u:2u))return false;
    if(op.unary&&op.text[0]=='+')return true;
    auto node=EiemExpression::Make();node->op=op.op;
    if(!op.unary){node->right=values_.back();values_.pop_back();}
    node->left=values_.back();values_.back()=node;return true;
  }
  Node Operand(){
    auto node=EiemExpression::Make();char c=text_[pos_];
    if(c=='$'||std::isalpha((unsigned char)c)){
      auto start=pos_++;while(pos_<text_.size()&&(std::isalnum((unsigned char)text_[pos_])||text_[pos_]=='_'))++pos_;
      auto token=text_.substr(start,pos_-start);
      if(token=="true"||token=="false")node->number=token=="true";
      else if(EiemVariableName(token)){node->op=EiemExpression::Variable;node->variable=token;}else return Fail();
    }else{
      char* end=nullptr;errno=0;node->number=std::strtod(text_.c_str()+pos_,&end);
      if(errno||end==text_.c_str()+pos_||!std::isfinite(node->number))return Fail();pos_=size_t(end-text_.c_str());
    }return node;
  }
 public:
  EiemExpressionParser(const std::string& text,std::string& error):text_(text),error_(error){}
  Node Parse(){
    static const Operator binary[]={{"||",1,EiemExpression::Or},{"&&",2,EiemExpression::And},
      {"==",3,EiemExpression::Equal},{"!=",3,EiemExpression::NotEqual},{"<=",4,EiemExpression::LessEqual},
      {">=",4,EiemExpression::GreaterEqual},{"<",4,EiemExpression::Less},{">",4,EiemExpression::Greater}};
    bool primary=true;
    for(;;){Space();if(pos_==text_.size())break;char c=text_[pos_];
      if(primary){
        if(c=='('){operators_.push_back({"(",0,EiemExpression::Number});++pos_;continue;}
        if(c=='!'||c=='-'||c=='+'){operators_.push_back({c=='!'?"!":c=='-'?"-":"+",5,c=='!'?EiemExpression::Not:EiemExpression::Negative,true});++pos_;continue;}
        auto value=Operand();if(!value)return {};values_.push_back(value);primary=false;continue;
      }
      if(c==')'){
        while(!operators_.empty()&&operators_.back().text[0]!='(')if(!Reduce())return Fail();
        if(operators_.empty())return Fail();operators_.pop_back();++pos_;continue;
      }
      const Operator* selected=nullptr;
      for(const auto& op:binary)if(text_.compare(pos_,std::char_traits<char>::length(op.text),op.text)==0){selected=&op;break;}
      if(!selected)return Fail();
      while(!operators_.empty()&&operators_.back().precedence>=selected->precedence)if(!Reduce())return Fail();
      operators_.push_back(*selected);pos_+=std::char_traits<char>::length(selected->text);primary=true;
    }
    if(primary)return Fail();
    while(!operators_.empty()){if(operators_.back().text[0]=='('||!Reduce())return Fail();}
    return values_.size()==1?values_.front():Fail();
  }
};
