/* Test imported from Alex Vincent's XHR2 timeout tests, written for Mozilla.
   https://hg.mozilla.org/mozilla-central/file/tip/content/base/test/
   Released into the public domain, according to
   https://bugzilla.mozilla.org/show_bug.cgi?id=525816#c86
*/

/* Notes:
   - All times are expressed in milliseconds in this test suite.
   - Test harness code is at the end of this file.
   - We generate only one request at a time, to avoid overloading the HTTP
   request handlers.
 */

var TIME_NORMAL_LOAD = 1000;
var TIME_LATE_TIMEOUT = 800;
var TIME_XHR_LOAD = 600;
var TIME_REGULAR_TIMEOUT = 400;
var TIME_SYNC_TIMEOUT = 200;
var TIME_DELAY = 200;

/*
 * This should point to a resource that responds after a delay of TIME_XHR_LOAD milliseconds.
 */
var STALLED_REQUEST_URL = "/resources/load-and-stall.php?name=../../../http/tests/xmlhttprequest/timeout/xmlhttprequest-timeout.js&stallFor=" + TIME_XHR_LOAD/1000 + "&stallAt=0&mimeType=text/plain";

var inWorker = false;
try {
  inWorker = !(self instanceof Window);
} catch (e) {
  inWorker = true;
}

function message(obj) {
  if (inWorker)
    self.postMessage(obj);
  else
    self.postMessage(obj, "*");
}

function is(got, expected, msg) {
  var obj = {};
  obj.type = "is";
  obj.got = got;
  obj.expected = expected;
  obj.msg = msg;

  message(obj);
}

function ok(bool, msg) {
  var obj = {};
  obj.type = "ok";
  obj.bool = bool;
  obj.msg = msg;

  message(obj);
}

/**
 * Generate and track results from a XMLHttpRequest with regards to timeouts.
 *
 * @param {String} id         The test description.
 * @param {Number} timeLimit  The initial setting for the request timeout.
 * @param {Number} resetAfter (Optional) The time after sending the request, to
 *                            reset the timeout.
 * @param {Number} resetTo    (Optional) The delay to reset the timeout to.
 *
 * @note The actual testing takes place in handleEvent(event).
 * The requests are generated in startXHR().
 *
 * @note If resetAfter and resetTo are omitted, only the initial timeout setting
 * applies.
 *
 * @constructor
 * @implements DOMEventListener
 */
function RequestTracker(async, id, timeLimit /*[, resetAfter, resetTo]*/) {
  this.async = async;
  this.id = id;
  this.timeLimit = timeLimit;

  if (arguments.length > 3) {
    this.mustReset  = true;
    this.resetAfter = arguments[3];
    this.resetTo    = arguments[4];
  }

  this.hasFired = false;
}
RequestTracker.prototype = {
  /**
   * Start the XMLHttpRequest!
   */
  startXHR: function() {
    var req = new XMLHttpRequest();
    this.request = req;
    req.open("GET", STALLED_REQUEST_URL, this.async);
    var me = this;
    function handleEvent(e) { return me.handleEvent(e); };
    req.onerror = handleEvent;
    req.onload = handleEvent;
    req.onabort = handleEvent;
    req.ontimeout = handleEvent;

    req.timeout = this.timeLimit;
    
    if (this.mustReset) {
      var resetTo = this.resetTo;
      self.setTimeout(function() {
        req.timeout = resetTo;
      }, this.resetAfter);
    }

    try {
      req.send(null);
    }
    catch (e) {
      // Synchronous case in workers.
      ok(!this.async && this.timeLimit < TIME_XHR_LOAD && e.name == "TimeoutError", "Unexpected error: " + e);
      TestCounter.testComplete();
    }
  },

  /**
   * Get a message describing this test.
   *
   * @returns {String} The test description.
   */
  getMessage: function() {
    var rv = this.id + ", ";
    if (this.mustReset) {
      rv += "original timeout at " + this.timeLimit + ", ";
      rv += "reset at " + this.resetAfter + " to " + this.resetTo;
    }
    else {
      rv += "timeout scheduled at " + this.timeLimit;
    }
    return rv;
  },

  /**
   * Check the event received, and if it's the right (and only) one we get.
   *
   * @param {DOMProgressEvent} evt An event of type "load" or "timeout".
   */
  handleEvent: function(evt) {
    if (this.hasFired) {
      ok(false, "Only one event should fire: " + this.getMessage());
      return;
    }
    this.hasFired = true;

    var type = evt.type, expectedType;
    // The XHR responds after TIME_XHR_LOAD milliseconds with a load event.
    var timeLimit = this.mustReset && (this.resetAfter < Math.min(TIME_XHR_LOAD, this.timeLimit)) ?
                    this.resetTo :
                    this.timeLimit;
    if ((timeLimit == 0) || (timeLimit >= TIME_XHR_LOAD)) {
      expectedType = "load";
    }
    else {
      expectedType = "timeout";
    }
    is(type, expectedType, this.getMessage());
    TestCounter.testComplete();
  }
};

/**
 * Generate and track XMLHttpRequests which will have abort() called on.
 *
 * @param shouldAbort {Boolean} True if we should call abort at all.
 * @param abortDelay  {Number}  The time in ms to wait before calling abort().
 */
function AbortedRequest(shouldAbort, abortDelay) {
  this.shouldAbort = shouldAbort;
  this.abortDelay  = abortDelay;
  this.hasFired    = false;
}
AbortedRequest.prototype = {
  /**
   * Start the XMLHttpRequest!
   */
  startXHR: function() {
    var req = new XMLHttpRequest();
    this.request = req;
    req.open("GET", STALLED_REQUEST_URL);
    var me = this;
    function handleEvent(e) { return me.handleEvent(e); };
    req.onerror = handleEvent;
    req.onload = handleEvent;
    req.onabort = handleEvent;
    req.ontimeout = handleEvent;

    req.timeout = TIME_REGULAR_TIMEOUT;
    var _this = this;

    function abortReq() {
      req.abort();
    }

    if (!this.shouldAbort) {
      self.setTimeout(function() {
        try {
          _this.noEventsFired();
        }
        catch (e) {
          ok(false, "Unexpected error: " + e);
          TestCounter.testComplete();
        }
      }, TIME_NORMAL_LOAD);
    }
    else {
      // Abort events can only be triggered on sent requests.
      req.send();
      if (this.abortDelay == -1) {
        abortReq();
      }
      else {
        self.setTimeout(abortReq, this.abortDelay);
      }
    }
  },

  /**
   * Ensure that no events fired at all, especially not our timeout event.
   */
  noEventsFired: function() {
    ok(!this.hasFired, "No events should fire for an unsent, unaborted request");
    // We're done; if timeout hasn't fired by now, it never will.
    TestCounter.testComplete();
  },

  /**
   * Get a message describing this test.
   *
   * @returns {String} The test description.
   */
  getMessage: function() {
    return "time to abort is " + this.abortDelay + ", timeout set at " + TIME_REGULAR_TIMEOUT;
  },

  /**
   * Check the event received, and if it's the right (and only) one we get.
   *
   * WebKit fires abort events even for DONE and UNSENT states, which is 
   * discussed in http://webkit.org/b/98404
   * That's why we chose to accept secondary "abort" events in this test.
   *
   * @param {DOMProgressEvent} evt An event of type "load" or "timeout".
   */
  handleEvent: function(evt) {
    if (this.hasFired && evt.type != "abort") {
      ok(false, "Only abort event should fire: " + this.getMessage());
      return;
    }

    var expectedEvent = (this.abortDelay >= TIME_REGULAR_TIMEOUT && !this.hasFired) ? "timeout" : "abort";
    this.hasFired = true;
    is(evt.type, expectedEvent, this.getMessage());
    TestCounter.testComplete();
  }
};

var SyncRequestSettingTimeoutAfterOpen = {
  startXHR: function() {
    var pass = false;
    var req = new XMLHttpRequest();
    req.open("GET", STALLED_REQUEST_URL, false);
    try {
      req.timeout = TIME_SYNC_TIMEOUT;
    }
    catch (e) {
      pass = true;
    }
    ok(pass, "Synchronous XHR must not allow a timeout to be set");
    TestCounter.testComplete();
  }
};

var SyncRequestSettingTimeoutBeforeOpen = {
  startXHR: function() {
    var pass = false;
    var req = new XMLHttpRequest();
    req.timeout = TIME_SYNC_TIMEOUT;
    try {
      req.open("GET", STALLED_REQUEST_URL, false);
    }
    catch (e) {
      pass = true;
    }
    ok(pass, "Synchronous XHR must not allow a timeout to be set");
    TestCounter.testComplete();
  }
};

var TestRequestGroups = {
  "simple" : [
    new RequestTracker(true, "no time out scheduled, load fires normally", 0),
    new RequestTracker(true, "load fires normally", TIME_NORMAL_LOAD),
    new RequestTracker(true, "timeout hit before load", TIME_REGULAR_TIMEOUT)
  ],

  "twice" : [
     new RequestTracker(true, "load fires normally with no timeout set, twice", 0, TIME_REGULAR_TIMEOUT, 0),
     new RequestTracker(true, "load fires normally with same timeout set twice", TIME_NORMAL_LOAD, TIME_REGULAR_TIMEOUT, TIME_NORMAL_LOAD),
     new RequestTracker(true, "timeout fires normally with same timeout set twice", TIME_REGULAR_TIMEOUT, TIME_DELAY, TIME_REGULAR_TIMEOUT)
  ],

  // FIXME: http://webkit.org/b/98156 - Late updates are not supported yet, these tests are not run.
  "overrides" : [
    new RequestTracker(true, "timeout disabled after initially set", TIME_NORMAL_LOAD, TIME_REGULAR_TIMEOUT, 0),
    new RequestTracker(true, "timeout overrides load after a delay", TIME_NORMAL_LOAD, TIME_DELAY, TIME_REGULAR_TIMEOUT),
    new RequestTracker(true, "timeout enabled after initially disabled", 0, TIME_REGULAR_TIMEOUT, TIME_NORMAL_LOAD)
  ],

  "overridesexpires" : [
    new RequestTracker(true, "timeout set to expiring value after load fires", TIME_NORMAL_LOAD, TIME_LATE_TIMEOUT, TIME_DELAY),
    // FIXME: http://webkit.org/b/98156 - Late updates are not supported yet, this test is not run.
    // new RequestTracker(true, "timeout set to expired value before load fires", TIME_NORMAL_LOAD, TIME_REGULAR_TIMEOUT, TIME_DELAY),
    new RequestTracker(true, "timeout set to non-expiring value after timeout fires", TIME_DELAY, TIME_REGULAR_TIMEOUT, TIME_NORMAL_LOAD)
  ],

  "aborted" : [
    new AbortedRequest(false),
    new AbortedRequest(true, -1),
    new AbortedRequest(true, TIME_NORMAL_LOAD)
  ],

  "abortedonmain" : [
    new AbortedRequest(true, 0),
    new AbortedRequest(true, TIME_DELAY)
  ],

  "synconmain" : [
    SyncRequestSettingTimeoutAfterOpen,
    SyncRequestSettingTimeoutBeforeOpen
  ],

  "synconworker" : [
    new RequestTracker(false, "no time out scheduled, load fires normally", 0),
    new RequestTracker(false, "load fires normally", TIME_NORMAL_LOAD),
    new RequestTracker(false, "timeout hit before load", TIME_REGULAR_TIMEOUT)
  ]
};

var TestRequests = [];

// This code controls moving from one test to another.
var TestCounter = {
  testComplete: function() {
    // Allow for the possibility there are other events coming.
    self.setTimeout(function() {
      TestCounter.next();
    }, TIME_NORMAL_LOAD);
  },

  next: function() {
    var test = TestRequests.shift();

    if (test) {
      test.startXHR();
    }
    else {
      message("done");
    }
  }
};

self.addEventListener("message", function (event) {
  if (event.data.type == "start") {
    TestRequests = TestRequestGroups[event.data.group];
    TestCounter.next();
  }
});
