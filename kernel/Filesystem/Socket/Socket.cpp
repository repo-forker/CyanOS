#include "Socket.h"

Socket::Socket(const StringView& name) :
    FSNode{name, 0, 0, NodeType::Socket, 0},
    m_connections{},
    m_server_wait_queue{},
    m_connections_wait_queue{},
    m_lock{}
{
}

Socket::~Socket() {}

Result<void> Socket::open(FileDescription&)
{
	return ResultError(ERROR_SUCCESS);
}

bool Socket::can_accept()
{
	return false;
}

Result<FSNode&> Socket::accept()
{
	ScopedLock local_lock(m_lock);

	m_server_wait_queue.wait_on_event([&]() { return !m_pending_connections.size(); }, local_lock);

	Connection& new_connection = m_connections.emplace_back("");
	m_pending_connections.first().is_accepted = true;
	m_pending_connections.last().connection = &new_connection;
	m_connections_wait_queue.wake_up();

	return new_connection;
}

Result<FSNode&> Socket::connect()
{
	ScopedLock local_lock(m_lock);

	NewPendingConnection& pending_connection = m_pending_connections.emplace_back();
	m_server_wait_queue.wake_up();

	m_connections_wait_queue.wait_on_event([&]() { return pending_connection.is_accepted == false; }, local_lock);

	ASSERT(pending_connection.connection);
	return *pending_connection.connection;
}

Result<void> Socket::close(FileDescription&)
{
	// connection are destroyed by the destructor.
	return ResultError(ERROR_SUCCESS);
}
