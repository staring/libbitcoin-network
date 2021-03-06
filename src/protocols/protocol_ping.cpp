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
#include <bitcoin/network/protocols/protocol_ping.hpp>

#include <functional>
#include <string>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/network/channel.hpp>
#include <bitcoin/network/p2p.hpp>
#include <bitcoin/network/protocols/protocol_timer.hpp>

namespace libbitcoin {
namespace network {

#define NAME "ping"
#define CLASS protocol_ping

using namespace bc::message;
using namespace std::placeholders;

protocol_ping::protocol_ping(p2p& network, channel::ptr channel)
  : protocol_timer(network, channel, true, NAME),
    settings_(network.network_settings()),
    CONSTRUCT_TRACK(protocol_ping)
{
}

void protocol_ping::start()
{
    protocol_timer::start(settings_.channel_heartbeat(), BIND1(send_ping, _1));

    SUBSCRIBE2(ping, handle_receive_ping, _1, _2);

    // Send initial ping message by simulating first heartbeat.
    set_event(error::success);
}

// This is fired by the callback (i.e. base timer and stop handler).
void protocol_ping::send_ping(const code& ec)
{
    if (stopped())
        return;

    if (ec && ec != error::channel_timeout)
    {
        log::debug(LOG_NETWORK)
            << "Failure in ping timer for [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }

    const auto nonce = pseudo_random();

    SUBSCRIBE3(pong, handle_receive_pong, _1, _2, nonce);
    SEND1(ping(nonce), handle_send_ping, _1);
}

bool protocol_ping::handle_receive_ping(const code& ec,
    message::ping::ptr message)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure getting ping from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    SEND1(pong(message->nonce), handle_send_pong, _1);

    // RESUBSCRIBE
    return true;
}

bool protocol_ping::handle_receive_pong(const code& ec,
    message::pong::ptr message, uint64_t nonce)
{
    if (stopped())
        return false;

    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure getting pong from [" << authority() << "] "
            << ec.message();
        stop(ec);
        return false;
    }

    if (message->nonce != nonce)
    {
        log::warning(LOG_NETWORK)
            << "Invalid pong nonce from [" << authority() << "]";

        // This could result from message overlap due to a short period,
        // but we assume the response is not as expected and terminate.
        stop(error::bad_stream);
    }

    return false;
}

void protocol_ping::handle_send_ping(const code& ec)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure sending ping to [" << authority() << "] "
            << ec.message();
        stop(ec);
        return;
    }
}

void protocol_ping::handle_send_pong(const code& ec)
{
    if (stopped())
        return;

    if (ec)
    {
        log::debug(LOG_NETWORK)
            << "Failure sending pong to [" << authority() << "] "
            << ec.message();
        stop(ec);
    }
}

} // namespace network
} // namespace libbitcoin
