/*
 * Copyright 2017-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Agent.hxx"
#include "odbus/Error.hxx"
#include "odbus/Connection.hxx"

inline DBusHandlerResult
SystemdAgent::HandleMessage(gcc_unused DBusConnection *_connection,
			    DBusMessage *message)
{
	if (dbus_message_is_signal(message, "org.freedesktop.systemd1.Agent",
				   "Released") &&
	    dbus_message_has_signature(message, "s")) {
		DBusMessageIter iter;
		dbus_message_iter_init(message, &iter);

		const char *path;
		dbus_message_iter_get_basic(&iter, &path);

		callback(path);
		return DBUS_HANDLER_RESULT_HANDLED;
	} else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult
SystemdAgent::HandleMessage(DBusConnection *connection,
			    DBusMessage *message, void *user_data)
{
	auto &agent = *(SystemdAgent *)user_data;

	return agent.HandleMessage(connection, message);
}

SystemdAgent::SystemdAgent(Callback _callback)
	:callback(_callback)
{
}

SystemdAgent::~SystemdAgent()
{
	SetConnection({});
}

void
SystemdAgent::SetConnection(ODBus::Connection _connection)
{
	if (connection)
		dbus_connection_remove_filter(connection,
					      HandleMessage, this);

	connection = std::move(_connection);

	if (connection) {
		ODBus::Error error;
		const char *match = "type='signal',"
			"sender='org.freedesktop.systemd1',"
			"interface='org.freedesktop.systemd1.Agent',"
			"member='Released',"
			"path='/org/freedesktop/systemd1/agent'";
		dbus_bus_add_match(connection, match, error);

		try {
			error.CheckThrow("DBus AddMatch error");
		} catch (...) {
			connection = {};
			throw;
		}

		dbus_connection_add_filter(connection, HandleMessage, this,
					   nullptr);
	}
}
