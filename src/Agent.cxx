/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Agent.hxx"
#include "odbus/Error.hxx"
#include "odbus/Connection.hxx"
#include "util/StringCompare.hxx"

/**
 * These systemd scopes are allocated by our software which uses the
 * process spawner.  Their cgroups are managed by this daemon.
 */
static constexpr const char *managed_scopes[] = {
    "/system.slice/cm4all-beng-spawn.scope/",
    "/system.slice/cm4all-workshop-spawn.scope/",
    "/system.slice/cm4all-openssh.scope/",
};

static const char *
GetManagedSuffix(const char *path)
{
    for (const char *i : managed_scopes) {
        const char *suffix = StringAfterPrefix(path, i);
        if (suffix != nullptr)
            return suffix;
    }

    return nullptr;
}

static void
AgentCgroupReleased(const char *path)
{
    const char *suffix = GetManagedSuffix(path);
    if (suffix == nullptr)
        return;

    printf("Cgroup released: %s\n", path);

    // TODO: implement
}

static DBusHandlerResult
AgentHandleMessage(gcc_unused DBusConnection *connection, gcc_unused DBusMessage *message, gcc_unused void *user_data)
{
    if (dbus_message_is_signal(message, "org.freedesktop.systemd1.Agent",
                               "Released") &&
        dbus_message_has_signature(message, "s")) {
        DBusMessageIter iter;
        dbus_message_iter_init(message, &iter);

        const char *path;
        dbus_message_iter_get_basic(&iter, &path);

        AgentCgroupReleased(path);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
AgentInit()
{
    auto connection = ODBus::Connection::GetSystem();

    dbus_connection_add_filter(connection, AgentHandleMessage, nullptr,
                               nullptr);

    ODBus::Error error;
    const char *match = "type='signal',"
        "sender='org.freedesktop.systemd1',"
        "interface='org.freedesktop.systemd1.Agent',"
        "member='Released',"
        "path='/org/freedesktop/systemd1/agent'";
    dbus_bus_add_match(connection, match, error);
    error.CheckThrow("DBus AddMatch error");
}
