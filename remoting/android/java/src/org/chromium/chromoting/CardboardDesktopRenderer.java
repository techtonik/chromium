// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.PointF;
import android.opengl.GLES20;
import android.opengl.Matrix;

import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;

import org.chromium.chromoting.jni.JniInterface;

import javax.microedition.khronos.egl.EGLConfig;

/**
 * Renderer for Cardboard view.
 */
public class CardboardDesktopRenderer implements CardboardView.StereoRenderer {
    private static final String TAG = "cr.CardboardRenderer";

    private static final int BYTE_PER_FLOAT = 4;
    private static final int POSITION_DATA_SIZE = 3;
    private static final int TEXTURE_COORDINATE_DATA_SIZE = 2;
    private static final float Z_NEAR = 0.1f;
    private static final float Z_FAR = 100.0f;
    private static final float DESKTOP_POSITION_X = 0.0f;
    private static final float DESKTOP_POSITION_Y = 0.0f;
    private static final float DESKTOP_POSITION_Z = -2.0f;
    private static final float MENU_BAR_POSITION_X = 0.0f;
    private static final float MENU_BAR_POSITION_Y = 0.0f;
    private static final float MENU_BAR_POSITION_Z = -1.5f;
    private static final float HALF_SKYBOX_SIZE = 100.0f;
    private static final float VIEW_POSITION_MIN = -1.0f;
    private static final float VIEW_POSITION_MAX = 3.0f;

    // Allows user to click even when looking outside the desktop
    // but within edge margin.
    private static final float EDGE_MARGIN = 0.1f;

    // Distance to move camera each time.
    private static final float CAMERA_MOTION_STEP = 0.5f;

    private final Activity mActivity;

    private float mCameraPosition;

    // Lock to allow multithreaded access to mCameraPosition.
    private final Object mCameraPositionLock = new Object();

    private float[] mCameraMatrix;
    private float[] mViewMatrix;
    private float[] mProjectionMatrix;

    // Make matrix member variable to avoid unnecessary initialization.
    private float[] mDesktopModelMatrix;
    private float[] mDesktopCombinedMatrix;
    private float[] mEyePointModelMatrix;
    private float[] mEyePointCombinedMatrix;
    private float[] mSkyboxCombinedMatrix;

    // Direction that user is looking towards.
    private float[] mForwardVector;

    // Eye position at the desktop distance.
    private PointF mEyeDesktopPosition;

    // Eye position at the menu bar distance;
    private PointF mEyeMenuBarPosition;

    private CardboardActivityDesktop mDesktop;
    private CardboardActivityEyePoint mEyePoint;
    private CardboardActivitySkybox mSkybox;
    private CardboardActivityMenuBar mMenuBar;

    // Lock for eye position related operations.
    // This protects access to mEyeDesktopPosition.
    private final Object mEyeDesktopPositionLock = new Object();

    // Flag to indicate whether to show menu bar.
    private boolean mMenuBarVisible;

    public CardboardDesktopRenderer(Activity activity) {
        mActivity = activity;
        mCameraPosition = 0.0f;

        mCameraMatrix = new float[16];
        mViewMatrix = new float[16];
        mProjectionMatrix = new float[16];
        mDesktopModelMatrix = new float[16];
        mDesktopCombinedMatrix = new float[16];
        mEyePointModelMatrix = new float[16];
        mEyePointCombinedMatrix = new float[16];
        mSkyboxCombinedMatrix = new float[16];

        mForwardVector = new float[3];
    }

    // This can be called on any thread.
    public void attachRedrawCallback() {
        JniInterface.provideRedrawCallback(new Runnable() {
            @Override
            public void run() {
                mDesktop.reloadTexture();
            }
        });
    }

    @Override
    public void onSurfaceCreated(EGLConfig config) {
        // Set the background clear color to black.
        GLES20.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        // Use culling to remove back faces.
        GLES20.glEnable(GLES20.GL_CULL_FACE);

        // Enable depth testing.
        GLES20.glEnable(GLES20.GL_DEPTH_TEST);

        mDesktop = new CardboardActivityDesktop();
        mEyePoint = new CardboardActivityEyePoint();
        mSkybox = new CardboardActivitySkybox(mActivity);
        mMenuBar = new CardboardActivityMenuBar(mActivity);

        attachRedrawCallback();
    }

    @Override
    public void onSurfaceChanged(int width, int height) {
    }

    @Override
    public void onNewFrame(HeadTransform headTransform) {
        // Position the eye at the origin.
        float eyeX = 0.0f;
        float eyeY = 0.0f;
        float eyeZ;
        synchronized (mCameraPositionLock) {
            eyeZ = mCameraPosition;
        }

        // We are looking toward the negative Z direction.
        float lookX = DESKTOP_POSITION_X;
        float lookY = DESKTOP_POSITION_Y;
        float lookZ = DESKTOP_POSITION_Z;

        // Set our up vector. This is where our head would be pointing were we holding the camera.
        float upX = 0.0f;
        float upY = 1.0f;
        float upZ = 0.0f;

        Matrix.setLookAtM(mCameraMatrix, 0, eyeX, eyeY, eyeZ, lookX, lookY, lookZ, upX, upY, upZ);

        headTransform.getForwardVector(mForwardVector, 0);
        mEyeDesktopPosition = getLookingPosition(DESKTOP_POSITION_Z);
        mEyeMenuBarPosition = getLookingPosition(MENU_BAR_POSITION_Z);
        mDesktop.maybeLoadDesktopTexture();
        mSkybox.maybeLoadTextureAndCleanImages();
    }

    @Override
    public void onDrawEye(Eye eye) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

        // Apply the eye transformation to the camera.
        Matrix.multiplyMM(mViewMatrix, 0, eye.getEyeView(), 0, mCameraMatrix, 0);

        mProjectionMatrix = eye.getPerspective(Z_NEAR, Z_FAR);

        drawSkybox();
        drawDesktop();
        drawMenuBar();
        drawEyePoint();
    }

    @Override
    public void onRendererShutdown() {
        mDesktop.cleanup();
        mEyePoint.cleanup();
        mSkybox.cleanup();
        mMenuBar.cleanup();
    }

    @Override
    public void onFinishFrame(Viewport viewport) {
    }

    private void drawDesktop() {
        if (!mDesktop.hasVideoFrame()) {
            // This can happen if the client is connected, but a complete
            // video frame has not yet been decoded.
            return;
        }

        Matrix.setIdentityM(mDesktopModelMatrix, 0);
        Matrix.translateM(mDesktopModelMatrix, 0, DESKTOP_POSITION_X,
                DESKTOP_POSITION_Y, DESKTOP_POSITION_Z);

        // Pass in Model View Matrix and Model View Project Matrix.
        Matrix.multiplyMM(mDesktopCombinedMatrix, 0, mViewMatrix, 0, mDesktopModelMatrix, 0);
        Matrix.multiplyMM(mDesktopCombinedMatrix, 0, mProjectionMatrix,
                0, mDesktopCombinedMatrix, 0);

        mDesktop.draw(mDesktopCombinedMatrix);
    }

    private void drawEyePoint() {
        if (!isLookingAtDesktop() || (isMenuBarVisible() && isLookingAtMenuBar())) {
            return;
        }

        float eyePointX = clamp(mEyeDesktopPosition.x, -mDesktop.getHalfWidth(),
                mDesktop.getHalfWidth());
        float eyePointY = clamp(mEyeDesktopPosition.y, -mDesktop.getHalfHeight(),
                mDesktop.getHalfHeight());
        Matrix.setIdentityM(mEyePointModelMatrix, 0);
        Matrix.translateM(mEyePointModelMatrix, 0, eyePointX, eyePointY,
                DESKTOP_POSITION_Z);
        Matrix.multiplyMM(mEyePointCombinedMatrix, 0, mViewMatrix, 0, mEyePointModelMatrix, 0);
        Matrix.multiplyMM(mEyePointCombinedMatrix, 0, mProjectionMatrix,
                0, mEyePointCombinedMatrix, 0);

        mEyePoint.draw(mEyePointCombinedMatrix);
    }

    private void drawSkybox() {
        // Since we will always put the skybox center in the origin, so skybox
        // model matrix will always be identity matrix which we could ignore.
        Matrix.multiplyMM(mSkyboxCombinedMatrix, 0, mProjectionMatrix,
                0, mViewMatrix, 0);
        mSkybox.draw(mSkyboxCombinedMatrix);
    }

    private void drawMenuBar() {
        if (!mMenuBarVisible) {
            return;
        }

        mMenuBar.draw(mViewMatrix, mProjectionMatrix, mEyeMenuBarPosition, MENU_BAR_POSITION_X,
                MENU_BAR_POSITION_Y, MENU_BAR_POSITION_Z);
    }

    /**
     * Return menu item that is currently looking at or null if not looking at menu bar.
     */
    public CardboardActivityMenuItem getMenuItem() {
        // Transform world view to model view.
        return mMenuBar.getLookingItem(new PointF(mEyeMenuBarPosition.x - MENU_BAR_POSITION_X,
                mEyeMenuBarPosition.y - MENU_BAR_POSITION_Y));
    }

    /**
     * Returns coordinates in units of pixels in the desktop bitmap.
     * This can be called on any thread.
     */
    public PointF getMouseCoordinates() {
        PointF result = new PointF();
        Point shapePixels = mDesktop.getFrameSizePixels();
        int heightPixels = shapePixels.x;
        int widthPixels = shapePixels.y;

        synchronized (mEyeDesktopPositionLock) {
            // Due to the coordinate direction, we only have to inverse x.
            result.x = (mEyeDesktopPosition.x + mDesktop.getHalfWidth())
                    / (2 * mDesktop.getHalfWidth()) * widthPixels;
            result.y = (-mEyeDesktopPosition.y + mDesktop.getHalfHeight())
                    / (2 * mDesktop.getHalfHeight()) * heightPixels;
            result.x = clamp(result.x, 0, widthPixels);
            result.y = clamp(result.y, 0, heightPixels);
        }
        return result;
    }

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values
     * does not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    private static float clamp(float value, float a, float b) {
        float min = (a > b) ? b : a;
        float max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    /**
     * Move the camera towards desktop.
     * This method can be called on any thread.
     */
    public void moveTowardsDesktop() {
        synchronized (mCameraPositionLock) {
            float newPosition = mCameraPosition - CAMERA_MOTION_STEP;
            if (newPosition >= VIEW_POSITION_MIN) {
                mCameraPosition = newPosition;
            }
        }
    }

    /**
     * Move the camera away from desktop.
     * This method can be called on any thread.
     */
    public void moveAwayFromDesktop() {
        synchronized (mCameraPositionLock) {
            float newPosition = mCameraPosition + CAMERA_MOTION_STEP;
            if (newPosition <= VIEW_POSITION_MAX) {
                mCameraPosition = newPosition;
            }
        }
    }

    /**
     * Return true if user is looking at the desktop.
     * This method can be called on any thread.
     */
    public boolean isLookingAtDesktop() {
        synchronized (mEyeDesktopPositionLock) {
            // TODO(shichengfeng): Move logic to CardboardActivityDesktop.
            return Math.abs(mEyeDesktopPosition.x) <= (mDesktop.getHalfWidth() + EDGE_MARGIN)
                && Math.abs(mEyeDesktopPosition.y) <= (mDesktop.getHalfHeight() + EDGE_MARGIN);
        }
    }

    /**
     * Return true if user is looking at the menu bar.
     */
    public boolean isLookingAtMenuBar() {
        return mMenuBar.contains(new PointF(mEyeMenuBarPosition.x - MENU_BAR_POSITION_X,
                mEyeMenuBarPosition.y - MENU_BAR_POSITION_Y));
    }

    /**
     * Get eye position at the given distance.
     */
    private PointF getLookingPosition(float distance) {
        if (Math.abs(mForwardVector[2]) < 0.00001f) {
            return new PointF(-Math.signum(mForwardVector[0]) * Float.MAX_VALUE,
                    -Math.signum(mForwardVector[1]) * Float.MAX_VALUE);
        } else {
            return new PointF(-mForwardVector[0] * distance / mForwardVector[2],
                    -mForwardVector[1] * distance / mForwardVector[2]);
        }
    }

    /**
     * Set the visibility of the menu bar.
     */
    public void setMenuBarVisible(boolean visible) {
        mMenuBarVisible = visible;
    }

    /**
     * Return true if menu bar is visible.
     */
    public boolean isMenuBarVisible() {
        return mMenuBarVisible;
    }
}