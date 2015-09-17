// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  var Toolbar = Polymer({
    is: 'downloads-toolbar',

    /** @param {!downloads.ActionService} actionService */
    setActionService: function(actionService) {
      /** @private {!downloads.ActionService} */
      this.actionService_ = actionService;
    },

    attached: function() {
      /** @private {!SearchFieldDelegate} */
      this.searchFieldDelegate_ = new ToolbarSearchFieldDelegate(this);
      this.$['search-input'].setDelegate(this.searchFieldDelegate_);
    },

    properties: {
      downloadsShowing: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
        observer: 'onDownloadsShowingChange_',
      },
    },

    /** @return {boolean} Whether removal can be undone. */
    canUndo: function() {
      return this.$['search-input'] != this.shadowRoot.activeElement;
    },

    /** @return {boolean} Whether "Clear all" should be allowed. */
    canClearAll: function() {
      return !this.$['search-input'].getValue() && this.downloadsShowing;
    },

    /** @private */
    onClearAllClick_: function() {
      assert(this.canClearAll());
      this.actionService_.clearAll();
    },

    /** @private */
    onDownloadsShowingChange_: function() {
      this.updateClearAll_();
    },

    /** @param {string} searchTerm */
    onSearchTermSearch: function(searchTerm) {
      this.actionService_.search(searchTerm);
      this.updateClearAll_();
    },

    /** @private */
    onOpenDownloadsFolderClick_: function() {
      this.actionService_.openDownloadsFolder();
    },

    /** @private */
    updateClearAll_: function() {
      this.$$('#actions .clear-all').hidden = !this.canClearAll();
      this.$$('paper-menu .clear-all').hidden = !this.canClearAll();
    },
  });

  /**
   * @constructor
   * @implements {SearchFieldDelegate}
   */
  // TODO(devlin): This is a bit excessive, and it would be better to just have
  // Toolbar implement SearchFieldDelegate. But for now, we don't know how to
  // make that happen with closure compiler.
  function ToolbarSearchFieldDelegate(toolbar) {
    this.toolbar_ = toolbar;
  }

  ToolbarSearchFieldDelegate.prototype = {
    /** @override */
    onSearchTermSearch: function(searchTerm) {
      this.toolbar_.onSearchTermSearch(searchTerm);
    }
  };

  return {Toolbar: Toolbar};
});

// TODO(dbeam): https://github.com/PolymerElements/iron-dropdown/pull/16/files
/** @suppress {checkTypes} */
(function() {
Polymer.IronDropdownScrollManager.pushScrollLock = function() {};
})();
