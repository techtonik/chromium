// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enhancedbookmarks;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.LruCache;
import android.util.Pair;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.BookmarksBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;

import java.util.List;

/**
 * A class that encapsulates {@link BookmarksBridge} and provides extra features such as undo, large
 * icon fetching, reader mode url redirecting, etc. This class should serve as the single class for
 * the UI to acquire data from the backend.
 */
public class EnhancedBookmarksModel extends BookmarksBridge {
    private static final int FAVICON_MAX_CACHE_SIZE = 10 * 1024 * 1024; // 10MB

    /**
     * Observer that listens to delete event. This interface is used by undo controllers to know
     * which bookmarks were deleted. Note this observer only listens to events that go through
     * enhanced bookmark model.
     */
    public interface EnhancedBookmarkDeleteObserver {

        /**
         * Callback being triggered immediately before bookmarks are deleted.
         * @param titles All titles of the bookmarks to be deleted.
         * @param isUndoable Whether the deletion is undoable.
         */
        void onDeleteBookmarks(String[] titles, boolean isUndoable);
    }

    private LargeIconBridge mLargeIconBridge;
    private ObserverList<EnhancedBookmarkDeleteObserver> mDeleteObservers = new ObserverList<>();
    private LruCache<String, Pair<Bitmap, Integer>> mFaviconCache;

    /**
     * Initialize enhanced bookmark model for last used non-incognito profile.
     */
    public EnhancedBookmarksModel() {
        this(Profile.getLastUsedProfile().getOriginalProfile());
    }

    @VisibleForTesting
    public EnhancedBookmarksModel(Profile profile) {
        super(profile);
        mLargeIconBridge = new LargeIconBridge();

        ActivityManager activityManager = ((ActivityManager) ApplicationStatus
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize = Math.min(activityManager.getMemoryClass() / 4 * 1024 * 1024,
                FAVICON_MAX_CACHE_SIZE);
        mFaviconCache = new LruCache<String, Pair<Bitmap, Integer>>(maxSize) {
            @Override
            protected int sizeOf(String key, Pair<Bitmap, Integer> icon) {
                int size = Integer.SIZE;
                if (icon.first != null) {
                    size += icon.first.getByteCount();
                }
                return size;
            }
        };
    }

    /**
     * Clean up all the bridges. This must be called after done using this class.
     */
    @Override
    public void destroy() {
        super.destroy();
        mLargeIconBridge.destroy();
        mFaviconCache = null;
    }

    /**
     * Add an observer that listens to delete events that go through enhanced bookmark model.
     * @param observer The observer to add.
     */
    public void addDeleteObserver(EnhancedBookmarkDeleteObserver observer) {
        mDeleteObservers.addObserver(observer);
    }

    /**
     * Remove the observer from listening to bookmark deleting events.
     * @param observer The observer to remove.
     */
    public void removeDeleteObserver(EnhancedBookmarkDeleteObserver observer) {
        mDeleteObservers.removeObserver(observer);
    }

    /**
     * Delete one or multiple bookmarks from model. If more than one bookmarks are passed here, this
     * method will group these delete operations into one undo bundle so that later if the user
     * clicks undo, all bookmarks deleted here will be restored.
     * @param bookmarks Bookmarks to delete. Note this array should not contain a folder and its
     *                  children, because deleting folder will also remove all its children, and
     *                  deleting children once more will cause errors.
     */
    public void deleteBookmarks(BookmarkId... bookmarks) {
        assert bookmarks != null && bookmarks.length > 0;
        // Store all titles of bookmarks.
        String[] titles = new String[bookmarks.length];
        boolean isUndoable = true;
        for (int i = 0; i < bookmarks.length; i++) {
            titles[i] = getBookmarkTitle(bookmarks[i]);
            isUndoable &= (bookmarks[i].getType() == BookmarkType.NORMAL);
        }

        if (bookmarks.length == 1) {
            deleteBookmark(bookmarks[0]);
        } else {
            startGroupingUndos();
            for (BookmarkId bookmark : bookmarks) {
                deleteBookmark(bookmark);
            }
            endGroupingUndos();
        }

        for (EnhancedBookmarkDeleteObserver observer : mDeleteObservers) {
            observer.onDeleteBookmarks(titles, isUndoable);
        }
    }

    /**
     * Calls {@link BookmarksBridge#moveBookmark(BookmarkId, BookmarkId, int)} in a reversed
     * order of the list, in order to let the last item appear at the top.
     */
    public void moveBookmarks(List<BookmarkId> bookmarkIds, BookmarkId newParentId) {
        for (int i = bookmarkIds.size() - 1; i >= 0; i--) {
            moveBookmark(bookmarkIds.get(i), newParentId, 0);
        }
    }

    @Override
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, String url) {
        url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        return super.addBookmark(parent, index, title, url);
    }

    /**
     * @see org.chromium.chrome.browser.BookmarksBridge.BookmarkItem#getTitle()
     */
    public String getBookmarkTitle(BookmarkId bookmarkId) {
        return getBookmarkById(bookmarkId).getTitle();
    }

    /**
     * Retrieves a favicon and fallback color for the given |url|. An LRU cache is used to store the
     * favicons. If the favicon is not already present in the cache, it is retrieved using
     * LargeIconBridge#getLargeIconForUrl().
     *
     * @see LargeIconBridge#getLargeIconForUrl(Profile, String, int, LargeIconCallback)
     */
    public void getLargeIcon(final String url, int minSize, final LargeIconCallback callback) {
        assert callback != null;
        LargeIconCallback callbackWrapper = callback;

        Pair<Bitmap, Integer> cached = mFaviconCache.get(url);
        if (cached != null) {
            callback.onLargeIconAvailable(cached.first, cached.second);
            return;
        }

        callbackWrapper = new LargeIconCallback() {
            @Override
            public void onLargeIconAvailable(Bitmap icon, int fallbackColor) {
                mFaviconCache.put(url, new Pair<Bitmap, Integer>(icon, fallbackColor));
                callback.onLargeIconAvailable(icon, fallbackColor);
            }
        };

        mLargeIconBridge.getLargeIconForUrl(Profile.getLastUsedProfile(), url, minSize,
                callbackWrapper);
    }

    /**
     * @return The id of the default folder to add bookmarks/folders to.
     */
    public BookmarkId getDefaultFolder() {
        return getMobileFolderId();
    }
}
