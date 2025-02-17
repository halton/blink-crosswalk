/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/svg/graphics/filters/SVGFEImage.h"

#include "SkBitmapSource.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/filters/Filter.h"
#include "core/platform/graphics/transforms/AffineTransform.h"
#include "core/platform/text/TextStream.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/SVGURIReference.h"

namespace WebCore {

FEImage::FEImage(Filter* filter, PassRefPtr<Image> image, const SVGPreserveAspectRatio& preserveAspectRatio)
    : FilterEffect(filter)
    , m_image(image)
    , m_document(0)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

FEImage::FEImage(Filter* filter, Document& document, const String& href, const SVGPreserveAspectRatio& preserveAspectRatio)
    : FilterEffect(filter)
    , m_document(&document)
    , m_href(href)
    , m_preserveAspectRatio(preserveAspectRatio)
{
}

PassRefPtr<FEImage> FEImage::createWithImage(Filter* filter, PassRefPtr<Image> image, const SVGPreserveAspectRatio& preserveAspectRatio)
{
    return adoptRef(new FEImage(filter, image, preserveAspectRatio));
}

PassRefPtr<FEImage> FEImage::createWithIRIReference(Filter* filter, Document& document, const String& href, const SVGPreserveAspectRatio& preserveAspectRatio)
{
    return adoptRef(new FEImage(filter, document, href, preserveAspectRatio));
}

void FEImage::determineAbsolutePaintRect()
{
    FloatRect paintRect = filter()->absoluteTransform().mapRect(filterPrimitiveSubregion());
    FloatRect srcRect;
    if (m_image) {
        srcRect.setSize(m_image->size());
        m_preserveAspectRatio.transformRect(paintRect, srcRect);
    } else if (RenderObject* renderer = referencedRenderer())
        srcRect = filter()->absoluteTransform().mapRect(renderer->repaintRectInLocalCoordinates());

    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

RenderObject* FEImage::referencedRenderer() const
{
    if (!m_document)
        return 0;
    Element* hrefElement = SVGURIReference::targetElementFromIRIString(m_href, *m_document);
    if (!hrefElement || !hrefElement->isSVGElement())
        return 0;
    return hrefElement->renderer();
}

void FEImage::applySoftware()
{
    RenderObject* renderer = referencedRenderer();
    if (!m_image && !renderer)
        return;

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    FloatRect destRect = filter()->absoluteTransform().mapRect(filterPrimitiveSubregion());

    FloatRect srcRect;
    if (renderer)
        srcRect = filter()->absoluteTransform().mapRect(renderer->repaintRectInLocalCoordinates());
    else {
        srcRect = FloatRect(FloatPoint(), m_image->size());
        m_preserveAspectRatio.transformRect(destRect, srcRect);
    }

    IntPoint paintLocation = absolutePaintRect().location();
    destRect.move(-paintLocation.x(), -paintLocation.y());

    // FEImage results are always in ColorSpaceDeviceRGB
    setResultColorSpace(ColorSpaceDeviceRGB);

    if (renderer) {
        SVGElement* contextNode = toSVGElement(renderer->node());
        if (contextNode->hasRelativeLengths()) {
            SVGLengthContext lengthContext(contextNode);
            float width = 0;
            float height = 0;

            // If we're referencing an element with percentage units, eg. <rect with="30%"> those values were resolved against the viewport.
            // Build up a transformation that maps from the viewport space to the filter primitive subregion.
            if (lengthContext.determineViewport(width, height))
                resultImage->context()->concatCTM(makeMapBetweenRects(FloatRect(0, 0, width, height), destRect));
        } else {
            const AffineTransform& absoluteTransform = filter()->absoluteTransform();
            resultImage->context()->concatCTM(absoluteTransform);
        }

        AffineTransform contentTransformation;
        SVGRenderingContext::renderSubtree(resultImage->context(), renderer, contentTransformation);
        return;
    }

    resultImage->context()->drawImage(m_image.get(), destRect, srcRect);
}

TextStream& FEImage::externalRepresentation(TextStream& ts, int indent) const
{
    IntSize imageSize;
    if (m_image)
        imageSize = m_image->size();
    else if (RenderObject* renderer = referencedRenderer())
        imageSize = enclosingIntRect(renderer->repaintRectInLocalCoordinates()).size();
    writeIndent(ts, indent);
    ts << "[feImage";
    FilterEffect::externalRepresentation(ts);
    ts << " image-size=\"" << imageSize.width() << "x" << imageSize.height() << "\"]\n";
    // FIXME: should this dump also object returned by SVGFEImage::image() ?
    return ts;
}

PassRefPtr<SkImageFilter> FEImage::createImageFilter(SkiaImageFilterBuilder* builder)
{
    if (!m_image)
        return 0;

    if (!m_image->nativeImageForCurrentFrame())
        return 0;

    return adoptRef(new SkBitmapSource(m_image->nativeImageForCurrentFrame()->bitmap()));
}

} // namespace WebCore
