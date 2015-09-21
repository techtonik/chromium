// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var resultQueue = new ResultQueue();
var registrationReference = null;

// Sends data back to the test. This must be in response to an earlier
// request, but it's ok to respond asynchronously. The request blocks until
// the response is sent.
function sendResultToTest(result) {
  console.log('sendResultToTest: ' + result);
  if (window.domAutomationController) {
    domAutomationController.send('' + result);
  }
}

function sendErrorToTest(error) {
  sendResultToTest(error.name + ' - ' + error.message);
}

function registerServiceWorker() {
  navigator.serviceWorker.register('service_worker.js', {scope: './'})
    .then(function() {
      return navigator.serviceWorker.ready;
    })
    .then(function(swRegistration) {
      sendResultToTest('ok - service worker registered');
    })
    .catch(sendErrorToTest);
}

function registerOneShot(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      return swRegistration.sync.register({'tag': tag});
    })
    .then(function(syncRegistration) {
      sendResultToTest('ok - ' + tag + ' registered');
    })
    .catch(sendErrorToTest);
}

function unregisterOneShot(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      return swRegistration.sync.getRegistration(tag);
    })
    .then(function(syncRegistration) {
      if (!syncRegistration) {
        sendResultToTest('error - ' + tag + ' not found');
        return;
      }
      return syncRegistration.unregister();
    })
    .then(function() {
      sendResultToTest('ok - ' + tag + ' unregistered');
    })
    .catch(sendErrorToTest);
}

function unregisterOneShotTwice(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      return swRegistration.sync.getRegistration(tag);
    })
    .then(function(syncRegistration) {
      if (!syncRegistration) {
        sendResultToTest('error - ' + tag + ' not found');
        return;
      }
      return syncRegistration.unregister();
    })
    .then(function() {
      return syncRegistration.unregister();
    })
    .then(sendErrorToTest, function() {
      sendResultToTest('ok - ' + tag + ' failed to unregister twice');
    })
    .catch(sendErrorToTest);
}

function getRegistrationOneShot(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      return swRegistration.sync.getRegistration(tag);
    })
    .then(function(syncRegistration) {
      if (!syncRegistration) {
        sendResultToTest('error - ' + tag + ' not found');
        return;
      }
      sendResultToTest('ok - ' + tag + ' found');
    })
    .catch(sendErrorToTest);
}

function getRegistrationsOneShot(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      return swRegistration.sync.getRegistrations();
    })
    .then(function(syncRegistrations) {
      var tags = syncRegistrations.map(function(syncRegistration) {
        return syncRegistration.tag;
      });
      sendResultToTest('ok - ' + tags.toString());
    })
    .catch(sendErrorToTest);
}

function completeDelayedOneShot() {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      swRegistration.active.postMessage({action: 'completeDelayedOneShot'});
      sendResultToTest('ok - delay completing');
    })
    .catch(sendErrorToTest);
}

function rejectDelayedOneShot() {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      swRegistration.active.postMessage({action: 'rejectDelayedOneShot'});
      sendResultToTest('ok - delay rejecting');
    })
    .catch(sendErrorToTest);
}

function notifyWhenDoneOneShot(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      swRegistration.active.postMessage({action: 'notifyWhenDone', tag: tag});
    })
    .catch(sendErrorToTest);
}

function notifyWhenDoneImmediateOneShot(tag) {
  if (registrationReference == null) {
    sendResultToTest('error - must call storeRegistration first');
    return;
  }

  registrationReference.done
    .then(function(success) {
      sendResultToTest('ok - ' + registrationReference.tag +
                       ' result: ' + success)
    }, function(err) {
      sendResultToTest('error - ' + registrationReference.tag + ' failed');
    })
    .catch(sendErrorToTest)
}


function storeRegistration(tag) {
  navigator.serviceWorker.ready
    .then(function(swRegistration) {
      return swRegistration.sync.getRegistration(tag);
    })
    .then(function(syncRegistration) {
      registrationReference = syncRegistration;
      sendResultToTest('ok - ' + tag + ' stored');
    })
    .catch(sendErrorToTest);
}

// Queue storing asynchronous results received from the Service Worker. Results
// are sent to the test when requested.
function ResultQueue() {
  // Invariant: this.queue.length == 0 || this.pendingGets == 0
  this.queue = [];
  this.pendingGets = 0;
}

// Adds a data item to the queue. Will be sent to the test if there are
// pendingGets.
ResultQueue.prototype.push = function(data) {
  if (this.pendingGets) {
    this.pendingGets--;
    sendResultToTest(data);
  } else {
    this.queue.unshift(data);
  }
};

// Called by native. Sends the next data item to the test if it is available.
// Otherwise increments pendingGets so it will be delivered when received.
ResultQueue.prototype.pop = function() {
  if (this.queue.length) {
    sendResultToTest(this.queue.pop());
  } else {
    this.pendingGets++;
  }
};

// Called by native. Immediately sends the next data item to the test if it is
// available, otherwise sends null.
ResultQueue.prototype.popImmediately = function() {
  sendResultToTest(this.queue.length ? this.queue.pop() : null);
};

navigator.serviceWorker.addEventListener('message', function(event) {
  var message = event.data;
  if (message.type == 'sync')
    resultQueue.push(message.data);
}, false);