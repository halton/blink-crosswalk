/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "core/editing/Editor.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Clipboard.h"
#include "core/dom/ClipboardEvent.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/EventNames.h"
#include "core/dom/KeyboardEvent.h"
#include "core/dom/NodeList.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/dom/TextEvent.h"
#include "core/editing/ApplyStyleCommand.h"
#include "core/editing/DeleteSelectionCommand.h"
#include "core/editing/IndentOutdentCommand.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/InsertListCommand.h"
#include "core/editing/ModifySelectionListLevel.h"
#include "core/editing/RemoveFormatCommand.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/ReplaceSelectionCommand.h"
#include "core/editing/SimplifyMarkupCommand.h"
#include "core/editing/SpellCheckRequester.h"
#include "core/editing/TextCheckingHelper.h"
#include "core/editing/TextIterator.h"
#include "core/editing/TypingCommand.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/loader/EmptyClients.h"
#include "core/page/EditorClient.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/KillRing.h"
#include "core/platform/Pasteboard.h"
#include "core/platform/Sound.h"
#include "core/platform/chromium/ClipboardChromium.h"
#include "core/platform/text/TextCheckerClient.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderTextControl.h"
#include "wtf/unicode/CharacterNames.h"

namespace WebCore {

using namespace std;
using namespace HTMLNames;
using namespace WTF;
using namespace Unicode;

Editor::RevealSelectionScope::RevealSelectionScope(Editor* editor)
    : m_editor(editor)
{
    ++m_editor->m_preventRevealSelection;
}

Editor::RevealSelectionScope::~RevealSelectionScope()
{
    ASSERT(m_editor->m_preventRevealSelection);
    --m_editor->m_preventRevealSelection;
    if (!m_editor->m_preventRevealSelection)
        m_editor->m_frame->selection().revealSelection(ScrollAlignment::alignToEdgeIfNeeded, RevealExtent);
}

namespace {

bool isSelectionInTextField(const VisibleSelection& selection)
{
    HTMLTextFormControlElement* textControl = enclosingTextFormControl(selection.start());
    return textControl && textControl->hasTagName(inputTag) && toHTMLInputElement(textControl)->isTextField();
}

} // namespace

// When an event handler has moved the selection outside of a text control
// we should use the target control's selection for this editing operation.
VisibleSelection Editor::selectionForCommand(Event* event)
{
    VisibleSelection selection = m_frame->selection().selection();
    if (!event)
        return selection;
    // If the target is a text control, and the current selection is outside of its shadow tree,
    // then use the saved selection for that text control.
    HTMLTextFormControlElement* textFormControlOfSelectionStart = enclosingTextFormControl(selection.start());
    HTMLTextFormControlElement* textFromControlOfTarget = isHTMLTextFormControlElement(event->target()->toNode()) ? toHTMLTextFormControlElement(event->target()->toNode()) : 0;
    if (textFromControlOfTarget && (selection.start().isNull() || textFromControlOfTarget != textFormControlOfSelectionStart)) {
        if (RefPtr<Range> range = textFromControlOfTarget->selection())
            return VisibleSelection(range.get(), DOWNSTREAM, selection.isDirectional());
    }
    return selection;
}

// Function considers Mac editing behavior a fallback when Page or Settings is not available.
EditingBehavior Editor::behavior() const
{
    if (!m_frame || !m_frame->settings())
        return EditingBehavior(EditingMacBehavior);

    return EditingBehavior(m_frame->settings()->editingBehaviorType());
}

static EditorClient& emptyEditorClient()
{
    DEFINE_STATIC_LOCAL(EmptyEditorClient, client, ());
    return client;
}

EditorClient& Editor::client() const
{
    if (Page* page = m_frame->page())
        return page->editorClient();
    return emptyEditorClient();
}

TextCheckerClient& Editor::textChecker() const
{
    return client().textChecker();
}

void Editor::handleKeyboardEvent(KeyboardEvent* event)
{
    client().handleKeyboardEvent(event);
}

bool Editor::handleTextEvent(TextEvent* event)
{
    // Default event handling for Drag and Drop will be handled by DragController
    // so we leave the event for it.
    if (event->isDrop())
        return false;

    if (event->isPaste()) {
        if (event->pastingFragment())
            replaceSelectionWithFragment(event->pastingFragment(), false, event->shouldSmartReplace(), event->shouldMatchStyle());
        else
            replaceSelectionWithText(event->data(), false, event->shouldSmartReplace());
        return true;
    }

    String data = event->data();
    if (data == "\n") {
        if (event->isLineBreak())
            return insertLineBreak();
        return insertParagraphSeparator();
    }

    return insertTextWithoutSendingTextEvent(data, false, event);
}

bool Editor::canEdit() const
{
    return m_frame->selection().rootEditableElement();
}

bool Editor::canEditRichly() const
{
    return m_frame->selection().isContentRichlyEditable();
}

// WinIE uses onbeforecut and onbeforepaste to enables the cut and paste menu items.  They
// also send onbeforecopy, apparently for symmetry, but it doesn't affect the menu items.
// We need to use onbeforecopy as a real menu enabler because we allow elements that are not
// normally selectable to implement copy/paste (like divs, or a document body).

bool Editor::canDHTMLCut()
{
    return !m_frame->selection().isInPasswordField() && !dispatchCPPEvent(eventNames().beforecutEvent, ClipboardNumb);
}

bool Editor::canDHTMLCopy()
{
    return !m_frame->selection().isInPasswordField() && !dispatchCPPEvent(eventNames().beforecopyEvent, ClipboardNumb);
}

bool Editor::canDHTMLPaste()
{
    return !dispatchCPPEvent(eventNames().beforepasteEvent, ClipboardNumb);
}

bool Editor::canCut() const
{
    return canCopy() && canDelete();
}

static HTMLImageElement* imageElementFromImageDocument(Document* document)
{
    if (!document)
        return 0;
    if (!document->isImageDocument())
        return 0;

    HTMLElement* body = document->body();
    if (!body)
        return 0;

    Node* node = body->firstChild();
    if (!node)
        return 0;
    if (!node->hasTagName(imgTag))
        return 0;
    return toHTMLImageElement(node);
}

bool Editor::canCopy() const
{
    if (imageElementFromImageDocument(m_frame->document()))
        return true;
    FrameSelection& selection = m_frame->selection();
    return selection.isRange() && !selection.isInPasswordField();
}

bool Editor::canPaste() const
{
    return canEdit();
}

bool Editor::canDelete() const
{
    FrameSelection& selection = m_frame->selection();
    return selection.isRange() && selection.rootEditableElement();
}

bool Editor::canDeleteRange(Range* range) const
{
    Node* startContainer = range->startContainer();
    Node* endContainer = range->endContainer();
    if (!startContainer || !endContainer)
        return false;

    if (!startContainer->rendererIsEditable() || !endContainer->rendererIsEditable())
        return false;

    if (range->collapsed(IGNORE_EXCEPTION)) {
        VisiblePosition start(range->startPosition(), DOWNSTREAM);
        VisiblePosition previous = start.previous();
        // FIXME: We sometimes allow deletions at the start of editable roots, like when the caret is in an empty list item.
        if (previous.isNull() || previous.deepEquivalent().deprecatedNode()->rootEditableElement() != startContainer->rootEditableElement())
            return false;
    }
    return true;
}

bool Editor::smartInsertDeleteEnabled()
{
    return client().smartInsertDeleteEnabled();
}

bool Editor::canSmartCopyOrDelete()
{
    return client().smartInsertDeleteEnabled() && m_frame->selection().granularity() == WordGranularity;
}

bool Editor::isSelectTrailingWhitespaceEnabled()
{
    return client().isSelectTrailingWhitespaceEnabled();
}

bool Editor::deleteWithDirection(SelectionDirection direction, TextGranularity granularity, bool killRing, bool isTypingAction)
{
    if (!canEdit())
        return false;

    if (m_frame->selection().isRange()) {
        if (isTypingAction) {
            ASSERT(m_frame->document());
            TypingCommand::deleteKeyPressed(*m_frame->document(), canSmartCopyOrDelete() ? TypingCommand::SmartDelete : 0, granularity);
            revealSelectionAfterEditingOperation();
        } else {
            if (killRing)
                addToKillRing(selectedRange().get(), false);
            deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
            // Implicitly calls revealSelectionAfterEditingOperation().
        }
    } else {
        TypingCommand::Options options = 0;
        if (canSmartCopyOrDelete())
            options |= TypingCommand::SmartDelete;
        if (killRing)
            options |= TypingCommand::KillRing;
        switch (direction) {
        case DirectionForward:
        case DirectionRight:
            ASSERT(m_frame->document());
            TypingCommand::forwardDeleteKeyPressed(*m_frame->document(), options, granularity);
            break;
        case DirectionBackward:
        case DirectionLeft:
            ASSERT(m_frame->document());
            TypingCommand::deleteKeyPressed(*m_frame->document(), options, granularity);
            break;
        }
        revealSelectionAfterEditingOperation();
    }

    // FIXME: We should to move this down into deleteKeyPressed.
    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    if (killRing)
        setStartNewKillRingSequence(false);

    return true;
}

void Editor::deleteSelectionWithSmartDelete(bool smartDelete)
{
    if (m_frame->selection().isNone())
        return;

    ASSERT(m_frame->document());
    DeleteSelectionCommand::create(*m_frame->document(), smartDelete)->apply();
}

void Editor::pasteAsPlainText(const String& pastingText, bool smartReplace)
{
    Node* target = findEventTargetFromSelection();
    if (!target)
        return;
    target->dispatchEvent(TextEvent::createForPlainTextPaste(m_frame->domWindow(), pastingText, smartReplace), IGNORE_EXCEPTION);
}

void Editor::pasteAsFragment(PassRefPtr<DocumentFragment> pastingFragment, bool smartReplace, bool matchStyle)
{
    Node* target = findEventTargetFromSelection();
    if (!target)
        return;
    target->dispatchEvent(TextEvent::createForFragmentPaste(m_frame->domWindow(), pastingFragment, smartReplace, matchStyle), IGNORE_EXCEPTION);
}

void Editor::pasteAsPlainTextBypassingDHTML()
{
    pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::pasteAsPlainTextWithPasteboard(Pasteboard* pasteboard)
{
    String text = pasteboard->plainText();
    if (client().shouldInsertText(text, selectedRange().get(), EditorInsertActionPasted))
        pasteAsPlainText(text, canSmartReplaceWithPasteboard(pasteboard));
}

void Editor::pasteWithPasteboard(Pasteboard* pasteboard, bool allowPlainText)
{
    RefPtr<Range> range = selectedRange();
    bool chosePlainText;
    RefPtr<DocumentFragment> fragment = pasteboard->documentFragment(m_frame, range, allowPlainText, chosePlainText);
    if (fragment && shouldInsertFragment(fragment, range, EditorInsertActionPasted))
        pasteAsFragment(fragment, canSmartReplaceWithPasteboard(pasteboard), chosePlainText);
}

bool Editor::canSmartReplaceWithPasteboard(Pasteboard* pasteboard)
{
    return client().smartInsertDeleteEnabled() && pasteboard->canSmartReplace();
}

bool Editor::shouldInsertFragment(PassRefPtr<DocumentFragment> fragment, PassRefPtr<Range> replacingDOMRange, EditorInsertAction givenAction)
{
    if (fragment) {
        Node* child = fragment->firstChild();
        if (child && fragment->lastChild() == child && child->isCharacterDataNode())
            return client().shouldInsertText(toCharacterData(child)->data(), replacingDOMRange.get(), givenAction);
    }

    return client().shouldInsertNode(fragment.get(), replacingDOMRange.get(), givenAction);
}

void Editor::replaceSelectionWithFragment(PassRefPtr<DocumentFragment> fragment, bool selectReplacement, bool smartReplace, bool matchStyle)
{
    if (m_frame->selection().isNone() || !m_frame->selection().isContentEditable() || !fragment)
        return;

    ReplaceSelectionCommand::CommandOptions options = ReplaceSelectionCommand::PreventNesting | ReplaceSelectionCommand::SanitizeFragment;
    if (selectReplacement)
        options |= ReplaceSelectionCommand::SelectReplacement;
    if (smartReplace)
        options |= ReplaceSelectionCommand::SmartReplace;
    if (matchStyle)
        options |= ReplaceSelectionCommand::MatchStyle;
    ASSERT(m_frame->document());
    ReplaceSelectionCommand::create(*m_frame->document(), fragment, options, EditActionPaste)->apply();
    revealSelectionAfterEditingOperation();

    if (m_frame->selection().isInPasswordField() || !isContinuousSpellCheckingEnabled())
        return;
    Node* nodeToCheck = m_frame->selection().rootEditableElement();
    if (!nodeToCheck)
        return;

    RefPtr<Range> rangeToCheck = Range::create(*m_frame->document(), firstPositionInNode(nodeToCheck), lastPositionInNode(nodeToCheck));
    TextCheckingParagraph textToCheck(rangeToCheck, rangeToCheck);
    bool asynchronous = true;
    chunkAndMarkAllMisspellingsAndBadGrammar(resolveTextCheckingTypeMask(TextCheckingTypeSpelling | TextCheckingTypeGrammar), textToCheck, asynchronous);
}

void Editor::replaceSelectionWithText(const String& text, bool selectReplacement, bool smartReplace)
{
    replaceSelectionWithFragment(createFragmentFromText(selectedRange().get(), text), selectReplacement, smartReplace, true);
}

PassRefPtr<Range> Editor::selectedRange()
{
    if (!m_frame)
        return 0;
    return m_frame->selection().toNormalizedRange();
}

bool Editor::shouldDeleteRange(Range* range) const
{
    if (!range || range->collapsed(IGNORE_EXCEPTION))
        return false;

    if (!canDeleteRange(range))
        return false;

    return client().shouldDeleteRange(range);
}

bool Editor::tryDHTMLCopy()
{
    if (m_frame->selection().isInPasswordField())
        return false;

    return !dispatchCPPEvent(eventNames().copyEvent, ClipboardWritable);
}

bool Editor::tryDHTMLCut()
{
    if (m_frame->selection().isInPasswordField())
        return false;

    return !dispatchCPPEvent(eventNames().cutEvent, ClipboardWritable);
}

bool Editor::tryDHTMLPaste()
{
    return !dispatchCPPEvent(eventNames().pasteEvent, ClipboardReadable);
}

bool Editor::shouldInsertText(const String& text, Range* range, EditorInsertAction action) const
{
    return client().shouldInsertText(text, range, action);
}

void Editor::notifyComponentsOnChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions options)
{
    client().respondToChangedSelection(m_frame);
    setStartNewKillRingSequence(true);
}

void Editor::respondToChangedContents(const VisibleSelection& endingSelection)
{
    if (AXObjectCache::accessibilityEnabled()) {
        Node* node = endingSelection.start().deprecatedNode();
        if (AXObjectCache* cache = m_frame->document()->existingAXObjectCache())
            cache->postNotification(node, AXObjectCache::AXValueChanged, false);
    }

    updateMarkersForWordsAffectedByEditing(true);
    client().respondToChangedContents();
}

TriState Editor::selectionUnorderedListState() const
{
    if (m_frame->selection().isCaret()) {
        if (enclosingNodeWithTag(m_frame->selection().selection().start(), ulTag))
            return TrueTriState;
    } else if (m_frame->selection().isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selection().selection().start(), ulTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selection().selection().end(), ulTag);
        if (startNode && endNode && startNode == endNode)
            return TrueTriState;
    }

    return FalseTriState;
}

TriState Editor::selectionOrderedListState() const
{
    if (m_frame->selection().isCaret()) {
        if (enclosingNodeWithTag(m_frame->selection().selection().start(), olTag))
            return TrueTriState;
    } else if (m_frame->selection().isRange()) {
        Node* startNode = enclosingNodeWithTag(m_frame->selection().selection().start(), olTag);
        Node* endNode = enclosingNodeWithTag(m_frame->selection().selection().end(), olTag);
        if (startNode && endNode && startNode == endNode)
            return TrueTriState;
    }

    return FalseTriState;
}

PassRefPtr<Node> Editor::insertOrderedList()
{
    if (!canEditRichly())
        return 0;

    ASSERT(m_frame->document());
    RefPtr<Node> newList = InsertListCommand::insertList(*m_frame->document(), InsertListCommand::OrderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

PassRefPtr<Node> Editor::insertUnorderedList()
{
    if (!canEditRichly())
        return 0;

    ASSERT(m_frame->document());
    RefPtr<Node> newList = InsertListCommand::insertList(*m_frame->document(), InsertListCommand::UnorderedList);
    revealSelectionAfterEditingOperation();
    return newList;
}

bool Editor::canIncreaseSelectionListLevel()
{
    ASSERT(m_frame->document());
    return canEditRichly() && IncreaseSelectionListLevelCommand::canIncreaseSelectionListLevel(*m_frame->document());
}

bool Editor::canDecreaseSelectionListLevel()
{
    ASSERT(m_frame->document());
    return canEditRichly() && DecreaseSelectionListLevelCommand::canDecreaseSelectionListLevel(*m_frame->document());
}

PassRefPtr<Node> Editor::increaseSelectionListLevel()
{
    if (!canEditRichly() || m_frame->selection().isNone())
        return 0;

    ASSERT(m_frame->document());
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevel(*m_frame->document());
    revealSelectionAfterEditingOperation();
    return newList;
}

PassRefPtr<Node> Editor::increaseSelectionListLevelOrdered()
{
    if (!canEditRichly() || m_frame->selection().isNone())
        return 0;

    ASSERT(m_frame->document());
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelOrdered(*m_frame->document());
    revealSelectionAfterEditingOperation();
    return newList.release();
}

PassRefPtr<Node> Editor::increaseSelectionListLevelUnordered()
{
    if (!canEditRichly() || m_frame->selection().isNone())
        return 0;

    ASSERT(m_frame->document());
    RefPtr<Node> newList = IncreaseSelectionListLevelCommand::increaseSelectionListLevelUnordered(*m_frame->document());
    revealSelectionAfterEditingOperation();
    return newList.release();
}

void Editor::decreaseSelectionListLevel()
{
    if (!canEditRichly() || m_frame->selection().isNone())
        return;

    ASSERT(m_frame->document());
    DecreaseSelectionListLevelCommand::decreaseSelectionListLevel(*m_frame->document());
    revealSelectionAfterEditingOperation();
}

void Editor::removeFormattingAndStyle()
{
    ASSERT(m_frame->document());
    RemoveFormatCommand::create(*m_frame->document())->apply();
}

void Editor::clearLastEditCommand()
{
    m_lastEditCommand.clear();
}

// Returns whether caller should continue with "the default processing", which is the same as
// the event handler NOT setting the return value to false
bool Editor::dispatchCPPEvent(const AtomicString &eventType, ClipboardAccessPolicy policy)
{
    Node* target = findEventTargetFromSelection();
    if (!target)
        return true;

    RefPtr<Clipboard> clipboard = newGeneralClipboard(policy, m_frame);

    RefPtr<Event> evt = ClipboardEvent::create(eventType, true, true, clipboard);
    target->dispatchEvent(evt, IGNORE_EXCEPTION);
    bool noDefaultProcessing = evt->defaultPrevented();
    if (noDefaultProcessing && policy == ClipboardWritable) {
        RefPtr<ChromiumDataObject> dataObject = static_cast<ClipboardChromium*>(clipboard.get())->dataObject();
        Pasteboard::generalPasteboard()->writeDataObject(dataObject.release());
    }

    // invalidate clipboard here for security
    clipboard->setAccessPolicy(ClipboardNumb);

    return !noDefaultProcessing;
}

Node* Editor::findEventTargetFrom(const VisibleSelection& selection) const
{
    Node* target = selection.start().element();
    if (!target)
        target = m_frame->document()->body();
    if (!target)
        return 0;

    return target;
}

Node* Editor::findEventTargetFromSelection() const
{
    return findEventTargetFrom(m_frame->selection().selection());
}

void Editor::applyStyle(StylePropertySet* style, EditAction editingAction)
{
    switch (m_frame->selection().selectionType()) {
    case VisibleSelection::NoSelection:
        // do nothing
        break;
    case VisibleSelection::CaretSelection:
        computeAndSetTypingStyle(style, editingAction);
        break;
    case VisibleSelection::RangeSelection:
        if (style) {
            ASSERT(m_frame->document());
            ApplyStyleCommand::create(*m_frame->document(), EditingStyle::create(style).get(), editingAction)->apply();
        }
        break;
    }
}

bool Editor::shouldApplyStyle(StylePropertySet* style, Range* range)
{
    return client().shouldApplyStyle(style, range);
}

void Editor::applyParagraphStyle(StylePropertySet* style, EditAction editingAction)
{
    switch (m_frame->selection().selectionType()) {
    case VisibleSelection::NoSelection:
        // do nothing
        break;
    case VisibleSelection::CaretSelection:
    case VisibleSelection::RangeSelection:
        if (style) {
            ASSERT(m_frame->document());
            ApplyStyleCommand::create(*m_frame->document(), EditingStyle::create(style).get(), editingAction, ApplyStyleCommand::ForceBlockProperties)->apply();
        }
        break;
    }
}

void Editor::applyStyleToSelection(StylePropertySet* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;

    if (client().shouldApplyStyle(style, m_frame->selection().toNormalizedRange().get()))
        applyStyle(style, editingAction);
}

void Editor::applyParagraphStyleToSelection(StylePropertySet* style, EditAction editingAction)
{
    if (!style || style->isEmpty() || !canEditRichly())
        return;

    if (client().shouldApplyStyle(style, m_frame->selection().toNormalizedRange().get()))
        applyParagraphStyle(style, editingAction);
}

bool Editor::selectionStartHasStyle(CSSPropertyID propertyID, const String& value) const
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(
        EditingStyle::styleAtSelectionStart(m_frame->selection().selection(), propertyID == CSSPropertyBackgroundColor).get());
}

TriState Editor::selectionHasStyle(CSSPropertyID propertyID, const String& value) const
{
    return EditingStyle::create(propertyID, value)->triStateOfStyle(m_frame->selection().selection());
}

String Editor::selectionStartCSSPropertyValue(CSSPropertyID propertyID)
{
    RefPtr<EditingStyle> selectionStyle = EditingStyle::styleAtSelectionStart(m_frame->selection().selection(),
        propertyID == CSSPropertyBackgroundColor);
    if (!selectionStyle || !selectionStyle->style())
        return String();

    if (propertyID == CSSPropertyFontSize)
        return String::number(selectionStyle->legacyFontSize(m_frame->document()));
    return selectionStyle->style()->getPropertyValue(propertyID);
}

void Editor::indent()
{
    ASSERT(m_frame->document());
    IndentOutdentCommand::create(*m_frame->document(), IndentOutdentCommand::Indent)->apply();
}

void Editor::outdent()
{
    ASSERT(m_frame->document());
    IndentOutdentCommand::create(*m_frame->document(), IndentOutdentCommand::Outdent)->apply();
}

static void dispatchEditableContentChangedEvents(PassRefPtr<Element> startRoot, PassRefPtr<Element> endRoot)
{
    if (startRoot)
        startRoot->dispatchEvent(Event::create(eventNames().webkitEditableContentChangedEvent), IGNORE_EXCEPTION);
    if (endRoot && endRoot != startRoot)
        endRoot->dispatchEvent(Event::create(eventNames().webkitEditableContentChangedEvent), IGNORE_EXCEPTION);
}

void Editor::appliedEditing(PassRefPtr<CompositeEditCommand> cmd)
{
    m_frame->document()->updateLayout();

    EditCommandComposition* composition = cmd->composition();
    ASSERT(composition);
    VisibleSelection newSelection(cmd->endingSelection());

    // Don't clear the typing style with this selection change.  We do those things elsewhere if necessary.
    changeSelectionAfterCommand(newSelection, 0);
    dispatchEditableContentChangedEvents(composition->startingRootEditableElement(), composition->endingRootEditableElement());

    if (!cmd->preservesTypingStyle())
        m_frame->selection().clearTypingStyle();

    // Command will be equal to last edit command only in the case of typing
    if (m_lastEditCommand.get() == cmd)
        ASSERT(cmd->isTypingCommand());
    else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        m_lastEditCommand = cmd;
        client().registerUndoStep(m_lastEditCommand->ensureComposition());
    }

    respondToChangedContents(newSelection);
}

void Editor::unappliedEditing(PassRefPtr<EditCommandComposition> cmd)
{
    m_frame->document()->updateLayout();

    VisibleSelection newSelection(cmd->startingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);
    dispatchEditableContentChangedEvents(cmd->startingRootEditableElement(), cmd->endingRootEditableElement());

    m_lastEditCommand = 0;
    client().registerRedoStep(cmd);
    respondToChangedContents(newSelection);
}

void Editor::reappliedEditing(PassRefPtr<EditCommandComposition> cmd)
{
    m_frame->document()->updateLayout();

    VisibleSelection newSelection(cmd->endingSelection());
    changeSelectionAfterCommand(newSelection, FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);
    dispatchEditableContentChangedEvents(cmd->startingRootEditableElement(), cmd->endingRootEditableElement());

    m_lastEditCommand = 0;
    client().registerUndoStep(cmd);
    respondToChangedContents(newSelection);
}

Editor::Editor(Frame& frame)
    : FrameDestructionObserver(&frame)
    , m_preventRevealSelection(0)
    , m_shouldStartNewKillRingSequence(false)
    // This is off by default, since most editors want this behavior (this matches IE but not FF).
    , m_shouldStyleWithCSS(false)
    , m_killRing(adoptPtr(new KillRing))
    , m_spellCheckRequester(adoptPtr(new SpellCheckRequester(frame)))
    , m_areMarkedTextMatchesHighlighted(false)
    , m_defaultParagraphSeparator(EditorParagraphSeparatorIsDiv)
    , m_overwriteModeEnabled(false)
{
}

Editor::~Editor()
{
}

void Editor::clear()
{
    m_frame->inputMethodController().clear();
    m_shouldStyleWithCSS = false;
    m_defaultParagraphSeparator = EditorParagraphSeparatorIsDiv;
}

bool Editor::insertText(const String& text, Event* triggeringEvent)
{
    return m_frame->eventHandler()->handleTextInputEvent(text, triggeringEvent);
}

bool Editor::insertTextWithoutSendingTextEvent(const String& text, bool selectInsertedText, TextEvent* triggeringEvent)
{
    if (text.isEmpty())
        return false;

    VisibleSelection selection = selectionForCommand(triggeringEvent);
    if (!selection.isContentEditable())
        return false;
    RefPtr<Range> range = selection.toNormalizedRange();

    if (!shouldInsertText(text, range.get(), EditorInsertActionTyped))
        return true;

    if (!text.isEmpty())
        updateMarkersForWordsAffectedByEditing(isSpaceOrNewline(text[0]));

    // Get the selection to use for the event that triggered this insertText.
    // If the event handler changed the selection, we may want to use a different selection
    // that is contained in the event target.
    selection = selectionForCommand(triggeringEvent);
    if (selection.isContentEditable()) {
        if (Node* selectionStart = selection.start().deprecatedNode()) {
            RefPtr<Document> document = &selectionStart->document();
            ASSERT(document);

            // Insert the text
            TypingCommand::Options options = 0;
            if (selectInsertedText)
                options |= TypingCommand::SelectInsertedText;
            TypingCommand::insertText(*document.get(), text, selection, options, triggeringEvent && triggeringEvent->isComposition() ? TypingCommand::TextCompositionConfirm : TypingCommand::TextCompositionNone);

            // Reveal the current selection
            if (Frame* editedFrame = document->frame()) {
                if (Page* page = editedFrame->page())
                    page->focusController().focusedOrMainFrame()->selection().revealSelection(ScrollAlignment::alignCenterIfNeeded);
            }
        }
    }

    return true;
}

bool Editor::insertLineBreak()
{
    if (!canEdit())
        return false;

    if (!shouldInsertText("\n", m_frame->selection().toNormalizedRange().get(), EditorInsertActionTyped))
        return true;

    VisiblePosition caret = m_frame->selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    ASSERT(m_frame->document());
    TypingCommand::insertLineBreak(*m_frame->document(), 0);
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

bool Editor::insertParagraphSeparator()
{
    if (!canEdit())
        return false;

    if (!canEditRichly())
        return insertLineBreak();

    if (!shouldInsertText("\n", m_frame->selection().toNormalizedRange().get(), EditorInsertActionTyped))
        return true;

    VisiblePosition caret = m_frame->selection().selection().visibleStart();
    bool alignToEdge = isEndOfEditableOrNonEditableContent(caret);
    ASSERT(m_frame->document());
    TypingCommand::insertParagraphSeparator(*m_frame->document(), 0);
    revealSelectionAfterEditingOperation(alignToEdge ? ScrollAlignment::alignToEdgeIfNeeded : ScrollAlignment::alignCenterIfNeeded);

    return true;
}

void Editor::cut()
{
    if (tryDHTMLCut())
        return; // DHTML did the whole operation
    if (!canCut()) {
        systemBeep();
        return;
    }
    RefPtr<Range> selection = selectedRange();
    if (shouldDeleteRange(selection.get())) {
        updateMarkersForWordsAffectedByEditing(true);
        String plainText = m_frame->selectedTextForClipboard();
        if (enclosingTextFormControl(m_frame->selection().start())) {
            Pasteboard::generalPasteboard()->writePlainText(plainText,
                canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
        } else {
            Pasteboard::generalPasteboard()->writeSelection(selection.get(), canSmartCopyOrDelete(), plainText);
        }
        deleteSelectionWithSmartDelete(canSmartCopyOrDelete());
    }
}

void Editor::copy()
{
    if (tryDHTMLCopy())
        return; // DHTML did the whole operation
    if (!canCopy()) {
        systemBeep();
        return;
    }

    if (enclosingTextFormControl(m_frame->selection().start())) {
        Pasteboard::generalPasteboard()->writePlainText(m_frame->selectedTextForClipboard(),
            canSmartCopyOrDelete() ? Pasteboard::CanSmartReplace : Pasteboard::CannotSmartReplace);
    } else {
        Document* document = m_frame->document();
        if (HTMLImageElement* imageElement = imageElementFromImageDocument(document))
            Pasteboard::generalPasteboard()->writeImage(imageElement, document->url(), document->title());
        else
            Pasteboard::generalPasteboard()->writeSelection(selectedRange().get(), canSmartCopyOrDelete(), m_frame->selectedTextForClipboard());
    }
}

void Editor::paste()
{
    ASSERT(m_frame->document());
    if (tryDHTMLPaste())
        return; // DHTML did the whole operation
    if (!canPaste())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    ResourceFetcher* loader = m_frame->document()->fetcher();
    ResourceCacheValidationSuppressor validationSuppressor(loader);
    if (m_frame->selection().isContentRichlyEditable())
        pasteWithPasteboard(Pasteboard::generalPasteboard(), true);
    else
        pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::pasteAsPlainText()
{
    if (tryDHTMLPaste())
        return;
    if (!canPaste())
        return;
    updateMarkersForWordsAffectedByEditing(false);
    pasteAsPlainTextWithPasteboard(Pasteboard::generalPasteboard());
}

void Editor::performDelete()
{
    if (!canDelete()) {
        systemBeep();
        return;
    }

    addToKillRing(selectedRange().get(), false);
    deleteSelectionWithSmartDelete(canSmartCopyOrDelete());

    // clear the "start new kill ring sequence" setting, because it was set to true
    // when the selection was updated by deleting the range
    setStartNewKillRingSequence(false);
}

void Editor::simplifyMarkup(Node* startNode, Node* endNode)
{
    if (!startNode)
        return;
    if (endNode) {
        if (&startNode->document() != &endNode->document())
            return;
        // check if start node is before endNode
        Node* node = startNode;
        while (node && node != endNode)
            node = NodeTraversal::next(node);
        if (!node)
            return;
    }

    ASSERT(m_frame->document());
    SimplifyMarkupCommand::create(*m_frame->document(), startNode, endNode ? NodeTraversal::next(endNode) : 0)->apply();
}

void Editor::copyURL(const KURL& url, const String& title)
{
    Pasteboard::generalPasteboard()->writeURL(url, title);
}

void Editor::copyImage(const HitTestResult& result)
{
    KURL url = result.absoluteLinkURL();
    if (url.isEmpty())
        url = result.absoluteImageURL();

    Pasteboard::generalPasteboard()->writeImage(result.innerNonSharedNode(), url, result.altDisplayString());
}

bool Editor::isContinuousSpellCheckingEnabled() const
{
    return client().isContinuousSpellCheckingEnabled();
}

void Editor::toggleContinuousSpellChecking()
{
    client().toggleContinuousSpellChecking();
    if (isContinuousSpellCheckingEnabled())
        return;
    for (Frame* frame = m_frame->page()->mainFrame(); frame && frame->document(); frame = frame->tree()->traverseNext()) {
        for (Node* node = frame->document()->rootNode(); node; node = NodeTraversal::next(node)) {
            node->setAlreadySpellChecked(false);
        }
    }
}

bool Editor::isGrammarCheckingEnabled()
{
    return client().isGrammarCheckingEnabled();
}

bool Editor::shouldEndEditing(Range* range)
{
    return client().shouldEndEditing(range);
}

bool Editor::shouldBeginEditing(Range* range)
{
    return client().shouldBeginEditing(range);
}

void Editor::clearUndoRedoOperations()
{
    client().clearUndoRedoOperations();
}

bool Editor::canUndo()
{
    return client().canUndo();
}

void Editor::undo()
{
    client().undo();
}

bool Editor::canRedo()
{
    return client().canRedo();
}

void Editor::redo()
{
    client().redo();
}

void Editor::elementDidBeginEditing(Element* element)
{
    if (isContinuousSpellCheckingEnabled() && unifiedTextCheckerEnabled()) {
        bool isTextField = false;
        HTMLTextFormControlElement* enclosingHTMLTextFormControlElement = 0;
        if (!isHTMLTextFormControlElement(element))
            enclosingHTMLTextFormControlElement = enclosingTextFormControl(firstPositionInNode(element));
        element = enclosingHTMLTextFormControlElement ? enclosingHTMLTextFormControlElement : element;
        Element* parent = element;
        if (isHTMLTextFormControlElement(element)) {
            HTMLTextFormControlElement* textControl = toHTMLTextFormControlElement(element);
            parent = textControl;
            element = textControl->innerTextElement();
            isTextField = textControl->hasTagName(inputTag) && toHTMLInputElement(textControl)->isTextField();
        }

        if (isTextField || !parent->isAlreadySpellChecked()) {
            // We always recheck textfields because markers are removed from them on blur.
            VisibleSelection selection = VisibleSelection::selectionFromContentsOfNode(element);
            markMisspellingsAndBadGrammar(selection);
            if (!isTextField)
                parent->setAlreadySpellChecked(true);
        }
    }
}

void Editor::didBeginEditing(Element* rootEditableElement)
{
    elementDidBeginEditing(rootEditableElement);
    client().didBeginEditing();
}

void Editor::didEndEditing()
{
    client().didEndEditing();
}

void Editor::setBaseWritingDirection(WritingDirection direction)
{
    Node* focusedElement = frame().document()->focusedElement();
    if (focusedElement && isHTMLTextFormControlElement(focusedElement)) {
        if (direction == NaturalWritingDirection)
            return;
        toHTMLElement(focusedElement)->setAttribute(dirAttr, direction == LeftToRightWritingDirection ? "ltr" : "rtl");
        focusedElement->dispatchInputEvent();
        frame().document()->updateStyleIfNeeded();
        return;
    }

    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create();
    style->setProperty(CSSPropertyDirection, direction == LeftToRightWritingDirection ? "ltr" : direction == RightToLeftWritingDirection ? "rtl" : "inherit", false);
    applyParagraphStyleToSelection(style.get(), EditActionSetWritingDirection);
}

WritingDirection Editor::baseWritingDirectionForSelectionStart() const
{
    WritingDirection result = LeftToRightWritingDirection;

    Position pos = m_frame->selection().selection().visibleStart().deepEquivalent();
    Node* node = pos.deprecatedNode();
    if (!node)
        return result;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return result;

    if (!renderer->isRenderBlockFlow()) {
        renderer = renderer->containingBlock();
        if (!renderer)
            return result;
    }

    RenderStyle* style = renderer->style();
    if (!style)
        return result;

    switch (style->direction()) {
    case LTR:
        return LeftToRightWritingDirection;
    case RTL:
        return RightToLeftWritingDirection;
    }

    return result;
}

void Editor::ignoreSpelling()
{
    if (RefPtr<Range> selectedRange = frame().selection().toNormalizedRange())
        frame().document()->markers()->removeMarkers(selectedRange.get(), DocumentMarker::Spelling);
}

void Editor::advanceToNextMisspelling(bool startBeforeSelection)
{
    // The basic approach is to search in two phases - from the selection end to the end of the doc, and
    // then we wrap and search from the doc start to (approximately) where we started.

    // Start at the end of the selection, search to edge of document.  Starting at the selection end makes
    // repeated "check spelling" commands work.
    VisibleSelection selection(frame().selection().selection());
    RefPtr<Range> spellingSearchRange(rangeOfContents(frame().document()));

    bool startedWithSelection = false;
    if (selection.start().deprecatedNode()) {
        startedWithSelection = true;
        if (startBeforeSelection) {
            VisiblePosition start(selection.visibleStart());
            // We match AppKit's rule: Start 1 character before the selection.
            VisiblePosition oneBeforeStart = start.previous();
            setStart(spellingSearchRange.get(), oneBeforeStart.isNotNull() ? oneBeforeStart : start);
        } else
            setStart(spellingSearchRange.get(), selection.visibleEnd());
    }

    Position position = spellingSearchRange->startPosition();
    if (!isEditablePosition(position)) {
        // This shouldn't happen in very often because the Spelling menu items aren't enabled unless the
        // selection is editable.
        // This can happen in Mail for a mix of non-editable and editable content (like Stationary),
        // when spell checking the whole document before sending the message.
        // In that case the document might not be editable, but there are editable pockets that need to be spell checked.

        position = firstEditablePositionAfterPositionInRoot(position, frame().document()->documentElement()).deepEquivalent();
        if (position.isNull())
            return;

        Position rangeCompliantPosition = position.parentAnchoredEquivalent();
        spellingSearchRange->setStart(rangeCompliantPosition.deprecatedNode(), rangeCompliantPosition.deprecatedEditingOffset(), IGNORE_EXCEPTION);
        startedWithSelection = false; // won't need to wrap
    }

    // topNode defines the whole range we want to operate on
    Node* topNode = highestEditableRoot(position);
    // FIXME: lastOffsetForEditing() is wrong here if editingIgnoresContent(highestEditableRoot()) returns true (e.g. a <table>)
    spellingSearchRange->setEnd(topNode, lastOffsetForEditing(topNode), IGNORE_EXCEPTION);

    // If spellingSearchRange starts in the middle of a word, advance to the next word so we start checking
    // at a word boundary. Going back by one char and then forward by a word does the trick.
    if (startedWithSelection) {
        VisiblePosition oneBeforeStart = startVisiblePosition(spellingSearchRange.get(), DOWNSTREAM).previous();
        if (oneBeforeStart.isNotNull())
            setStart(spellingSearchRange.get(), endOfWord(oneBeforeStart));
        // else we were already at the start of the editable node
    }

    if (spellingSearchRange->collapsed(IGNORE_EXCEPTION))
        return; // nothing to search in

    // We go to the end of our first range instead of the start of it, just to be sure
    // we don't get foiled by any word boundary problems at the start.  It means we might
    // do a tiny bit more searching.
    Node* searchEndNodeAfterWrap = spellingSearchRange->endContainer();
    int searchEndOffsetAfterWrap = spellingSearchRange->endOffset();

    int misspellingOffset = 0;
    GrammarDetail grammarDetail;
    int grammarPhraseOffset = 0;
    RefPtr<Range> grammarSearchRange;
    String badGrammarPhrase;
    String misspelledWord;

    bool isSpelling = true;
    int foundOffset = 0;
    String foundItem;
    RefPtr<Range> firstMisspellingRange;
    if (unifiedTextCheckerEnabled()) {
        grammarSearchRange = spellingSearchRange->cloneRange(IGNORE_EXCEPTION);
        foundItem = TextCheckingHelper(client(), spellingSearchRange).findFirstMisspellingOrBadGrammar(isGrammarCheckingEnabled(), isSpelling, foundOffset, grammarDetail);
        if (isSpelling) {
            misspelledWord = foundItem;
            misspellingOffset = foundOffset;
        } else {
            badGrammarPhrase = foundItem;
            grammarPhraseOffset = foundOffset;
        }
    } else {
        misspelledWord = TextCheckingHelper(client(), spellingSearchRange).findFirstMisspelling(misspellingOffset, false, firstMisspellingRange);
        grammarSearchRange = spellingSearchRange->cloneRange(IGNORE_EXCEPTION);
        if (!misspelledWord.isEmpty()) {
            // Stop looking at start of next misspelled word
            CharacterIterator chars(grammarSearchRange.get());
            chars.advance(misspellingOffset);
            grammarSearchRange->setEnd(chars.range()->startContainer(), chars.range()->startOffset(), IGNORE_EXCEPTION);
        }

        if (isGrammarCheckingEnabled())
            badGrammarPhrase = TextCheckingHelper(client(), grammarSearchRange).findFirstBadGrammar(grammarDetail, grammarPhraseOffset, false);
    }

    // If we found neither bad grammar nor a misspelled word, wrap and try again (but don't bother if we started at the beginning of the
    // block rather than at a selection).
    if (startedWithSelection && !misspelledWord && !badGrammarPhrase) {
        spellingSearchRange->setStart(topNode, 0, IGNORE_EXCEPTION);
        // going until the end of the very first chunk we tested is far enough
        spellingSearchRange->setEnd(searchEndNodeAfterWrap, searchEndOffsetAfterWrap, IGNORE_EXCEPTION);

        if (unifiedTextCheckerEnabled()) {
            grammarSearchRange = spellingSearchRange->cloneRange(IGNORE_EXCEPTION);
            foundItem = TextCheckingHelper(client(), spellingSearchRange).findFirstMisspellingOrBadGrammar(isGrammarCheckingEnabled(), isSpelling, foundOffset, grammarDetail);
            if (isSpelling) {
                misspelledWord = foundItem;
                misspellingOffset = foundOffset;
            } else {
                badGrammarPhrase = foundItem;
                grammarPhraseOffset = foundOffset;
            }
        } else {
            misspelledWord = TextCheckingHelper(client(), spellingSearchRange).findFirstMisspelling(misspellingOffset, false, firstMisspellingRange);
            grammarSearchRange = spellingSearchRange->cloneRange(IGNORE_EXCEPTION);
            if (!misspelledWord.isEmpty()) {
                // Stop looking at start of next misspelled word
                CharacterIterator chars(grammarSearchRange.get());
                chars.advance(misspellingOffset);
                grammarSearchRange->setEnd(chars.range()->startContainer(), chars.range()->startOffset(), IGNORE_EXCEPTION);
            }

            if (isGrammarCheckingEnabled())
                badGrammarPhrase = TextCheckingHelper(client(), grammarSearchRange).findFirstBadGrammar(grammarDetail, grammarPhraseOffset, false);
        }
    }

    if (!badGrammarPhrase.isEmpty()) {
        // We found bad grammar. Since we only searched for bad grammar up to the first misspelled word, the bad grammar
        // takes precedence and we ignore any potential misspelled word. Select the grammar detail, update the spelling
        // panel, and store a marker so we draw the green squiggle later.

        ASSERT(badGrammarPhrase.length() > 0);
        ASSERT(grammarDetail.location != -1 && grammarDetail.length > 0);

        // FIXME 4859190: This gets confused with doubled punctuation at the end of a paragraph
        RefPtr<Range> badGrammarRange = TextIterator::subrange(grammarSearchRange.get(), grammarPhraseOffset + grammarDetail.location, grammarDetail.length);
        frame().selection().setSelection(VisibleSelection(badGrammarRange.get(), SEL_DEFAULT_AFFINITY));
        frame().selection().revealSelection();

        frame().document()->markers()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, grammarDetail.userDescription);
    } else if (!misspelledWord.isEmpty()) {
        // We found a misspelling, but not any earlier bad grammar. Select the misspelling, update the spelling panel, and store
        // a marker so we draw the red squiggle later.

        RefPtr<Range> misspellingRange = TextIterator::subrange(spellingSearchRange.get(), misspellingOffset, misspelledWord.length());
        frame().selection().setSelection(VisibleSelection(misspellingRange.get(), DOWNSTREAM));
        frame().selection().revealSelection();

        client().updateSpellingUIWithMisspelledWord(misspelledWord);
        frame().document()->markers()->addMarker(misspellingRange.get(), DocumentMarker::Spelling);
    }
}

String Editor::misspelledWordAtCaretOrRange(Node* clickedNode) const
{
    if (!isContinuousSpellCheckingEnabled() || !clickedNode || !isSpellCheckingEnabledFor(clickedNode))
        return String();

    VisibleSelection selection = m_frame->selection().selection();
    if (!selection.isContentEditable() || selection.isNone())
        return String();

    VisibleSelection wordSelection(selection.base());
    wordSelection.expandUsingGranularity(WordGranularity);
    RefPtr<Range> wordRange = wordSelection.toNormalizedRange();

    // In compliance with GTK+ applications, additionally allow to provide suggestions when the current
    // selection exactly match the word selection.
    if (selection.isRange() && !areRangesEqual(wordRange.get(), selection.toNormalizedRange().get()))
        return String();

    String word = wordRange->text();
    if (word.isEmpty())
        return String();

    int wordLength = word.length();
    int misspellingLocation = -1;
    int misspellingLength = 0;
    textChecker().checkSpellingOfString(word, &misspellingLocation, &misspellingLength);

    return misspellingLength == wordLength ? word : String();
}

void Editor::showSpellingGuessPanel()
{
    if (client().spellingUIIsShowing()) {
        client().showSpellingUI(false);
        return;
    }

    advanceToNextMisspelling(true);
    client().showSpellingUI(true);
}

void Editor::clearMisspellingsAndBadGrammar(const VisibleSelection &movingSelection)
{
    RefPtr<Range> selectedRange = movingSelection.toNormalizedRange();
    if (selectedRange)
        frame().document()->markers()->removeMarkers(selectedRange.get(), DocumentMarker::MisspellingMarkers());
}

void Editor::markMisspellingsAndBadGrammar(const VisibleSelection &movingSelection)
{
    markMisspellingsAndBadGrammar(movingSelection, isContinuousSpellCheckingEnabled() && isGrammarCheckingEnabled(), movingSelection);
}

void Editor::markMisspellingsAfterTypingToWord(const VisiblePosition &wordStart, const VisibleSelection& selectionAfterTyping)
{
    if (unifiedTextCheckerEnabled()) {
        TextCheckingTypeMask textCheckingOptions = 0;

        if (isContinuousSpellCheckingEnabled())
            textCheckingOptions |= TextCheckingTypeSpelling;

        if (!(textCheckingOptions & TextCheckingTypeSpelling))
            return;

        if (isGrammarCheckingEnabled())
            textCheckingOptions |= TextCheckingTypeGrammar;

        VisibleSelection adjacentWords = VisibleSelection(startOfWord(wordStart, LeftWordIfOnBoundary), endOfWord(wordStart, RightWordIfOnBoundary));
        if (textCheckingOptions & TextCheckingTypeGrammar) {
            VisibleSelection selectedSentence = VisibleSelection(startOfSentence(wordStart), endOfSentence(wordStart));
            markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWords.toNormalizedRange().get(), selectedSentence.toNormalizedRange().get());
        } else
            markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, adjacentWords.toNormalizedRange().get(), adjacentWords.toNormalizedRange().get());
        return;
    }

    if (!isContinuousSpellCheckingEnabled())
        return;

    // Check spelling of one word
    RefPtr<Range> misspellingRange;
    markMisspellings(VisibleSelection(startOfWord(wordStart, LeftWordIfOnBoundary), endOfWord(wordStart, RightWordIfOnBoundary)), misspellingRange);

    // Autocorrect the misspelled word.
    if (!misspellingRange)
        return;

    // Get the misspelled word.
    const String misspelledWord = plainText(misspellingRange.get());
    String autocorrectedString = textChecker().getAutoCorrectSuggestionForMisspelledWord(misspelledWord);

    // If autocorrected word is non empty, replace the misspelled word by this word.
    if (!autocorrectedString.isEmpty()) {
        VisibleSelection newSelection(misspellingRange.get(), DOWNSTREAM);
        if (newSelection != frame().selection().selection()) {
            if (!frame().selection().shouldChangeSelection(newSelection))
                return;
            frame().selection().setSelection(newSelection);
        }

        if (!frame().editor().shouldInsertText(autocorrectedString, misspellingRange.get(), EditorInsertActionTyped))
            return;
        frame().editor().replaceSelectionWithText(autocorrectedString, false, false);

        // Reset the charet one character further.
        frame().selection().moveTo(frame().selection().end());
        frame().selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity);
    }

    if (!isGrammarCheckingEnabled())
        return;

    // Check grammar of entire sentence
    markBadGrammar(VisibleSelection(startOfSentence(wordStart), endOfSentence(wordStart)));
}

void Editor::markMisspellingsOrBadGrammar(const VisibleSelection& selection, bool checkSpelling, RefPtr<Range>& firstMisspellingRange)
{
    // This function is called with a selection already expanded to word boundaries.
    // Might be nice to assert that here.

    // This function is used only for as-you-type checking, so if that's off we do nothing. Note that
    // grammar checking can only be on if spell checking is also on.
    if (!isContinuousSpellCheckingEnabled())
        return;

    RefPtr<Range> searchRange(selection.toNormalizedRange());
    if (!searchRange)
        return;

    // If we're not in an editable node, bail.
    Node* editableNode = searchRange->startContainer();
    if (!editableNode || !editableNode->rendererIsEditable())
        return;

    if (!isSpellCheckingEnabledFor(editableNode))
        return;

    TextCheckingHelper checker(client(), searchRange);
    if (checkSpelling)
        checker.markAllMisspellings(firstMisspellingRange);
    else if (isGrammarCheckingEnabled())
        checker.markAllBadGrammar();
}

bool Editor::isSpellCheckingEnabledFor(Node* node) const
{
    if (!node)
        return false;
    const Element* focusedElement = node->isElementNode() ? toElement(node) : node->parentElement();
    if (!focusedElement)
        return false;
    return focusedElement->isSpellCheckingEnabled();
}

bool Editor::isSpellCheckingEnabledInFocusedNode() const
{
    return isSpellCheckingEnabledFor(m_frame->selection().start().deprecatedNode());
}

void Editor::markMisspellings(const VisibleSelection& selection, RefPtr<Range>& firstMisspellingRange)
{
    markMisspellingsOrBadGrammar(selection, true, firstMisspellingRange);
}

void Editor::markBadGrammar(const VisibleSelection& selection)
{
    RefPtr<Range> firstMisspellingRange;
    markMisspellingsOrBadGrammar(selection, false, firstMisspellingRange);
}

void Editor::markAllMisspellingsAndBadGrammarInRanges(TextCheckingTypeMask textCheckingOptions, Range* spellingRange, Range* grammarRange)
{
    ASSERT(m_frame);
    ASSERT(unifiedTextCheckerEnabled());

    bool shouldMarkGrammar = textCheckingOptions & TextCheckingTypeGrammar;

    // This function is called with selections already expanded to word boundaries.
    if (!spellingRange || (shouldMarkGrammar && !grammarRange))
        return;

    // If we're not in an editable node, bail.
    Node* editableNode = spellingRange->startContainer();
    if (!editableNode || !editableNode->rendererIsEditable())
        return;

    if (!isSpellCheckingEnabledFor(editableNode))
        return;

    Range* rangeToCheck = shouldMarkGrammar ? grammarRange : spellingRange;
    TextCheckingParagraph fullParagraphToCheck(rangeToCheck);

    bool asynchronous = m_frame && m_frame->settings() && m_frame->settings()->asynchronousSpellCheckingEnabled();
    chunkAndMarkAllMisspellingsAndBadGrammar(textCheckingOptions, fullParagraphToCheck, asynchronous);
}

void Editor::chunkAndMarkAllMisspellingsAndBadGrammar(TextCheckingTypeMask textCheckingOptions, const TextCheckingParagraph& fullParagraphToCheck, bool asynchronous)
{
    if (fullParagraphToCheck.isRangeEmpty() || fullParagraphToCheck.isEmpty())
        return;

    // Since the text may be quite big chunk it up and adjust to the sentence boundary.
    const int kChunkSize = 16 * 1024;
    int start = fullParagraphToCheck.checkingStart();
    int end = fullParagraphToCheck.checkingEnd();
    start = std::min(start, end);
    end = std::max(start, end);
    const int kNumChunksToCheck = asynchronous ? (end - start + kChunkSize - 1) / (kChunkSize) : 1;
    int currentChunkStart = start;
    RefPtr<Range> checkRange = fullParagraphToCheck.checkingRange();
    if (kNumChunksToCheck == 1 && asynchronous) {
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, checkRange.get(), checkRange.get(), asynchronous, 0);
        return;
    }

    for (int iter = 0; iter < kNumChunksToCheck; ++iter) {
        checkRange = fullParagraphToCheck.subrange(currentChunkStart, kChunkSize);
        setStart(checkRange.get(), startOfSentence(checkRange->startPosition()));
        setEnd(checkRange.get(), endOfSentence(checkRange->endPosition()));

        int checkingLength = 0;
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, checkRange.get(), checkRange.get(), asynchronous, iter, &checkingLength);
        currentChunkStart += checkingLength;
    }
}

void Editor::markAllMisspellingsAndBadGrammarInRanges(TextCheckingTypeMask textCheckingOptions, Range* checkRange, Range* paragraphRange, bool asynchronous, int requestNumber, int* checkingLength)
{
    TextCheckingParagraph sentenceToCheck(checkRange, paragraphRange);
    if (checkingLength)
        *checkingLength = sentenceToCheck.checkingLength();

    RefPtr<SpellCheckRequest> request = SpellCheckRequest::create(resolveTextCheckingTypeMask(textCheckingOptions), TextCheckingProcessBatch, checkRange, paragraphRange, requestNumber);

    if (asynchronous) {
        m_spellCheckRequester->requestCheckingFor(request);
    } else {
        Vector<TextCheckingResult> results;
        checkTextOfParagraph(textChecker(), sentenceToCheck.text(), resolveTextCheckingTypeMask(textCheckingOptions), results);
        markAndReplaceFor(request, results);
    }
}

void Editor::markAndReplaceFor(PassRefPtr<SpellCheckRequest> request, const Vector<TextCheckingResult>& results)
{
    ASSERT(request);

    TextCheckingTypeMask textCheckingOptions = request->data().mask();
    TextCheckingParagraph paragraph(request->checkingRange(), request->paragraphRange());

    bool shouldMarkSpelling = textCheckingOptions & TextCheckingTypeSpelling;
    bool shouldMarkGrammar = textCheckingOptions & TextCheckingTypeGrammar;

    // Expand the range to encompass entire paragraphs, since text checking needs that much context.
    int selectionOffset = 0;
    int ambiguousBoundaryOffset = -1;
    bool selectionChanged = false;
    bool restoreSelectionAfterChange = false;
    bool adjustSelectionForParagraphBoundaries = false;

    if (shouldMarkSpelling) {
        if (m_frame->selection().selectionType() == VisibleSelection::CaretSelection) {
            // Attempt to save the caret position so we can restore it later if needed
            Position caretPosition = m_frame->selection().end();
            selectionOffset = paragraph.offsetTo(caretPosition, ASSERT_NO_EXCEPTION);
            restoreSelectionAfterChange = true;
            if (selectionOffset > 0 && (static_cast<unsigned>(selectionOffset) > paragraph.text().length() || paragraph.textCharAt(selectionOffset - 1) == newlineCharacter))
                adjustSelectionForParagraphBoundaries = true;
            if (selectionOffset > 0 && static_cast<unsigned>(selectionOffset) <= paragraph.text().length() && isAmbiguousBoundaryCharacter(paragraph.textCharAt(selectionOffset - 1)))
                ambiguousBoundaryOffset = selectionOffset - 1;
        }
    }

    for (unsigned i = 0; i < results.size(); i++) {
        int spellingRangeEndOffset = paragraph.checkingEnd();
        const TextCheckingResult* result = &results[i];
        int resultLocation = result->location + paragraph.checkingStart();
        int resultLength = result->length;
        bool resultEndsAtAmbiguousBoundary = ambiguousBoundaryOffset >= 0 && resultLocation + resultLength == ambiguousBoundaryOffset;

        // Only mark misspelling if:
        // 1. Current text checking isn't done for autocorrection, in which case shouldMarkSpelling is false.
        // 2. Result falls within spellingRange.
        // 3. The word in question doesn't end at an ambiguous boundary. For instance, we would not mark
        //    "wouldn'" as misspelled right after apostrophe is typed.
        if (shouldMarkSpelling && result->type == TextCheckingTypeSpelling && resultLocation >= paragraph.checkingStart() && resultLocation + resultLength <= spellingRangeEndOffset && !resultEndsAtAmbiguousBoundary) {
            ASSERT(resultLength > 0 && resultLocation >= 0);
            RefPtr<Range> misspellingRange = paragraph.subrange(resultLocation, resultLength);
            misspellingRange->startContainer()->document().markers()->addMarker(misspellingRange.get(), DocumentMarker::Spelling, result->replacement, result->hash);
        } else if (shouldMarkGrammar && result->type == TextCheckingTypeGrammar && paragraph.checkingRangeCovers(resultLocation, resultLength)) {
            ASSERT(resultLength > 0 && resultLocation >= 0);
            for (unsigned j = 0; j < result->details.size(); j++) {
                const GrammarDetail* detail = &result->details[j];
                ASSERT(detail->length > 0 && detail->location >= 0);
                if (paragraph.checkingRangeCovers(resultLocation + detail->location, detail->length)) {
                    RefPtr<Range> badGrammarRange = paragraph.subrange(resultLocation + detail->location, detail->length);
                    badGrammarRange->startContainer()->document().markers()->addMarker(badGrammarRange.get(), DocumentMarker::Grammar, detail->userDescription, result->hash);
                }
            }
        }
    }

    if (selectionChanged) {
        TextCheckingParagraph extendedParagraph(paragraph);
        // Restore the caret position if we have made any replacements
        extendedParagraph.expandRangeToNextEnd();
        if (restoreSelectionAfterChange && selectionOffset >= 0 && selectionOffset <= extendedParagraph.rangeLength()) {
            RefPtr<Range> selectionRange = extendedParagraph.subrange(0, selectionOffset);
            m_frame->selection().moveTo(selectionRange->endPosition(), DOWNSTREAM);
            if (adjustSelectionForParagraphBoundaries)
                m_frame->selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity);
        } else {
            // If this fails for any reason, the fallback is to go one position beyond the last replacement
            m_frame->selection().moveTo(m_frame->selection().end());
            m_frame->selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity);
        }
    }
}

void Editor::markMisspellingsAndBadGrammar(const VisibleSelection& spellingSelection, bool markGrammar, const VisibleSelection& grammarSelection)
{
    if (unifiedTextCheckerEnabled()) {
        if (!isContinuousSpellCheckingEnabled())
            return;

        // markMisspellingsAndBadGrammar() is triggered by selection change, in which case we check spelling and grammar, but don't autocorrect misspellings.
        TextCheckingTypeMask textCheckingOptions = TextCheckingTypeSpelling;
        if (markGrammar && isGrammarCheckingEnabled())
            textCheckingOptions |= TextCheckingTypeGrammar;
        markAllMisspellingsAndBadGrammarInRanges(textCheckingOptions, spellingSelection.toNormalizedRange().get(), grammarSelection.toNormalizedRange().get());
        return;
    }

    RefPtr<Range> firstMisspellingRange;
    markMisspellings(spellingSelection, firstMisspellingRange);
    if (markGrammar)
        markBadGrammar(grammarSelection);
}

void Editor::updateMarkersForWordsAffectedByEditing(bool doNotRemoveIfSelectionAtWordBoundary)
{
    if (textChecker().shouldEraseMarkersAfterChangeSelection(TextCheckingTypeSpelling))
        return;

    // We want to remove the markers from a word if an editing command will change the word. This can happen in one of
    // several scenarios:
    // 1. Insert in the middle of a word.
    // 2. Appending non whitespace at the beginning of word.
    // 3. Appending non whitespace at the end of word.
    // Note that, appending only whitespaces at the beginning or end of word won't change the word, so we don't need to
    // remove the markers on that word.
    // Of course, if current selection is a range, we potentially will edit two words that fall on the boundaries of
    // selection, and remove words between the selection boundaries.
    //
    VisiblePosition startOfSelection = frame().selection().selection().start();
    VisiblePosition endOfSelection = frame().selection().selection().end();
    if (startOfSelection.isNull())
        return;
    // First word is the word that ends after or on the start of selection.
    VisiblePosition startOfFirstWord = startOfWord(startOfSelection, LeftWordIfOnBoundary);
    VisiblePosition endOfFirstWord = endOfWord(startOfSelection, LeftWordIfOnBoundary);
    // Last word is the word that begins before or on the end of selection
    VisiblePosition startOfLastWord = startOfWord(endOfSelection, RightWordIfOnBoundary);
    VisiblePosition endOfLastWord = endOfWord(endOfSelection, RightWordIfOnBoundary);

    if (startOfFirstWord.isNull()) {
        startOfFirstWord = startOfWord(startOfSelection, RightWordIfOnBoundary);
        endOfFirstWord = endOfWord(startOfSelection, RightWordIfOnBoundary);
    }

    if (endOfLastWord.isNull()) {
        startOfLastWord = startOfWord(endOfSelection, LeftWordIfOnBoundary);
        endOfLastWord = endOfWord(endOfSelection, LeftWordIfOnBoundary);
    }

    // If doNotRemoveIfSelectionAtWordBoundary is true, and first word ends at the start of selection,
    // we choose next word as the first word.
    if (doNotRemoveIfSelectionAtWordBoundary && endOfFirstWord == startOfSelection) {
        startOfFirstWord = nextWordPosition(startOfFirstWord);
        endOfFirstWord = endOfWord(startOfFirstWord, RightWordIfOnBoundary);
        if (startOfFirstWord == endOfSelection)
            return;
    }

    // If doNotRemoveIfSelectionAtWordBoundary is true, and last word begins at the end of selection,
    // we choose previous word as the last word.
    if (doNotRemoveIfSelectionAtWordBoundary && startOfLastWord == endOfSelection) {
        startOfLastWord = previousWordPosition(startOfLastWord);
        endOfLastWord = endOfWord(startOfLastWord, RightWordIfOnBoundary);
        if (endOfLastWord == startOfSelection)
            return;
    }

    if (startOfFirstWord.isNull() || endOfFirstWord.isNull() || startOfLastWord.isNull() || endOfLastWord.isNull())
        return;

    // Now we remove markers on everything between startOfFirstWord and endOfLastWord.
    // However, if an autocorrection change a single word to multiple words, we want to remove correction mark from all the
    // resulted words even we only edit one of them. For example, assuming autocorrection changes "avantgarde" to "avant
    // garde", we will have CorrectionIndicator marker on both words and on the whitespace between them. If we then edit garde,
    // we would like to remove the marker from word "avant" and whitespace as well. So we need to get the continous range of
    // of marker that contains the word in question, and remove marker on that whole range.
    Document* document = m_frame->document();
    ASSERT(document);
    RefPtr<Range> wordRange = Range::create(*document, startOfFirstWord.deepEquivalent(), endOfLastWord.deepEquivalent());

    document->markers()->removeMarkers(wordRange.get(), DocumentMarker::MisspellingMarkers(), DocumentMarkerController::RemovePartiallyOverlappingMarker);
}

PassRefPtr<Range> Editor::rangeForPoint(const IntPoint& windowPoint)
{
    Document* document = m_frame->documentAtPoint(windowPoint);
    if (!document)
        return 0;

    Frame* frame = document->frame();
    ASSERT(frame);
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    IntPoint framePoint = frameView->windowToContents(windowPoint);
    VisibleSelection selection(frame->visiblePositionForPoint(framePoint));

    return selection.toNormalizedRange().get();
}

void Editor::revealSelectionAfterEditingOperation(const ScrollAlignment& alignment, RevealExtentOption revealExtentOption)
{
    if (m_preventRevealSelection)
        return;

    m_frame->selection().revealSelection(alignment, revealExtentOption);
}

bool Editor::setSelectionOffsets(int selectionStart, int selectionEnd)
{
    Element* rootEditableElement = m_frame->selection().rootEditableElement();
    if (!rootEditableElement)
        return false;

    RefPtr<Range> range = TextIterator::rangeFromLocationAndLength(rootEditableElement, selectionStart, selectionEnd - selectionStart);
    if (!range)
        return false;

    return m_frame->selection().setSelectedRange(range.get(), VP_DEFAULT_AFFINITY, true);
}

void Editor::transpose()
{
    if (!canEdit())
        return;

    VisibleSelection selection = m_frame->selection().selection();
    if (!selection.isCaret())
        return;

    // Make a selection that goes back one character and forward two characters.
    VisiblePosition caret = selection.visibleStart();
    VisiblePosition next = isEndOfParagraph(caret) ? caret : caret.next();
    VisiblePosition previous = next.previous();
    if (next == previous)
        return;
    previous = previous.previous();
    if (!inSameParagraph(next, previous))
        return;
    RefPtr<Range> range = makeRange(previous, next);
    if (!range)
        return;
    VisibleSelection newSelection(range.get(), DOWNSTREAM);

    // Transpose the two characters.
    String text = plainText(range.get());
    if (text.length() != 2)
        return;
    String transposed = text.right(1) + text.left(1);

    // Select the two characters.
    if (newSelection != m_frame->selection().selection()) {
        if (!m_frame->selection().shouldChangeSelection(newSelection))
            return;
        m_frame->selection().setSelection(newSelection);
    }

    // Insert the transposed characters.
    if (!shouldInsertText(transposed, range.get(), EditorInsertActionTyped))
        return;
    replaceSelectionWithText(transposed, false, false);
}

void Editor::addToKillRing(Range* range, bool prepend)
{
    if (m_shouldStartNewKillRingSequence)
        killRing()->startNewSequence();

    String text = plainText(range);
    if (prepend)
        killRing()->prepend(text);
    else
        killRing()->append(text);
    m_shouldStartNewKillRingSequence = false;
}

void Editor::changeSelectionAfterCommand(const VisibleSelection& newSelection,  FrameSelection::SetSelectionOptions options)
{
    // If the new selection is orphaned, then don't update the selection.
    if (newSelection.start().isOrphan() || newSelection.end().isOrphan())
        return;

    // If there is no selection change, don't bother sending shouldChangeSelection, but still call setSelection,
    // because there is work that it must do in this situation.
    // The old selection can be invalid here and calling shouldChangeSelection can produce some strange calls.
    // See <rdar://problem/5729315> Some shouldChangeSelectedDOMRange contain Ranges for selections that are no longer valid
    bool selectionDidNotChangeDOMPosition = newSelection == m_frame->selection().selection();
    if (selectionDidNotChangeDOMPosition || m_frame->selection().shouldChangeSelection(newSelection))
        m_frame->selection().setSelection(newSelection, options);

    // Some editing operations change the selection visually without affecting its position within the DOM.
    // For example when you press return in the following (the caret is marked by ^):
    // <div contentEditable="true"><div>^Hello</div></div>
    // WebCore inserts <div><br></div> *before* the current block, which correctly moves the paragraph down but which doesn't
    // change the caret's DOM position (["hello", 0]). In these situations the above FrameSelection::setSelection call
    // does not call EditorClient::respondToChangedSelection(), which, on the Mac, sends selection change notifications and
    // starts a new kill ring sequence, but we want to do these things (matches AppKit).
    if (selectionDidNotChangeDOMPosition)
        client().respondToChangedSelection(m_frame);
}

IntRect Editor::firstRectForRange(Range* range) const
{
    LayoutUnit extraWidthToEndOfLine = 0;
    ASSERT(range->startContainer());
    ASSERT(range->endContainer());

    IntRect startCaretRect = RenderedPosition(VisiblePosition(range->startPosition()).deepEquivalent(), DOWNSTREAM).absoluteRect(&extraWidthToEndOfLine);
    if (startCaretRect == LayoutRect())
        return IntRect();

    IntRect endCaretRect = RenderedPosition(VisiblePosition(range->endPosition()).deepEquivalent(), UPSTREAM).absoluteRect();
    if (endCaretRect == LayoutRect())
        return IntRect();

    if (startCaretRect.y() == endCaretRect.y()) {
        // start and end are on the same line
        return IntRect(min(startCaretRect.x(), endCaretRect.x()),
            startCaretRect.y(),
            abs(endCaretRect.x() - startCaretRect.x()),
            max(startCaretRect.height(), endCaretRect.height()));
    }

    // start and end aren't on the same line, so go from start to the end of its line
    return IntRect(startCaretRect.x(),
        startCaretRect.y(),
        startCaretRect.width() + extraWidthToEndOfLine,
        startCaretRect.height());
}

bool Editor::shouldChangeSelection(const VisibleSelection& oldSelection, const VisibleSelection& newSelection, EAffinity affinity, bool stillSelecting) const
{
    return client().shouldChangeSelectedRange(oldSelection.toNormalizedRange().get(), newSelection.toNormalizedRange().get(), affinity, stillSelecting);
}

void Editor::computeAndSetTypingStyle(StylePropertySet* style, EditAction editingAction)
{
    if (!style || style->isEmpty()) {
        m_frame->selection().clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    RefPtr<EditingStyle> typingStyle;
    if (m_frame->selection().typingStyle()) {
        typingStyle = m_frame->selection().typingStyle()->copy();
        typingStyle->overrideWithStyle(style);
    } else
        typingStyle = EditingStyle::create(style);

    typingStyle->prepareToApplyAt(m_frame->selection().selection().visibleStart().deepEquivalent(), EditingStyle::PreserveWritingDirection);

    // Handle block styles, substracting these from the typing style.
    RefPtr<EditingStyle> blockStyle = typingStyle->extractAndRemoveBlockProperties();
    if (!blockStyle->isEmpty()) {
        ASSERT(m_frame->document());
        ApplyStyleCommand::create(*m_frame->document(), blockStyle.get(), editingAction)->apply();
    }

    // Set the remaining style as the typing style.
    m_frame->selection().setTypingStyle(typingStyle);
}

void Editor::textAreaOrTextFieldDidBeginEditing(Element* e)
{
    elementDidBeginEditing(e);
}

void Editor::textFieldDidEndEditing(Element* e)
{
    // Remove markers when deactivating a selection in an <input type="text"/>.
    // Prevent new ones from appearing too.
    m_spellCheckRequester->cancelCheck();
    HTMLTextFormControlElement* textFormControlElement = toHTMLTextFormControlElement(e);
    HTMLElement* innerText = textFormControlElement->innerTextElement();
    DocumentMarker::MarkerTypes markerTypes(DocumentMarker::Spelling);
    if (isGrammarCheckingEnabled() || unifiedTextCheckerEnabled())
        markerTypes.add(DocumentMarker::Grammar);
    for (Node* node = innerText; node; node = NodeTraversal::next(node, innerText)) {
        m_frame->document()->markers()->removeMarkers(node, markerTypes);
    }

    client().textFieldDidEndEditing(e);
}

void Editor::textDidChangeInTextField(Element* e)
{
    client().textDidChangeInTextField(e);
}

bool Editor::doTextFieldCommandFromEvent(Element* e, KeyboardEvent* ke)
{
    return client().doTextFieldCommandFromEvent(e, ke);
}

bool Editor::findString(const String& target, bool forward, bool caseFlag, bool wrapFlag, bool startInSelection)
{
    FindOptions options = (forward ? 0 : Backwards) | (caseFlag ? 0 : CaseInsensitive) | (wrapFlag ? WrapAround : 0) | (startInSelection ? StartInSelection : 0);
    return findString(target, options);
}

bool Editor::findString(const String& target, FindOptions options)
{
    VisibleSelection selection = m_frame->selection().selection();

    RefPtr<Range> resultRange = rangeOfString(target, selection.firstRange().get(), options);

    if (!resultRange)
        return false;

    m_frame->selection().setSelection(VisibleSelection(resultRange.get(), DOWNSTREAM));
    m_frame->selection().revealSelection();
    return true;
}

PassRefPtr<Range> Editor::findStringAndScrollToVisible(const String& target, Range* previousMatch, FindOptions options)
{
    RefPtr<Range> nextMatch = rangeOfString(target, previousMatch, options);
    if (!nextMatch)
        return 0;

    nextMatch->firstNode()->renderer()->scrollRectToVisible(nextMatch->boundingBox(),
        ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);

    return nextMatch.release();
}

PassRefPtr<Range> Editor::rangeOfString(const String& target, Range* referenceRange, FindOptions options)
{
    if (target.isEmpty())
        return 0;

    // Start from an edge of the reference range, if there's a reference range that's not in shadow content. Which edge
    // is used depends on whether we're searching forward or backward, and whether startInSelection is set.
    RefPtr<Range> searchRange(rangeOfContents(m_frame->document()));

    bool forward = !(options & Backwards);
    bool startInReferenceRange = referenceRange && (options & StartInSelection);
    if (referenceRange) {
        if (forward)
            searchRange->setStart(startInReferenceRange ? referenceRange->startPosition() : referenceRange->endPosition());
        else
            searchRange->setEnd(startInReferenceRange ? referenceRange->endPosition() : referenceRange->startPosition());
    }

    RefPtr<Node> shadowTreeRoot = referenceRange && referenceRange->startContainer() ? referenceRange->startContainer()->nonBoundaryShadowTreeRootNode() : 0;
    if (shadowTreeRoot) {
        if (forward)
            searchRange->setEnd(shadowTreeRoot.get(), shadowTreeRoot->childNodeCount());
        else
            searchRange->setStart(shadowTreeRoot.get(), 0);
    }

    RefPtr<Range> resultRange(findPlainText(searchRange.get(), target, options));
    // If we started in the reference range and the found range exactly matches the reference range, find again.
    // Build a selection with the found range to remove collapsed whitespace.
    // Compare ranges instead of selection objects to ignore the way that the current selection was made.
    if (startInReferenceRange && areRangesEqual(VisibleSelection(resultRange.get()).toNormalizedRange().get(), referenceRange)) {
        searchRange = rangeOfContents(m_frame->document());
        if (forward)
            searchRange->setStart(referenceRange->endPosition());
        else
            searchRange->setEnd(referenceRange->startPosition());

        if (shadowTreeRoot) {
            if (forward)
                searchRange->setEnd(shadowTreeRoot.get(), shadowTreeRoot->childNodeCount());
            else
                searchRange->setStart(shadowTreeRoot.get(), 0);
        }

        resultRange = findPlainText(searchRange.get(), target, options);
    }

    // If nothing was found in the shadow tree, search in main content following the shadow tree.
    if (resultRange->collapsed(ASSERT_NO_EXCEPTION) && shadowTreeRoot) {
        searchRange = rangeOfContents(m_frame->document());
        if (forward)
            searchRange->setStartAfter(shadowTreeRoot->shadowHost());
        else
            searchRange->setEndBefore(shadowTreeRoot->shadowHost());

        resultRange = findPlainText(searchRange.get(), target, options);
    }

    // If we didn't find anything and we're wrapping, search again in the entire document (this will
    // redundantly re-search the area already searched in some cases).
    if (resultRange->collapsed(ASSERT_NO_EXCEPTION) && options & WrapAround) {
        searchRange = rangeOfContents(m_frame->document());
        resultRange = findPlainText(searchRange.get(), target, options);
        // We used to return false here if we ended up with the same range that we started with
        // (e.g., the reference range was already the only instance of this text). But we decided that
        // this should be a success case instead, so we'll just fall through in that case.
    }

    return resultRange->collapsed(ASSERT_NO_EXCEPTION) ? 0 : resultRange.release();
}

void Editor::setMarkedTextMatchesAreHighlighted(bool flag)
{
    if (flag == m_areMarkedTextMatchesHighlighted)
        return;

    m_areMarkedTextMatchesHighlighted = flag;
    m_frame->document()->markers()->repaintMarkers(DocumentMarker::TextMatch);
}

void Editor::respondToChangedSelection(const VisibleSelection& oldSelection, FrameSelection::SetSelectionOptions options)
{
    bool closeTyping = options & FrameSelection::CloseTyping;
    bool isContinuousSpellCheckingEnabled = this->isContinuousSpellCheckingEnabled();
    bool isContinuousGrammarCheckingEnabled = isContinuousSpellCheckingEnabled && isGrammarCheckingEnabled();
    if (isContinuousSpellCheckingEnabled) {
        VisibleSelection newAdjacentWords;
        VisibleSelection newSelectedSentence;
        bool caretBrowsing = m_frame->settings() && m_frame->settings()->caretBrowsingEnabled();
        if (m_frame->selection().selection().isContentEditable() || caretBrowsing) {
            VisiblePosition newStart(m_frame->selection().selection().visibleStart());
            newAdjacentWords = VisibleSelection(startOfWord(newStart, LeftWordIfOnBoundary), endOfWord(newStart, RightWordIfOnBoundary));
            if (isContinuousGrammarCheckingEnabled)
                newSelectedSentence = VisibleSelection(startOfSentence(newStart), endOfSentence(newStart));
        }

        // Don't check spelling and grammar if the change of selection is triggered by spelling correction itself.
        bool shouldCheckSpellingAndGrammar = !(options & FrameSelection::SpellCorrectionTriggered);

        // When typing we check spelling elsewhere, so don't redo it here.
        // If this is a change in selection resulting from a delete operation,
        // oldSelection may no longer be in the document.
        if (shouldCheckSpellingAndGrammar
            && closeTyping
            && oldSelection.isContentEditable()
            && oldSelection.start().inDocument()
            && !isSelectionInTextField(oldSelection)) {
            spellCheckOldSelection(oldSelection, newAdjacentWords, newSelectedSentence);
        }

        if (textChecker().shouldEraseMarkersAfterChangeSelection(TextCheckingTypeSpelling)) {
            if (RefPtr<Range> wordRange = newAdjacentWords.toNormalizedRange())
                m_frame->document()->markers()->removeMarkers(wordRange.get(), DocumentMarker::Spelling);
        }
        if (textChecker().shouldEraseMarkersAfterChangeSelection(TextCheckingTypeGrammar)) {
            if (RefPtr<Range> sentenceRange = newSelectedSentence.toNormalizedRange())
                m_frame->document()->markers()->removeMarkers(sentenceRange.get(), DocumentMarker::Grammar);
        }
    }

    // When continuous spell checking is off, existing markers disappear after the selection changes.
    if (!isContinuousSpellCheckingEnabled)
        m_frame->document()->markers()->removeMarkers(DocumentMarker::Spelling);
    if (!isContinuousGrammarCheckingEnabled)
        m_frame->document()->markers()->removeMarkers(DocumentMarker::Grammar);

    m_frame->inputMethodController().cancelCompositionIfSelectionIsInvalid();

    notifyComponentsOnChangedSelection(oldSelection, options);
}

void Editor::spellCheckAfterBlur()
{
    if (!m_frame->selection().selection().isContentEditable())
        return;

    if (isSelectionInTextField(m_frame->selection().selection())) {
        // textFieldDidEndEditing() and textFieldDidBeginEditing() handle this.
        return;
    }

    VisibleSelection empty;
    spellCheckOldSelection(m_frame->selection().selection(), empty, empty);
}

void Editor::spellCheckOldSelection(const VisibleSelection& oldSelection, const VisibleSelection& newAdjacentWords, const VisibleSelection& newSelectedSentence)
{
    VisiblePosition oldStart(oldSelection.visibleStart());
    VisibleSelection oldAdjacentWords = VisibleSelection(startOfWord(oldStart, LeftWordIfOnBoundary), endOfWord(oldStart, RightWordIfOnBoundary));
    if (oldAdjacentWords  != newAdjacentWords) {
        if (isContinuousSpellCheckingEnabled() && isGrammarCheckingEnabled()) {
            VisibleSelection selectedSentence = VisibleSelection(startOfSentence(oldStart), endOfSentence(oldStart));
            markMisspellingsAndBadGrammar(oldAdjacentWords, selectedSentence != newSelectedSentence, selectedSentence);
        } else {
            markMisspellingsAndBadGrammar(oldAdjacentWords, false, oldAdjacentWords);
        }
    }
}

static Node* findFirstMarkable(Node* node)
{
    while (node) {
        if (!node->renderer())
            return 0;
        if (node->renderer()->isText())
            return node;
        if (node->renderer()->isTextControl())
            node = toRenderTextControl(node->renderer())->textFormControlElement()->visiblePositionForIndex(1).deepEquivalent().deprecatedNode();
        else if (node->firstChild())
            node = node->firstChild();
        else
            node = node->nextSibling();
    }

    return 0;
}

bool Editor::selectionStartHasMarkerFor(DocumentMarker::MarkerType markerType, int from, int length) const
{
    Node* node = findFirstMarkable(m_frame->selection().start().deprecatedNode());
    if (!node)
        return false;

    unsigned int startOffset = static_cast<unsigned int>(from);
    unsigned int endOffset = static_cast<unsigned int>(from + length);
    Vector<DocumentMarker*> markers = m_frame->document()->markers()->markersFor(node);
    for (size_t i = 0; i < markers.size(); ++i) {
        DocumentMarker* marker = markers[i];
        if (marker->startOffset() <= startOffset && endOffset <= marker->endOffset() && marker->type() == markerType)
            return true;
    }

    return false;
}

TextCheckingTypeMask Editor::resolveTextCheckingTypeMask(TextCheckingTypeMask textCheckingOptions)
{
    bool shouldMarkSpelling = textCheckingOptions & TextCheckingTypeSpelling;
    bool shouldMarkGrammar = textCheckingOptions & TextCheckingTypeGrammar;

    TextCheckingTypeMask checkingTypes = 0;
    if (shouldMarkSpelling)
        checkingTypes |= TextCheckingTypeSpelling;
    if (shouldMarkGrammar)
        checkingTypes |= TextCheckingTypeGrammar;

    return checkingTypes;
}

bool Editor::unifiedTextCheckerEnabled() const
{
    return WebCore::unifiedTextCheckerEnabled(m_frame);
}

void Editor::toggleOverwriteModeEnabled()
{
    m_overwriteModeEnabled = !m_overwriteModeEnabled;
    frame().selection().setShouldShowBlockCursor(m_overwriteModeEnabled);
};

} // namespace WebCore
