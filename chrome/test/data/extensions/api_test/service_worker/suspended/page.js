// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var logForDebugging = false;
function log(message) {
  if (logForDebugging)
    console.log(message);
}
navigator.serviceWorker.register('/sw.js').then(function(registration) {
  log('ServiceWorker registration successful with scope: ',
      registration.scope);
  // This resolves once there's an active worker at this scope.
  return navigator.serviceWorker.ready;
}).then(function() {
  chrome.test.sendMessage('registered');
}).catch(function(err) {
  // registration failed :(
  var errorMsg = err.name + ': ' + err.message;
  log('ServiceWorker registration failed: ' + errorMsg);
});
