/*
 * author: Max Kellermann <mk@cm4all.com>
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
		error.CheckThrow("DBus AddMatch error");

		dbus_connection_add_filter(connection, HandleMessage, this,
					   nullptr);
	}
}
