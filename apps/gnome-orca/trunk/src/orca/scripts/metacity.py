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

"""Custom script for metacity."""

__id__        = "$Id: metacity.py 2821 2007-09-12 14:28:07Z wwalker $"
__version__   = "$Revision: 2821 $"
__date__      = "$Date: 2007-09-12 10:28:07 -0400 (Wed, 12 Sep 2007) $"
__copyright__ = "Copyright (c) 2005-2007 Sun Microsystems Inc."
__license__   = "LGPL"

import orca.braille as braille
import orca.default as default
import orca.rolenames as rolenames
import orca.orca as orca
import orca.speech as speech

from orca.orca_i18n import _

########################################################################
#                                                                      #
# The Metacity script class.                                           #
#                                                                      #
########################################################################

class Script(default.Script):

    def __init__(self, app):
        """Creates a new script for the given application.

        Arguments:
        - app: the application to create a script for.
        """

        default.Script.__init__(self, app)

    def presentStatusBar(self, obj):
        """Presents information about the metacity status bar."""

        # We have to stop speech, as Metacity has a key grab and we're not
        # getting keys
        #
        speech.stop()

        # Do we know about this window?  Traverse through our list of apps
        # and go through the toplevel windows in each to see if we know
        # about this one.  If we do, it's accessible.  If we don't, it is
        # not.
        #
        found = False
        for app in self.getKnownApplications():
            i = 0
            while i < app.childCount:
                win = app.child(i)
                if win is None:
                    print "app error " + app.name
                elif win.name == obj.name:
                    found = True
                i = i + 1

        text = obj.name

        # Translators: the "Workspace " and "Desk " strings are
        # the prefix of what metacity shows when you press
        # Ctrl+Alt and the left or right arrow keys to switch
        # between workspaces.  The goal here is to find a match
        # with that prefix.
        #
        if text.startswith(_("Workspace ")) or text.startswith(_("Desk ")):
            pass
        elif not found:
            # Translators: inaccessible means that the application cannot
            # be read by Orca.  This usually means the application is not
            # friendly to the assistive technology infrastructure.
            #
            text += ". " + _("inaccessible")

        braille.displayMessage(text)
        speech.speak(text)

    def onNameChanged(self, event):
        """The status bar in metacity tells us what toplevel window
        will be activated when tab is released.  We will key off the
        text inserted event to determine when to say something, as it
        seems to be the more reliable event.

        Arguments:
        - event: the name changed Event
        """

        # Ignore changes on the status bar.  We handle them in onTextInserted.
        #
        if event.source.role != rolenames.ROLE_STATUSBAR:
            default.Script.onNameChanged(self, event)

    def onStateChanged(self, event):
        """The status bar in metacity tells us what toplevel window
        will be activated when tab is released.  We will key off the
        text inserted event to determine when to say something, as it
        seems to be the more reliable event.

        Arguments:
        - event: the object:state-changed: Event
        """

        # Ignore changes on the status bar.  We handle them in
        # onTextInserted.  The only exception is if the status bar is
        # suddenly showing.  Then, we want to present it because we
        # typically do not get onTextInserted events at that time.
        #
        if event.source.role != rolenames.ROLE_STATUSBAR:
            default.Script.onStateChanged(self, event)
        elif (event.type.startswith("object:state-changed:showing")) \
            and event.detail1:
            self.presentStatusBar(event.source)

    def onTextInserted(self, event):
        """Called whenever text is inserted into an object.  This seems to
        be the most reliable event to let us know something is changing.

        Arguments:
        - event: the Event
        """

        if event.source.role != rolenames.ROLE_STATUSBAR:
            default.Script.onTextInserted(self, event)

        self.presentStatusBar(event.source)

    def onTextDeleted(self, event):
        """Called whenever text is deleted from an object.

        Arguments:
        - event: the Event
        """

        # Ignore changes on the status bar.  We handle them in onTextInserted.
        #
        if event.source.role != rolenames.ROLE_STATUSBAR:
            default.Script.onTextDeleted(self, event)

    def onCaretMoved(self, event):
        """Called whenever the caret moves.

        Arguments:
        - event: the Event
        """

        # Ignore changes on the status bar.  We handle them in onTextInserted.
        #
        if event.source.role != rolenames.ROLE_STATUSBAR:
            default.Script.onCaretMoved(self, event)
