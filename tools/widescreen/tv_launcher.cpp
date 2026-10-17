// Private portable launcher: no shell, installation, or shared save directories.
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
namespace fs=std::filesystem;
#ifndef SMP_PREVIEW_EXE
#define SMP_PREVIEW_EXE L"smp-gui-tv-v2.exe"
#endif
static std::string utf8(const std::wstring& value) {
  int size=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
  std::string result(size,0);WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),result.data(),size,nullptr,nullptr);return result;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR arguments,int) {
  HANDLE lock=INVALID_HANDLE_VALUE;
  try {
    wchar_t buffer[32768];auto size=GetModuleFileNameW(nullptr,buffer,32768);
    if(!size || size>=32768)throw std::runtime_error("Launcher path unavailable");
    fs::path own(std::wstring(buffer,size)),root=own.parent_path(),app=root/L"app";
    const auto exe=app/SMP_PREVIEW_EXE,rom=root/L"games"/L"Super Metroid.sfc";
    if(!fs::exists(exe) || !fs::exists(rom))throw std::runtime_error("Extract the entire ZIP before launching");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    lock=CreateFileW((root/L"launcher.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,&security,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock==INVALID_HANDLE_VALUE)throw std::runtime_error("This preview is already running, or its folder is not writable");
    auto settings=app/L"settings.bml";auto source=fs::exists(settings)?settings:root/L"settings-template.bml";
    std::ifstream input(source,std::ios::binary);if(!input)throw std::runtime_error("Settings template missing");
    std::string text((std::istreambuf_iterator<char>(input)),{});input.close();
    const char* keys[]={"Games","Saves","States","Screenshots","Patches","Cheats"};
    const wchar_t* folders[]={L"games",L"saves",L"states",L"screenshots",L"patches",L"cheats"};
    for(unsigned i=0;i<6;i++) {
      fs::create_directories(root/folders[i]);std::string prefix=std::string("  ")+keys[i]+": ";
      std::istringstream lines(text);std::string line,next;unsigned replaced=0;
      while(std::getline(lines,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.rfind(prefix,0)==0){line=prefix+utf8((root/folders[i]).generic_wstring())+"/";replaced++;}
        next+=line+"\n";
      }
      if(replaced!=1)throw std::runtime_error("Settings path section is missing or duplicated");
      text=std::move(next);
    }
#ifdef SMP_RETEST
    // Dedicated retest root only. The immutable checkpoint is recopied each launch;
    // normal play saves/settings live elsewhere and are never opened here.
    auto checkpoint=root/L"checkpoint/Super Metroid.bsz";
    if(!fs::is_regular_file(checkpoint))throw std::runtime_error("Retest checkpoint missing");
    fs::copy_file(checkpoint,root/L"states/Super Metroid.bsz",fs::copy_options::overwrite_existing);
    auto option=text.find("  AutoLoadStateOnLoad: false");
    if(option!=std::string::npos)text.replace(option,28,"  AutoLoadStateOnLoad: true");
    if(text.find("  AutoLoadStateOnLoad: true")==std::string::npos)throw std::runtime_error("Retest auto-load setting missing");
#endif
#ifdef SMP_RETEST_QUEUE
    if(!fs::is_regular_file(root/L"QUEUE-MODE") || !fs::is_regular_file(root/L"queue/cases.tsv"))throw std::runtime_error("Checkpoint queue files missing");
    auto option=text.find("  AutoLoadStateOnLoad: true");
    if(option!=std::string::npos)text.replace(option,27,"  AutoLoadStateOnLoad: false");
    if(text.find("  AutoLoadStateOnLoad: false")==std::string::npos)throw std::runtime_error("Queue auto-load setting missing");
#endif
    auto temp=app/L"settings.bml.tmp";
    {std::ofstream output(temp,std::ios::binary);output.write(text.data(),text.size());output.close();if(!output)throw std::runtime_error("Could not save local settings");}
    if(!MoveFileExW(temp.c_str(),settings.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace local settings");
    fs::create_directories(root/L"logs");fs::create_directories(app/L"Firmware");
    std::wstring options=arguments?arguments:L"";
    auto stem=own.stem().wstring();
    bool windowed=CompareStringOrdinal(stem.c_str(),-1,L"Configure Controls",-1,TRUE)==CSTR_EQUAL || options.find(L"--windowed")!=std::wstring::npos;
    {std::ofstream f(root/L"logs"/L"launch-plan.json");f<<"{\"tv_layout\":true,\"fullscreen\":"<<(windowed?"false":"true")<<"}\n";}
    if(options.find(L"--prepare-only")!=std::wstring::npos){CloseHandle(lock);return 0;}
    std::wstring command=L"\""+exe.wstring()+L"\" --smp-wide --smp-tv \"--smp-report="+(root/L"logs"/L"last-session.json").wstring()+L"\"";
#ifdef SMP_REDUCE_SLOWDOWN
    command+=L" --smp-reduce-slowdown";
#endif
#ifdef SMP_RETEST_QUEUE
    command+=L" \"--smp-queue="+(root/L"queue/cases.tsv").wstring()+L"\"";
#endif
    if(!windowed)command+=fs::is_regular_file(root/L"CAPTURE-SAFE-FULLSCREEN")?
      L" --smp-capture-fullscreen":L" --fullscreen";
    command+=L" \""+rom.wstring()+L"\"";
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,0,nullptr,app.c_str(),&si,&pi))throw std::runtime_error("Could not start preview");
    {std::ofstream f(root/L"logs"/L"launcher-session.json");f<<"{\"pid\":"<<pi.dwProcessId<<",\"tv_layout\":true,\"fullscreen\":"<<(windowed?"false":"true")<<"}\n";}
    CloseHandle(pi.hThread);WaitForSingleObject(pi.hProcess,INFINITE);DWORD code=0;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);CloseHandle(lock);return int(code);
  }catch(const std::exception& error) {
    if(lock!=INVALID_HANDLE_VALUE)CloseHandle(lock);
    std::string message=error.what();std::wstring wide(message.begin(),message.end());
    MessageBoxW(nullptr,wide.c_str(),L"Super Metroid TV preview",MB_OK|MB_ICONERROR);return 2;
  }
}
