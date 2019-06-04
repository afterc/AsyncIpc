#pragma once

#include <map>

#include "ipc/ipc_thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"

namespace IPC
{
class Endpoint : public Sender, public Listener
{
public:
	Endpoint(const std::string& name, Listener* listener, bool start_now = true);
	~Endpoint();

	void Start();

	bool IsConnected() const;

	template<typename F, typename S>
	void Bind(int msg_type, F func, S* s);

	virtual bool Send(Message* message) override;

	virtual bool OnMessageReceived(Message* message) override;

	virtual void OnChannelConnected(int32 peer_pid) override;

	virtual void OnChannelError() override;

private:
	void CreateChannel();
	void OnSendMessage(scoped_refptr<Message> message);
	void CloseChannel(HANDLE wait_event);
	void SetConnected(bool c);

	template<typename F, typename S>
	void callProxy(F func, S* s, IPC::Message* msg);

private:
	std::string name_;
	Thread thread_;

	Channel* channel_;
	Listener* listener_;

	mutable Lock lock_;
	bool is_connected_;

	std::map<int, std::function<void(IPC::Message* msg)>> m_handlers;
};

template<typename F, typename S>
inline void Endpoint::Bind(int msg_type, F func, S* s)
{
	m_handlers[msg_type] = std::bind(&Endpoint::callProxy<F, S>, this, func, s, std::placeholders::_1);
}

template<typename F, typename S>
inline void Endpoint::callProxy(F func, S* s, IPC::Message* msg)
{
	(s->*func)(msg);
}

}