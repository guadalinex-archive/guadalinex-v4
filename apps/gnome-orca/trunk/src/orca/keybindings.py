# Orca
#
# Copyright 2005-2007 Sun Microsystems Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

"""Provides support for defining keybindings and matching them to input
events."""

__id__        = "$Id: keybindings.py 2351 2007-05-07 21:56:41Z wwalker $"
__version__   = "$Revision: 2351 $"
__date__      = "$Date: 2007-05-07 17:56:41 -0400 (Mon, 07 May 2007) $"
__copyright__ = "Copyright (c) 2005-2007 Sun Microsystems Inc."
__license__   = "LGPL"

try:
    # This can fail due to gtk not being available.  We want to
    # be able to recover from that if possible.  The main driver
    # for this is to allow "orca --text-setup" to work even if
    # the desktop is not running.
    #
    import gtk
except:
    pass

import atspi
import debug
import settings

from orca_i18n import _           # for gettext support

_keysymsCache = {}
_keycodeCache = {}

def getAllKeysyms(keysym):
    """Given a keysym, find all other keysyms associated with the key
    that is mapped to the given keysym.  This allows us, for example,
    to determine that the key bound to KP_Insert is also bound to KP_0."""

    if not _keysymsCache.has_key(keysym):
        # The keysym itself is always part of the list.
        #
        _keysymsCache[keysym] = [keysym]

        # Find the numerical value of the keysym
        #
        keyval = gtk.gdk.keyval_from_name(keysym)

        if keyval != 0:
            # Find the keycodes for the keysym.  Since a keysym
            # can be associated with more than one key, we'll shoot
            # for the keysym that's in group 0, regardless of shift
            # level (each entry is of the form [keycode, group,
            # level]).
            #
            keymap = gtk.gdk.keymap_get_default()
            entries = keymap.get_entries_for_keyval(keyval)
            keycode = 0
            if entries:
                for entry in entries:
                    if entry[1] == 0:  # group = 0
                        keycode = entry[0]
                        break

            # Find the keysyms bound to the keycode.  These are what
            # we are looking for.
            #
            if keycode != 0:
                entries = keymap.get_entries_for_keycode(keycode)
                if entries:
                    for entry in entries:
                        keyval = entry[0]
                        name = gtk.gdk.keyval_name(keyval)
                        if name and (name != keysym):
                            _keysymsCache[keysym].append(name)

    return _keysymsCache[keysym]

def getKeycode(keysym):
    """Converts an XKeysym string (e.g., 'KP_Enter') to a keycode that
    should match the event.hw_code for key events.

    This whole situation is caused by the fact that Solaris chooses
    to give us different keycodes for the same key, and the keypad
    is the primary place where this happens: if NumLock is not on,
    there is no telling the difference between keypad keys and the
    other navigation keys (e.g., arrows, page up/down, etc.).  One,
    for example, would expect to get KP_End for the '1' key on the
    keypad if NumLock were not on.  Instead, we get 'End' and the
    keycode for it matches the keycode for the other 'End' key.  Odd.
    If NumLock is on, we at least get KP_* keys.

    So...when setting up keybindings, we say we're interested in
    KeySyms, but those keysyms are carefully chosen so as to result
    in a keycode that matches the actual key on the keyboard.  This
    is why we use KP_1 instead of KP_End and so on in our keybindings.

    Arguments:
    - keysym: a string that is a valid representation of an XKeysym.

    Returns an integer representing a key code that should match the
    event.hw_code for key events.
    """
    if not _keycodeCache.has_key(keysym):
        keymap = gtk.gdk.keymap_get_default()

        # Find the numerical value of the keysym
        #
        keyval = gtk.gdk.keyval_from_name(keysym)
        if keyval == 0:
            return 0

        # Now find the keycodes for the keysym.   Since a keysym can
        # be associated with more than one key, we'll shoot for the
        # keysym that's in group 0, regardless of shift level (each
        # entry is of the form [keycode, group, level]).
        #
        _keycodeCache[keysym] = 0
        entries = keymap.get_entries_for_keyval(keyval)
        if entries:
            for entry in entries:
                if entry[1] == 0:  # group = 0
                    _keycodeCache[keysym] = entry[0]
                    break
                if _keycodeCache[keysym] == 0:
                    _keycodeCache[keysym] = entries[0][0]

        #print keysym, keyval, entries, _keycodeCache[keysym]

    return _keycodeCache[keysym]

def getModifierNames(mods):
    """Gets the modifier names of a numeric modifier mask as a human
    consumable string.
    """

    text = ""
    if mods & (1 << settings.MODIFIER_ORCA):
        text += _("Orca") + "+"
    #if mods & (1 << atspi.Accessibility.MODIFIER_NUMLOCK):
    #    text += _("Num_Lock") + "+"
    if mods & 128:
        # Translators: this is presented in a GUI to represent the
        # "right alt" modifier.
        #
        text += _("Alt_R") + "+"
    if mods & (1 << atspi.Accessibility.MODIFIER_META3):
        # Translators: this is presented in a GUI to represent the
        # "super" modifier.
        #
        text += _("Super") + "+"
    if mods & (1 << atspi.Accessibility.MODIFIER_META2):
        # Translators: this is presented in a GUI to represent the
        # "meta 2" modifier.
        #
        text += _("Meta2") + "+"
    #if mods & (1 << atspi.Accessibility.MODIFIER_META):
    #    text += _("Meta") + "+"
    if mods & (1 << atspi.Accessibility.MODIFIER_ALT):
        # Translators: this is presented in a GUI to represent the
        # "left alt" modifier.
        #
        text += _("Alt_L") + "+"
    if mods & (1 << atspi.Accessibility.MODIFIER_CONTROL):
        # Translators: this is presented in a GUI to represent the
        # "control" modifier.
        #
        text += _("Ctrl") + "+"
    if mods & (1 << atspi.Accessibility.MODIFIER_SHIFTLOCK):
        # Translators: this is presented in a GUI to represent the
        # "caps lock" modifier.
        #
        text += _("Caps_Lock") + "+"
    if mods & (1 << atspi.Accessibility.MODIFIER_SHIFT):
        # Translators: this is presented in a GUI to represent the
        # "shift " modifier.
        #
        text += _("Shift") + "+"
    return text

class KeyBinding:
    """A single key binding, consisting of a keycode, a modifier mask,
    and the InputEventHandler.
    """

    def __init__(self, keysymstring, modifier_mask, modifiers, handler):
        """Creates a new key binding.

        Arguments:
        - keysymstring: the keysymstring - this is typically a string
          from /usr/include/X11/keysymdef.h with the preceding 'XK_'
          removed (e.g., XK_KP_Enter becomes the string 'KP_Enter').
        - modifier_mask: bit mask where a set bit tells us what modifiers
          we care about (see atspi.Accessibility.MODIFIER_*)
        - modifiers: the state the modifiers we care about must be in for
          this key binding to match an input event (see also
          atspi.Accessibility.MODIFIER_*)
        - handler: the InputEventHandler for this key binding
        """

        self.keysymstring = keysymstring
        self.modifier_mask = modifier_mask
        self.modifiers = modifiers
        self.handler = handler
        self.keycode = None

    def matches(self, keycode, modifiers):
        """Returns true if this key binding matches the given keycode and
        modifier state.
        """

        # We lazily bind the keycode.  The primary reason for doing this
        # is so that atspi does not have to be initialized before setting
        # keybindings in the user's preferences file.
        #
        if not self.keycode:
            self.keycode = getKeycode(self.keysymstring)

        if self.keycode == keycode:
            result = modifiers & self.modifier_mask
            return result == self.modifiers
        else:
            return False

class KeyBindings:
    """Structure that maintains a set of KeyBinding instances.
    """

    def __init__(self):
        self.keyBindings = []

    def __str__(self):
        result = "[\n"
        for keyBinding in self.keyBindings:
            result += "  [%x %x %s %s]\n" % (keyBinding.modifier_mask,
                                             keyBinding.modifiers,
                                             keyBinding.keysymstring,
                                             keyBinding.handler._description)
        result += "]"
        return result
    
    def add(self, keyBinding):
        """Adds the given KeyBinding instance to this set of keybindings.
        """

        self.keyBindings.append(keyBinding)

    def remove(self, keyBinding):
        """Removes the given KeyBinding instance from this set of keybindings.
        """

        for i in range(0, len(self.keyBindings)):
            if keyBinding == self.keyBindings[i]:
                del self.keyBindings[i]

    def removeByHandler(self, handler):
        """Removes the given KeyBinding instance from this set of keybindings.
        """
        i = len(self.keyBindings)
        while i > 0:
            if self.keyBindings[i - 1].handler == handler:
                del self.keyBindings[i - 1]
            i = i - 1

    def hasKeyBinding (self, newKeyBinding, typeOfSearch="strict"):
        """Return True if keyBinding is already in self.keyBindings.

           The typeOfSearch can be:
              "strict":      matches description, modifiers, key
              "description": matches only description.
              "keys":        matches only modifiers and key.
        """

        hasIt = False

        for keyBinding in self.keyBindings:
            if typeOfSearch == "strict":
                if (keyBinding.handler._description \
                    == newKeyBinding.handler._description) \
                    and (keyBinding.keysymstring \
                         == newKeyBinding.keysymstring) \
                    and (keyBinding.modifier_mask \
                         == newKeyBinding.modifier_mask) \
                    and (keyBinding.modifiers \
                         == newKeyBinding.modifiers):
                    hasIt = True
            elif typeOfSearch == "description":
                if keyBinding.handler._description \
                    == newKeyBinding.handler._description:
                    hasIt = True
            elif typeOfSearch == "keys":
                if (keyBinding.keysymstring \
                    == newKeyBinding.keysymstring) \
                    and (keyBinding.modifier_mask \
                         == newKeyBinding.modifier_mask) \
                    and (keyBinding.modifiers \
                         == newKeyBinding.modifiers):
                    hasIt = True

        return hasIt

    def getInputHandler(self, keyboardEvent):
        """Returns the input handler of the key binding that matches the
        given keycode and modifiers, or None if no match exists.
        """
        handler = None
        for keyBinding in self.keyBindings:
            if keyBinding.matches(keyboardEvent.hw_code, \
                                  keyboardEvent.modifiers):
                handler = keyBinding.handler
                break
        return handler

    def consumeKeyboardEvent(self, script, keyboardEvent):
        """Attempts to consume the given keyboard event.  If these
        keybindings have a handler for the given keyboardEvent, it is
        assumed the event will always be consumed.
        """

        consumed = False
        handler = self.getInputHandler(keyboardEvent)
        if handler:
            consumed = True
            if keyboardEvent.type == atspi.Accessibility.KEY_PRESSED_EVENT:
                try:
                    handler.processInputEvent(script, keyboardEvent)
                except:
                    debug.printException(debug.LEVEL_SEVERE)

        return consumed
