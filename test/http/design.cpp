//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/unit_test/suite.hpp>

#include <beast/core/error.hpp>
#include <beast/http/basic_parser.hpp>
#include <beast/http/concepts.hpp>
#include <beast/http/header_parser.hpp>
#include <beast/http/message_parser.hpp>

namespace beast {
namespace http {

/*

Parse states:

- need header
- at body
- at body-eof
- need chunk header
- at chunk



*/

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest,
    class Fields>
void
parse_some(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    header_parser<isRequest, Fields>& parser,
    error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(parser.need_more());
    BOOST_ASSERT(! parser.is_done());
    auto used =
        parser.write(dynabuf.data(), ec);
    if(ec)
        return;
    dynabuf.consume(used);
    if(parser.need_more())
    {
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> mb;
        auto const size =
            read_size_helper(dynabuf, 65536);
        BOOST_ASSERT(size > 0);
        try
        {
            mb.emplace(dynabuf.prepare(size));
        }
        catch(std::length_error const&)
        {
            ec = error::buffer_overflow;
            return;
        }
        dynabuf.commit(stream.read_some(*mb, ec));
        if(ec == boost::asio::error::eof)
        {
            // Caller will see eof on next read.
            ec = {};
            parser.write_eof(ec);
            if(ec)
                return;
            BOOST_ASSERT(! parser.need_more());
        }
        else if(ec)
        {
            return;
        }
    }
}

/** Parse some data from the stream.
*/
template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest,
    class Body,
    class Fields>
void
parse_some(
    SyncReadStream& stream,
    DynamicBuffer& dynabuf,
    message_parser<isRequest, Body, Fields>& parser,
    error_code& ec)
{
    static_assert(is_SyncReadStream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(is_DynamicBuffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    // See if the parser needs more structured
    // data in order to make forward progress
    //
    if(parser.need_more())
    {
        // Give the parser what we have already
        //
        auto used =
            parser.write(dynabuf.data(), ec);
        if(ec)
            return;
        dynabuf.consume(used);
        if(parser.need_more())
        {
            // Parser needs even more, try to read it
            //
            boost::optional<typename
                DynamicBuffer::mutable_buffers_type> mb;
            auto const size =
                read_size_helper(dynabuf, 65536); // magic number?
            BOOST_ASSERT(size > 0);
            try
            {
                mb.emplace(dynabuf.prepare(size));
            }
            catch(std::length_error const&)
            {
                // Convert the exception to an error
                ec = error::buffer_overflow;
                return;
            }
            auto const bytes_transferred =
                stream.read_some(*mb, ec);
            if(ec == boost::asio::error::eof)
            {
                dynabuf.commit(bytes_transferred);
                // Caller will see eof on next read.
                ec = {};
                parser.write_eof(ec);
                if(ec)
                    return;
                BOOST_ASSERT(! parser.need_more());
                BOOST_ASSERT(parser.is_done());
            }
            else if(! ec)
            {
                dynabuf.commit(bytes_transferred);
            }
            else
            {
                return;
            }
        }
    }
    else if(! parser.is_done())
    {
        // Apply any remaining bytes in dynabuf
        //
        parser.consume(dynabuf, ec);
        if(ec)
            return;

        // Parser wants a direct read
        //
        auto const mb = parser.prepare(
            dynabuf, 65536); // magic number?
        auto const bytes_transferred =
            stream.read_some(mb, ec);
        if(ec == boost::asio::error::eof)
        {
            dynabuf.commit(bytes_transferred);
            // Caller will see eof on next read.
            ec = {};
            parser.write_eof(ec);
            if(ec)
                return;
            BOOST_ASSERT(! parser.need_more());
            BOOST_ASSERT(parser.is_done());
        }
        else if(! ec)
        {
            parser.commit(bytes_transferred);
        }
        else
        {
            return;
        }
    }
}

class design_test : public beast::unit_test::suite
{
public:
    void
    run() override
    {
    }
};

BEAST_DEFINE_TESTSUITE(design,http,beast);

} // http
} // beast
