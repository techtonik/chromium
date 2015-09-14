// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelDelegate;
import org.chromium.chrome.browser.customtabs.CustomTab;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.common.TopControlsState;

/**
 * The delegate that provides global management functionality for Contextual Search.
 */
public interface ContextualSearchManagementDelegate {
    /**
     * @return Whether the Search Panel is showing.
     */
    boolean isShowingSearchPanel();

    /**
     * Sets the preference state to enabled or disabled.
     *
     * @param enabled Whether the preference should be set to enabled.
     */
    void setPreferenceState(boolean enabled);

    /**
     * @return Whether the Opt-out promo is available to be be shown in the panel.
     */
    boolean isPromoAvailable();

    /**
     * Called when the promo Panel gets closed, to log the outcome.
     */
    void logPromoOutcome();

    /**
     * Updates the top controls state for the base tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process. See {@link Tab#updateTopControlsState(int current, boolean animate)}
     * @param current The desired current state for the controls.  Pass
     *                {@link TopControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *                should jump immediately.
     */
    void updateTopControlsState(int current, boolean animate);

    /**
     * Promotes the current Content View Core in the Contextual Search Panel to its own Tab.
     */
    void promoteToTab();

    /**
     * Gets the Search Content View's vertical scroll position. If the Search Content View
     * is not available it returns -1.
     * @return The Search Content View scroll position.
     */
    float getSearchContentViewVerticalScroll();

    /**
     * Sets the delegate responsible for manipulating the ContextualSearchLayout.
     * @param delegate The ContextualSearchLayoutDelegate.
     */
    void setContextualSearchPanelDelegate(ContextualSearchPanelDelegate delegate);

    /**
     * Gets whether the device is running in compatibility mode for Contextual Search.
     * If so, a new tab showing search results should be opened instead of showing the panel.
     * @return whether the device is running in compatibility mode.
     */
    boolean isRunningInCompatibilityMode();

    /**
     * Opens the resolved search URL in a new tab.
     */
    void openResolvedSearchUrlInNewTab();

    /**
     * Preserves the Base Page's selection next time it loses focus.
     */
    void preserveBasePageSelectionOnNextLossOfFocus();

    /**
     * Dismisses the Contextual Search bar completely.  This will hide any panel that's currently
     * showing as well as any bar that's peeking.
     */
    void dismissContextualSearchBar();

    /**
     * Notifies that the Contextual Search Panel did get closed.
     * @param reason The reason the panel is closing.
     */
    void onCloseContextualSearch(StateChangeReason reason);

    /**
     * Gets the {@code ContentViewCore} associated with Contextual Search Panel.
     * @return Contextual Search Panel's {@code ContentViewCore}.
     */
    ContentViewCore getSearchContentViewCore();

    /**
     * @return The resource id that contains how large the top controls are.
     */
    int getControlContainerHeightResource();

    /**
     * @return Whether the current activity contains a {@link CustomTab}.
     */
    boolean isCustomTab();

    /**
     * This method is called when the panel's ContentViewCore is created.
     * @param contentView The created ContentViewCore.
     */
    void onContentViewCreated(ContentViewCore contentView);

    /**
     * This method is called when the panel's ContentViewCore is destroyed.
     */
    void onContentViewDestroyed();

    /**
     * This is called on navigation of the contextual search pane This is called on navigation
     * of the contextual search panel.
     * @param isFailure If the request resulted in an error page.
     */
    void onContextualSearchRequestNavigation(boolean isFailure);

    /**
     * This is called when the search panel is shown or is hidden.
     * @param isVisible True if the panel is now visible.
     */
    void onContentViewVisibilityChanged(boolean isVisible);

    /**
     * This is called when the panel has loaded search results.
     */
    void onSearchResultsLoaded();

    /**
     * Called when an external navigation occurs.
     * @param url The URL being navigated to.
     */
    void onExternalNavigation(String url);

    /**
     * Handles the WebContentsObserver#didNavigateMainFrame callback.
     * @param url The URL of the navigation.
     * @param httpResultCode The HTTP result code of the navigation.
     */
    void handleDidNavigateMainFrame(String url, int httpResultCode);

    /**
     * Called when the WebContents for the panel starts loading.
     */
    void onStartedLoading();

    /**
     * Determine if a particular navigation should be ignored.
     * @param externalNavHandler External navigation handler for the activity the panel is in.
     * @param navigationParams The navigation params for the current navigation.
     * @return True if the navigation should be ignored.
     */
    boolean shouldInterceptNavigation(ExternalNavigationHandler externalNavHandler,
            NavigationParams navigationParams);
}
