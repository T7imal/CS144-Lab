#include "tcp_connection.hh"

#include <chrono>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // auto now = std::chrono::system_clock::now();
    // auto timestamp =
    // std::chrono::time_point_cast<std::chrono::milliseconds>(now); auto
    // milliseconds = timestamp.time_since_epoch().count(); cerr << "time in
    // milliseconds: " << milliseconds << endl; cerr << endl;
    // cerr << endl << "TCP state : " << state().name() << endl;

    if (!_connection_active)
        return;

    // cerr << "receive segment: " << seg.header().summary() << " with " << seg.length_in_sequence_space() << " bytes"
    // << endl;

    _time_since_last_segment_received = 0;
    TCPState tcp_state = state();

    if (seg.header().rst) {
        // SYN_SENT 状态下收到 rst with ack，且 ackno 合法，关闭连接
        // LISTEN 状态下收到 rst，不关闭连接
        // 其他状态下收到 rst，若 rst 的 seqno 合法，关闭连接
        if ((tcp_state == TCPState::State::SYN_SENT && seg.header().ack &&
             seg.header().ackno == _sender.next_seqno()) ||
            (tcp_state != TCPState::State::LISTEN && _receiver.segment_received(seg))) {
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _connection_active = false;
        }
        return;
    }

    if (tcp_state == TCPState::State::LISTEN) {
        if (seg.header().syn) {
            _receiver.segment_received(seg);
            // 发送syn+ack
            _sender.fill_window();
        }
        if (seg.header().ack) {
            // 忽略
        }
    } else if (tcp_state == TCPState::State::SYN_SENT) {
        if (seg.header().syn && seg.header().ack) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            // 发送ack
            _sender.send_empty_segment();
        } else if (seg.header().syn) {
            _receiver.segment_received(seg);
            // 发送ack
            _sender.send_empty_segment();
        }
    } else if (tcp_state == TCPState::State::SYN_RCVD) {
        if (!_receiver.segment_received(seg)) {
            _sender.send_empty_segment();
        } else if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            _sender.send_empty_segment();
        }
        // _receiver.segment_received(seg);
        // _sender.ack_received(seg.header().ackno, seg.header().win);
    } else if (tcp_state == TCPState::State::ESTABLISHED) {
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            // cerr << "send empty segment 1" << endl;
            _sender.send_empty_segment();
        } else if (!_receiver.segment_received(seg)) {
            // cerr << "send empty segment 2" << endl;
            _sender.send_empty_segment();
        } else if (seg.header().fin && seg.length_in_sequence_space() == 1) {
            // cerr << "send empty segment 3" << endl;
            _sender.send_empty_segment();
        } else {
            _sender.fill_window();
            if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
                // cerr << "send empty segment 4" << endl;
                _sender.send_empty_segment();
            }
        }
    } else if (tcp_state == TCPState::State::CLOSE_WAIT) {
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            // cerr << "send empty segment 1" << endl;
            _sender.send_empty_segment();
        } else if (!_receiver.segment_received(seg)) {
            // cerr << "send empty segment 2" << endl;
            _sender.send_empty_segment();
        } else if (seg.header().fin && seg.length_in_sequence_space() == 1) {
            // cerr << "send empty segment 3" << endl;
            _sender.send_empty_segment();
        } else {
            _sender.fill_window();
            if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
                // cerr << "send empty segment 4" << endl;
                _sender.send_empty_segment();
            }
        }
    } else if (tcp_state == TCPState::State::LAST_ACK) {
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            // cerr << "send empty segment 1" << endl;
            _sender.send_empty_segment();
        } else if (!_receiver.segment_received(seg)) {
            // cerr << "send empty segment 2" << endl;
            _sender.send_empty_segment();
        } else if (seg.length_in_sequence_space() > 0) {
            // cerr << "send empty segment 3" << endl;
            _sender.send_empty_segment();
        }
    } else if (tcp_state == TCPState::State::FIN_WAIT_1) {
        // cerr << "payload:" << seg.payload().copy() << endl;
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            // cerr << "send empty segment 1" << endl;
            _sender.send_empty_segment();
        } else if (!_receiver.segment_received(seg)) {
            // cerr << "send empty segment 2" << endl;
            _sender.send_empty_segment();
        } else if (seg.length_in_sequence_space() > 0) {
            // cerr << "send empty segment 3" << endl;
            _sender.send_empty_segment();
        }
    } else if (tcp_state == TCPState::State::FIN_WAIT_2) {
        // cerr << "FIN_WAIT_2" << endl;
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            // cerr << "send empty segment 1" << endl;
            _sender.send_empty_segment();
        } else if (!_receiver.segment_received(seg)) {
            // cerr << "send empty segment 2" << endl;
            _sender.send_empty_segment();
        } else if (seg.length_in_sequence_space() > 0) {
            // cerr << "send empty segment 3" << endl;
            _sender.send_empty_segment();
        }
    } else if (tcp_state == TCPState::State::CLOSING) {
        if (!_sender.ack_received(seg.header().ackno, seg.header().win)) {
            // cerr << "send empty segment 1" << endl;
            _sender.send_empty_segment();
        } else if (!_receiver.segment_received(seg)) {
            // cerr << "send empty segment 2" << endl;
            _sender.send_empty_segment();
        } else if (seg.length_in_sequence_space() > 0) {
            // cerr << "send empty segment 3" << endl;
            _sender.send_empty_segment();
        }
    } else if (tcp_state == TCPState::State::TIME_WAIT) {
        // 当上一状态发送的 ack 丢失时，对方会重发fin，此时需要重新发送ack
        if (seg.header().fin) {
            // _receiver.segment_received(seg);
            // _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
        }
    } else if (tcp_state == TCPState::State::CLOSED) {
        // 不可能收到
    } else if (tcp_state == TCPState::State::RESET) {
        // 不可能收到
    } else {
        // 报出异常
        cerr << "Error: unknown state" << endl;
    }

    // if (seg.header().win == 0 && _sender.bytes_in_flight() == 0)
    // {
    //     TCPSegment new_seg;
    //     new_seg.header().seqno = _sender.next_seqno() - 1;
    //     _sender.segments_out().push(new_seg);
    //     _sender.unacked_segments().push(new_seg);
    // }

    _send_segments();

    // cerr << "tcp_state:" << state().name() << endl;
}

bool TCPConnection::active() const { return _connection_active; }

size_t TCPConnection::write(const string &data) {
    if (!_connection_active)
        return 0;

    size_t written = _sender.stream_in().write(data);
    _sender.fill_window();
    _send_segments();
    return written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to
//! this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_connection_active)
        return;

    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        while (!_sender.segments_out().empty()) {
            _sender.segments_out().pop();
        }
        _sender.send_empty_segment();
        _sender.segments_out().front().header().rst = true;
        _send_segments();
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _connection_active = false;
        return;
    }

    _send_segments();
}

void TCPConnection::end_input_stream() {
    // cerr << "end input stream" << endl;
    _sender.stream_in().end_input();
    // 发送fin
    _sender.fill_window();
    _send_segments();
}

void TCPConnection::connect() {
    // _last_window_size = _receiver.window_size();
    _sender.fill_window();
    _send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _connection_active = false;
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _sender.send_empty_segment();
            _sender.segments_out().front().header().rst = true;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::_send_segments() {
    // if (_last_window_size != _receiver.window_size() &&
    //     _sender.segments_out().empty() && _sender.bytes_in_flight() == 0)
    // {
    //     if (state() == TCPState::State::ESTABLISHED ||
    //         state() == TCPState::State::FIN_WAIT_1 ||
    //         state() == TCPState::State::FIN_WAIT_2)
    //     {
    //         TCPSegment seg;
    //         seg.header().seqno = _sender.next_seqno() - 1;
    //         // cerr << "tell window size changed" << endl;
    //         _sender.segments_out().push(seg);
    //         _sender.unacked_segments().push(seg);
    //     }
    // }

    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        optional<WrappingInt32> ackno = _receiver.ackno();
        // 建立连接后，每个包都要发送ack
        if (ackno.has_value()) {
            seg.header().ack = true;
            seg.header().ackno = ackno.value();
        }
        // seg.header().win = _receiver.window_size() <=
        // numeric_limits<uint16_t>::max()
        //     ? _receiver.window_size()
        //     : numeric_limits<uint16_t>::max();
        seg.header().win = _receiver.window_size();
        // _last_window_size = seg.header().win;
        _segments_out.emplace(seg);
        // cerr << "send segment: " << seg.header().summary() << " with " << seg.length_in_sequence_space() << " bytes"
        // << endl;
    }

    _clean_shutdown();
    // cerr << "tcp_state:" << state().name() << endl;
}

void TCPConnection::_clean_shutdown() {
    if (_receiver.stream_out().input_ended() && !(_sender.stream_in().eof())) {
        _linger_after_streams_finish = false;
    }
    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _connection_active = false;
        }
    }
}