// Queue checkpoint loading is deliberately isolated from ordinary save-state slots.
#include <chrono>
#include <filesystem>
#include <fstream>
static double smpQueueNow(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}

auto Program::smpQueueLoad(const std::filesystem::path& path) -> bool {
  auto memory=file::read(path.u8string().c_str());
  if(memory.size()<20 || memory.size()>16*1024*1024 || memory::readl<4>(memory.data())!=State::Signature)return false;
  uint64_t encoded=memory::readl<4>(memory.data()+4),decoded=memory::readl<8>(memory.data()+12);
  if(encoded<8 || encoded>memory.size()-12 || decoded<65536 || decoded>8*1024*1024)return false;
  auto data=Decode::RLE<1>({memory.data()+12,unsigned(encoded)});
  if(data.size()!=decoded)return false;
  serializer saved{data.data(),unsigned(data.size())};audio.clear();
  if(!emulator->unserialize(saved))return false;
  smpPending={};smpCleanFrame={};screenshot={};smpgui_load(superFamicom.program.data(),superFamicom.program.size());smpgui_request(smpWide);rewindReset();audio.clear();
  return true;
}

auto Program::smpQueueTick() -> void {
  if(!emulator->loaded() || smpQueue.phase()==SmpRetestQueue::Phase::Failed)return;
  try {
    if(!smpgui_known() || smpr_requested())throw std::runtime_error("Queue needs Super Metroid outside a replay trial");
    if(!smpQueueInitialized){
      smpQueueInitialized=true;
      std::filesystem::path manifest{(const char*)smpQueueManifest};auto root=manifest.parent_path().parent_path();
      if(!smpgui_known() || smpr_requested() || movie.mode!=Movie::Mode::Inactive || settings.emulator.runAhead.frames)throw std::runtime_error("Queue needs the normal Super Metroid runtime");
      std::ifstream marker(root/"QUEUE-MODE");std::string value;std::getline(marker,value);
      if(value!="smp-isolated-queue-v1")throw std::runtime_error("Queue installation marker missing");
      for(auto item:{std::pair{"saves",(const char*)settings.path.saves},std::pair{"states",(const char*)settings.path.states}})
        if(std::filesystem::weakly_canonical(root/item.first)!=std::filesystem::weakly_canonical(item.second))throw std::runtime_error("Queue save paths must be isolated");
      smpQueue.open(manifest);
    }
    auto previous=smpQueue.phase();
    if(smpQueue.tick(smpQueueNow(),!inactive() && !fastForwarding && !rewinding)){
      if(!smpQueueLoad(smpQueue.current().checkpoint))throw std::runtime_error("Queue checkpoint could not be loaded");
      smpQueue.loaded(smpQueueNow());
      presentation.setTitle({"Super Metroid Retest Queue - ",smpQueue.current().code.c_str()," - ",smpQueue.current().label.c_str()});
      std::ofstream log(std::filesystem::path((const char*)smpQueueManifest).parent_path()/"queue-log.tsv",std::ios::app);log<<"loaded\t"<<smpQueue.current().code<<"\t"<<uint64_t(smpQueueNow()*1000)<<"\t"<<smpQueue.current().label<<"\n";
    }
    if(previous!=SmpRetestQueue::Phase::Complete && smpQueue.phase()==SmpRetestQueue::Phase::Complete){
      showMessage("Retest queue complete");
      std::ofstream log(std::filesystem::path((const char*)smpQueueManifest).parent_path()/"queue-log.tsv",std::ios::app);log<<"complete\t"<<uint64_t(smpQueueNow()*1000)<<"\n";
      log.flush();
      if(std::getenv("SMP_QUEUE_TEST_EXIT"))quit();
    }
  }catch(const std::exception& e){
    smpQueue.fail();presentation.pauseEmulation.setChecked();showMessage({"Retest queue stopped: ",e.what()});
  }
}
