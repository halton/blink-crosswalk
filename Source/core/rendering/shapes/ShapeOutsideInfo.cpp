/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/shapes/ShapeOutsideInfo.h"

#include "core/rendering/FloatingObjects.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderBox.h"

namespace WebCore {
bool ShapeOutsideInfo::isEnabledFor(const RenderBox* box)
{
    ShapeValue* shapeValue = box->style()->shapeOutside();
    if (!box->isFloating() || !shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        return shapeValue->shape();
    case ShapeValue::Image:
        return shapeValue->isImageValid();
    case ShapeValue::Outside:
        return false;
    }

    return false;
}

bool ShapeOutsideInfo::computeSegmentsForContainingBlockLine(const RenderBlock* containingBlock, const FloatingObject* floatingObject, LayoutUnit lineTop, LayoutUnit lineHeight)
{
    LayoutUnit shapeTop = floatingObject->logicalTop(containingBlock->isHorizontalWritingMode()) + std::max(LayoutUnit(), containingBlock->marginBeforeForChild(m_renderer));
    LayoutUnit lineTopInShapeCoordinates = lineTop - shapeTop + logicalTopOffset();
    return computeSegmentsForLine(lineTopInShapeCoordinates, lineHeight);
}

bool ShapeOutsideInfo::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight)
{
    if (shapeSizeDirty() || m_lineTop != lineTop || m_lineHeight != lineHeight) {
        if (ShapeInfo<RenderBox>::computeSegmentsForLine(lineTop, lineHeight)) {
            m_leftSegmentMarginBoxDelta = m_segments[0].logicalLeft + m_renderer->marginStart();
            m_rightSegmentMarginBoxDelta = m_segments[m_segments.size()-1].logicalRight - m_renderer->logicalWidth() - m_renderer->marginEnd();
        } else {
            m_leftSegmentMarginBoxDelta = m_renderer->logicalWidth() + m_renderer->marginStart();
            m_rightSegmentMarginBoxDelta = -m_renderer->logicalWidth() - m_renderer->marginEnd();
        }
        m_lineTop = lineTop;
    }

    return m_segments.size();
}

ShapeValue* ShapeOutsideInfo::shapeValue() const
{
    return m_renderer->style()->shapeOutside();
}

}
