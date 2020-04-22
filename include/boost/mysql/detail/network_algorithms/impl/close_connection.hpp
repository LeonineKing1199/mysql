//
// Copyright (c) 2019-2020 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_

#include "boost/mysql/detail/network_algorithms/quit_connection.hpp"
#include <boost/asio/yield.hpp>

namespace boost {
namespace mysql {
namespace detail {

template <typename Stream>
error_code close_channel(
    channel<Stream>& chan
)
{
    error_code err;
    chan.next_layer().shutdown(Stream::shutdown_both, err);
    chan.next_layer().close(err);
    return err;
}

} // detail
} // mysql
} // boost



template <typename StreamType>
void boost::mysql::detail::close_connection(
    channel<StreamType>& chan,
    error_code& code,
    error_info& info
)
{
    // Close = quit + close stream. We close the stream regardless of the quit failing or not
    if (chan.next_layer().is_open())
    {
        quit_connection(chan, code, info);
        auto err = close_channel(chan);
        if (!code) code = err;
    }
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CompletionToken,
    boost::mysql::detail::close_connection_signature
)
boost::mysql::detail::async_close_connection(
    channel<StreamType>& chan,
    CompletionToken&& token,
    error_info* info
)
{
    struct op : async_op<StreamType, CompletionToken, close_connection_signature, op>
    {
        using async_op<StreamType, CompletionToken, close_connection_signature, op>::async_op;

        void operator()(
            error_code err,
            bool cont=true
        )
        {
            error_code close_err;
            reenter(*this)
            {
                if (!this->get_channel().next_layer().is_open())
                {
                    this->complete(cont, error_code());
                    yield break;
                }

                // We call close regardless of the quit outcome
                // There are no async versions of shutdown or close
                yield async_quit_connection(
                    this->get_channel(),
                    std::move(*this),
                    this->get_output_info()
                );
                close_err = close_channel(this->get_channel());
                this->complete(cont, err ? err : close_err);
            }
        }
    };

    return op::initiate(std::forward<CompletionToken>(token), chan, info);
}

#include <boost/asio/unyield.hpp>


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_NETWORK_ALGORITHMS_IMPL_CLOSE_CONNECTION_HPP_ */
