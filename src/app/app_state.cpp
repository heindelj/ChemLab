#include "app/app_state.h"

Structure* AppState::ActiveStructure() {
    if (activeStructure < 0 || activeStructure >= (int)structures.size()) return nullptr;
    return &structures[activeStructure];
}

const Structure* AppState::ActiveStructure() const {
    if (activeStructure < 0 || activeStructure >= (int)structures.size()) return nullptr;
    return &structures[activeStructure];
}

Frames* AppState::ActiveFrames() {
    Structure* s = ActiveStructure();
    return s ? &s->frames : nullptr;
}

Atoms* AppState::ActiveAtoms() {
    Structure* s = ActiveStructure();
    if (!s || s->frames.nframes == 0) return nullptr;
    if (s->activeFrame < 0 || s->activeFrame >= (int)s->frames.nframes) s->activeFrame = 0;
    return &s->frames.atoms[s->activeFrame];
}

const Atoms* AppState::ActiveAtoms() const {
    const Structure* s = ActiveStructure();
    if (!s || s->frames.nframes == 0) return nullptr;
    const int f = (s->activeFrame < 0 || s->activeFrame >= (int)s->frames.nframes) ? 0 : s->activeFrame;
    return &s->frames.atoms[f];
}

int AppState::ActiveFrameIndex() const {
    const Structure* s = ActiveStructure();
    return s ? s->activeFrame : -1;
}

int AppState::FrameCount() const {
    const Structure* s = ActiveStructure();
    return s ? (int)s->frames.nframes : 0;
}
