#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <thread>
#include "../../src/eiem_load_performance.h"
std::vector<std::string> logs;
void Log(const char* format,...){char line[8192];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);logs.emplace_back(line);}
int main(){using namespace EiemLoadPerformance;
 MainFrame(123);auto ms=Frequency()/1000;
 Record(ViewRead,true,2*ms);Bytes(ViewRead,1024);Record(ViewRead,true,4*ms);Bytes(ViewRead,2048);
 Add(Operation::ManagedInvoke,2);Set(Workload::Mods,4);FinishRuntimeSample();
 std::thread worker([&]{Record(ViewRead,true,10*ms);Bytes(ViewRead,4096);Add(Operation::ManagedInvoke,5);});worker.join();
 Record(BufferRead,false,ms);Record(BufferRead,true,3*ms);
 Flush(true);auto find=[](const char* marker){for(const auto& line:logs)if(line.find(marker)!=std::string::npos)return line;return std::string{};};auto view=find("stage=view-read ");
 if(view.find("calls=3 ")==std::string::npos||view.find("mainCalls=2 ")==std::string::npos||view.find("mainTotalMs=6.000000")==std::string::npos||view.find("mainMaxMs=4.000000")==std::string::npos||view.find("bytes=7168")==std::string::npos||view.find("p95UpperMs=16.000000")==std::string::npos)return 1;
 if(find("stage=backing-read changed=0").empty()||find("stage=backing-read changed=1").empty()||find("wholeProcess=1").empty())return 2;
 if(counters[ViewRead][1].calls.load()||counters[ViewRead][1].mainCalls.load()||counters[ViewRead][1].bytes.load())return 3;
 if(view.find("mainP95UpperMs=4.000000")==std::string::npos||view.find("mainP99UpperMs=4.000000")==std::string::npos||view.find("p99UpperMs=16.000000")==std::string::npos)return 4;
 auto operation=find("operation=managed-invoke ");if(operation.find("calls=7 ")==std::string::npos||operation.find("mainCalls=2")==std::string::npos)return 5;
 if(find("runtimeWorkload=1 ").find("mods=4 ")==std::string::npos||find("window=1 windowId=1 ").empty())return 6;
 logs.clear();Record(File,true,Frequency()*2);Flush(true);if(find("stage=file changed=1").find("p95UpperMs=-1.000000")==std::string::npos)return 7;
 auto started=Clock();for(unsigned i=0;i<100000;++i){Scope sample(File);}double overheadUs=double(Clock()-started)*1000000/Frequency()/100000;
 printf("PASS performance aggregation: mainAndWorkerPartitioned=1 backingKindSplit=1 readBytes=7168 mainP95P99=1 overflowHistogram=1 resetWindow=1 operationsAndWorkload=1 uptimeWindows=1 wholeProcessMemoryLabelled=1 diagnosticScopeAverageUs=%.6f\n",overheadUs);return 0;
}
