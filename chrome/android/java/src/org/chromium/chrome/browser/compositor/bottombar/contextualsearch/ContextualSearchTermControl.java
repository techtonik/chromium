// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Search Term View that is used as a dynamic resource.
 */
public class ContextualSearchTermControl extends ContextualSearchInflater {
    /**
     * The search term View.
     */
    private TextView mSearchTerm;

    /**
     * @param panel             The panel delegate.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchTermControl(ContextualSearchPanelDelegate panel,
                                          Context context,
                                          ViewGroup container,
                                          DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.contextual_search_term_view, R.id.contextual_search_term_view,
                context, container, resourceLoader);
    }

    /**
     * Sets the search term to display in the control.
     * @param searchTerm The string that represents the search term.
     */
    public void setSearchTerm(String searchTerm) {
        inflate();

        mSearchTerm.setText(sanitizeText(searchTerm));

        invalidate();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mSearchTerm = (TextView) view.findViewById(R.id.contextual_search_term);
    }
}
