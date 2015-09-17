// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

/**
 * Interface defining operations related to a single {@link MediaRoute}.
 */
public interface RouteController {
    /**
     * Close the route.
     */
    void close();

    /**
     * Send a string message to the route and invokes the {@link RouteDelegate} with the
     * passed callback id on success or failure.
     * @param message The message to send.
     * @param nativeCallbackId The id of the callback handling the result.
     */
    void sendStringMessage(String message, int nativeCallbackId);
}
