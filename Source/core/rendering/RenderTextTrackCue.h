/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderTextTrackCue_h
#define RenderTextTrackCue_h

#include "core/platform/graphics/FloatPoint.h"
#include "core/rendering/RenderBlockFlow.h"
#include "core/rendering/RenderInline.h"

namespace WebCore {

class TextTrackCueBox;

class RenderTextTrackCue FINAL : public RenderBlockFlow {
public:
    explicit RenderTextTrackCue(TextTrackCueBox*);

private:
    virtual void layout() OVERRIDE;
    virtual bool supportsPartialLayout() const OVERRIDE { return false; }

    bool isOutside() const;
    bool isOverlapping() const;
    bool shouldSwitchDirection(InlineFlowBox*, LayoutUnit) const;

    void moveBoxesByStep(LayoutUnit);
    bool switchDirection(bool&, LayoutUnit&);

    bool findFirstLineBox(InlineFlowBox*&);
    bool initializeLayoutParameters(InlineFlowBox*, LayoutUnit&, LayoutUnit&);
    void placeBoxInDefaultPosition(LayoutUnit, bool&);
    void repositionCueSnapToLinesSet();
    void repositionCueSnapToLinesNotSet();
    void repositionGenericCue();

    TextTrackCue* m_cue;
    FloatPoint m_fallbackPosition;
};

} // namespace WebCore

#endif // RenderTextTrackCue_h
