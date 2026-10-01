#pragma once
#include "DxMath.hpp"

namespace directx::portable {
// One instance per draw, not per device or frame: a subsequent draw must see
// all SetTransform changes. XYZRHW streams never prepare this cache.
struct DrawTransforms {
    D3DXMATRIX world, view, projection;
    bool prepared = false;

    template<class State>
    void Prepare(const State& state, unsigned worldKey, unsigned viewKey, unsigned projectionKey) {
#ifndef PH3_LEGACY_VERTEX_CONSTANTS
        if (prepared) return;
#endif
        world = state.Transform(worldKey);
        view = state.Transform(viewKey);
        projection = state.Transform(projectionKey);
        prepared = true;
    }
};
}
