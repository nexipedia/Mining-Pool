#ifndef NEXUSPOOL_WALLET_CONNECTION_HPP
#define NEXUSPOOL_WALLET_CONNECTION_HPP

#include "network/connection.hpp"
#include "network/socket.hpp"
#include "network/types.hpp"
#include <spdlog/spdlog.h>
#include "chrono/timer_factory.hpp"
#include "timer_manager.hpp"
#include "stats/stats_printer.hpp"

#include <memory>

namespace asio { class io_context; }

namespace nexuspool
{
    namespace config { class Config; }
    namespace stats { class Collector; }
    namespace protocol { class Protocol; }
    class Worker;

    class Wallet_connection : public std::enable_shared_from_this<Wallet_connection>
    {
    public:

        Wallet_connection(std::shared_ptr<asio::io_context> io_context, config::Config& config,
            chrono::Timer_factory::Sptr timer_factory, network::Socket::Sptr socket);

        bool connect(network::Endpoint const& wallet_endpoint);

        // stop the component and destroy all workers
        void stop();

    private:

        void process_data(network::Shared_payload&& receive_buffer);

        // Convert the Header of a Block into a Byte Stream for Reading and Writing Across Sockets
        LLP::CBlock deserialize_block(network::Shared_payload data);

        void retry_connect(network::Endpoint const& wallet_endpoint);

        std::shared_ptr<::asio::io_context> m_io_context;
        config::Config& m_config;
        network::Socket::Sptr m_socket;
        network::Connection::Sptr m_connection;
        std::shared_ptr<spdlog::logger> m_logger;
        std::shared_ptr<stats::Collector> m_stats_collector;
        Timer_manager m_timer_manager;
        std::uint32_t m_current_height;
    };
}

#endif
