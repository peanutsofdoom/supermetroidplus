#include "bsnes.hpp"
#include <sfc/interface/interface.hpp>
Video video;
Audio audio;
Input input;
unique_pointer<Emulator::Interface> emulator;

auto locate(string name) -> string {
  string location = {Path::program(), name};
  if(inode::exists(location)) return location;

  location = {Path::userData(), Emulator::Name, "/", name};
  if(inode::exists(location)) return location;

  directory::create({Path::userSettings(), Emulator::Name, "/"});
  return {Path::userSettings(), Emulator::Name, "/", name};
}

#include <nall/main.hpp>
auto nall::main(Arguments arguments) -> void {
  settings.location = locate("settings.bml");

  bool smpReadyShip=false, smpExploreShip=false, smpCaptureFullScreen=false;
  string smpCompareBundle, smpOriginalExecutable;
  for(auto argument : arguments) {
    if(argument.beginsWith("--smp-record=")) {
      smpr_request(argument.trimLeft("--smp-record=",1L),false);
    } else if(argument.beginsWith("--smp-replay=")) {
      smpr_request(argument.trimLeft("--smp-replay=",1L),true);
    } else if(argument.beginsWith("--smp-compare=")) {
      smpCompareBundle = argument.trimLeft("--smp-compare=",1L);
    } else if(argument.beginsWith("--smp-original-executable=")) {
      smpOriginalExecutable = argument.trimLeft("--smp-original-executable=",1L);
    } else if(argument == "--smp-explore-ship") {
      smpReadyShip=true;smpExploreShip=true;
    } else if(argument == "--smp-ready-ship") {
      smpReadyShip=true;
    } else if(argument.beginsWith("--smp-trial-frames=")) {
      smpr_limit(argument.trimLeft("--smp-trial-frames=",1L).natural());
    } else if(argument == "--smp-tv") {
      program.smpTv = true;
    } else if(argument == "--smp-wide") {
      program.smpWide = true;
    } else if(argument == "--smp-reduce-slowdown") {
      program.smpReduceSlowdown = true;
    } else if(argument.beginsWith("--smp-report=")) {
      program.smpReport = argument.trimLeft("--smp-report=", 1L);
    } else if(argument.beginsWith("--smp-queue=")) {
      program.smpQueueManifest = argument.trimLeft("--smp-queue=", 1L);
    } else if(argument == "--smp-capture-fullscreen") {
      smpCaptureFullScreen=true;
    } else if(argument == "--fullscreen") {
      program.startFullScreen = true;
    } else if(argument.beginsWith("--locale=")) {
      Application::locale().scan(locate("Locale/"));
      Application::locale().select(argument.trimLeft("--locale=", 1L));
    } else if(argument.beginsWith("--settings=")) {
      settings.location = argument.trimLeft("--settings=", 1L);
    } else if(inode::exists(argument)) {
      //game without option
      program.gameQueue.append({"Auto;", argument});
    } else if(argument.find(";")) {
      //game with option
      auto game = argument.split(";", 1L);
      if(inode::exists(game.last())) program.gameQueue.append(argument);
    }
  }

  if(program.smpReduceSlowdown && (smpr_requested() || smpCompareBundle || smpOriginalExecutable)) {
    MessageDialog("Reduce Original Slowdown changes emulated CPU timing. Native input-recording/replay trials require the original-timing launcher. Steam video recording is unaffected.").error();
    return;
  }
  if(smpCompareBundle || smpOriginalExecutable) {
    if(!smpCompareBundle || !smpOriginalExecutable)
      throw std::runtime_error("Comparison requires bundle and original executable paths");
    smpr_request_compare(smpCompareBundle,smpOriginalExecutable);
  }
  if(smpReadyShip)smpr_ready_ship(smpExploreShip);
  settings.load();
  smpgui_reduce_slowdown(program.smpReduceSlowdown);
  smpgui_request(program.smpWide);
  smpgui_tv(program.smpTv);
  Application::setName(Emulator::Name);
  Application::setScreenSaver(settings.general.screenSaver);
  Application::setToolTips(settings.general.toolTips);

  Instances::presentation.construct();
  Instances::settingsWindow.construct();
  Instances::cheatDatabase.construct();
  Instances::cheatWindow.construct();
  Instances::stateWindow.construct();
  Instances::toolsWindow.construct();
  emulator = new SuperFamicom::Interface;
  // Keep the original presentation HWND for Steam OpenGL capture. The WGL
  // native fullscreen popup leaves Steam tracking the old 768x456 surface.
  if(smpCaptureFullScreen)program.startFullScreen=false;
  program.create();
  if(smpCaptureFullScreen && emulator->loaded())program.toggleVideoPseudoFullScreen();

  if(Emulator::Version.find(".") && settings.general.betaWarning && 0) {
    MessageDialog dialog;
    dialog.setTitle(Emulator::Name);
    dialog.setText(
      "This is a nightly release. Bugs and regressions are possible!\n"
      "If you experience issues, please report them to me.\n"
      "If stability is required, please use a stable release.\n"
    );
    dialog.setOption("Don't show this message again");
    dialog.information();
    if(dialog.checked()) {
      settings.general.betaWarning = false;
      settings.save();
    }
  }

  Application::run();
  Instances::presentation.destruct();
  Instances::settingsWindow.destruct();
  Instances::cheatDatabase.destruct();
  Instances::cheatWindow.destruct();
  Instances::stateWindow.destruct();
  Instances::toolsWindow.destruct();
}
