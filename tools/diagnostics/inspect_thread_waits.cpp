// Read-only live hang inspection through Windows Wait Chain Traversal.
// No game memory reads, suspension, injection or resource changes.
#include <windows.h>
#include <tlhelp32.h>
#include <wct.h>
#include <iostream>
#include <vector>

int wmain(int argc,wchar_t** argv){
 if(argc!=2){std::cerr<<"usage: inspect_thread_waits <process-id>\n";return 2;}
 wchar_t* end=nullptr;auto value=wcstoul(argv[1],&end,10);
 if(!value||*end){std::cerr<<"invalid process-id\n";return 2;}DWORD pid=value;
 auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
 if(snapshot==INVALID_HANDLE_VALUE){std::cerr<<"thread snapshot error="<<GetLastError()<<'\n';return 1;}
 std::vector<DWORD> threads;THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
 if(Thread32First(snapshot,&entry))do{if(entry.th32OwnerProcessID==pid)threads.push_back(entry.th32ThreadID);}while(Thread32Next(snapshot,&entry));
 CloseHandle(snapshot);if(threads.empty()){std::cerr<<"process has no live threads\n";return 1;}
 auto session=OpenThreadWaitChainSession(0,nullptr);
 if(!session){std::cerr<<"wait-chain session error="<<GetLastError()<<'\n';return 1;}
 size_t accessible=0,blocked=0;std::cout<<"pid="<<pid<<" threads="<<threads.size()<<std::endl;
 for(auto tid:threads){WAITCHAIN_NODE_INFO nodes[WCT_MAX_NODE_COUNT]{};DWORD count=WCT_MAX_NODE_COUNT;BOOL cycle=FALSE;
  if(!GetThreadWaitChain(session,0,WCTP_GETINFO_ALL_FLAGS,tid,&count,nodes,&cycle)){std::cout<<"tid="<<tid<<" error="<<GetLastError()<<std::endl;continue;}
  ++accessible;if(count>1||cycle)++blocked;
  std::cout<<"tid="<<tid<<" cycle="<<int(cycle)<<" nodes="<<count;
  for(DWORD i=0;i<count;++i){const auto& n=nodes[i];std::cout<<" [type="<<int(n.ObjectType)<<" status="<<int(n.ObjectStatus);
   if(n.ObjectType==WctThreadType)std::cout<<" pid="<<n.ThreadObject.ProcessId<<" tid="<<n.ThreadObject.ThreadId<<" waitMs="<<n.ThreadObject.WaitTime;
   std::cout<<']';}std::cout<<std::endl;
 }
 CloseThreadWaitChainSession(session);std::cout<<"accessible="<<accessible<<" blocked="<<blocked<<std::endl;return accessible?0:1;
}
