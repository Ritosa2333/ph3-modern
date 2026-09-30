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
double measureStart=-1;
unsigned measuredSteps=0,measuredPresents=0;
std::vector<double> frameCosts;
#ifdef PH3_PERF_SCAN
void reportSlowFrame(double cost,int kind){
    if(cost<1000.0/60)return;
    auto manager=ETaskManager::GetInstance();
    if(!manager)return;
    for(auto task:manager->GetTaskList()){
        auto system=dynamic_cast<StgSystemController*>(task.GetPointer());
        if(!system)continue;
        auto stage=system->GetStageController();
        if(!stage)return;
        auto state=stage->GetStageInformation();
        EM_ASM({
            const d=globalThis.ph3Diagnostics;if(!d)return;
            const p=d.perfScan||(d.perfScan=({stages:{},slowFrames:0}));
            ++p.slowFrames;
            const index=$2;
            const s=p.stages[index]||(p.stages[index]=({slow16:0,slow33:0,slow50:0,maxFrameMs:0,maxPresentMs:0}));
            ++s.slow16;if($0>=1000/30)++s.slow33;if($0>=50)++s.slow50;
            const field=$1?'maxPresentMs':'maxFrameMs';
            if($0>s[field]){
                s[field]=$0;s[$1?'maxPresent':'maxFrame']=({frame:$3,enemies:$4,playerShots:$5,enemyShots:$6});
            }
        },cost,kind,state->GetStageIndex(),state->GetCurrentFrame(),
          stage->GetEnemyManager()->GetEnemyCount(),
          stage->GetShotManager()->GetShotCount(StgShotObject::OWNER_PLAYER),
          stage->GetShotManager()->GetShotCount(StgShotObject::OWNER_ENEMY));
        return;
    }
}
#endif
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
    EFpsController::GetInstance()->SetMeasuredBrowserFps(logic,display);
    EM_ASM({const d=globalThis.ph3Diagnostics;if(d)d.performance=({logicHz:$0,displayFps:$1,frameMsP95:$2,frameMsMax:$3});},logic,display,p95,maximum);
    measureStart=now;measuredSteps=measuredPresents=0;frameCosts.clear();
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
EM_BOOL frame(double now,void*){
    if(failed)return EM_FALSE;
    if(EM_ASM_INT({return globalThis.ph3Diagnostics?.phase==='error'?1:0;}))return EM_FALSE;
    if(EM_ASM_INT({return document.hidden?1:0;})){previous=-1;accumulated=0;nextStateReport=0;measureStart=-1;measuredSteps=measuredPresents=0;frameCosts.clear();return EM_TRUE;}
    accumulated+=previous<0?0:std::min(100.0,now-previous);previous=now;
    try {
        // Native has a real loader thread. On Web process at most one queued
        // load per browser callback so unrelated async loads no longer become
        // one giant synchronous hitch.
        if(auto* files=FileManager::GetBase())files->PumpLoads();
        const bool observePerformance=EM_ASM_INT({return globalThis.ph3Diagnostics?.observePerformance?1:0;})!=0;
        const double begin=observePerformance?emscripten_get_now():0;
        unsigned steps=0;
        bool rendered=false;
        auto device=EDirectGraphics::GetInstance()->GetDevice();
        device->BeginBrowserFrame();
#ifdef PH3_PERF_SCAN
        const int fastSteps=EM_ASM_INT({
            const value=Number(Module['ph3PerfFastSteps']||0);
            return Number.isFinite(value)?Math.max(0,Math.min(64,value|0)):0;
        });
        if(fastSteps>1){
            accumulated=0;
            for(int i=0;i<fastSteps;++i){
                const bool render=i+1==fastSteps;
                const double tickBegin=emscripten_get_now();
                if(!EApplication::GetInstance()->RunFrame(render))return EM_FALSE;
                const double tickCost=emscripten_get_now()-tickBegin;
                reportSlowFrame(tickCost,0);
                if(!EApplication::GetInstance()->DidBrowserFrameWork())continue;
                ++frames;++steps;rendered=rendered||render;
            }
        }else
#endif
        while(accumulated+0.001>=1000.0/60){
            accumulated-=1000.0/60;
            const bool render=accumulated+0.001<1000.0/60;
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
        if(rendered){
#ifdef PH3_PERF_SCAN
            const double presentBegin=emscripten_get_now();
#endif
            if(FAILED(device->Present(nullptr,nullptr,nullptr,nullptr)))
                throw std::runtime_error("Browser presentation failed");
#ifdef PH3_PERF_SCAN
            reportSlowFrame(emscripten_get_now()-presentBegin,1);
#endif
        }
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
