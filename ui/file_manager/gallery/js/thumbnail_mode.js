// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Thumbnail Mode.
 * @param {!HTMLElement} container A container.
 * @param {!GalleryDataModel} dataModel Gallery data model.
 * @param {!cr.ui.ListSelectionModel} selectionModel List selection model.
 * @param {function()} changeToSlideModeCallback A callback to be called to
 *     change to slide mode.
 * @constructor
 * @struct
 */
function ThumbnailMode(
    container, dataModel, selectionModel, changeToSlideModeCallback) {
  this.changeToSlideModeCallback_ = changeToSlideModeCallback;
  this.thumbnailView_ = new ThumbnailView(container, dataModel, selectionModel);
  this.thumbnailView_.addEventListener(
      'thumbnail-double-click', this.onThumbnailDoubleClick_.bind(this));
}

/**
 * Return name of this mode.
 * @return {string} Mode name.
 */
ThumbnailMode.prototype.getName = function() { return 'thumbnail'; };

/**
 * Return title of this mode.
 * @return {string} Mode title.
 */
ThumbnailMode.prototype.getTitle = function() { return 'GALLERY_THUMBNAIL'; };

/**
 * Executes an action. An action is executed immediately since this mode does
 * not have busy state.
 */
ThumbnailMode.prototype.executeWhenReady = function(action) { action(); };

/**
 * @return {boolean} Always true. Toolbar is always visible.
 */
ThumbnailMode.prototype.hasActiveTool = function() { return true; };

/**
 * Handles key down event.
 * @param {!Event} event An event.
 * @return {boolean} True when an event is handled.
 *
 * TODO(yawano): Support up and down key.
 * TODO(yawano): Move not-mode-specific logics to ThumbnailView.
 */
ThumbnailMode.prototype.onKeyDown = function(event) {
  switch (event.keyIdentifier) {
    case 'Right':
      if (isRTL())
        this.thumbnailView_.selectPreviousItem();
      else
        this.thumbnailView_.selectNextItem();
      return true;
    case 'Left':
      if (isRTL())
        this.thumbnailView_.selectNextItem();
      else
        this.thumbnailView_.selectPreviousItem();
      return true;
    case 'Enter':
      this.changeToSlideModeCallback_();
      return true;
  }

  return false;
};

/**
 * Handles thumbnail double click event of Thumbnail View.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailMode.prototype.onThumbnailDoubleClick_ = function(event) {
  this.changeToSlideModeCallback_();
};

/**
 * Shows thumbnail view.
 */
ThumbnailMode.prototype.show = function() {
  this.thumbnailView_.show();
};

/**
 * Hides thumbnail view.
 */
ThumbnailMode.prototype.hide = function() {
  this.thumbnailView_.hide();
};

/**
 * Performs thumbnail mode enter animation.
 * @param {number} index Selected thumbnail index.
 * @param {!ImageRect} rect A rect from which the transformation starts.
 */
ThumbnailMode.prototype.performEnterAnimation = function(index, rect) {
  this.thumbnailView_.performEnterAnimation(index, rect);
};

/**
 * Returns thumbnail rect of the index.
 * @param {number} index An index of thumbnail.
 * @return {!ClientRect} A rect of thumbnail.
 */
ThumbnailMode.prototype.getThumbnailRect = function(index) {
  return this.thumbnailView_.getThumbnailRect(index);
};

/**
 * Thumbnail view.
 * @param {!HTMLElement} container A container.
 * @param {!GalleryDataModel} dataModel Gallery data model.
 * @param {!cr.ui.ListSelectionModel} selectionModel List selection model.
 * @constructor
 * @extends {cr.EventTarget}
 * @struct
 *
 * TODO(yawano): Implement multi selection.
 * TODO(yawano): Optimization. Remove DOMs outside of viewport, reuse them.
 * TODO(yawano): Extract ThumbnailView as a polymer element.
 */
function ThumbnailView(container, dataModel, selectionModel) {
  cr.EventTarget.call(this);

  /**
   * @private {!HTMLElement}
   */
  this.container_ = container;

  /**
   * @private {!GalleryDataModel}
   */
  this.dataModel_ = dataModel;

  /**
   * @private {!cr.ui.ListSelectionModel}
   */
  this.selectionModel_ = selectionModel;

  /**
   * @private {!Object}
   */
  this.thumbnails_ = {};

  /**
   * @private {boolean}
   */
  this.scrolling_ = false;

  /**
   * @private {number}
   */
  this.initialScreenY_ = 0;

  /**
   * @private {number}
   */
  this.initialScrollTop_ = 0;

  /**
   * @private {number}
   */
  this.scrollbarTimeoutId_ = 0;

  /**
   * @private {!HTMLElement}
   */
  this.list_ = assertInstanceof(document.createElement('ul'), HTMLElement);
  this.container_.appendChild(this.list_);

  /**
   * @private {!HTMLElement}
   */
  this.scrollbar_ = assertInstanceof(
      document.createElement('div'), HTMLElement);
  this.scrollbar_.classList.add('scrollbar');

  /**
   * @private {!HTMLElement}
   */
  this.scrollbarThumb_ = assertInstanceof(
      document.createElement('div'), HTMLElement);
  this.scrollbarThumb_.classList.add('thumb');
  this.scrollbar_.appendChild(this.scrollbarThumb_);
  this.container_.appendChild(this.scrollbar_);

  /**
   * @private {!HTMLElement}
   */
  this.animationThumbnail_ = assertInstanceof(
      document.createElement('div'), HTMLElement);
  this.animationThumbnail_.classList.add('animation-thumbnail');
  this.container_.appendChild(this.animationThumbnail_);

  this.container_.addEventListener('scroll', this.onScroll_.bind(this));
  this.container_.addEventListener('click', this.onClick_.bind(this));
  this.container_.addEventListener('dblclick', this.onDblClick_.bind(this));

  this.scrollbarThumb_.addEventListener(
      'mousedown', this.onScrollbarThumbMouseDown_.bind(this));
  window.addEventListener('mousemove', this.onWindowMouseMove_.bind(this));
  window.addEventListener('mouseup', this.onWindowMouseUp_.bind(this));

  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  this.dataModel_.addEventListener('content', this.onContent_.bind(this));
  this.selectionModel_.addEventListener(
      'change', this.onSelectionChange_.bind(this));
}

ThumbnailView.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Row height.
 * @const {number}
 *
 * TODO(yawano): Change so that Gallery adjust row height depending on image
 *     collection and window size to cover viewport as much as possible.
 */
ThumbnailView.ROW_HEIGHT = 160; // px

/**
 * Margins between thumbnails. This should be synced with CSS.
 * @const {number}
 */
ThumbnailView.MARGIN = 4; // px

/**
 * Timeout to fade out scrollbar.
 * @const {number}
 */
ThumbnailView.SCROLLBAR_TIMEOUT = 1500; // ms

/**
 * Shows thumbnail view.
 */
ThumbnailView.prototype.show = function() {
  this.container_.hidden = false;
};

/**
 * Hides thumbnail view.
 */
ThumbnailView.prototype.hide = function() {
  this.container_.hidden = true;
};

/**
 * Handles scroll bar thumb mouse down event.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onScrollbarThumbMouseDown_ = function(event) {
  this.scrolling_ = true;
  this.initialScreenY_ = event.screenY;
  this.initialScrollTop_ = this.container_.scrollTop;
};

/**
 * Handles mouse move event of window.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onWindowMouseMove_ = function(event) {
  if (this.scrolling_) {
    var diff = event.screenY - this.initialScreenY_;
    var scrollTop = this.initialScrollTop_ +
        (diff * this.container_.scrollHeight / this.scrollbar_.clientHeight);
    this.container_.scrollTop = scrollTop;
  }

  this.resetTimerOfScrollbar_();
};

/**
 * Handles mouse up event of window.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onWindowMouseUp_ = function(event) {
  this.scrolling_ = false;
  this.resetTimerOfScrollbar_();
};

/**
 * Handles scroll of viewport.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onScroll_ = function(event) {
  this.updateScrollBar_();
};

/**
 * Selects next item to the current selection.
 */
ThumbnailView.prototype.selectNextItem = function() {
  var index = this.selectionModel_.selectedIndex;
  if (index + 1 < this.dataModel_.length) {
    this.selectionModel_.selectedIndex = index + 1;
    this.scrollTo_(index + 1);
  }
};

/**
 * Selects previous item to the current selection.
 */
ThumbnailView.prototype.selectPreviousItem = function() {
  var index = this.selectionModel_.selectedIndex;
  if (index - 1 >= 0) {
    this.selectionModel_.selectedIndex = index - 1;
    this.scrollTo_(index - 1);
  }
};

/**
 * Scrolls viewport to show the thumbnail of the index.
 * @param {number} index Index of a thumbnail which becomes to appear in the
 *     viewport.
 * @private
 *
 * TODO(yawano): Add scroll animation.
 */
ThumbnailView.prototype.scrollTo_ = function(index) {
  var thumbnailRect = this.getThumbnailRect(index);

  if (thumbnailRect.top - ThumbnailView.MARGIN < ImageEditor.Toolbar.HEIGHT) {
    this.container_.scrollTop -=
        ImageEditor.Toolbar.HEIGHT - thumbnailRect.top + ThumbnailView.MARGIN;
  } else if (thumbnailRect.bottom + ThumbnailView.MARGIN >
      this.container_.clientHeight) {
    this.container_.scrollTop += thumbnailRect.bottom + ThumbnailView.MARGIN -
        this.container_.clientHeight;
  }
};

/**
 * Updates scroll bar.
 * @private
 */
ThumbnailView.prototype.updateScrollBar_ = function() {
  var scrollTop = this.container_.scrollTop;
  var scrollHeight = this.container_.scrollHeight;
  var clientHeight = this.container_.clientHeight;

  // If viewport is not long enough to scroll, do not show scrollbar.
  if (scrollHeight <= clientHeight) {
    this.scrollbar_.hidden = true;
    return;
  }

  this.scrollbar_.hidden = false;

  var thumbHeight =
      ~~(this.scrollbar_.clientHeight * clientHeight / scrollHeight);
  var thumbTop = ~~(scrollTop * this.scrollbar_.clientHeight / scrollHeight);

  this.scrollbarThumb_.style.height = thumbHeight + 'px';
  this.scrollbarThumb_.style.marginTop = thumbTop + 'px';

  this.resetTimerOfScrollbar_();
};

/**
 * Resets timer to fade out scrollbar. If scrollbar is already faded-out, this
 * method makes it visible and set timeout. If user is scrolling, this method
 * just clears existing timer.
 * @private
 */
ThumbnailView.prototype.resetTimerOfScrollbar_ = function() {
  this.scrollbar_.classList.toggle('transparent', false);

  if (this.scrollbarTimeoutId_) {
    clearTimeout(this.scrollbarTimeoutId_);
    this.scrollbarTimeoutId_ = 0;
  }

  // If user is scrolling, do not set timeout.
  if (this.scrolling_)
    return;

  this.scrollbarTimeoutId_ = setTimeout(function() {
    this.scrollbarTimeoutId_ = 0;
    this.scrollbar_.classList.toggle('transparent', true);
  }.bind(this), ThumbnailView.SCROLLBAR_TIMEOUT);
};

/**
 * Handles splice event of data model.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onSplice_ = function(event) {
  if (event.removed) {
    for (var i = 0; i < event.removed.length; i++) {
      this.remove_(event.removed[i]);
    }
  }

  if (event.added && event.added.length > 0) {
    // Get a thumbnail before which new thumbnail is inserted.
    var insertBefore = null;
    var galleryItem = this.dataModel_.item(event.index + event.added.length);
    if (galleryItem)
      insertBefore = this.thumbnails_[galleryItem.getEntry().toURL()];

    for (var i = 0; i < event.added.length; i++) {
      this.insert_(event.added[i], insertBefore);
    }
  }
};

/**
 * Handles content event of data model.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onContent_ = function(event) {
  var galleryItem = event.item;
  var thumbnail = this.thumbnails_[galleryItem.getEntry().toURL()];
  if (thumbnail)
    thumbnail.update();
};

/**
 * Handles selection change event.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onSelectionChange_ = function(event) {
  var changes = event.changes;

  for (var i = 0; i < changes.length; i++) {
    var change = changes[i];

    var galleryItem = this.dataModel_.item(change.index);
    if (!galleryItem)
      continue;

    var thumbnail = this.thumbnails_[galleryItem.getEntry().toURL()];
    if (!thumbnail)
      continue;

    thumbnail.setSelected(change.selected);
  }
};

/**
 * Handles click event.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onClick_ = function(event) {
  var target = event.target;
  if (target.matches('.selection.frame'))
    this.selectBySelectionFrame_(/** @type {!HTMLElement} */ (target));
};

/**
 * Handles double click event.
 * @param {!Event} event An event.
 * @private
 */
ThumbnailView.prototype.onDblClick_ = function(event) {
  var target = event.target;
  if (target.matches('.selection.frame')) {
    this.selectBySelectionFrame_(/** @type{!HTMLElement} */ (target));
    var thumbnailDoubleClickEvent = new Event('thumbnail-double-click');
    this.dispatchEvent(thumbnailDoubleClickEvent);
  }
};

/**
 * Selects a thumbnail by selection frame.
 * @param {!HTMLElement} selectionFrame Selection frame.
 * @private
 */
ThumbnailView.prototype.selectBySelectionFrame_ = function(selectionFrame) {
  var thumbnailContainer = selectionFrame.parentNode;

  // Unselect all other items.
  this.selectionModel_.unselectAll();

  var thumbnail = thumbnailContainer.getThumbnail();
  var index = this.dataModel_.indexOf(thumbnail.getGalleryItem());
  this.selectionModel_.setIndexSelected(index, true);
};

/**
 * Inserts an item.
 * @param {!Gallery.Item} galleryItem A gallery item.
 * @param {!ThumbnailView.Thumbnail} insertBefore A thumbnail before which new
 *     thumbnail is inserted. Set null for adding at the end of the list.
 * @private
 */
ThumbnailView.prototype.insert_ = function(galleryItem, insertBefore) {
  var thumbnail = new ThumbnailView.Thumbnail(galleryItem);
  this.thumbnails_[galleryItem.getEntry().toURL()] = thumbnail;
  if (insertBefore) {
    this.list_.insertBefore(
        thumbnail.getContainer(), insertBefore.getContainer());
  } else {
    this.list_.appendChild(thumbnail.getContainer());
  }

  // Set selection state.
  var index = this.dataModel_.indexOf(galleryItem);
  thumbnail.setSelected(this.selectionModel_.getIndexSelected(index));

  this.updateScrollBar_();
};

/**
 * Removes an item.
 * @param {!Gallery.Item} galleryItem A gallery item.
 * @private
 */
ThumbnailView.prototype.remove_ = function(galleryItem) {
  var thumbnail = this.thumbnails_[galleryItem.getEntry().toURL()];
  this.list_.removeChild(thumbnail.getContainer());
  delete this.thumbnails_[galleryItem.getEntry().toURL()];
};

/**
 * Returns thumbnail instance at specified index.
 * @param {number} index Index of the thumbnail.
 * @return {!ThumbnailView.Thumbnail} Thumbnail at the index.
 * @private
 */
ThumbnailView.prototype.getThumbnailAt_ = function(index) {
  var galleryItem = this.dataModel_.item(index);
  return this.thumbnails_[galleryItem.getEntry().toURL()];
};

/**
 * Returns a rect of the specified thumbnail.
 * @param {number} index An index of the thumbnail.
 * @return {!ClientRect} Rect of the thumbnail.
 */
ThumbnailView.prototype.getThumbnailRect = function(index) {
  var thumbnail = this.getThumbnailAt_(index);
  return thumbnail.getContainer().getBoundingClientRect();
};

/**
 * Performs enter animation.
 * @param {number} index Index of the thumbnail which is animated.
 * @param {!ImageRect} rect A rect from which the transformation starts.
 *
 * TODO(yawano): Consider to move this logic to thumbnail mode.
 */
ThumbnailView.prototype.performEnterAnimation = function(index, rect) {
  this.scrollTo_(index);
  this.updateScrollBar_();

  var thumbnailRect = this.getThumbnailRect(index);
  var thumbnail = this.getThumbnailAt_(index);

  // If thumbnail is not loaded yet, do not perform animation.
  if (!thumbnail.getBackgroundImage())
    return;

  // Hide animating thumbnail.
  thumbnail.setTransparent(true);

  this.animationThumbnail_.style.backgroundImage =
      thumbnail.getBackgroundImage();
  this.animationThumbnail_.classList.add('animating');
  this.animationThumbnail_.width = thumbnail.getWidth();
  this.animationThumbnail_.height = ThumbnailView.ROW_HEIGHT;

  var animationPlayer = this.animationThumbnail_.animate([{
    height: rect.height,
    left: rect.left,
    top: rect.top,
    width: rect.width,
    offset: 0,
    easing: 'linear'
  }, {
    height: thumbnailRect.height,
    left: thumbnailRect.left,
    top: thumbnailRect.top,
    width: thumbnailRect.width,
    offset: 1
  }], 250);

  animationPlayer.addEventListener('finish', function() {
    this.animationThumbnail_.classList.remove('animating');
    thumbnail.setTransparent(false);
  }.bind(this));
};

/**
 * Thumbnail.
 * @param {!Gallery.Item} galleryItem A gallery item.
 * @constructor
 * @struct
 */
ThumbnailView.Thumbnail = function(galleryItem) {
  /**
   * @private {!Gallery.Item}
   */
  this.galleryItem_ = galleryItem;

  /**
   * @private {boolean}
   */
  this.selected_ = false;

  /**
   * @private {ThumbnailLoader}
   */
  this.thumbnailLoader_ = null;

  /**
   * @private {number}
   */
  this.thumbnailLoadRequestId_ = 0;

  /**
   * @private {number}
   */
  this.width_ = 0;

  /**
   * @private {!HTMLElement}
   */
  this.container_ = assertInstanceof(document.createElement('li'), HTMLElement);
  this.container_.classList.add('thumbnail');
  this.container_.setAttribute('aria-label', this.galleryItem_.getFileName());

  /**
   * @private {!HTMLElement}
   */
  this.imageFrame_ = assertInstanceof(
      document.createElement('div'), HTMLElement);
  this.imageFrame_.classList.add('image', 'frame');
  this.container_.appendChild(this.imageFrame_);

  /**
   * @private {!HTMLElement}
   */
  this.selectionFrame_ = assertInstanceof(
      document.createElement('div'), HTMLElement);
  this.selectionFrame_.classList.add('selection', 'frame');
  this.container_.appendChild(this.selectionFrame_);

  this.container_.style.height = ThumbnailView.ROW_HEIGHT + 'px';
  this.container_.getThumbnail =
      function(thumbnail) { return thumbnail; }.bind(null, this);

  this.update();
};

/**
 * Returns a gallery item.
 * @return {!Gallery.Item} A gallery item.
 */
ThumbnailView.Thumbnail.prototype.getGalleryItem = function() {
  return this.galleryItem_;
};

/**
 * Change selection state of this thumbnail.
 * @param {boolean} selected True to make this thumbnail selected.
 */
ThumbnailView.Thumbnail.prototype.setSelected = function(selected) {
  this.selected_ = selected;
  this.container_.classList.toggle('selected', selected);
};

/**
 * Returns a container.
 * @return {!HTMLElement} A container.
 */
ThumbnailView.Thumbnail.prototype.getContainer = function() {
  return this.container_;
};

/**
 * Sets this thumbnail as transparent.
 * @param {boolean} transparent True to make this thumbnail transparent.
 */
ThumbnailView.Thumbnail.prototype.setTransparent = function(transparent) {
  this.container_.classList.toggle('transparent', transparent);
};

/**
 * Returns width of this thumbnail.
 * @return {number} Width of this thumbnail.
 */
ThumbnailView.Thumbnail.prototype.getWidth = function() {
  return this.width_;
};

/**
 * Sets width of this thumbnail.
 * @param {number} width Width.
 * @private
 */
ThumbnailView.Thumbnail.prototype.setWidth_ = function(width) {
  if (this.width_ === width)
    return;

  this.width_ = width;
  this.container_.style.width = this.width_ + 'px';
};

/**
 * Returns background image style of this thumbnail.
 * @return {string} Background image.
 */
ThumbnailView.Thumbnail.prototype.getBackgroundImage = function() {
  return this.imageFrame_.style.backgroundImage;
};

/**
 * Updates thumbnail.
 */
ThumbnailView.Thumbnail.prototype.update = function() {
  // Calculate and set width.
  var metadata = this.galleryItem_.getMetadataItem();
  if (metadata) {
    var rotated = metadata.imageRotation % 2 === 1;
    var imageWidth = rotated ? metadata.imageHeight : metadata.imageWidth;
    var imageHeight = rotated ? metadata.imageWidth : metadata.imageHeight;
    this.setWidth_(~~(imageWidth * ThumbnailView.ROW_HEIGHT / imageHeight));
  } else {
    this.setWidth_(ThumbnailView.ROW_HEIGHT);
  }

  // Set thumbnail.
  var thumbnailMetadata = this.galleryItem_.getThumbnailMetadataItem();
  if (thumbnailMetadata) {
    this.thumbnailLoadRequestId_++;

    // TODO(yawano): Add error handling.
    this.thumbnailLoader_ = new ThumbnailLoader(
        this.galleryItem_.getEntry(), undefined, thumbnailMetadata);
    this.thumbnailLoader_.loadAsDataUrl(ThumbnailLoader.FillMode.FIT)
        .then(function(thumbnailLoadRequestId, result) {
          // Discard the result of old request.
          if (thumbnailLoadRequestId !== this.thumbnailLoadRequestId_)
            return;

          // Update width by using the widh of actual data.
          this.setWidth_(
              ~~(result.width * ThumbnailView.ROW_HEIGHT / result.height));

          this.imageFrame_.style.backgroundImage = 'url(' + result.data + ')';
        }.bind(this, this.thumbnailLoadRequestId_));
  }
};
