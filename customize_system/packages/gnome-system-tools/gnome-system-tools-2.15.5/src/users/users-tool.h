/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* main.c: this file is part of users-admin, a ximian-setup-tool frontend 
 * for user administration.
 * 
 * Copyright (C) 2005 Carlos Garnacho
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Carlos Garnacho Parro <carlosg@gnome.org>
 */

#ifndef __USERS_TOOL_H__
#define __USERS_TOOL_H__

G_BEGIN_DECLS

#include "gst-tool.h"
#include "user-profiles.h"

#define GST_TYPE_USERS_TOOL            (gst_users_tool_get_type ())
#define GST_USERS_TOOL(obj)            (GTK_CHECK_CAST ((obj), GST_TYPE_USERS_TOOL, GstUsersTool))
#define GST_USERS_TOOL_CLASS(class)    (GTK_CHECK_CLASS_CAST ((class), GST_TYPE_USERS_TOOL, GstUsersToolClass))
#define GST_IS_USERS_TOOL(obj)         (GTK_CHECK_TYPE ((obj), GST_TYPE_USERS_TOOL))
#define GST_IS_USERS_TOOL_CLASS(class) (GTK_CHECK_CLASS_TYPE ((class), GST_TYPE_USERS_TOOL))

typedef struct _GstUsersTool      GstUsersTool;
typedef struct _GstUsersToolClass GstUsersToolClass;

struct _GstUsersTool {
	GstTool tool;

	GstUserProfiles *profiles;

	OobsObject *users_config;
	OobsObject *groups_config;
	gint minimum_uid;
	gint maximum_uid;
	gint minimum_gid;
	gint maximum_gid;

	gboolean showall;
};

struct _GstUsersToolClass {
	GstToolClass parent_class;
};

GType    gst_users_tool_get_type           (void);

GstTool *gst_users_tool_new                (void);


G_END_DECLS

#endif /* __USERS_TOOL_H__ */
