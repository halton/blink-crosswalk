/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "core/css/StyleSheetList.h"

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/html/HTMLStyleElement.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace HTMLNames;

StyleSheetList::StyleSheetList(Document* document)
    : m_document(document)
{
}

StyleSheetList::~StyleSheetList()
{
}

inline const Vector<RefPtr<StyleSheet> >& StyleSheetList::styleSheets() const
{
    if (!m_document)
        return m_detachedStyleSheets;
    return m_document->styleEngine()->styleSheetsForStyleSheetList();
}

void StyleSheetList::detachFromDocument()
{
    m_detachedStyleSheets = m_document->styleEngine()->styleSheetsForStyleSheetList();
    m_document = 0;
}

unsigned StyleSheetList::length() const
{
    return styleSheets().size();
}

StyleSheet* StyleSheetList::item(unsigned index)
{
    const Vector<RefPtr<StyleSheet> >& sheets = styleSheets();
    return index < sheets.size() ? sheets[index].get() : 0;
}

HTMLStyleElement* StyleSheetList::getNamedItem(const String& name) const
{
    if (!m_document)
        return 0;

    // IE also supports retrieving a stylesheet by name, using the name/id of the <style> tag
    // (this is consistent with all the other collections)
    // ### Bad implementation because returns a single element (are IDs always unique?)
    // and doesn't look for name attribute.
    // But unicity of stylesheet ids is good practice anyway ;)
    // FIXME: We should figure out if we should change this or fix the spec.
    Element* element = m_document->getElementById(name);
    if (element && element->hasTagName(styleTag))
        return toHTMLStyleElement(element);
    return 0;
}

CSSStyleSheet* StyleSheetList::anonymousNamedGetter(const AtomicString& name)
{
    HTMLStyleElement* item = getNamedItem(name);
    if (!item)
        return 0;
    return item->sheet();
}

} // namespace WebCore
