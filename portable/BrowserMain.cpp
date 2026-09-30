#include "GcLibImpl.hpp"
#include "StgSystem.hpp"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <clocale>
#include <vector>

namespace {
double previous=-1, accumulated=0;
unsigned frames=0;
bool failed=false;
double nextStateReport=0;
double runtimeFpsStart=-1;
unsigned runtimeFpsSteps=0,runtimeFpsPresents=0;
double measureStart=-1;
unsigned measuredSteps=0,measuredPresents=0;
std::vector<double> frameCosts;
void measure(double begin,unsigned steps,bool presented){
    double now=emscripten_get_now();
    if(measureStart<0)measureStart=begin;
    measuredSteps+=steps;
    if(presented){++measuredPresents;frameCosts.push_back(now-begin);}
    const double elapsed=now-measureStart;
    if(elapsed<1000)return;
    const double logic=measuredSteps*1000.0/elapsed,display=measuredPresents*1000.0/elapsed;
    std::sort(frameCosts.begin(),frameCosts.end());
    const double p95=frameCosts.empty()?0:frameCosts[std::min(frameCosts.size()-1,std::size_t(frameCosts.size()*.95))];
    const double maximum=frameCosts.empty()?0:frameCosts.back();
    EM_ASM({const d=globalThis.ph3Diagnostics;if(d)d.performance=({logicHz:$0,displayFps:$1,frameMsP95:$2,frameMsMax:$3});},logic,display,p95,maximum);
    measureStart=now;measuredSteps=measuredPresents=0;frameCosts.clear();
}
void updateRuntimeFps(double now,unsigned steps,bool presented){
    if(runtimeFpsStart<0)runtimeFpsStart=now;
    runtimeFpsSteps+=steps;
    if(presented)++runtimeFpsPresents;
    const double elapsed=now-runtimeFpsStart;
    if(elapsed<1000)return;
    EFpsController::GetInstance()->SetBrowserRuntimeFps(
        float(runtimeFpsSteps*1000.0/elapsed),float(runtimeFpsPresents*1000.0/elapsed));
    runtimeFpsStart=now;runtimeFpsSteps=runtimeFpsPresents=0;
}
void fail(const std::string& message){
    failed=true;std::fprintf(stderr,"Woo fatal: %s\n",message.c_str());
    EM_ASM({const message=UTF8ToString($0);const d=globalThis.ph3Diagnostics;if(d){d.errors.push(message);d.phase='error';}const e=document.getElementById('status');if(e)e.textContent=message;},message.c_str());
}
bool reportState(){
    auto manager=ETaskManager::GetInstance();
    if(!manager)return true;
    for(auto task:manager->GetTaskList()){
        auto system=dynamic_cast<StgSystemController*>(task.GetPointer());
        if(!system)continue;
        auto info=system->GetSystemInformation();
        if(info->IsError()){fail(gstd::StringUtility::ConvertWideToMulti(info->GetErrorMessage(),CP_UTF8));return false;}
        auto stage=system->GetStageController();
        if(!stage){EM_ASM({if(globalThis.ph3Diagnostics)globalThis.ph3Diagnostics.stage=null;});continue;}
        auto state=stage->GetStageInformation();
        auto player=stage->GetPlayerObject();
        auto main=state->GetMainScriptInformation();
        auto path=main==NULL?std::string():gstd::StringUtility::ConvertWideToMulti(main->GetScriptPath(),CP_UTF8);
        EM_ASM({const d=globalThis.ph3Diagnostics;if(d)d.stage=({frame:$0,index:$1,path:UTF8ToString($2),
            playerPresent:!!$3,x:$4,y:$5,lives:$6,playerState:$7,enemies:$8,playerShots:$9,enemyShots:$10,paused:!!$11,spells:$12,replay:!!$13,score:$14});},
            state->GetCurrentFrame(),state->GetStageIndex(),path.c_str(),player!=NULL,
            player!=NULL?player->GetX():0,player!=NULL?player->GetY():0,
            player!=NULL?player->GetLife():0,player!=NULL?player->GetState():-1,
            stage->GetEnemyManager()->GetEnemyCount(),
            stage->GetShotManager()->GetShotCount(StgShotObject::OWNER_PLAYER),
            stage->GetShotManager()->GetShotCount(StgShotObject::OWNER_ENEMY),
            state->IsPause(),player!=NULL?player->GetSpell():0,state->IsReplay(),double(state->GetScore()));
    }
    return true;
}
#if defined(WOO_REPLAY_VERIFY) || defined(PH3_REPLAY_BOUNDARY)
bool replayActive(){
    auto manager=ETaskManager::GetInstance();
    if(!manager)return false;
    for(auto task:manager->GetTaskList()){
        auto system=dynamic_cast<StgSystemController*>(task.GetPointer());
        if(system&&system->GetStageController())
            return system->GetStageController()->GetStageInformation()->IsReplay();
    }
    return false;
}
#endif
EM_BOOL frame(double now,void*){
    if(failed)return EM_FALSE;
    if(EM_ASM_INT({return globalThis.ph3Diagnostics?.phase==='error'?1:0;}))return EM_FALSE;
    if(EM_ASM_INT({return document.hidden?1:0;})){previous=-1;accumulated=0;nextStateReport=0;runtimeFpsStart=-1;runtimeFpsSteps=runtimeFpsPresents=0;measureStart=-1;measuredSteps=measuredPresents=0;frameCosts.clear();return EM_TRUE;}
    try {
        auto fpsController=EFpsController::GetInstance();
        const int targetBefore=fpsController->GetBrowserTargetFps();
        const double deltaLimit=targetBefore>STANDARD_FPS?1000.0/STANDARD_FPS:100.0;
        accumulated+=previous<0?0:std::min(deltaLimit,now-previous);previous=now;
        const bool observePerformance=EM_ASM_INT({return globalThis.ph3Diagnostics?.observePerformance?1:0;})!=0;
        const double begin=observePerformance?emscripten_get_now():0;
        unsigned steps=0;
        bool rendered=false;
        bool diagnosticFast=false;
        auto device=EDirectGraphics::GetInstance()->GetDevice();
        device->BeginBrowserFrame();
#if defined(WOO_REPLAY_VERIFY) || defined(PH3_REPLAY_BOUNDARY)
        // Diagnostic-only fast playback. Each call still consumes exactly one
        // original input/tick; only wall-clock waiting and intermediate draws
        // are omitted. The golden comparison must validate this lane.
        const int requestedFastSteps=EM_ASM_INT({return Module['wooReplayFastSteps']|0;});
        const int fastSteps=requestedFastSteps>0&&replayActive()?std::min(requestedFastSteps,32):0;
        if(fastSteps){
            diagnosticFast=true;
            accumulated=0;
            for(int i=0;i<fastSteps;++i){
                if(!EApplication::GetInstance()->RunFrame(i==fastSteps-1))return EM_FALSE;
                if(!EApplication::GetInstance()->DidBrowserFrameWork())continue;
                ++frames;++steps;rendered=i==fastSteps-1;
            }
        }else
#endif
        for(;;){
            const int targetFps=fpsController->GetBrowserTargetFps();
            const double stepMs=1000.0/targetFps;
            if(accumulated+0.001<stepMs)break;
            accumulated-=stepMs;
            const bool renderPermit=fpsController->AdvanceBrowserRenderPermit();
            const bool render=accumulated+0.001<stepMs&&renderPermit;
            if(!EApplication::GetInstance()->RunFrame(render))return EM_FALSE;
            if(!EApplication::GetInstance()->DidBrowserFrameWork())continue;
            ++frames;
            ++steps;
            rendered=render;
        }
        // Diagnostics are an observer, not part of the 60 Hz simulation.
        // Crossing Wasm->JS and transcoding the script path for every catch-up
        // tick can itself create the stalls the observer is trying to measure.
        // Full stage diagnostics are deliberately low-frequency. They walk
        // task/object owners, transcode a path and cross Wasm -> JS; doing
        // that at simulation frequency can itself push a borderline frame
        // over 16.7 ms and trigger a catch-up spiral. This observer is not
        // gameplay state and browser tests only need a fresh snapshot within
        // a human-scale interval.
        const bool observeState=EM_ASM_INT({return globalThis.ph3Diagnostics?.observeStage?1:0;})!=0;
        if(observeState&&steps&&(nextStateReport<=0||now>=nextStateReport)){
            if(!reportState())return EM_FALSE;
            nextStateReport=now+100.0;
        }
        if(rendered&&FAILED(device->Present(nullptr,nullptr,nullptr,nullptr)))
            throw std::runtime_error("Browser presentation failed");
        if(!diagnosticFast)updateRuntimeFps(now,steps,rendered);
        if(observePerformance)measure(begin,steps,rendered);
        EM_ASM({if(globalThis.ph3Diagnostics)globalThis.ph3Diagnostics.frames=$0;},frames);
    }catch(gstd::wexception& error){fail(gstd::StringUtility::ConvertWideToMulti(error.what(),CP_UTF8));}
    catch(const std::exception& error){fail(error.what());}
    return failed?EM_FALSE:EM_TRUE;
}
}
int main(){
    try {
        // musl's wide formatter converts through LC_CTYPE. Without UTF-8,
        // original Japanese log messages fail with EILSEQ in the C locale.
        if (!std::setlocale(LC_CTYPE,"C.UTF-8"))
            throw std::runtime_error("Cannot initialize the UTF-8 text locale");
        auto config=DnhConfiguration::CreateInstance();
        auto logger=ELogger::CreateInstance();logger->Initialize(config->IsLogFile(),false);
        EPathProperty::CreateInstance();
        auto app=EApplication::CreateInstance();
        if(!app->StartBrowser())throw std::runtime_error("Woo application initialization failed");
        emscripten_request_animation_frame_loop(frame,nullptr);
    }catch(gstd::wexception& error){fail(gstd::StringUtility::ConvertWideToMulti(error.what(),CP_UTF8));}
    catch(const std::exception& error){fail(error.what());}
    return failed?1:0;
}
