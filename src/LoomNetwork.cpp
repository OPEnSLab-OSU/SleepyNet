#include "LoomNetwork.h"

template<size_t send_buffer, size_t recv_buffer, size_t json_document_max>
LoomNetwork<send_buffer, recv_buffer, json_document_max>::LoomNetwork()
	: m_buffer_send()
	, m_buffer_recv()
	, m_last_error(Error::NET_OK)
	, m_status(Status::NET_CLOSED) {}

