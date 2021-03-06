/**
 * Copyright (c) 2011-2015 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_NETWORK_PROXY_HPP
#define LIBBITCOIN_NETWORK_PROXY_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/network/const_buffer.hpp>
#include <bitcoin/network/define.hpp>
#include <bitcoin/network/message_subscriber.hpp>
#include <bitcoin/network/socket.hpp>

namespace libbitcoin {
namespace network {

/// Manages all socket communication, thread safe.
class BCT_API proxy
  : public enable_shared_from_base<proxy>
{
public:
    typedef std::shared_ptr<proxy> ptr;
    typedef std::function<void()> completion_handler;
    typedef std::function<void(const code&)> result_handler;
    typedef subscriber<const code&> stop_subscriber;
    typedef resubscriber<const code&, const std::string&, const_buffer,
        result_handler> send_subscriber;

    /// Construct an instance.
    proxy(threadpool& pool, socket::ptr socket, uint32_t magic);

    /// Validate proxy stopped.
    ~proxy();

    /// This class is not copyable.
    proxy(const proxy&) = delete;
    void operator=(const proxy&) = delete;

    /// Send a message on the socket.
    template <class Message>
    void send(Message&& packet, result_handler handler)
    {
        const auto message = std::forward<Message>(packet);
        const auto& command = message.command;
        const auto buffer = const_buffer(message::serialize(message, magic_));
        do_send(error::success, command, buffer, handler);

        ////// Bypass queuing for block messages.
        //// const auto success = error::success;
        ////if (message.command == chain::block::command)
        ////    send_subscriber_->do_relay(success, command, buffer, handler);
        ////else
        ////    send_subscriber_->relay(success, command, buffer, handler);
    }

    /// Subscribe to messages of the specified type on the socket.
    template <class Message>
    void subscribe(message_handler<Message>&& handler)
    {
        auto stopped = std::forward<message_handler<Message>>(handler);
        message_subscriber_.subscribe<Message>(stopped);
    }

    /// Subscribe to the stop event.
    virtual void subscribe_stop(result_handler handler);

    /// Get the authority of the far end of this socket.
    virtual const config::authority& authority() const;

    /// Read messages from this socket.
    virtual void start(result_handler handler);

    /// Stop reading or sending messages on this socket.
    virtual void stop(const code& ec);

protected:
    virtual bool stopped() const;
    virtual void handle_activity() = 0;
    virtual void handle_stopping() = 0;

private:
    typedef byte_source<message::heading::buffer> heading_source;
    typedef boost::iostreams::stream<heading_source> heading_stream;
    typedef byte_source<data_chunk> payload_source;
    typedef boost::iostreams::stream<payload_source> payload_stream;

    static config::authority authority_factory(socket::ptr socket);

    void do_close();
    void stop(const boost_code& ec);

    void read_heading();
    void handle_read_heading(const boost_code& ec, size_t);

    void read_payload(const message::heading& head);
    void handle_read_payload(const boost_code& ec, size_t,
        const message::heading& head);

    bool do_send(const code& ec, const std::string& command,
        const_buffer buffer, result_handler handler);
    void handle_send(const boost_code& ec, const_buffer buffer,
        result_handler handler);

    const uint32_t magic_;
    const config::authority authority_;

    // These are thread safe.
    std::atomic<bool> stopped_;
    socket::ptr socket_;
    stop_subscriber::ptr stop_subscriber_;
    message_subscriber message_subscriber_;
    ////send_subscriber::ptr send_subscriber_;

    // These are protected by sequential ordering.
    data_chunk payload_buffer_;
    message::heading::buffer heading_buffer_;
};

} // namespace network
} // namespace libbitcoin

#endif

