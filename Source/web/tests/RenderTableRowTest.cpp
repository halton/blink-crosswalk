/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/RenderTableRow.h"

#include "FrameTestHelpers.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebView.h"
#include "core/dom/Document.h"
#include "core/page/Frame.h"

#include <gtest/gtest.h>

using namespace WebKit;

namespace WebCore {

namespace {

class RenderTableRowDeathTest : public testing::Test {
    // It's unfortunate that we have to get the whole browser stack to test one RenderObject
    // but the code needs it.
    static Frame* frame()
    {
        static WebView* webView;

        if (webView)
            return toWebFrameImpl(webView->mainFrame())->frame();

        webView = FrameTestHelpers::createWebViewAndLoad("about:blank");
        webView->setFocus(true);
        return toWebFrameImpl(webView->mainFrame())->frame();
    }

    static Document* document()
    {
        return frame()->document();
    }

    virtual void SetUp()
    {
        m_row = RenderTableRow::createAnonymous(document());
    }

    virtual void TearDown()
    {
        m_row->destroy();
    }

protected:
    RenderTableRow* m_row;
};

TEST_F(RenderTableRowDeathTest, CanSetRow)
{
    static const unsigned rowIndex = 10;
    m_row->setRowIndex(rowIndex);
    EXPECT_EQ(rowIndex, m_row->rowIndex());
}

TEST_F(RenderTableRowDeathTest, CanSetRowToMaxRowIndex)
{
    m_row->setRowIndex(maxRowIndex);
    EXPECT_EQ(maxRowIndex, m_row->rowIndex());
}

// FIXME: Re-enable these tests once ASSERT_DEATH is supported for Android.
// See: https://bugs.webkit.org/show_bug.cgi?id=74089
#if !OS(ANDROID)

TEST_F(RenderTableRowDeathTest, CrashIfRowOverflowOnSetting)
{
    ASSERT_DEATH(m_row->setRowIndex(maxRowIndex + 1), "");
}

TEST_F(RenderTableRowDeathTest, CrashIfSettingUnsetRowIndex)
{
    ASSERT_DEATH(m_row->setRowIndex(unsetRowIndex), "");
}

#endif

}

} // namespace WebCore
