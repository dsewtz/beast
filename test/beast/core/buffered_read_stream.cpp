//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/buffered_read_stream.hpp>

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace beast {

class buffered_read_stream_test
    : public unit_test::suite
    , public test::enable_yield_to
{
    using self = buffered_read_stream_test;

public:
    void testSpecialMembers()
    {
        net::io_context ioc;
        {
            buffered_read_stream<test::stream, multi_buffer> srs(ioc);
            buffered_read_stream<test::stream, multi_buffer> srs2(std::move(srs));
            srs = std::move(srs2);
            BEAST_EXPECT(&srs.get_executor().context() == &ioc);
            BEAST_EXPECT(
                &srs.get_executor().context() == &srs2.get_executor().context());
        }
        {
            test::stream ts{ioc};
            buffered_read_stream<test::stream&, multi_buffer> srs(ts);
        }
    }

    struct loop : std::enable_shared_from_this<loop>
    {
        static std::size_t constexpr limit = 100;
        std::string s_;
        std::size_t n_ = 0;
        std::size_t cap_;
        unit_test::suite& suite_;
        net::io_context& ioc_;
        boost::optional<test::stream> ts_;
        boost::optional<test::fail_count> fc_;
        boost::optional<buffered_read_stream<
            test::stream&, multi_buffer>> brs_;

        loop(
            unit_test::suite& suite,
            net::io_context& ioc,
            std::size_t cap)
            : cap_(cap)
            , suite_(suite)
            , ioc_(ioc)
        {
        }

        void
        run()
        {
            do_read();
        }

        void
        on_read(error_code ec, std::size_t)
        {
            if(! ec)
            {
                suite_.expect(s_ ==
                    "Hello, world!", __FILE__, __LINE__);
                return;
            }
            ++n_;
            if(! suite_.expect(n_ < limit, __FILE__, __LINE__))
                return;
            s_.clear();
            do_read();
        }

        void
        do_read()
        {
            s_.resize(13);
            fc_.emplace(n_);
            ts_.emplace(ioc_, *fc_, ", world!");
            brs_.emplace(*ts_);
            brs_->buffer().commit(net::buffer_copy(
                brs_->buffer().prepare(5), net::buffer("Hello", 5)));
            net::async_read(*brs_,
                net::buffer(&s_[0], s_.size()),
                    std::bind(
                        &loop::on_read,
                        shared_from_this(),
                        std::placeholders::_1,
                        std::placeholders::_2));
        }
    };

    void
    testAsyncLoop()
    {
        std::make_shared<loop>(*this, ioc_, 0)->run();
        std::make_shared<loop>(*this, ioc_, 3)->run();
    }

    void testRead(yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;
        std::string s;
        s.resize(13);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::read(srs, net::buffer(&s[0], s.size()), ec);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.capacity(3);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::read(srs, net::buffer(&s[0], s.size()), ec);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::async_read(
                srs, net::buffer(&s[0], s.size()), do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts(ioc_, fc, ", world!");
            buffered_read_stream<
                test::stream&, multi_buffer> srs(ts);
            srs.capacity(3);
            srs.buffer().commit(net::buffer_copy(
                srs.buffer().prepare(5), net::buffer("Hello", 5)));
            error_code ec = test::error::test_failure;
            net::async_read(
                srs, net::buffer(&s[0], s.size()), do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(s == "Hello, world!");
                break;
            }
        }
        BEAST_EXPECT(n < limit);
    }

    struct copyable_handler
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    void
    testAsioHandlerInvoke()
    {
        // make sure things compile, also can set a
        // breakpoint in asio_handler_invoke to make sure
        // it is instantiated.
        net::io_context ioc;
        net::strand<
            net::io_context::executor_type> s(
                ioc.get_executor());
        test::stream ts{ioc};
        buffered_read_stream<
            test::stream&, multi_buffer> brs(ts);
        brs.async_read_some(net::mutable_buffer{},
            net::bind_executor(
                s, copyable_handler{}));
    }

    void run() override
    {
        testSpecialMembers();

        yield_to([&](yield_context yield){
            testRead(yield);});

        testAsyncLoop();
        testAsioHandlerInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,buffered_read_stream);

} // beast
} // boost
