#include "wallet_connection.hpp"
#include "pool_manager.hpp"
#include "packet.hpp"
#include "LLP/block.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace nexuspool
{
Wallet_connection::Wallet_connection(std::shared_ptr<asio::io_context> io_context,
    std::shared_ptr<spdlog::logger> logger,
    std::weak_ptr<Pool_manager> pool_manager,
    common::Mining_mode mining_mode,
    std::uint16_t connection_retry_interval, 
    std::uint16_t get_height_interval,
    chrono::Timer_factory::Sptr timer_factory, 
    network::Socket::Sptr socket)
    : m_io_context{ std::move(io_context) }
    , m_logger{std::move(logger)}
    , m_pool_manager{std::move(pool_manager)}
    , m_mining_mode{mining_mode}
    , m_connection_retry_interval{ connection_retry_interval }
    , m_get_height_interval{ get_height_interval }
    , m_socket{ std::move(socket) }
    , m_timer_manager{ std::move(timer_factory) }
    , m_current_height{0}
    , m_get_block_pool_manager{false}
{
}

void Wallet_connection::stop()
{
    m_timer_manager.stop();

    // close connection
    m_connection.reset();
}

void Wallet_connection::retry_connect(network::Endpoint const& wallet_endpoint)
{
    m_connection = nullptr;		// close connection (socket etc)

    // retry connect
    m_logger->info("Connection retry {} seconds", m_connection_retry_interval);
    m_timer_manager.start_connection_retry_timer(m_connection_retry_interval, shared_from_this(), wallet_endpoint);
}

bool Wallet_connection::connect(network::Endpoint const& wallet_endpoint)
{
    std::weak_ptr<Wallet_connection> weak_self = shared_from_this();
    auto connection = m_socket->connect(wallet_endpoint, [weak_self, wallet_endpoint](auto result, auto receive_buffer)
        {
            auto self = weak_self.lock();
            if (self)
            {
                if (result == network::Result::connection_declined ||
                    result == network::Result::connection_aborted ||
                    result == network::Result::connection_closed ||
                    result == network::Result::connection_error)
                {
                    self->m_logger->error("Connection to wallet not sucessful. Result: {}", result);
                    self->retry_connect(wallet_endpoint);
                }
                else if (result == network::Result::connection_ok)
                {
                    self->m_logger->info("Connection to wallet established");

                    Packet packet;
                    packet.m_header = Packet::SET_CHANNEL;
                    packet.m_length = 4;
                    packet.m_data = std::make_shared<network::Payload>(uint2bytes(self->m_mining_mode == common::Mining_mode::PRIME ? 1U : 2U));
                    self->m_connection->transmit(packet.get_bytes());

                    self->m_timer_manager.start_get_height_timer(self->m_get_height_interval, self->m_connection);
                }
                else
                {	// data received
                    self->process_data(std::move(receive_buffer));
                }
            }
        });

    if (!connection)
    {
        return false;
    }

    m_connection = std::move(connection);
    return true;
}

void Wallet_connection::process_data(network::Shared_payload&& receive_buffer)
{
    // if we don't have a connection to the wallet we cant do anything useful.
    if (!m_connection)
    {
        return;
    }

    Packet packet{ std::move(receive_buffer) };
    if (!packet.is_valid())
    {
        // log invalid packet
        m_logger->error("Received packet is invalid. Header: {0}", packet.m_header);
        return;
    }

    if (packet.m_header == Packet::PING)
    {
        Packet response;
        response = response.get_packet(Packet::PING);
        m_connection->transmit(response.get_bytes());
    }
    else if (packet.m_header == Packet::BLOCK_HEIGHT)
    {
        auto pool_manager_shared = m_pool_manager.lock();
        if (!pool_manager_shared)
            return;

        auto const height = bytes2uint(*packet.m_data);
        if (height > m_current_height)
        {
            m_current_height = height;
            m_logger->info("Nexus Network: New Block with height {}", m_current_height);

            // update height at pool_manager
            pool_manager_shared->set_current_height(m_current_height);

            // get new block from wallet for pool_manager
            Packet packet_get_block;
            packet_get_block.m_header = Packet::GET_BLOCK;
            m_connection->transmit(packet_get_block.get_bytes());
            m_get_block_pool_manager = true; 

            // clear pending get_block handlers
            std::scoped_lock lock(m_get_block_mutex);
            std::queue<Get_block_handler> empty_queue;
            std::swap(m_pending_get_block_handlers, empty_queue);
        }
        else
        {
            // send the height message to all miners
            pool_manager_shared->set_current_height(m_current_height);
        }
    }
    // Block from wallet received
    else if (packet.m_header == Packet::BLOCK_DATA)
    {
        auto block = LLP::deserialize_block(std::move(*packet.m_data));
        if (block.nHeight == m_current_height)
        {
            if (m_get_block_pool_manager) // pool_manager get_block has priority
            {
                auto pool_manager_shared = m_pool_manager.lock();
                if (!pool_manager_shared)
                    return;

                pool_manager_shared->set_block(std::move(block));
                m_get_block_pool_manager = false;
            }
            else
            {
                // get oldest pending_get_block handler from miner_connection and call it then pop()
                std::scoped_lock lock(m_get_block_mutex);
                auto handler = m_pending_get_block_handlers.front();
                handler(block);
                m_pending_get_block_handlers.pop();
            }
        }
        else
        {
            m_logger->warn("Block Obsolete Height = {}, Skipping over.", block.nHeight);
        }
    }
    else if (packet.m_header == Packet::ACCEPT)
    {
        m_logger->info("Block Accepted By Nexus Network.");
        //m_stats_collector->block_accepted();
        // store block into DB
    }
    else if (packet.m_header == Packet::REJECT)
    {
        m_logger->warn("Block Rejected by Nexus Network.");
        Packet packet_get_block;
        packet_get_block.m_header = Packet::GET_BLOCK;
        m_connection->transmit(packet_get_block.get_bytes());
        //  m_stats_collector->block_rejected();
    }
    else
    {
        m_logger->error("Invalid header received.");
    }
}

void Wallet_connection::submit_block(std::vector<std::uint8_t> const& block_data, std::vector<std::uint8_t> const& nonce)
{
    m_logger->info("Submitting Block...");

    Packet packet;
    packet.m_header = Packet::SUBMIT_BLOCK;

    packet.m_data = std::make_shared<std::vector<std::uint8_t>>(block_data);
    packet.m_data->insert(packet.m_data->end(), nonce.begin(), nonce.end());
    packet.m_length = 72;

    m_connection->transmit(packet.get_bytes());
}

void Wallet_connection::get_block(Get_block_handler&& handler)
{
    if (!m_connection)
    {
        return;
    }

    Packet packet_get_block;
    packet_get_block.m_header = Packet::GET_BLOCK;
    m_connection->transmit(packet_get_block.get_bytes());

    // store block request handler in pending list (handler comes from miner_connection)
    std::scoped_lock lock(m_get_block_mutex);
    m_pending_get_block_handlers.emplace(handler);
}

}