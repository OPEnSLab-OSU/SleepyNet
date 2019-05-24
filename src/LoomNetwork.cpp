#include "LoomNetwork.h"

template<size_t send_buffer, size_t recv_buffer>
LoomNetwork<send_buffer, recv_buffer>::LoomNetwork()
	: m_mac()
	, m_buffer_send()
	, m_buffer_recv()
	, m_last_error(Error::NET_OK)
	, m_status(Status::NET_WAIT_REFRESH) {}

template<size_t send_buffer, size_t recv_buffer>
void LoomNetwork<send_buffer, recv_buffer>::net_sleep_wake_ack()
{
	// if there's an error in the machine, we have to wait for it to be cleared
	if (m_last_error != Error::NET_OK) return;
	// wake the MAC layer up, and get its state
	m_mac.sleep_wake_ack();
	LoomMAC::State mac_status = m_mac.get_status();
	// loop until the MAC layer is ready to sleep, or we're wating on the network
	while (mac_status != LoomMAC::State::MAC_SLEEP_RDY && mac_status != LoomMAC::State::MAC_WAIT_REFRESH) {
		// update our status with the status from the MAC layer
		// if the MAC closed, error and return
		if (mac_status == LoomMAC::State::MAC_CLOSED) 
			return m_halt_error(Error::MAC_FAIL);
		// if the mac is ready for data, check our circular buffers!
		else if (mac_status == LoomMAC::State::MAC_DATA_SEND_RDY) {
			if (m_buffer_send.size() > 0) {
				// send the first packet corresponding to the address indicated by the MAC layer
				uint16_t addr = m_mac.get_cur_send_address();
				auto iter = m_buffer_send.crange().begin();
				auto end = m_buffer_send.crange().end();
				for (; iter != end; ++iter) {
					if (iter->get_next_hop() == addr) break;
				}
				// if there isn't any, send none and move on
				if (iter == end) m_mac.send_pass();
				else {
					// send, and if it succeded, destroy the item
					if (m_mac.send_fragment(*iter)) {
						m_buffer_send.remove(iter);
						// hey there's a new spot!
						m_status |= Status::NET_SEND_RDY;
					}
					// else we will try again later
					// the MAC layer will automatically trigger a refresh if 
					// sending fails consecutivly, so we don't need to do that here
					// flip the send ready bit if we haven't already
				}
			}
			// else tell the mac layer we got nothing
			else m_mac.send_pass();
		}
		// if the mac has data ready to be copied, do that
		else if (mac_status == LoomMAC::State::MAC_DATA_RECV_RDY) {
			// create a lookahead copy of the fragment
			const LoomNetworkFragment& recv_frag = m_mac.recv_fragment();
			// if we have a fragment that is addressed to us, add it to the recv buffer
			if (recv_frag.get_dst() == m_addr) {
				if (!m_buffer_recv.emplace_back(recv_frag)) 
					return m_halt_error(Error::RECV_BUF_FULL);
				// flip the recv ready bit
				m_status |= Status::NET_RECV_RDY;
			}
			// else the packet needs to be routed
			else {
				uint16_t nexthop = m_next_hop(recv_frag.get_dst());
				if (nexthop == LoomNetworkFragment::ADDR_NONE) 
					return m_halt_error(Error::ROUTE_FAIL);
				// push the packet to the send buffer, tagging it with the next hop address
				if (!m_buffer_send.emplace_back(recv_frag, m_next_hop(recv_frag.get_dst())))
					return m_halt_error(Error::SEND_BUF_FULL);
			}
		}
		// update the status
		LoomMAC::State mac_status = m_mac.get_status();
	}
	// set the sleep and wake bit
	if (mac_status == LoomMAC::State::MAC_WAIT_REFRESH) {
		m_status |= Status::NET_WAIT_REFRESH;
		m_status &= ~Status::NET_SLEEP_RDY;
	}
	else if (mac_status == LoomMAC::State::MAC_SLEEP_RDY) {
		m_status |= Status::NET_SLEEP_RDY;
		m_status &= ~Status::NET_WAIT_REFRESH;
	}
	else return m_halt_error(Error::INVAL_STATE);
}

template<size_t send_buffer, size_t recv_buffer>
void LoomNetwork<send_buffer, recv_buffer>::net_wait_ack()
{
	// if there's an error in the machine, we have to wait for it to be cleared
	if (m_last_error != Error::NET_OK) return;
	// have the mac layer check the network
	m_mac.check_for_refresh();
	// then see if the state changed
	if (m_mac.get_state() == LoomMAC::State::MAC_SLEEP_RDY) {
		m_status |= Status::NET_SLEEP_RDY;
		m_status &= Status::NET_WAIT_REFRESH;
	}
}

template<size_t send_buffer, size_t recv_buffer>
void LoomNetwork<send_buffer, recv_buffer>::app_send(const LoomNetworkFragment& send)
{
	// push the send fragment into the buffer
	m_buffer_send.emplace_back(send);
	// if the send buffer is full, disallow further sending
	if (m_buffer_send.full()) m_status &= ~Status::NET_SEND_RDY;
}

template<size_t send_buffer, size_t recv_buffer>
void LoomNetwork<send_buffer, recv_buffer>::app_send(const uint16_t dst_addr, const uint8_t seq, const uint8_t* raw_payload, const uint8_t length)
{
	// push the send fragment into the buffer
	m_buffer_send.emplace_back({ 
		std::forward(dst_addr), m_addr, std::forward(seq), std::forward(raw_payload), std::forward(length), m_next_hop(dst_addr) 
		});
	// if the send buffer is full, disallow further sending
	if (m_buffer_send.full()) m_status &= ~Status::NET_SEND_RDY;
}

template<size_t send_buffer, size_t recv_buffer>
LoomNetworkFragment LoomNetwork<send_buffer, recv_buffer>::app_recv()
{
	// create a copy of the last recieved object
	const LoomNetworkFragment frag(m_buffer_recv.front());
	// destroy the stored object
	m_buffer_recv.destroy_front();
	// if the buffer is emptey, tell the user that there's no more data
	if (m_buffer_recv.empty()) m_status &= ~Status::NET_RECV_RDY;
	// return the copy
	return frag;
}

