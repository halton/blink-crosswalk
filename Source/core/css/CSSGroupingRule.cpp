/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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
#include "core/css/CSSGroupingRule.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleRule.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

CSSGroupingRule::CSSGroupingRule(StyleRuleGroup* groupRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_groupRule(groupRule)
    , m_childRuleCSSOMWrappers(groupRule->childRules().size())
{
}

CSSGroupingRule::~CSSGroupingRule()
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->setParentRule(0);
    }
}

unsigned CSSGroupingRule::insertRule(const String& ruleString, unsigned index, ExceptionState& es)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());

    if (index > m_groupRule->childRules().size()) {
        es.throwDOMException(IndexSizeError, ExceptionMessages::failedToExecute("insertRule", "CSSGroupingRule", "the index " + String::number(index) + " must be less than or equal to the length of the rule list."));
        return 0;
    }

    CSSStyleSheet* styleSheet = parentStyleSheet();
    CSSParser parser(parserContext(), UseCounter::getFrom(styleSheet));
    RefPtr<StyleRuleBase> newRule = parser.parseRule(styleSheet ? styleSheet->contents() : 0, ruleString);
    if (!newRule) {
        es.throwDOMException(SyntaxError, ExceptionMessages::failedToExecute("insertRule", "CSSGroupingRule", "the rule '" + ruleString + "' is invalid and cannot be parsed."));
        return 0;
    }

    if (newRule->isImportRule()) {
        // FIXME: an HierarchyRequestError should also be thrown for a @charset or a nested
        // @media rule. They are currently not getting parsed, resulting in a SyntaxError
        // to get raised above.
        es.throwDOMException(HierarchyRequestError, ExceptionMessages::failedToExecute("insertRule", "CSSGroupingRule", "'@import' rules cannot be inserted inside a group rule."));
        return 0;
    }
    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_groupRule->wrapperInsertRule(index, newRule);

    m_childRuleCSSOMWrappers.insert(index, RefPtr<CSSRule>());
    return index;
}

void CSSGroupingRule::deleteRule(unsigned index, ExceptionState& es)
{
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());

    if (index >= m_groupRule->childRules().size()) {
        es.throwDOMException(IndexSizeError, ExceptionMessages::failedToExecute("deleteRule", "CSSGroupingRule", "the index " + String::number(index) + " is greated than the length of the rule list."));
        return;
    }

    CSSStyleSheet::RuleMutationScope mutationScope(this);

    m_groupRule->wrapperRemoveRule(index);

    if (m_childRuleCSSOMWrappers[index])
        m_childRuleCSSOMWrappers[index]->setParentRule(0);
    m_childRuleCSSOMWrappers.remove(index);
}

void CSSGroupingRule::appendCssTextForItems(StringBuilder& result) const
{
    unsigned size = length();
    for (unsigned i = 0; i < size; ++i) {
        result.appendLiteral("  ");
        result.append(item(i)->cssText());
        result.append('\n');
    }
}

unsigned CSSGroupingRule::length() const
{
    return m_groupRule->childRules().size();
}

CSSRule* CSSGroupingRule::item(unsigned index) const
{
    if (index >= length())
        return 0;
    ASSERT(m_childRuleCSSOMWrappers.size() == m_groupRule->childRules().size());
    RefPtr<CSSRule>& rule = m_childRuleCSSOMWrappers[index];
    if (!rule)
        rule = m_groupRule->childRules()[index]->createCSSOMWrapper(const_cast<CSSGroupingRule*>(this));
    return rule.get();
}

CSSRuleList* CSSGroupingRule::cssRules() const
{
    if (!m_ruleListCSSOMWrapper)
        m_ruleListCSSOMWrapper = adoptPtr(new LiveCSSRuleList<CSSGroupingRule>(const_cast<CSSGroupingRule*>(this)));
    return m_ruleListCSSOMWrapper.get();
}

void CSSGroupingRule::reattach(StyleRuleBase* rule)
{
    ASSERT(rule);
    m_groupRule = static_cast<StyleRuleGroup*>(rule);
    for (unsigned i = 0; i < m_childRuleCSSOMWrappers.size(); ++i) {
        if (m_childRuleCSSOMWrappers[i])
            m_childRuleCSSOMWrappers[i]->reattach(m_groupRule->childRules()[i].get());
    }
}

} // namespace WebCore
