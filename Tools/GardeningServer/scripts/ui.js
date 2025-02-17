/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

var ui = ui || {};

(function () {

ui.displayURLForBuilder = function(builderName)
{
    return config.kPlatforms[config.currentPlatform].waterfallURL + '?' + $.param({
        'builder': builderName
    });
}

ui.displayNameForBuilder = function(builderName)
{
    return builderName.replace(/Webkit /, '');
}

ui.urlForTest = function(testName)
{
    return 'http://trac.webkit.org/browser/trunk/LayoutTests/' + testName;
}

ui.urlForFlakinessDashboard = function(opt_testNameList)
{
    var testsParameter = opt_testNameList ? opt_testNameList.join(',') : '';
    return 'http://test-results.appspot.com/dashboards/flakiness_dashboard.html#tests=' + encodeURIComponent(testsParameter);
}

ui.urlForEmbeddedFlakinessDashboard = function(opt_testNameList)
{
    return ui.urlForFlakinessDashboard(opt_testNameList) + '&showChrome=false';
}

ui.rolloutReasonForTestNameList = function(testNameList)
{
    return 'Broke:\n' + testNameList.map(function(testName) {
        return '* ' + testName;
    }).join('\n');
}

ui.onebar = base.extends('div', {
    init: function()
    {
        this.id = 'onebar';
        this.innerHTML =
            '<ul>' +
                '<li><a href="#unexpected">Unexpected Failures</a></li>' +
                '<li><a href="#expected">Expected Failures</a></li>' +
                '<li><a href="#results">Results</a></li>' +
            '</ul>' +
            '<div id="unexpected"></div>' +
            '<div id="expected"></div>' +
            '<div id="results"></div>';
        this._tabNames = [
            'unexpected',
            'expected',
            'results',
        ]

        this._tabIndexToSavedScrollOffset = {};
        this._tabs = $(this).tabs({
            disabled: [2],
            show: function(event, ui) { this._restoreScrollOffset(ui.index); },
        });
    },
    _saveScrollOffset: function() {
        var tabIndex = this._tabs.tabs('option', 'selected');
        this._tabIndexToSavedScrollOffset[tabIndex] = document.body.scrollTop;
    },
    _restoreScrollOffset: function(tabIndex)
    {
        document.body.scrollTop = this._tabIndexToSavedScrollOffset[tabIndex] || 0;
    },
    _setupHistoryHandlers: function()
    {
        function currentHash() {
            var hash = window.location.hash;
            return (!hash || hash == '#') ? '#unexpected' : hash;
        }

        var self = this;
        $('.ui-tabs-nav a').bind('mouseup', function(event) {
            var href = event.target.getAttribute('href');
            var hash = currentHash();
            if (href != hash) {
                self._saveScrollOffset();
                window.location = href
            }
        });

        window.onhashchange = function(event) {
            var tabName = currentHash().substring(1);
            self._selectInternal(tabName);
        };

        // When navigating from the browser chrome, we'll
        // scroll to the #tabname contents. popstate fires before
        // we scroll, so we can save the scroll offset first.
        window.onpopstate = function() {
            self._saveScrollOffset();
        };
    },
    attach: function()
    {
        document.body.insertBefore(this, document.body.firstChild);
        this._setupHistoryHandlers();
    },
    tabNamed: function(tabName)
    {
        if (this._tabNames.indexOf(tabName) == -1)
            return null;
        tab = document.getElementById(tabName);
        // We perform this sanity check below to make sure getElementById
        // hasn't given us a node in some other unrelated part of the document.
        // that shouldn't happen normally, but it could happen if an attacker
        // has somehow sneakily added a node to our document.
        if (tab.parentNode != this)
            return null;
        return tab;
    },
    unexpected: function()
    {
        return this.tabNamed('unexpected');
    },
    expected: function()
    {
        return this.tabNamed('expected');
    },
    results: function()
    {
        return this.tabNamed('results');
    },
    _selectInternal: function(tabName) {
        var tabIndex = this._tabNames.indexOf(tabName);
        this._tabs.tabs('enable', tabIndex);
        this._tabs.tabs('select', tabIndex);
    },
    select: function(tabName)
    {
        this._saveScrollOffset();
        this._selectInternal(tabName);
        window.location = '#' + tabName;
    }
});

// FIXME: Loading a module shouldn't set off a timer.  The controller should kick this off.
setInterval(function() {
    Array.prototype.forEach.call(document.querySelectorAll("time.relative"), function(time) {
        time.update && time.update();
    });
}, config.kRelativeTimeUpdateFrequency);

ui.RelativeTime = base.extends('time', {
    init: function()
    {
        this.className = 'relative';
    },
    date: function()
    {
        return this._date;
    },
    update: function()
    {
        this.textContent = this._date ? base.relativizeTime(this._date) : '';
    },
    setDate: function(date)
    {
        this._date = date;
        this.update();
    }
});

ui.StatusArea = base.extends('div',  {
    init: function()
    {
        // This is a Singleton.
        if (ui.StatusArea._instance)
            return ui.StatusArea._instance;
        ui.StatusArea._instance = this;

        this.className = 'status';
        document.body.appendChild(this);
        this._currentId = 0;
        this._unfinishedIds = {};

        this.appendChild(new ui.actions.List([new ui.actions.Close()]));
        $(this).bind('close', this.close.bind(this));

        var processing = document.createElement('progress');
        processing.className = 'process-text';
        processing.textContent = 'Processing...';
        this.appendChild(processing);
    },
    close: function()
    {
        this.style.visibility = 'hidden';
        Array.prototype.forEach.call(this.querySelectorAll('.status-content'), function(node) {
            node.parentNode.removeChild(node);
        });
    },
    addMessage: function(id, message)
    {
        this.style.visibility = 'visible';
        $(this).addClass('processing');

        var element = document.createElement('div');
        $(element).addClass('message').text(message);

        var content = this.querySelector('#' + id);
        if (!content) {
            content = document.createElement('div');
            content.id = id;
            content.className = 'status-content';
            this.appendChild(content);
        }

        content.appendChild(element);
        if (element.offsetTop < this.scrollTop || element.offsetTop + element.offsetHeight > this.scrollTop + this.offsetHeight)
            this.scrollTop = element.offsetTop;
    },
    // FIXME: It's unclear whether this code could live here or in a controller.
    addFinalMessage: function(id, message)
    {
        this.addMessage(id, message);

        delete this._unfinishedIds[id];
        if (!Object.keys(this._unfinishedIds).length)
            $(this).removeClass('processing');
    },
    newId: function() {
        var id = 'status-content-' + ++this._currentId;
        this._unfinishedIds[id] = 1;
        return id;
    }
});

ui.revisionDetails = base.extends('span', {
    init: function() {
        var theSpan = this;
        theSpan.appendChild(document.createTextNode('Latest revision processed by every bot: '));

        var latestRevision = model.latestRevisionWithNoBuildersInFlight();
        var latestRevisions = model.latestRevisionByBuilder();

        // Get the list of builders sorted with the most recent one first.
        var builders = Object.keys(latestRevisions);
        builders.sort(function (a, b) { return parseInt(latestRevisions[b]) - parseInt(latestRevisions[a])});

        var summaryNode = document.createElement('summary');
        var summaryLinkNode = base.createLinkNode(trac.changesetURL(latestRevision), latestRevision);
        summaryNode.appendChild(summaryLinkNode);

        var revisionsTableNode = document.createElement('table');
        builders.forEach(function(builderName) {
            var trNode = document.createElement('tr');

            var tdNode = document.createElement('td');
            tdNode.appendChild(base.createLinkNode(ui.displayURLForBuilder(builderName), builderName.replace('WebKit ', '')));
            trNode.appendChild(tdNode);

            var tdNode = document.createElement('td');
            tdNode.appendChild(document.createTextNode(latestRevisions[builderName]));
            trNode.appendChild(tdNode)

            revisionsTableNode.appendChild(trNode)
        });

        var revisionsNode = document.createElement('details');
        revisionsNode.appendChild(summaryNode);
        revisionsNode.appendChild(revisionsTableNode);
        theSpan.appendChild(revisionsNode);

        // This adds a pop-up when we hover over the summary if the details aren't being shown.
        var revisionsPopUp = $('<span id="revisionPopUp">').appendTo(summaryLinkNode);
        revisionsPopUp.append($(revisionsTableNode).clone());
        $(summaryLinkNode).mouseover(function(ev) {
            if (!revisionsNode.open) {
                var tPosX = $(summaryNode).position().left;
                var tPosY = $(summaryNode).position().top + 16;
                $(revisionsPopUp).css({'position': 'absolute', 'top': tPosY, 'left': tPosX});
                $(revisionsPopUp).addClass('active')
            }
        });
        $(summaryLinkNode).mouseout(function(ev) {
            if (!revisionsNode.open) {
                $(revisionsPopUp).removeClass("active");
            }
        });

        var totRevision = model.latestRevision();
        theSpan.appendChild(document.createTextNode(', trunk is at '));
        theSpan.appendChild(base.createLinkNode(trac.changesetURL(totRevision), totRevision));

        checkout.lastBlinkRollRevision(function(revision) {
            theSpan.appendChild(document.createTextNode(', last roll is to '));
            theSpan.appendChild(base.createLinkNode(trac.changesetURL(revision), revision));
        }, function() {});

        rollbot.fetchCurrentRoll(function(roll) {
            theSpan.appendChild(document.createTextNode(', current autoroll '));
            if (roll) {
                var linkText = "" + roll.fromRevision + ":" + roll.toRevision;
                theSpan.appendChild(base.createLinkNode(roll.url, linkText));
                if (roll.isStopped)
                    theSpan.appendChild(document.createTextNode(' (STOPPED) '));
            } else {
                theSpan.appendChild(document.createTextNode(' None'));
            }
        });
    }
});

})();
