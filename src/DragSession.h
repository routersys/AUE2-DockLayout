#pragma once

#include <windows.h>

#include "FloatOrigin.h"

namespace dl {

class DragSession {
public:
    bool BeginPanel(POINT clientPt);
    bool BeginFloat(HWND window);
    void Track(POINT screenPt);
    void Commit(POINT screenPt);
    void Finish();
    void Cancel();
    void Release();

    bool Holding() const;
    bool Dragging() const;
    HWND Window() const { return window_; }

private:
    enum class Phase { Idle, Armed, Dragging };

    bool Start();
    void Execute();
    void ExecuteGroup(int target);
    void ExecuteInsert(int target, bool after);
    void ExecuteDetach();
    void Replay(POINT screenPt);
    void Reset();

    Phase phase_ = Phase::Idle;
    HWND window_ = nullptr;
    POINT origin_ = {};
    int panel_ = -1;
    bool pending_ = false;
    bool hasOrigin_ = false;
    FloatOrigin floatOrigin_;
};

DragSession& Drag();

}
