/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef Pair_h
#define Pair_h

#include "core/css/CSSPrimitiveValue.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

// A primitive value representing a pair.  This is useful for properties like border-radius, background-size/position,
// and border-spacing (all of which are space-separated sets of two values).  At the moment we are only using it for
// border-radius and background-size, but (FIXME) border-spacing and background-position could be converted over to use
// it (eliminating some extra -webkit- internal properties).
class Pair : public RefCounted<Pair> {
public:
    enum IdenticalValuesPolicy { DropIdenticalValues, KeepIdenticalValues };

    static PassRefPtr<Pair> create()
    {
        return adoptRef(new Pair);
    }
    static PassRefPtr<Pair> create(PassRefPtr<CSSPrimitiveValue> first, PassRefPtr<CSSPrimitiveValue> second, IdenticalValuesPolicy identicalValuesPolicy)
    {
        return adoptRef(new Pair(first, second, identicalValuesPolicy));
    }
    virtual ~Pair() { }

    CSSPrimitiveValue* first() const { return m_first.get(); }
    CSSPrimitiveValue* second() const { return m_second.get(); }
    IdenticalValuesPolicy identicalValuesPolicy() const { return m_identicalValuesPolicy; }

    void setFirst(PassRefPtr<CSSPrimitiveValue> first) { m_first = first; }
    void setSecond(PassRefPtr<CSSPrimitiveValue> second) { m_second = second; }
    void setIdenticalValuesPolicy(IdenticalValuesPolicy identicalValuesPolicy) { m_identicalValuesPolicy = identicalValuesPolicy; }

    String cssText() const
    {
        return generateCSSString(first()->cssText(), second()->cssText(), m_identicalValuesPolicy);
    }

    bool equals(const Pair& other) const
    {
        return compareCSSValuePtr(m_first, other.m_first)
            && compareCSSValuePtr(m_second, other.m_second)
            && m_identicalValuesPolicy == other.m_identicalValuesPolicy;
    }

    String serializeResolvingVariables(const HashMap<AtomicString, String>& variables) const
    {
        return generateCSSString(
            first()->customSerializeResolvingVariables(variables),
            second()->customSerializeResolvingVariables(variables),
            m_identicalValuesPolicy);
    }

    bool hasVariableReference() const { return first()->hasVariableReference() || second()->hasVariableReference(); }

private:
    Pair()
        : m_first(0)
        , m_second(0)
        , m_identicalValuesPolicy(DropIdenticalValues) { }

    Pair(PassRefPtr<CSSPrimitiveValue> first, PassRefPtr<CSSPrimitiveValue> second, IdenticalValuesPolicy identicalValuesPolicy)
        : m_first(first)
        , m_second(second)
        , m_identicalValuesPolicy(identicalValuesPolicy) { }

    static String generateCSSString(const String& first, const String& second, IdenticalValuesPolicy identicalValuesPolicy)
    {
        if (identicalValuesPolicy == DropIdenticalValues && first == second)
            return first;
        return first + ' ' + second;
    }

    RefPtr<CSSPrimitiveValue> m_first;
    RefPtr<CSSPrimitiveValue> m_second;
    IdenticalValuesPolicy m_identicalValuesPolicy;
};

} // namespace

#endif
