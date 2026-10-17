#include "../bsnes.hpp"
#include <cstdlib>
#include "../../../../../tools/widescreen/host/tv_presentation.hpp"
#include "platform.cpp"
#include "game.cpp"
#include "game-pak.cpp"
#include "game-rom.cpp"
#include "paths.cpp"
#include "states.cpp"
#include "retest-queue.cpp"
#include "movies.cpp"
#include "rewind.cpp"
#include "video.cpp"
#include "audio.cpp"
#include "input.cpp"
#include "utility.cpp"
#include "patch.cpp"
#include "hacks.cpp"
#include "filter.cpp"
#include "viewport.cpp"
Program program;

auto Program::disableSmpWide() -> void {
  smpWide=false; smpPending={}; smpCleanFrame={}; screenshot={};
  presentation.smpWidescreen.setChecked(false);
  smpgui_request(false); smpgui_reset();
}

auto Program::setSmpCoordinates(bool visible) -> void {
  smpgui_clear_coordinates();
  smpCoordinates=visible;
  presentation.smpCoordinates.setChecked(visible);
  // Repaint the retained unadorned frame even while paused. This does not run
  // the emulator, touch its input, or copy another full frame each video tick.
  if(smpCleanFrame.data) {
    auto frame=smpCleanFrame;
    presentFrame(frame.data,frame.pitch,frame.width,frame.height,frame.scale,true);
  }
}

auto Program::create() -> void {
#ifdef SMP_COORDINATES
  smpCoordinates=true;
#endif
  Emulator::platform = this;

  presentation.create();
  presentation.setVisible();
  presentation.viewport.setFocused();

  settingsWindow.create();
  videoSettings.create();
  audioSettings.create();
  inputSettings.create();
  hotkeySettings.create();
  pathSettings.create();
  emulatorSettings.create();
  enhancementSettings.create();
  compatibilitySettings.create();
  driverSettings.create();

  toolsWindow.create();
  cheatFinder.create();
  cheatDatabase.create();
  cheatWindow.create();
  cheatEditor.create();
  stateWindow.create();
  stateManager.create();
  manifestViewer.create();

  if(settings.general.crashed) {
    MessageDialog(
      "Driver crash detected. Hardware drivers have been disabled.\n"
      "Please reconfigure drivers in the advanced settings panel."
    ).setAlignment(*presentation).information();
    settings.video.driver = "None";
    settings.audio.driver = "None";
    settings.input.driver = "None";
  }

  settings.general.crashed = true;
  settings.save();
  updateVideoDriver(presentation);
  updateAudioDriver(presentation);
  updateInputDriver(presentation);
  settings.general.crashed = false;
  settings.save();

  driverSettings.videoDriverChanged();
  driverSettings.audioDriverChanged();
  driverSettings.inputDriverChanged();

  if(gameQueue) load();
  if(startFullScreen && emulator->loaded()) {
    toggleVideoFullScreen();
  }
  Application::onMain({&Program::main, this});
}

auto Program::main() -> void {
  updateStatus();
  smpr_tick();
  if(smpr_requested()) {
    static string lastPlaytestStatus;
    string currentPlaytestStatus=smpr_status();
    if(currentPlaytestStatus!=lastPlaytestStatus) {
      presentation.setTitle({"Super Metroid Playtest - ",currentPlaytestStatus});
      lastPlaytestStatus=currentPlaytestStatus;
    }
    if(smpr_finished())presentation.pauseEmulation.setChecked();
  }
  video.poll();

  if(Application::modal()) {
    audio.clear();
    return;
  }

  inputManager.poll();
  inputManager.pollHotkeys();
  if(smpQueueManifest)smpQueueTick();

  if(inactive()) {
    audio.clear();
    usleep(20 * 1000);
    if(settings.emulator.runAhead.frames == 0) viewportRefresh();
    return;
  }

  if(smpr_requested()) {
    if(smpr_finished()) { showMessage(smpr_status()); presentation.pauseEmulation.setChecked(); return; }
    bool compatible=!settings.emulator.runAhead.frames && !settings.rewind.frequency &&
      !fastForwarding && !rewinding && !emulator->frameSkip() && movie.mode==Movie::Mode::Inactive;
    if(!smpr_before_frame(emulator->configuration(),compatible)) {
      showMessage(smpr_status());presentation.pauseEmulation.setChecked();return;
    }
    static bool markHeld=false,stopHeld=false;
#if defined(PLATFORM_WINDOWS)
    bool modifier=GetAsyncKeyState(VK_CONTROL)<0 && GetAsyncKeyState(VK_SHIFT)<0;
    bool mark=modifier && GetAsyncKeyState(VK_F9)<0,stop=modifier && GetAsyncKeyState(VK_F10)<0;
#else
    bool modifier=Keyboard::pressed("Control") && Keyboard::pressed("Shift");
    bool mark=modifier && Keyboard::pressed("F9"),stop=modifier && Keyboard::pressed("F10");
#endif
    if(mark && !markHeld) { smpr_marker();showMessage(smpr_status()); }
    if(stop && !stopHeld) { smpr_stop();showMessage(smpr_status()); }
    markHeld=mark;stopHeld=stop;
    if(smpr_finished())return;
  }
  rewindRun();
  smpPending={};
  smpgui_begin(!settings.emulator.runAhead.frames && !fastForwarding && !rewinding);

  if(!settings.emulator.runAhead.frames || fastForwarding || rewinding) {
    emulator->run();
  } else {
    emulator->setRunAhead(true);
    emulator->run();
    auto state = emulator->serialize(0);
    if(settings.emulator.runAhead.frames >= 2) emulator->run();
    if(settings.emulator.runAhead.frames >= 3) emulator->run();
    if(settings.emulator.runAhead.frames >= 4) emulator->run();
    emulator->setRunAhead(false);
    emulator->run();
    state.setMode(serializer::Mode::Load);
    emulator->unserialize(state);
  }

  smpgui_emulation_done();
  if(smpPending.data) {
    const bool compactTv=smpTv && video.smpCompact(true);
    auto wide=smpgui_finish(smpPending.data,smpPending.pitch,smpPending.width,smpPending.height,compactTv);
    smpFrameWide=wide!=nullptr;
    if(wide && compactTv) presentFrame(wide,684*4,684,224,1);
    else if(wide && smpTv) presentFrame(wide,2048*4,2048,448,1);
    else if(wide) presentFrame(wide,684*4,684,480,1);
    else presentFrame(smpPending.data,smpPending.pitch,smpPending.width,smpPending.height,smpPending.scale);
    smpPending={};
  }

  if(smpQueue.hasLabel() && std::getenv("SMP_QUEUE_TEST_SCREENSHOTS") && smpQueue.remaining()==9 && smpQueue.current().code!=(const char*)smpQueueCaptured){
    smpQueueCaptured=smpQueue.current().code.c_str();captureScreenshot();
  }
  if(smpReport && smpgui_report_due())smpgui_report((const char*)smpReport);
  smpr_after_frame();
  if(smpr_finished()) {
    showMessage(smpr_status());presentation.pauseEmulation.setChecked();
    if(smpReport)smpgui_report((const char*)smpReport);
    // Automated read-only comparison runs may request a normal final exit.
    // Ordinary play never enables this path.
    if(smpr_readonly_replay() && std::getenv("SMP_COMPARE_EXIT"))return quit();
  }
  if(emulatorSettings.autoSaveMemory.checked()) {
    auto currentTime = chrono::timestamp();
    if(currentTime - autoSaveTime >= settings.emulator.autoSaveMemory.interval) {
      autoSaveTime = currentTime;
      emulator->save();
    }
  }
}

auto Program::quit() -> void {
  smpr_stop();
  smpgui_report((const char*)smpReport);
  //make closing the program feel more responsive
  presentation.setVisible(false);
  Application::processEvents();

  //in case the emulator was closed prior to initialization completing:
  settings.general.crashed = false;

  unload();
  settings.save();
  video.reset();
  audio.reset();
  input.reset();

  #if defined(PLATFORM_WINDOWS)
  //in rare cases, when Application::exit() calls exit(0), a crash will occur.
  //this seems to be due to the internal state of certain ruby drivers.
  auto processID = GetCurrentProcessId();
  auto handle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, true, processID);
  TerminateProcess(handle, 0);
  #endif

  Application::exit();
}
