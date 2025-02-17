/**
 * @fileoverview Closure compiler externs for the Polymer library.
 *
 * @externs
 * @license
 * Copyright (c) 2015 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt. The complete set of authors may be
 * found at http://polymer.github.io/AUTHORS.txt. The complete set of
 * contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt. Code
 * distributed by Google as part of the polymer project is also subject to an
 * additional IP rights grant found at http://polymer.github.io/PATENTS.txt.
 */

/**
 * @param {!{is: string}} descriptor The Polymer descriptor of the element.
 * @see https://github.com/Polymer/polymer/blob/0.8-preview/PRIMER.md#custom-element-registration
 */
var Polymer = function(descriptor) {};


/** @constructor @extends {HTMLElement} */
var PolymerElement = function() {};

/**
 * A mapping from ID to element in this Polymer Element's local DOM.
 * @type {!Object}
 */
PolymerElement.prototype.$;

/**
 * True if the element has been attached to the DOM.
 * @type {boolean}
 */
PolymerElement.prototype.isAttached;

/**
 * The root node of the element.
 * @type {!Node}
 */
PolymerElement.prototype.root;

/**
 * Returns the first node in this element’s local DOM that matches selector.
 * @param {string} selector
 */
PolymerElement.prototype.$$ = function(selector) {};

/** @type {string} The Custom element tag name. */
PolymerElement.prototype.is;

/** @type {string} The native element this element extends. */
PolymerElement.prototype.extends;

/**
 * An array of objects whose properties get added to this element.
 * @see https://www.polymer-project.org/1.0/docs/devguide/behaviors.html
 * @type {!Array<!Object>|undefined}
 */
PolymerElement.prototype.behaviors;

/**
 * A string-separated list of dependent properties that should result in a
 * change function being called. These observers differ from single-property
 * observers in that the change handler is called asynchronously.
 *
 * @type {!Object<string, string>|undefined}
 */
PolymerElement.prototype.observers;

/** On create callback. */
PolymerElement.prototype.created = function() {};
/** On ready callback. */
PolymerElement.prototype.ready = function() {};
/** On registered callback. */
PolymerElement.prototype.registered = function() {};
/** On attached to the DOM callback. */
PolymerElement.prototype.attached = function() {};
/** On detached from the DOM callback. */
PolymerElement.prototype.detached = function() {};

/**
 * Callback fired when an attribute on the element has been changed.
 *
 * @param {string} name The name of the attribute that changed.
 */
PolymerElement.prototype.attributeChanged = function(name) {};

/** @typedef {!{
 *    type: !Function,
 *    reflectToAttribute: (boolean|undefined),
 *    readOnly: (boolean|undefined),
 *    notify: (boolean|undefined),
 *    value: *,
 *    computed: (string|undefined),
 *    observer: (string|undefined)
 *  }} */
PolymerElement.PropertyConfig;

/** @typedef {!Object<string, (!Function|!PolymerElement.PropertyConfig)>} */
PolymerElement.Properties;

/** @type {!PolymerElement.Properties} */
PolymerElement.prototype.properties;

/** @type {!Object<string, *>} */
PolymerElement.prototype.hostAttributes;

/**
 * An object that maps events to event handler function names.
 * @type {!Object<string, string>}
 */
PolymerElement.prototype.listeners;

/**
 * Return the element whose local dom within which this element is contained.
 * @type {?Element}
 */
PolymerElement.prototype.domHost;

/**
 * Notifies the event binding system of a change to a property.
 * @param  {string} path  The path to set.
 * @param  {*}      value The value to send in the update notification.
 * @param {boolean=} fromAbove When true, specifies that the change came from
 *     above this element and thus upward notification is not necessary.
 * @return {boolean} True if notification actually took place, based on a dirty
 *     check of whether the new value was already known.
 */
PolymerElement.prototype.notifyPath = function(path, value, fromAbove) {};

/**
 * Convienence method for setting a value to a path and notifying any
 * elements bound to the same path.
 *
 * Note, if any part in the path except for the last is undefined,
 * this method does nothing (this method does not throw when
 * dereferencing undefined paths).
 *
 * @param {(string|Array<(string|number)>)} path Path to the value
 *   to read.  The path may be specified as a string (e.g. `foo.bar.baz`)
 *   or an array of path parts (e.g. `['foo.bar', 'baz']`).  Note that
 *   bracketed expressions are not supported; string-based path parts
 *   *must* be separated by dots.  Note that when dereferencing array
 *   indicies, the index may be used as a dotted part directly
 *   (e.g. `users.12.name` or `['users', 12, 'name']`).
 * @param {*} value Value to set at the specified path.
 * @param {Object=} root Root object from which the path is evaluated.
*/
PolymerElement.prototype.set = function(path, value, root) {};

/**
 * Convienence method for reading a value from a path.
 *
 * Note, if any part in the path is undefined, this method returns
 * `undefined` (this method does not throw when dereferencing undefined
 * paths).
 *
 * @param {(string|Array<(string|number)>)} path Path to the value
 *   to read.  The path may be specified as a string (e.g. `foo.bar.baz`)
 *   or an array of path parts (e.g. `['foo.bar', 'baz']`).  Note that
 *   bracketed expressions are not supported; string-based path parts
 *   *must* be separated by dots.  Note that when dereferencing array
 *   indicies, the index may be used as a dotted part directly
 *   (e.g. `users.12.name` or `['users', 12, 'name']`).
 * @param {Object=} root Root object from which the path is evaluated.
 * @return {*} Value at the path, or `undefined` if any part of the path
 *   is undefined.
 */
PolymerElement.prototype.get = function(path, root) {};

/**
 * Adds items onto the end of the array at the path specified.
 *
 * The arguments after `path` and return value match that of
 * `Array.prototype.push`.
 *
 * This method notifies other paths to the same array that a
 * splice occurred to the array.
 *
 * @param {string} path Path to array.
 * @param {...*} var_args Items to push onto array
 * @return {number} New length of the array.
 */
PolymerElement.prototype.push = function(path, var_args) {};

/**
 * Removes an item from the end of array at the path specified.
 *
 * The arguments after `path` and return value match that of
 * `Array.prototype.pop`.
 *
 * This method notifies other paths to the same array that a
 * splice occurred to the array.
 *
 * @param {string} path Path to array.
 * @return {*} Item that was removed.
 */
PolymerElement.prototype.pop = function(path) {};

/**
 * Starting from the start index specified, removes 0 or more items
 * from the array and inserts 0 or more new itms in their place.
 *
 * The arguments after `path` and return value match that of
 * `Array.prototype.splice`.
 *
 * This method notifies other paths to the same array that a
 * splice occurred to the array.
 *
 * @param {string} path Path to array.
 * @param {number} start Index from which to start removing/inserting.
 * @param {number} deleteCount Number of items to remove.
 * @param {...*} var_args Items to insert into array.
 * @return {!Array} Array of removed items.
 */
PolymerElement.prototype.splice = function(path, start, deleteCount, var_args) {};

/**
 * Removes an item from the beginning of array at the path specified.
 *
 * The arguments after `path` and return value match that of
 * `Array.prototype.pop`.
 *
 * This method notifies other paths to the same array that a
 * splice occurred to the array.
 *
 * @param {string} path Path to array.
 * @return {*} Item that was removed.
 */
PolymerElement.prototype.shift = function(path) {};

/**
 * Adds items onto the beginning of the array at the path specified.
 *
 * The arguments after `path` and return value match that of
 * `Array.prototype.push`.
 *
 * This method notifies other paths to the same array that a
 * splice occurred to the array.
 *
 * @param {string} path Path to array.
 * @param {...*} var_args Items to insert info array
 * @return {number} New length of the array.
 */
PolymerElement.prototype.unshift = function(path, var_args) {};

/**
 * Fire an event.
 *
 * @param {string} type An event name.
 * @param {Object=} detail
 * @param {{
 *   bubbles: (boolean|undefined),
 *   cancelable: (boolean|undefined),
 *   node: (!HTMLElement|undefined)}=} options
 * @return {Object} event
 */
PolymerElement.prototype.fire = function(type, detail, options) {};

/**
 * Toggles the named boolean class on the host element, adding the class if
 * bool is truthy and removing it if bool is falsey. If node is specified, sets
 * the class on node instead of the host element.
 * @param {string} name
 * @param {boolean} bool
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.toggleClass = function(name, bool, node) {};

/**
 * Toggles the named boolean attribute on the host element, adding the attribute
 * if bool is truthy and removing it if bool is falsey. If node is specified,
 * sets the attribute on node instead of the host element.
 * @param {string} name
 * @param {boolean} bool
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.toggleAttribute = function(name, bool, node) {};

/**
 * Moves a boolean attribute from oldNode to newNode, unsetting the attribute
 * (if set) on oldNode and setting it on newNode.
 * @param {string} name
 * @param {!HTMLElement} newNode
 * @param {!HTMLElement} oldNode
 */
PolymerElement.prototype.attributeFollows = function(name, newNode, oldNode) {};

/**
 * Convenience method to add an event listener on a given element, late bound to
 * a named method on this element.
 * @param {!EventTarget} node Element to add event listener to.
 * @param {string} eventName Name of event to listen for.
 * @param {string} methodName Name of handler method on this to call.
 */
PolymerElement.prototype.listen = function(node, eventName, methodName) {};

/**
 * Convenience method to remove an event listener from a given element.
 * @param {!EventTarget} node Element to remove event listener from.
 * @param {string} eventName Name of event to stop listening for.
 * @param {string} methodName Name of handler method on this to remove.
 */
PolymerElement.prototype.unlisten = function(node, eventName, methodName) {};

/**
 * Override scrolling behavior to all direction, one direction, or none.
 *
 * Valid scroll directions:
 * 'all': scroll in any direction
 * 'x': scroll only in the 'x' direction
 * 'y': scroll only in the 'y' direction
 * 'none': disable scrolling for this node
 *
 * @param {string=} direction Direction to allow scrolling Defaults to all.
 * @param {HTMLElement=} node Element to apply scroll direction setting.
 *     Defaults to this.
 */
PolymerElement.prototype.setScrollDirection = function(direction, node) {};

/**
 * @param {!Function} method
 * @param {number=} wait
 * @return {number} A handle which can be used to cancel the job.
 */
PolymerElement.prototype.async = function(method, wait) {};

/**
 * @param {...*} var_args
 */
PolymerElement.prototype.factoryImpl = function(var_args) {};

Polymer.Base;

/**
 * Used by the promise-polyfill on its own.
 *
 * @param {!Function} method
 * @param {number=} wait
 * @return {number} A handle which can be used to cancel the job.
 */
Polymer.Base.async = function(method, wait) {};

/**
 * Returns a property descriptor object for the property specified.
 *
 * This method allows introspecting the configuration of a Polymer element's
 * properties as configured in its `properties` object.  Note, this method
 * normalizes shorthand forms of the `properties` object into longhand form.
 *
 * @param {string} property Name of property to introspect.
 * @return {Object} Property descriptor for specified property.
*/
Polymer.Base.getPropertyInfo = function(property) {};

Polymer.Gestures;

/**
 * Gets the original target of the given event.
 *
 * Cheaper than Polymer.dom(ev).path[0];
 * See https://github.com/Polymer/polymer/blob/master/src/standard/gestures.html#L191
 *
 * @param {Event} ev .
 * @return {Element} The original target of the event.
 */
Polymer.Gestures.findOriginalTarget = function(ev) {};


/**
 * @param {number} handle
 */
PolymerElement.prototype.cancelAsync = function(handle) {};

/**
 * Call debounce to collapse multiple requests for a named task into one
 * invocation, which is made after the wait time has elapsed with no new
 * request. If no wait time is given, the callback is called at microtask timing
 * (guaranteed to be before paint).
 * @param {string} jobName
 * @param {!Function} callback
 * @param {number=} wait
 */
PolymerElement.prototype.debounce = function(jobName, callback, wait) {};

/**
 * Cancels an active debouncer without calling the callback.
 * @param {string} jobName
 */
PolymerElement.prototype.cancelDebouncer = function(jobName) {};

/**
 * Calls the debounced callback immediately and cancels the debouncer.
 * @param {string} jobName
 */
PolymerElement.prototype.flushDebouncer = function(jobName) {};

/**
 * @param {string} jobName
 * @return {boolean} True if the named debounce task is waiting to run.
 */
PolymerElement.prototype.isDebouncerActive = function(jobName) {};


/**
 * Applies a CSS transform to the specified node, or this element if no node is
 * specified. transform is specified as a string.
 * @param {string} transform
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.transform = function(transform, node) {};

/**
 * Transforms the specified node, or this element if no node is specified.
 * @param {number|string} x
 * @param {number|string} y
 * @param {number|string} z
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.translate3d = function(x, y, z, node) {};

/**
 * Dynamically imports an HTML document.
 * @param {string} href
 * @param {Function=} onload
 * @param {Function=} onerror
 */
PolymerElement.prototype.importHref = function(href, onload, onerror) {};

/**
 * Delete an element from an array.
 * @param {!Array|string} array Path to array from which to remove the item (or
 *     the array itself).
 * @param {*} item Item to remove
 * @return {!Array} The array with the item removed.
 */
PolymerElement.prototype.arrayDelete = function(array, item) {};

/**
 * Resolve a url to make it relative to the current doc.
 * @param {string} url
 * @return {string}
 */
PolymerElement.prototype.resolveUrl = function(url) {};

/**
 * Re-evaluates and applies custom CSS properties based on dynamic
 * changes to this element's scope, such as adding or removing classes
 * in this element's local DOM.
 *
 * For performance reasons, Polymer's custom CSS property shim relies
 * on this explicit signal from the user to indicate when changes have
 * been made that affect the values of custom properties.
 *
 * @param {Object=} properties Properties object which, if provided is mixed
 *     into the element's `customStyle` property. This argument provides a
 *     shortcut for setting `customStyle` and then calling `updateStyles`.
 */
PolymerElement.prototype.updateStyles = function(properties) {};

/**
 * @type {!Object<string, string|undefined>}
 */
PolymerElement.prototype.customStyle;

/** @type {Node|undefined} */
PolymerElement.prototype.shadyRoot;

/**
 * Logs a message to the console.
 *
 * @param {!Array} var_args
 * @protected
 */
PolymerElement.prototype._log = function(var_args) {};

/**
 * Logs a message to the console with a 'warn' level.
 *
 * @param {!Array} var_args
 * @protected
 */
PolymerElement.prototype._warn = function(var_args) {};

/**
 * Logs a message to the console with an 'error' level.
 *
 * @param {!Array} var_args
 * @protected
 */
PolymerElement.prototype._error = function(var_args) {};

/**
 * Formats string arguments together for a console log.
 *
 * @param {...*} var_args
 * @return {!Array} The formatted array of args to a log function.
 * @protected
 */
PolymerElement.prototype._logf = function(var_args) {};


/**
 * A Polymer DOM API for manipulating DOM such that local DOM and light DOM
 * trees are properly maintained.
 *
 * @constructor
 */
var PolymerDomApi = function() {};

/** @param {!Node} node */
PolymerDomApi.prototype.appendChild = function(node) {};

/**
 * @param {!Node} oldNode
 * @param {!Node} newNode
 */
PolymerDomApi.prototype.replaceChild = function(oldNode, newNode) {};

/**
 * @param {!Node} node
 * @param {!Node} beforeNode
 */
PolymerDomApi.prototype.insertBefore = function(node, beforeNode) {};

/** @param {!Node} node */
PolymerDomApi.prototype.removeChild = function(node) {};

/** @type {!Array<!Node>} */
PolymerDomApi.prototype.childNodes;

/** @type {?Node} */
PolymerDomApi.prototype.parentNode;

/** @type {?Node} */
PolymerDomApi.prototype.firstChild;

/** @type {?Node} */
PolymerDomApi.prototype.lastChild;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.firstElementChild;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.lastElementChild;

/** @type {?Node} */
PolymerDomApi.prototype.previousSibling;

/** @type {?Node} */
PolymerDomApi.prototype.nextSibling;

/** @type {string} */
PolymerDomApi.prototype.textContent;

/** @type {string} */
PolymerDomApi.prototype.innerHTML;

/**
 * @param {string} selector
 * @return {?HTMLElement}
 */
PolymerDomApi.prototype.querySelector = function(selector) {};

/**
 * @param {string} selector
 * @return {!Array<!HTMLElement>}
 */
PolymerDomApi.prototype.querySelectorAll = function(selector) {};

/** @return {!Array<!Node>} */
PolymerDomApi.prototype.getDistributedNodes = function() {};

/** @return {!Array<!Node>} */
PolymerDomApi.prototype.getDestinationInsertionPoints = function() {};

/** @return {?Node} */
PolymerDomApi.prototype.getOwnerRoot = function() {};

/**
 * @param {string} attribute
 * @param {string|number|boolean} value Values are converted to strings with
 *     ToString, so we accept number and boolean since both convert easily to
 *     strings.
 */
PolymerDomApi.prototype.setAttribute = function(attribute, value) {};

/** @param {string} attribute */
PolymerDomApi.prototype.removeAttribute = function(attribute) {};

/** @type {?DOMTokenList} */
PolymerDomApi.prototype.classList;

/**
 * @param {string} selector
 * @return {!Array<!HTMLElement>}
 */
PolymerDomApi.prototype.queryDistributedElements = function(selector) {};

/**
 * A Polymer Event API.
 *
 * @constructor
 */
var PolymerEventApi = function() {};

/** @type {?EventTarget} */
PolymerEventApi.prototype.rootTarget;

/** @type {?EventTarget} */
PolymerEventApi.prototype.localTarget;

/** @type {?Array<!Element>|undefined} */
PolymerEventApi.prototype.path;

/**
 * Returns a Polymer-friendly API for manipulating DOM of a specified node or
 * an event API for a specified event..
 *
 * @param {?Node|?Event} nodeOrEvent
 * @return {!PolymerDomApi|!PolymerEventApi}
 */
Polymer.dom = function(nodeOrEvent) {};

Polymer.dom.flush = function() {};

Polymer.CaseMap;

/**
 * Convert a string from dash to camel-case.
 * @param {string} dash
 * @return {string} The string in camel-case.
 */
Polymer.CaseMap.dashToCamelCase = function(dash) {};

/**
 * Convert a string from camel-case to dash format.
 * @param {string} camel
 * @return {string} The string in dash format.
 */
Polymer.CaseMap.camelToDashCase = function(camel) {};


/**
 * A Polymer data structure abstraction.
 *
 * @param {?Array} userArray
 * @constructor
 */
Polymer.Collection = function(userArray) {};

Polymer.Collection.prototype.initMap = function() {};

/**
 * @param {*} item
 */
Polymer.Collection.prototype.add = function(item) {};

/**
 * @param {number|string} key
 */
Polymer.Collection.prototype.removeKey = function(key) {};

/**
 * @param {*} item
 * @return {number|string} The key of the item removed.
 */
Polymer.Collection.prototype.remove = function(item) {};

/**
 * @param {*} item
 * @return {number|string} The key of the item.
 */
Polymer.Collection.prototype.getKey = function(item) {};

/**
 * @return {!Array<number|string>} The key of the item removed.
 */
Polymer.Collection.prototype.getKeys = function() {};

/**
 * @param {number|string} key
 * @param {*} item
 */
Polymer.Collection.prototype.setItem = function(key, item) {};

/**
 * @param {number|string} key
 * @return {*} The item for the given key if present.
 */
Polymer.Collection.prototype.getItem = function(key) {};

/**
 * @return {!Array} The items in the collection
 */
Polymer.Collection.prototype.getItems = function() {};

/**
 * @param {!Array} userArray
 * @return {!Polymer.Collection} A new Collection wrapping the given array.
 */
Polymer.Collection.get = function(userArray) {};

/**
 * @param {!Array} userArray
 * @param {!Array<!PolymerSplice>} splices
 * @return {!Array<!PolymerKeySplice>} KeySplices with added and removed keys
 */
Polymer.Collection.applySplices = function(userArray, splices) {};

/**
 * Settings pulled from
 * https://github.com/Polymer/polymer/blob/master/src/lib/settings.html
 */
Polymer.Settings;

/** @type {boolean} */
Polymer.Settings.wantShadow;

/** @type {boolean} */
Polymer.Settings.hasShadow;

/** @type {boolean} */
Polymer.Settings.nativeShadow;

/** @type {boolean} */
Polymer.Settings.useShadow;

/** @type {boolean} */
Polymer.Settings.useNativeShadow;

/** @type {boolean} */
Polymer.Settings.useNativeImports;

/** @type {boolean} */
Polymer.Settings.useNativeCustomElements;


/**
 * @see https://github.com/Polymer/polymer/blob/master/src/lib/template/templatizer.html
 * @polymerBehavior
 */
Polymer.Templatizer = {
  ctor: function() {},

  /**
   * @param {?Object} model
   * @return {?Element}
   */
  stamp: function(model) {},

  /**
   * @param {?Element} template
   */
  templatize: function(template) {},

  /**
   * Returns the template "model" associated with a given element, which
   * serves as the binding scope for the template instance the element is
   * contained in. A template model is an instance of `Polymer.Base`, and
   * should be used to manipulate data associated with this template instance.
   *
   * Example:
   *
   *   var model = modelForElement(el);
   *   if (model.index < 10) {
   *     model.set('item.checked', true);
   *   }
   *
   * @param {!HTMLElement} el Element for which to return a template model.
   * @return {(!PolymerElement)|undefined} Model representing the binding scope for
   *   the element.
   */
  modelForElement: function(el) {}
};



/**
 * A node produced by Templatizer which has a templateInstance property.
 *
 * @constructor
 * @extends {HTMLElement}
 */
var TemplatizerNode = function() {};


/** @type {?PolymerElement} */
TemplatizerNode.prototype._templateInstance;



/**
 * @see https://github.com/Polymer/polymer/blob/master/src/lib/template/array-selector.html
 * @extends {PolymerElement}
 * @constructor
 */
var ArraySelectorElement = function() {};


/**
 * Returns whether the item is currently selected.
 *
 * @param {*} item Item from `items` array to test
 * @return {boolean} Whether the item is selected
 */
ArraySelectorElement.prototype.isSelected = function(item) {};


/**
 * Clears the selection state.
 */
ArraySelectorElement.prototype.clearSelection = function() {};


/**
 * Deselects the given item if it is already selected.
 *
 * @param {*} item Item from `items` array to deselect
 */
ArraySelectorElement.prototype.deselect = function(item) {};


/**
 * Selects the given item.  When `toggle` is true, this will automatically
 * deselect the item if already selected.
 *
 * @param {*} item Item from `items` array to select
 */
ArraySelectorElement.prototype.select = function(item) {};


/**
 * An Event type fired when moving while finger/button is down.
 * state - a string indicating the tracking state:
 *     + start: fired when tracking is first detected (finger/button down and
 *              moved past a pre-set distance threshold)
 *     + track: fired while tracking
 *     + end: fired when tracking ends
 * x - clientX coordinate for event
 * y - clientY coordinate for event
 * dx - change in pixels horizontally since the first track event
 * dy - change in pixels vertically since the first track event
 * ddx - change in pixels horizontally since last track event
 * ddy - change in pixels vertically since last track event
 * hover() - a function that may be called to determine the element currently
 *           being hovered
 *
 * @typedef {{
 *   state: string,
 *   x: number,
 *   y: number,
 *   dx: number,
 *   dy: number,
 *   ddx: number,
 *   ddy: number,
 *   hover: (function(): Node)
 * }}
 */
var PolymerTrackEvent;

/**
 * An Event type fired when a finger does down, up, or taps.
 * x - clientX coordinate for event
 * y - clientY coordinate for event
 * sourceEvent - the original DOM event that caused the down action
 *
 * @typedef {{
 *   x: number,
 *   y: number,
 *   sourceEvent: Event
 * }}
 */
var PolymerTouchEvent;

/**
 * @typedef {{
 *   index: number,
 *   removed: !Array,
 *   addedCount: number
 * }}
 */
var PolymerSplice;

/**
 * @typedef {{
 *   added: !Array<string|number>,
 *   removed: !Array<string|number>
 * }}
 */
var PolymerKeySplice;

/**
 * @typedef {{
 *   indexSplices: ?Array<!PolymerSplice>,
 *   keySplices: ?Array<!PolymerKeySplice>
 * }}
 */
var PolymerSpliceChange;

/**
 * The interface that iconsets should obey. Iconsets are registered by setting
 * their name in the IronMeta 'iconset' db, and a value of type Polymer.Iconset.
 * 
 * Used by iron-icon but needs to live here since iron-icon, iron-iconset, etc don't
 * depend on each other at all and talk only through iron-meta.
 *
 * @interface
 */
Polymer.Iconset = function() {};

/**
 * Applies an icon to the given element as a css background image. This
 * method does not size the element, and it's usually necessary to set
 * the element's height and width so that the background image is visible.
 *
 * @param {Element} element The element to which the icon is applied.
 * @param {string} icon The name of the icon to apply.
 * @param {string=} theme (optional) The name or index of the icon to apply.
 * @param {number=} scale (optional, defaults to 1) Icon scaling factor.
 */
Polymer.Iconset.prototype.applyIcon = function(
      element, icon, theme, scale) {};
