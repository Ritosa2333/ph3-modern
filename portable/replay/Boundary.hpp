#pragma once
// Read-only lifecycle evidence. This does not start, skip, or finish a stage.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "Trace.hpp"
namespace woo { namespace replay {
inline void Boundary(StgStageController* controller, bool begin) {
#ifdef WOO_REPLAY_VERIFY
    if(begin)woo::verify::Begin(controller);else woo::verify::End(controller);
#endif
#ifdef __EMSCRIPTEN__
    auto state = controller->GetStageInformation();
    if (!state->IsReplay()) return;
    auto recorded = state->GetReplayData();
    EM_ASM({
        const d=globalThis.ph3Diagnostics;if(!d)return;
        const list=d.replayBoundaries||(d.replayBoundaries=[]);
        if(list.length>=256){d.errors.push('Replay lifecycle event overflow');d.phase='error';return;}
        list.push(({kind:$0?'start':'end',index:$1,frame:$2,score:$3,
            expectedFrame:$4,expectedScore:$5,time:performance.now()}));
    },begin,state->GetStageIndex(),state->GetCurrentFrame(),double(state->GetScore()),
      recorded!=NULL?recorded->GetEndFrame():-1,recorded!=NULL?double(recorded->GetLastScore()):0.0);
#else
    (void)controller;(void)begin;
#endif
}
} }
