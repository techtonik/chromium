// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a function that logs a 'not available' error to the console and
 * returns undefined.
 *
 * @param {string} messagePrefix text to prepend to the exception message.
 */
function generateDisabledMethodStub(messagePrefix, opt_messageSuffix) {
  var message = messagePrefix + ' is not available in packaged apps.';
  if (opt_messageSuffix) message = message + ' ' + opt_messageSuffix;
  return function() {
    console.error(message);
    return;
  };
}

/**
 * Returns a function that throws a 'not available' error.
 *
 * @param {string} messagePrefix text to prepend to the exception message.
 */
function generateThrowingMethodStub(messagePrefix, opt_messageSuffix) {
  var message = messagePrefix + ' is not available in packaged apps.';
  if (opt_messageSuffix) message = message + ' ' + opt_messageSuffix;
  return function() {
    throw new Error(message);
  };
}

/**
 * Replaces the given methods of the passed in object with stubs that log
 * 'not available' errors to the console and return undefined.
 *
 * This should be used on methods attached via non-configurable properties,
 * such as window.alert. disableGetters should be used when possible, because
 * it is friendlier towards feature detection.
 *
 * In most cases, the useThrowingStubs should be false, so the stubs used to
 * replace the methods log an error to the console, but allow the calling code
 * to continue. We shouldn't break library code that uses feature detection
 * responsibly, such as:
 *     if(window.confirm) {
 *       var result = window.confirm('Are you sure you want to delete ...?');
 *       ...
 *     }
 *
 * useThrowingStubs should only be true for methods that are deprecated in the
 * Web platform, and should not be used by a responsible library, even in
 * conjunction with feature detection. A great example is document.write(), as
 * the HTML5 specification recommends against using it, and says that its
 * behavior is unreliable. No reasonable library code should ever use it.
 * HTML5 spec: http://www.w3.org/TR/html5/dom.html#dom-document-write
 *
 * @param {Object} object The object with methods to disable. The prototype is
 *     preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the stub (this is the name that the object is commonly referred
 *     to by web developers, e.g. "document" instead of "HTMLDocument").
 * @param {Array<string>} methodNames names of methods to disable.
 * @param {Boolean} useThrowingStubs if true, the replaced methods will throw
 *     an error instead of silently returning undefined
 */
function disableMethods(object, objectName, methodNames, useThrowingStubs) {
  $Array.forEach(methodNames, function(methodName) {
    var messagePrefix = objectName + '.' + methodName + '()';
    object[methodName] = useThrowingStubs ?
        generateThrowingMethodStub(messagePrefix) :
        generateDisabledMethodStub(messagePrefix);
  });
}

/**
 * Replaces the given properties of the passed in object with stubs that log
 * 'not available' warnings to the console and return undefined when gotten. If
 * a property's setter is later invoked, the getter and setter are restored to
 * default behaviors.
 *
 * @param {Object} object The object with properties to disable. The prototype
 *     is preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the getter stub (this is the name that the object is commonly
 *     referred to by web developers, e.g. "document" instead of
 *     "HTMLDocument").
 * @param {Array<string>} propertyNames names of properties to disable.
 */
function disableGetters(object, objectName, propertyNames, opt_messageSuffix) {
  $Array.forEach(propertyNames, function(propertyName) {
    var stub = generateDisabledMethodStub(objectName + '.' + propertyName,
                                          opt_messageSuffix);
    stub._is_platform_app_disabled_getter = true;
    $Object.defineProperty(object, propertyName, {
      configurable: true,
      enumerable: false,
      get: stub,
      set: function(value) {
        var descriptor = $Object.getOwnPropertyDescriptor(this, propertyName);
        if (!descriptor || !descriptor.get ||
            descriptor.get._is_platform_app_disabled_getter) {
          // The stub getter is still defined.  Blow-away the property to
          // restore default getter/setter behaviors and re-create it with the
          // given value.
          delete this[propertyName];
          this[propertyName] = value;
        } else {
          // Do nothing.  If some custom getter (not ours) has been defined,
          // there would be no way to read back the value stored by a default
          // setter. Also, the only way to clear a custom getter is to first
          // delete the property.  Therefore, the value we have here should
          // just go into a black hole.
        }
      }
    });
  });
}

/**
 * Replaces the given properties of the passed in object with stubs that log
 * 'not available' warnings to the console when set.
 *
 * @param {Object} object The object with properties to disable. The prototype
 *     is preferred.
 * @param {string} objectName The display name to use in the error message
 *     thrown by the setter stub (this is the name that the object is commonly
 *     referred to by web developers, e.g. "document" instead of
 *     "HTMLDocument").
 * @param {Array<string>} propertyNames names of properties to disable.
 */
function disableSetters(object, objectName, propertyNames, opt_messageSuffix) {
  $Array.forEach(propertyNames, function(propertyName) {
    var stub = generateDisabledMethodStub(objectName + '.' + propertyName,
                                          opt_messageSuffix);
    $Object.defineProperty(object, propertyName, {
      configurable: true,
      enumerable: false,
      get: function() {
        return;
      },
      set: stub
    });
  });
}

// Disable benign Document methods.
disableMethods(HTMLDocument.prototype, 'document', ['open', 'clear', 'close']);

// Replace evil Document methods with exception-throwing stubs.
disableMethods(HTMLDocument.prototype, 'document', ['write', 'writeln'], true);

// Disable history.
Object.defineProperty(window, "history", { value: {} });
disableGetters(window.history, 'history',
    ['back', 'forward', 'go', 'length', 'pushState', 'replaceState']);

// Disable find.
disableMethods(window, 'window', ['find']);
disableMethods(Window.prototype, 'window', ['find']);

// Disable modal dialogs. Shell windows disable these anyway, but it's nice to
// warn.
disableMethods(window, 'window', ['alert', 'confirm', 'prompt']);
disableMethods(Window.prototype, 'window', ['alert', 'confirm', 'prompt']);

// Disable window.*bar.
disableGetters(window, 'window',
    ['locationbar', 'menubar', 'personalbar', 'scrollbars', 'statusbar',
     'toolbar']);

// Disable window.localStorage.
// Sometimes DOM security policy prevents us from doing this (e.g. for data:
// URLs) so wrap in try-catch.
try {
  disableGetters(window, 'window',
      ['localStorage'],
      'Use chrome.storage.local instead.');
} catch (e) {}

// Document instance properties that we wish to disable need to be set when
// the document begins loading, since only then will the "document" reference
// point to the page's document (it will be reset between now and then).
// We can't listen for the "readystatechange" event on the document (because
// the object that it's dispatched on doesn't exist yet), but we can instead
// do it at the window level in the capturing phase.
window.addEventListener('readystatechange', function(event) {
  if (document.readyState != 'loading')
    return;

  // Deprecated document properties from
  // https://developer.mozilla.org/en/DOM/document.
  // To deprecate document.all, simply changing its getter and setter would
  // activate its cache mechanism, and degrade the performance. Here we assign
  // it first to 'undefined' to avoid this.
  document.all = undefined;
  disableGetters(document, 'document',
      ['alinkColor', 'all', 'bgColor', 'fgColor', 'linkColor', 'vlinkColor']);
}, true);

// Disable onunload, onbeforeunload.
disableSetters(window, 'window', ['onbeforeunload', 'onunload']);
disableSetters(Window.prototype, 'window', ['onbeforeunload', 'onunload']);
var eventTargetAddEventListener = EventTarget.prototype.addEventListener;
EventTarget.prototype.addEventListener = function(type) {
  if (type === 'unload' || type === 'beforeunload')
    generateDisabledMethodStub(type)();
  else
    return $Function.apply(eventTargetAddEventListener, this, arguments);
};
