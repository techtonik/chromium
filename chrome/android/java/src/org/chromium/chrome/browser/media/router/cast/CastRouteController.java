// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.chromium.chrome.browser.media.router.RouteController;
import org.chromium.chrome.browser.media.router.RouteDelegate;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * A wrapper around the established Cast application session.
 */
public class CastRouteController implements RouteController {
    private static final String TAG = "cr_MediaRouter";

    private static final String MEDIA_NAMESPACE = "urn:x-cast:com.google.cast.media";
    private static final String RECEIVER_NAMESPACE = "urn:x-cast:com.google.cast.receiver";
    private static final String MEDIA_MESSAGE_TYPES[] = {
            "PLAY",
            "LOAD",
            "PAUSE",
            "SEEK",
            "GET_STATUS",
            "STOP_MEDIA",
            "SET_VOLUME",
            "GET_STATUS",
            "EDIT_TRACKS_INFO",
            "QUEUE_LOAD",
            "QUEUE_INSERT",
            "QUEUE_UPDATE",
            "QUEUE_REMOVE",
            "QUEUE_REORDER"
    };

    private static class CastMessagingChannel implements Cast.MessageReceivedCallback {
        private final CastRouteController mSession;

        public CastMessagingChannel(CastRouteController session) {
            mSession = session;
        }

        @Override
        public void onMessageReceived(CastDevice castDevice, String namespace, String message) {
            if (MEDIA_NAMESPACE.equals(namespace) || RECEIVER_NAMESPACE.equals(namespace)) {
                mSession.onMessage("v2_message", message);
            } else {
                mSession.onAppMessage(namespace, message);
            }
        }
    }

    private final String mMediaRouteId;
    private final String mOrigin;
    private final int mTabId;
    private final CastMessagingChannel mMessageChannel;
    private final RouteDelegate mRouteDelegate;
    private final CastDevice mCastDevice;
    private final MediaSource mSource;

    // Ids of the connected Cast clients.
    private Set<String> mClients = new HashSet<String>();
    private Set<String> mNamespaces = new HashSet<String>();
    private GoogleApiClient mApiClient;
    private String mSessionId;
    private String mApplicationStatus;
    private ApplicationMetadata mApplicationMetadata;
    private int mSequenceNumber;
    private boolean mStoppingApplication;
    private boolean mDetached;

    /**
     * Initializes a new {@link CastRouteController} instance.
     * @param apiClient The Google Play Services client used to create the session.
     * @param sessionId The session identifier to use with the Cast SDK.
     * @param mediaRouteId The media route identifier associated with this session.
     * @param origin The origin of the frame requesting the route.
     * @param tabId the id of the tab containing the frame requesting the route.
     * @param source The {@link MediaSource} corresponding to this session.
     * @param mediaRouter The {@link ChromeMediaRouter} instance managing this session.
     */
    public CastRouteController(
            GoogleApiClient apiClient,
            String sessionId,
            ApplicationMetadata metadata,
            String applicationStatus,
            CastDevice castDevice,
            String mediaRouteId,
            String origin,
            int tabId,
            MediaSource source,
            RouteDelegate delegate) {
        mApiClient = apiClient;
        mSessionId = sessionId;
        mMediaRouteId = mediaRouteId;
        mOrigin = origin;
        mTabId = tabId;
        mSource = source;
        mRouteDelegate = delegate;
        mApplicationMetadata = metadata;
        mApplicationStatus = applicationStatus;
        mCastDevice = castDevice;

        mMessageChannel = new CastMessagingChannel(this);
        addNamespace(RECEIVER_NAMESPACE);
        addNamespace(MEDIA_NAMESPACE);
    }

    public CastRouteController createJoinedController(String mediaRouteId, String origin, int tabId,
            MediaSource source) {
        return new CastRouteController(mApiClient, mSessionId, mApplicationMetadata,
                mApplicationStatus, mCastDevice, mediaRouteId, origin, tabId, source,
                mRouteDelegate);
    }

    /**
     * @return the id of the Cast session controlled by the route.
     */
    public String getSessionId() {
        return mSessionId;
    }

    @Override
    public void close() {
        if (mStoppingApplication) return;

        // close() have been called before from another code path.
        if (mApiClient == null) return;

        if (mApiClient.isConnected() || mApiClient.isConnecting()) {
            mStoppingApplication = true;
            Cast.CastApi.stopApplication(mApiClient, mSessionId)
                    .setResultCallback(new ResultCallback<Status>() {
                        @Override
                        public void onResult(Status status) {
                            onMessage("remove_session", "\"" + mSessionId + "\"");
                            // TODO(avayvod): handle a failure to stop the application.
                            // https://crbug.com/535577

                            for (String namespace : mNamespaces) unregisterNamespace(namespace);
                            mNamespaces.clear();

                            mClients.clear();
                            mSessionId = null;
                            mApiClient = null;

                            mRouteDelegate.onRouteClosed(CastRouteController.this);
                            mStoppingApplication = false;
                        }
                    });
        }
    }

    @Override
    public void sendStringMessage(String message, int callbackId) {
        if (handleInternalMessage(message, callbackId)) return;

        // TODO(avayvod): figure out what to do with custom namespace messages.
        mRouteDelegate.onMessageSentResult(false, callbackId);
    }

    @Override
    public String getSourceId() {
        return mSource.getUrn();
    }

    @Override
    public String getRouteId() {
        return mMediaRouteId;
    }

    @Override
    public String getSinkId() {
        return mCastDevice.getDeviceId();
    }

    @Override
    public String getOrigin() {
        return mOrigin;
    }

    @Override
    public int getTabId() {
        return mTabId;
    }

    @Override
    public void markDetached() {
        mDetached = true;
    }

    @Override
    public boolean isDetached() {
        return mDetached;
    }

    /**
     * Sends the internal Cast message to the Cast clients on the page via the media router.
     * @param type The type of the message (e.g. "new_session" or "v2_message")
     * @param message The message itself (encoded JSON).
     */
    public void onMessage(String type, String message) {
        for (String client : mClients) {
            mRouteDelegate.onMessage(mMediaRouteId,
                    buildInternalMessage(type, message, client, mSequenceNumber));
        }
        mSequenceNumber++;
    }

    /**
     * Forwards the application specific message to the page via the media router.
     * @param namespace The application specific namespace this message belongs to.
     * @param message The message within the namespace that's being send by the receiver.
     */
    public void onAppMessage(String namespace, String message) {
        try {
            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionId);
            jsonMessage.put("namespaceName", namespace);
            jsonMessage.put("message", message);
            onMessage("app_message", jsonMessage.toString());
        } catch (JSONException e) {
            Log.d(TAG, "Failed to create the message wrapper", e);
        }
    }

    private void addNamespace(String namespace) {
        assert !mNamespaces.contains(namespace);
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return;

        // If application metadata is null, register the callback anyway.
        if (mApplicationMetadata != null
                && !mApplicationMetadata.isNamespaceSupported(namespace)) return;

        try {
            Cast.CastApi.setMessageReceivedCallbacks(mApiClient, namespace, mMessageChannel);
            mNamespaces.add(namespace);
        } catch (IOException e) {
            Log.e(TAG, "Failed to register namespace listener for %s", namespace, e);
        }
    }

    private void unregisterNamespace(String namespace) {
        assert mNamespaces.contains(namespace);

        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return;

        try {
            Cast.CastApi.removeMessageReceivedCallbacks(mApiClient, namespace);
        } catch (IOException e) {
            Log.e(TAG, "Failed to remove the namespace listener for %s", namespace, e);
        }
    }

    private boolean handleInternalMessage(String message, int callbackId) {
        boolean success = true;
        try {
            JSONObject jsonMessage = new JSONObject(message);

            String messageType = jsonMessage.getString("type");
            if ("client_connect".equals(messageType)) {
                success = handleClientConnectMessage(jsonMessage);
            } else if ("v2_message".equals(messageType)) {
                success = handleCastV2Message(message, jsonMessage);
            } else if ("app_message".equals(messageType)) {
                success = handleAppMessage(message, jsonMessage);
            } else {
                Log.d(TAG, "Unsupported message: %s", message);
                return false;
            }
        } catch (JSONException e) {
            return false;
        }

        mRouteDelegate.onMessageSentResult(success, callbackId);
        return true;
    }

    private boolean handleClientConnectMessage(JSONObject jsonMessage)
            throws JSONException {
        String clientId = jsonMessage.getString("clientId");

        if (mClients.contains(clientId)) return false;

        mClients.add(clientId);

        mRouteDelegate.onMessage(mMediaRouteId,
                buildInternalMessage("new_session", buildSessionMessage(), clientId, -1));
        return true;
    }

    // An example of the Cast V2 message:
    //    {
    //        "type": "v2_message",
    //        "message": {
    //          "type": "...",
    //          ...
    //        },
    //        "sequenceNumber": 0,
    //        "timeoutMillis": 0,
    //        "clientId": "144042901280235697"
    //    }
    private boolean handleCastV2Message(String message, JSONObject jsonMessage)
            throws JSONException {
        assert "v2_message".equals(jsonMessage.getString("type"));

        String clientId = jsonMessage.getString("clientId");
        if (!mClients.contains(clientId)) return false;

        JSONObject jsonCastMessage = jsonMessage.getJSONObject("message");
        String messageType = jsonCastMessage.getString("type");
        if ("STOP".equals(messageType)) {
            close();
            return true;
        } else if (Arrays.asList(MEDIA_MESSAGE_TYPES).contains(messageType)) {
            return sendCastMessage(jsonMessage.getString("message"), MEDIA_NAMESPACE);
        }

        return true;
    }

    // An example of the Cast application message:
    // {
    //   "type":"app_message",
    //   "message": {
    //     "sessionId":"...",
    //     "namespaceName":"...",
    //     "message": ...
    //   },
    //   "sequenceNumber":0,
    //   "timeoutMillis":3000,
    //   "clientId":"14417311915272175"
    // }
    private boolean handleAppMessage(String message, JSONObject jsonMessage) throws JSONException {
        assert "app_message".equals(jsonMessage.getString("type"));

        String clientId = jsonMessage.getString("clientId");
        if (!mClients.contains(clientId)) return false;

        JSONObject jsonAppMessageWrapper = jsonMessage.getJSONObject("message");
        if (!mSessionId.equals(jsonAppMessageWrapper.getString("sessionId"))) return false;

        String namespaceName = jsonAppMessageWrapper.getString("namespaceName");
        if (namespaceName == null || namespaceName.isEmpty()) return false;

        if (!mNamespaces.contains(namespaceName)) addNamespace(namespaceName);
        sendCastMessage(jsonAppMessageWrapper.getString("message"), namespaceName);

        return true;
    }

    private boolean sendCastMessage(String message, String namespace) {
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return false;

        if (!message.contains("\"requestId\"")) {
            try {
                JSONObject jsonMessage = new JSONObject(message);
                jsonMessage.put("requestId", 0);
                message = jsonMessage.toString();
            } catch (JSONException e) {
                Log.w(TAG, "Cast message is not a valid JSON");
            }
        }

        try {
            Cast.CastApi.sendMessage(mApiClient, namespace, message)
                    .setResultCallback(
                            new ResultCallback<Status>() {
                                @Override
                                public void onResult(Status result) {
                                    // TODO(avayvod): should actually report the result of sending
                                    // status here.
                                }
                            });
        } catch (Exception e) {
            Log.e(TAG, "Exception while sending message", e);
            return false;
        }
        return true;
    }

    private String buildInternalMessage(
            String type, String message, String clientId, int sequenceNumber) {
        return String.format(Locale.US,
                "{ \"type\": \"%s\"," + "\"message\": %s," + "\"sequenceNumber\":%d,"
                        + "\"timeoutMillis\":0," + "\"clientId\": \"%s\"}",
                type, message, sequenceNumber, clientId);
    }

    public void updateSessionStatus() {
        if (mApiClient == null || (!mApiClient.isConnected() && !mApiClient.isConnecting())) return;

        try {
            mApplicationStatus = Cast.CastApi.getApplicationStatus(mApiClient);
            mApplicationMetadata = Cast.CastApi.getApplicationMetadata(mApiClient);

            for (String clientId : mClients) {
                mRouteDelegate.onMessage(mMediaRouteId, buildInternalMessage(
                        "update_session", buildSessionMessage(), clientId, -1));
            }
        } catch (IllegalStateException e) {
            Log.d(TAG, "Can't get application status", e);
        }
    }

    private String buildSessionMessage() {
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return "{}";

        try {
            // "volume" is a part of "receiver" initialized below.
            JSONObject jsonVolume = new JSONObject();
            jsonVolume.put("level", Cast.CastApi.getVolume(mApiClient));
            jsonVolume.put("muted", Cast.CastApi.isMute(mApiClient));

            // "receiver" is a part of "message" initialized below.
            JSONObject jsonReceiver = new JSONObject();
            jsonReceiver.put("label", mCastDevice.getDeviceId());
            jsonReceiver.put("friendlyName", mCastDevice.getFriendlyName());
            jsonReceiver.put("capabilities", getCapabilities(mCastDevice));
            jsonReceiver.put("volume", jsonVolume);
            jsonReceiver.put("isActiveInput", Cast.CastApi.getActiveInputState(mApiClient));
            jsonReceiver.put("displayStatus", null);
            jsonReceiver.put("receiverType", "cast");

            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionId);
            jsonMessage.put("appId", mApplicationMetadata.getApplicationId());
            jsonMessage.put("displayName", mApplicationMetadata.getName());
            jsonMessage.put("statusText", mApplicationStatus);
            jsonMessage.put("receiver", jsonReceiver);
            jsonMessage.put("namespaces", extractNamespaces(mApplicationMetadata));
            jsonMessage.put("media", new JSONArray());
            jsonMessage.put("status", "connected");
            jsonMessage.put("transportId", "web-4");

            return jsonMessage.toString();
        } catch (JSONException e) {
            Log.w(TAG, "Building session message failed", e);
            return "{}";
        }
    }

    private JSONArray getCapabilities(CastDevice device) {
        JSONArray jsonCapabilities = new JSONArray();
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_IN)) {
            jsonCapabilities.put("audio_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_OUT)) {
            jsonCapabilities.put("audio_out");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_IN)) {
            jsonCapabilities.put("video_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_OUT)) {
            jsonCapabilities.put("video_out");
        }
        return jsonCapabilities;
    }

    private JSONArray extractNamespaces(ApplicationMetadata metadata) throws JSONException {
        JSONArray jsonNamespaces = new JSONArray();
        // TODO(avayvod): Need a way to retrieve all the supported namespaces (e.g. YouTube).
        // See crbug.com/529680.
        for (String namespace : mNamespaces) {
            JSONObject jsonNamespace = new JSONObject();
            jsonNamespace.put("name", namespace);
            jsonNamespaces.put(jsonNamespace);
        }
        return jsonNamespaces;
    }
}
