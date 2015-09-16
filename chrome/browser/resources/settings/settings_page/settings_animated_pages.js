// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-animated-pages' is a container for a page and animated subpages.
 * It provides a set of common behaviors and animations.
 *
 * Example:
 *
 *    <cr-settings-animated-pages current-route="{{currentRoute}}"
          route-root="advanced/privacy" redirect-root-route-to="advanced">
 *      <!-- Insert your section controls here -->
 *    </cr-settings-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-animated-pages
 */
Polymer({
  is: 'cr-settings-animated-pages',

  properties: {
    /**
     * Contains the current route.
     */
    currentRoute: {
      type: Object,
      notify: true,
      observer: 'currentRouteChanged_',
    },

    /**
     * Routes with this section activate this element. For instance, if this
     * property is 'search', and currentRoute.section is also set to 'search',
     * this element will display the subpage in currentRoute.subpage.
     */
    section: {
      type: String,
    },
  },

  /** @override */
  created: function() {
    this.addEventListener('subpage-back', function() {
      assert(this.currentRoute.section == this.section);
      assert(this.currentRoute.subpage.length >= 1);

      this.setSubpageChain(this.currentRoute.subpage.slice(0, -1));
    }.bind(this));
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    // route.section is only non-empty when the user is within a subpage.
    // When the user is not in a subpage, but on the Basic page, route.section
    // is an empty string.
    var newRouteIsSubpage = newRoute && newRoute.section == this.section;
    var oldRouteIsSubpage = oldRoute && oldRoute.section == this.section;

    // If two routes are at the same level, or if either the new or old route is
    // not a subpage, fade in and out.
    if (!newRouteIsSubpage || !oldRouteIsSubpage ||
        newRoute.subpage.length == oldRoute.subpage.length) {
      this.$.animatedPages.exitAnimation = 'fade-out-animation';
      this.$.animatedPages.entryAnimation = 'fade-in-animation';
    } else {
      // For transitioning between subpages at different levels, slide.
      if (newRoute.subpage.length > oldRoute.subpage.length) {
        this.$.animatedPages.exitAnimation = 'slide-left-animation';
        this.$.animatedPages.entryAnimation = 'slide-from-right-animation';
      } else {
        this.$.animatedPages.exitAnimation = 'slide-right-animation';
        this.$.animatedPages.entryAnimation = 'slide-from-left-animation';
      }
    }

    if (newRouteIsSubpage) {
      // TODO(tommycli): Support paths where the final component carries
      // data rather than referring to a specific subpage.
      // E.g. internet > internet/known-networks > internet/detail/wifi1_guid
      this.$.animatedPages.selected = newRoute.subpage.slice(-1)[0];
    } else {
      this.$.animatedPages.selected = '';
    }
  },

  /**
   * Buttons in this pageset should use this method to transition to subpages.
   */
  setSubpageChain: function(subpage) {
    this.currentRoute = {
      page: this.currentRoute.page,
      section: subpage.length > 0 ? this.section : '',
      subpage: subpage,
    };
  },
});
