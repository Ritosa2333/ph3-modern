#include "GcLibImpl.hpp"
#include "StgSystem.hpp"
#include "DirectInput.hpp"
#include "DirectSound.hpp"
#include "input/TouchController.hpp"
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
std::vector<double> callbackCosts;
double measuredQueuedLoadMax=0;
touhou::input::TouchController touchController;
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
void measure(double begin,unsigned steps,bool presented,double queuedLoadMs){
    double now=emscripten_get_now();
    if(measureStart<0)measureStart=begin;
    measuredSteps+=steps;
    if(presented){++measuredPresents;frameCosts.push_back(now-begin);}
    // Required loading can block a browser callback even when it draws nothing.
    callbackCosts.push_back(now-begin);
    measuredQueuedLoadMax=std::max(measuredQueuedLoadMax,queuedLoadMs);
    const double elapsed=now-measureStart;
    if(elapsed<1000)return;
    const double logic=measuredSteps*1000.0/elapsed,display=measuredPresents*1000.0/elapsed;
    std::sort(frameCosts.begin(),frameCosts.end());
    const double p95=frameCosts.empty()?0:frameCosts[std::min(frameCosts.size()-1,std::size_t(frameCosts.size()*.95))];
    const double maximum=frameCosts.empty()?0:frameCosts.back();
    std::sort(callbackCosts.begin(),callbackCosts.end());
    const double callbackP95=callbackCosts.empty()?0:callbackCosts[std::min(callbackCosts.size()-1,std::size_t(callbackCosts.size()*.95))];
    const double callbackMaximum=callbackCosts.empty()?0:callbackCosts.back();
    EM_ASM({const d=globalThis.ph3Diagnostics;if(d)d.performance=({timingVersion:2,
        logicHz:$0,displayFps:$1,frameMsP95:$2,frameMsMax:$3,
        callbackMsP95:$4,callbackMsMax:$5,queuedLoadMsMax:$6});},
        logic,display,p95,maximum,callbackP95,callbackMaximum,measuredQueuedLoadMax);
    measureStart=now;measuredSteps=measuredPresents=0;
    frameCosts.clear();callbackCosts.clear();measuredQueuedLoadMax=0;
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
struct HsobTouchContext {
    touhou::input::TouchState state;
    StgPlayerObject* player=nullptr;
};
HsobTouchContext getTouchContext(){
    HsobTouchContext result;
    auto manager=ETaskManager::GetInstance();
    if(!manager)return result;
    for(auto task:manager->GetTaskList()){
        auto system=dynamic_cast<StgSystemController*>(task.GetPointer());
        if(!system)continue;
        auto stage=system->GetStageController();
        if(!stage)return result;
        auto info=stage->GetStageInformation();
        auto systemInfo=system->GetSystemInformation();
        auto playerRef=stage->GetPlayerObject();
        auto player=playerRef.GetPointer();
        if(systemInfo->GetScene()!=StgSystemInformation::SCENE_STG||info->IsPause()||player==nullptr)return result;
        result.player=player;
        result.state.context=player->IsPermitShot()?1:2;
        result.state.instance=player->GetObjectID();
        result.state.ready=player->GetState()==StgPlayerObject::STATE_NORMAL;
        result.state.x=float(player->GetX());result.state.y=float(player->GetY());
        result.state.fast=float(player->GetFastSpeed());result.state.slow=float(player->GetSlowSpeed());
        const RECT clip=player->GetClip();
        result.state.min_x=float(clip.left);result.state.min_y=float(clip.top);
        result.state.max_x=float(clip.right);result.state.max_y=float(clip.bottom);
        return result;
    }
    return result;
}
bool nonTouchHeld(EDirectInput* input,int key){return input->IsNonTouchKeyDown(key);}
void applyTouchInput(){
    auto input=EDirectInput::GetInstance();
    if(!input)return;
    auto context=getTouchContext();
    const bool keySlow=nonTouchHeld(input,DIK_LSHIFT)||nonTouchHeld(input,DIK_RSHIFT);
    const bool arrows=nonTouchHeld(input,DIK_LEFT)||nonTouchHeld(input,DIK_RIGHT)||
        nonTouchHeld(input,DIK_UP)||nonTouchHeld(input,DIK_DOWN);
    auto sample=touchController.sample(context.state,std::uint64_t(emscripten_get_now()),keySlow,arrows);
    bool left=sample.keys[37],right=sample.keys[39],up=sample.keys[38],down=sample.keys[40];
    if(context.player&&context.state.context==1&&context.state.ready&&sample.motion){
        if(sample.motion==2){
            context.player->SetX(std::clamp(double(sample.x),double(context.state.min_x),double(context.state.max_x)));
            context.player->SetY(std::clamp(double(sample.y),double(context.state.min_y),double(context.state.max_y)));
            EM_ASM({const d=globalThis.ph3Diagnostics;if(d)d.unlimitedTouchUsed=true;});
        }else{
            const double speed=(sample.keys[16]||keySlow)?context.state.slow:context.state.fast;
            const double dead=std::max(0.5,speed*0.45);
            const double dx=double(sample.x)-context.player->GetX(),dy=double(sample.y)-context.player->GetY();
            left=left||dx<-dead;right=right||dx>dead;up=up||dy<-dead;down=down||dy>dead;
        }
    }
    input->SetDirectTouchKeyState(DIK_LEFT,left);
    input->SetDirectTouchKeyState(DIK_RIGHT,right);
    input->SetDirectTouchKeyState(DIK_UP,up);
    input->SetDirectTouchKeyState(DIK_DOWN,down);
    input->SetDirectTouchKeyState(DIK_Z,sample.keys[90]);
    input->SetDirectTouchKeyState(DIK_X,sample.keys[88]);
    input->SetDirectTouchKeyState(DIK_LSHIFT,sample.keys[16]);
    input->SetDirectTouchKeyState(DIK_LCONTROL,sample.keys[17]);
    input->SetDirectTouchKeyState(DIK_ESCAPE,sample.keys[27]);
}
#ifdef WOO_REPLAY_VERIFY
StgStageController* captureStage(){
    auto manager=ETaskManager::GetInstance();if(!manager)return nullptr;
    for(auto task:manager->GetTaskList()){
        auto system=dynamic_cast<StgSystemController*>(task.GetPointer());
        if(system&&system->GetStageController()&&system->GetStageController()->GetStageInformation()->IsReplay())
            return system->GetStageController();
    }
    return nullptr;
}
bool captureNextFrame(){
    if(!EM_ASM_INT({return Module['wooCaptureFrames']?.length?1:0;}))return false;
    auto stage=captureStage();if(!stage)return false;
    auto info=stage->GetStageInformation();
    return EM_ASM_INT({return Module['wooCaptureFrames'].some(p=>p.stage===$0&&p.frame===$1)?1:0;},
        info->GetStageIndex(),info->GetCurrentFrame()+1)!=0;
}
void captureRenderedFrame(){
    auto stage=captureStage();if(!stage)return;
    auto info=stage->GetStageInformation();
    if(FAILED(EDirectGraphics::GetInstance()->GetDevice()->Present(nullptr,nullptr,nullptr,nullptr)))
        throw std::runtime_error("Diagnostic capture presentation failed");
    EM_ASM({
        (function(){
        const requests=Module['wooCaptureFrames'],index=requests.findIndex(p=>p.stage===$0&&p.frame===$1);
        if(index<0)return;
        const canvas=document.querySelector('#canvas'),gl=canvas.getContext('webgl2');
        const previous=gl.getParameter(gl.READ_FRAMEBUFFER_BINDING);
        gl.bindFramebuffer(gl.READ_FRAMEBUFFER,null);
        const width=canvas.width,height=canvas.height,pixels=new Uint8Array(width*height*4);
        gl.readPixels(0,0,width,height,gl.RGBA,gl.UNSIGNED_BYTE,pixels);
        gl.bindFramebuffer(gl.READ_FRAMEBUFFER,previous);
        const image=document.createElement('canvas');image.width=width;image.height=height;
        const ctx=image.getContext('2d'),data=ctx.createImageData(width,height);
        for(let y=0;y<height;y++)data.data.set(pixels.subarray((height-1-y)*width*4,(height-y)*width*4),y*width*4);
        ctx.putImageData(data,0,0);
        (Module['wooCaptures']||(Module['wooCaptures']=[])).push(({
            stage:$0,frame:$1,width:width,height:height,png:image.toDataURL('image/png')}));
        requests.splice(index,1);
        })();
    },info->GetStageIndex(),info->GetCurrentFrame());
}
#endif
bool runGameFrame(bool render){
    applyTouchInput();
#ifdef WOO_REPLAY_VERIFY
    const bool capture=captureNextFrame();
    const bool result=EApplication::GetInstance()->RunFrame(render||capture);
    if(result&&capture&&EApplication::GetInstance()->DidBrowserFrameWork())captureRenderedFrame();
    return result;
#else
    return EApplication::GetInstance()->RunFrame(render);
#endif
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
    if(EM_ASM_INT({return document.hidden?1:0;})){previous=-1;accumulated=0;nextStateReport=0;runtimeFpsStart=-1;runtimeFpsSteps=runtimeFpsPresents=0;measureStart=-1;measuredSteps=measuredPresents=0;frameCosts.clear();callbackCosts.clear();measuredQueuedLoadMax=0;return EM_TRUE;}
    try {
        const bool observePerformance=EM_ASM_INT({return globalThis.ph3Diagnostics?.observePerformance?1:0;})!=0;
        const double begin=observePerformance?emscripten_get_now():0;
        auto fpsController=EFpsController::GetInstance();
        const int targetBefore=fpsController->GetBrowserTargetFps();
        const double deltaLimit=targetBefore>STANDARD_FPS?1000.0/STANDARD_FPS:100.0;
        accumulated+=previous<0?0:std::min(deltaLimit,now-previous);previous=now;
        // Native has a real loader thread. On Web process at most one queued
        // load per browser callback so unrelated async loads no longer become
        // one giant synchronous hitch.
        const double loadBegin=observePerformance?emscripten_get_now():0;
        if(auto* files=FileManager::GetBase())files->PumpLoads();
        const double queuedLoadMs=observePerformance?emscripten_get_now()-loadBegin:0;
        unsigned steps=0;
        bool rendered=false;
        bool diagnosticFast=false;
        auto device=EDirectGraphics::GetInstance()->GetDevice();
        device->BeginBrowserFrame();
        int fastSteps=0;
#if defined(WOO_REPLAY_VERIFY) || defined(PH3_REPLAY_BOUNDARY)
        // Diagnostic-only fast playback. Each call still consumes exactly one
        // original input/tick; only wall-clock waiting and intermediate draws
        // are omitted. The golden comparison must validate this lane.
        const int requestedFastSteps=EM_ASM_INT({return Module['wooReplayFastSteps']|0;});
        if(requestedFastSteps>0&&replayActive())fastSteps=std::min(requestedFastSteps,32);
#endif
#ifdef PH3_PERF_SCAN
        bool perfFastMode=false;
        const int requestedPerfFastSteps=EM_ASM_INT({
            const value=Number(Module['ph3PerfFastSteps']||0);
            return Number.isFinite(value)?Math.max(0,Math.min(64,value|0)):0;
        });
        if(fastSteps==0&&requestedPerfFastSteps>1){fastSteps=requestedPerfFastSteps;perfFastMode=true;}
#endif
        if(fastSteps>0){
            diagnosticFast=true;
            accumulated=0;
            for(int i=0;i<fastSteps;++i){
                const bool render=i+1==fastSteps;
#ifdef PH3_PERF_SCAN
                const double tickBegin=perfFastMode?emscripten_get_now():0;
#endif
                if(!runGameFrame(render))return EM_FALSE;
#ifdef PH3_PERF_SCAN
                if(perfFastMode)reportSlowFrame(emscripten_get_now()-tickBegin,0);
#endif
                if(!EApplication::GetInstance()->DidBrowserFrameWork())continue;
                ++frames;++steps;rendered=rendered||render;
            }
        }else{
            for(;;){
                const int targetFps=fpsController->GetBrowserTargetFps();
                const double stepMs=1000.0/targetFps;
                if(accumulated+0.001<stepMs)break;
                accumulated-=stepMs;
                const bool renderPermit=fpsController->AdvanceBrowserRenderPermit();
                const bool render=accumulated+0.001<stepMs&&renderPermit;
                if(!runGameFrame(render))return EM_FALSE;
                if(!EApplication::GetInstance()->DidBrowserFrameWork())continue;
                ++frames;
                ++steps;
                rendered=render;
            }
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
            // GPU destruction is safe only after all deferred draws are flushed.
            static unsigned textureMaintenance=0;
            if(++textureMaintenance%30==0)
                if(auto* textures=TextureManager::GetBase())textures->TrimBrowserCache();
#ifdef PH3_PERF_SCAN
            reportSlowFrame(emscripten_get_now()-presentBegin,1);
#endif
        }
        if(!diagnosticFast)updateRuntimeFps(now,steps,rendered);
        if(observePerformance)measure(begin,steps,rendered,queuedLoadMs);
        EM_ASM({if(globalThis.ph3Diagnostics)globalThis.ph3Diagnostics.frames=$0;},frames);
    }catch(gstd::wexception& error){fail(gstd::StringUtility::ConvertWideToMulti(error.what(),CP_UTF8));}
    catch(const std::exception& error){fail(error.what());}
    return failed?EM_FALSE:EM_TRUE;
}
}

extern "C" {
EMSCRIPTEN_KEEPALIVE void hsob_input_key(int key,int down){
    auto input=EDirectInput::GetInstance();
    if(input)input->SetInjectedKeyState(key,down!=0);
}
EMSCRIPTEN_KEEPALIVE void hsob_input_pulse(int key){
    auto input=EDirectInput::GetInstance();
    if(input)input->PulseInjectedKey(key);
}
EMSCRIPTEN_KEEPALIVE void hsob_input_clear(){
    auto input=EDirectInput::GetInstance();
    if(input)input->ClearInjectedKeyState();
}
EMSCRIPTEN_KEEPALIVE void hsob_touch_config(int enabled,int mode,double sensitivity,int focusMode,int doubleTap){
    touchController.enabled=enabled!=0;
    touchController.set_mode(std::clamp(mode,0,3));
    touchController.unlimited=mode==1;
    touchController.sensitivity=float(std::clamp(sensitivity,1.0,3.0));
    touchController.two_finger=focusMode==2;
    touchController.double_tap=doubleTap!=0;
    if(!touchController.enabled)touchController.cancel_transient();
}
EMSCRIPTEN_KEEPALIVE void hsob_touch_controls(int fire,int focus,std::uint32_t bomb,std::uint32_t escape,double x,double y){
    touchController.controls(fire!=0,focus!=0,bomb,escape,float(x),float(y));
}
EMSCRIPTEN_KEEPALIVE void hsob_touch_controls_baseline(int fire,int focus,std::uint32_t bomb,std::uint32_t escape,double x,double y){
    touchController.controls_baseline(fire!=0,focus!=0,bomb,escape,float(x),float(y));
}
EMSCRIPTEN_KEEPALIVE void hsob_touch_pointer(int type,int id,double x,double y,double now){
    auto input=EDirectInput::GetInstance();
    auto context=getTouchContext();
    const bool keySlow=input&&(input->IsNonTouchKeyDown(DIK_LSHIFT)||input->IsNonTouchKeyDown(DIK_RSHIFT));
    touchController.pointer(std::clamp(type,0,2),id,float(std::clamp(x,0.0,1.0)),float(std::clamp(y,0.0,1.0)),
        std::uint64_t(std::max(0.0,now)),context.state,keySlow);
}
EMSCRIPTEN_KEEPALIVE void hsob_touch_cancel(){
    touchController.cancel_transient();
    auto input=EDirectInput::GetInstance();
    if(input)input->ClearDirectTouchKeyState();
}
EMSCRIPTEN_KEEPALIVE void hsob_touch_begin_session(){
    touchController.begin_session();
}
EMSCRIPTEN_KEEPALIVE void hsob_set_ogg_decode_mode(int full){
    directx::portable::SetVorbisFullDecode(full!=0);
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
