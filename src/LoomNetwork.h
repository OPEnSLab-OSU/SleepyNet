#pragma once

#include "ArduinoJson.h"
#include "CircularBuffer.h"
#include "LoomNetworkFragment.h"

/**
 * Loom Network Layer
 */

template<size_t send_buffer = 16, size_t recv_buffer = 16, size_t json_document_max = 1024>
class LoomNetwork {
public:
	enum Status : uint8_t {
		NET_SLEEP_RDY = 1,
		APP_RECV_RDY = (1 << 2),
		APP_SEND_RDY = (1 << 3),
		NET_CLOSED = (1 << 4),
	};

	enum class Error {
		NET_OK,
		RECV_BUF_FULL,
		SEND_BUF_FULL,
	};

	LoomNetwork();

	uint32_t net_sleep_next_wake_time() const;
	void net_sleep_wake_ack();
	JsonArray& app_send_get_buf() const;
	void app_send_ack();
	const JsonArrayConst& app_recv_get_buf() const;
	void app_recv_ack();

	Error get_last_error() const;

private:
	enum class State {

	};

	uint16_t m_calculate_address(const JsonString& str);
	static void s_json_deleter(StaticJsonDocument<json_document_max>& json) { json.clear(); }

	CircularBuffer<LoomNetworkFragment, send_buffer> m_buffer_send;
	CircularBuffer<LoomNetworkFragment, recv_buffer> m_buffer_recv;

	Error m_last_error;
	Status m_status;
};