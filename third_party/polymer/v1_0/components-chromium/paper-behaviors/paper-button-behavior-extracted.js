/** @polymerBehavior Polymer.PaperButtonBehavior */
  Polymer.PaperButtonBehaviorImpl = {

    properties: {

      /**
       * The z-depth of this element, from 0-5. Setting to 0 will remove the
       * shadow, and each increasing number greater than 0 will be "deeper"
       * than the last.
       *
       * @attribute elevation
       * @type number
       * @default 1
       */
      elevation: {
        type: Number,
        reflectToAttribute: true,
        readOnly: true
      }

    },

    observers: [
      '_calculateElevation(focused, disabled, active, pressed, receivedFocusFromKeyboard)',
      '_computeKeyboardClass(receivedFocusFromKeyboard)'
    ],

    hostAttributes: {
      role: 'button',
      tabindex: '0',
      animated: true
    },

    _calculateElevation: function() {
      var e = 1;
      if (this.disabled) {
        e = 0;
      } else if (this.active || this.pressed) {
        e = 4;
      } else if (this.receivedFocusFromKeyboard) {
        e = 3;
      }
      this._setElevation(e);
    },

    _computeKeyboardClass: function(receivedFocusFromKeyboard) {
      this.classList.toggle('keyboard-focus', receivedFocusFromKeyboard);
    },

    /**
     * In addition to `IronButtonState` behavior, when space key goes down, 
     * create a ripple down effect.
     */
    _spaceKeyDownHandler: function(event) {
      Polymer.IronButtonStateImpl._spaceKeyDownHandler.call(this, event);
      if (this.hasRipple()) {
        this._ripple.uiDownAction();
      }
    },

    /**
     * In addition to `IronButtonState` behavior, when space key goes up, 
     * create a ripple up effect.
     */
    _spaceKeyUpHandler: function(event) {
      Polymer.IronButtonStateImpl._spaceKeyUpHandler.call(this, event);
      if (this.hasRipple()) {
        this._ripple.uiUpAction();
      }
    }

  };

  /** @polymerBehavior */
  Polymer.PaperButtonBehavior = [
    Polymer.IronButtonState,
    Polymer.IronControlState,
    Polymer.PaperRippleBehavior,
    Polymer.PaperButtonBehaviorImpl
  ];