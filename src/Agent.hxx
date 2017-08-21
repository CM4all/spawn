/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef AGENT_HXX
#define AGENT_HXX

#include "odbus/Connection.hxx"
#include "util/BindMethod.hxx"

class SystemdAgent {
	ODBus::Connection connection;

	typedef BoundMethod<void(const char *path)> Callback;
	const Callback callback;

public:
	explicit SystemdAgent(Callback _callback);
	~SystemdAgent();

	void SetConnection(ODBus::Connection _connection);

private:
	DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					DBusMessage *message);
	static DBusHandlerResult HandleMessage(DBusConnection *dbus_connection,
					       DBusMessage *message,
					       void *user_data);
};

#endif
