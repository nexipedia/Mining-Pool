#ifndef NEXUSPOOL_PERSISTANCE_COMMAND_COMMAND_IMPL_HPP
#define NEXUSPOOL_PERSISTANCE_COMMAND_COMMAND_IMPL_HPP

#include "persistance/command/command.hpp"
#include "sqlite/sqlite3.h"
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <iostream>

namespace spdlog { class logger; }
namespace nexuspool {
namespace persistance {
namespace command {


struct Column_sqlite
{
	enum Datatype :std::uint8_t
	{
		string = 0,
		int32,
		int64,
		double_t
	};

	Datatype m_type;
	std::variant<std::string, std::int32_t, std::int64_t, double> m_data;
};
struct Command_type_sqlite
{
	sqlite3_stmt* m_statement;
	std::vector<Column_sqlite> m_columns;
};

struct Result_sqlite
{
	using Result_columns = std::vector<Column_sqlite>;
	std::vector<Result_columns> m_rows;
};

class Command_base_database_sqlite : public Command_base_database
{
public:
	virtual ~Command_base_database_sqlite() = default;
	Command_base_database_sqlite(sqlite3* handle) : m_handle{handle} {}
	Class get_class() const override { return Class::database_sqlite; }

protected:

	void bind_param(sqlite3_stmt* stmt, const char* name, std::string const& value);
	void bind_param(sqlite3_stmt* stmt, const char* name, int value);
	void bind_param(sqlite3_stmt* stmt, const char* name, int64_t value);
	void bind_param(sqlite3_stmt* stmt, const char* name, double value);

	sqlite3* m_handle;

};


class Command_banned_user_and_ip_impl : public Command_base_database_sqlite
{
public:

	explicit Command_banned_user_and_ip_impl(sqlite3* handle);

	~Command_banned_user_and_ip_impl() { sqlite3_finalize(m_banned_user_and_ip_stmt); }
	std::any get_command() const override;
	Type get_type() const override { return Type::get_banned_user_and_ip; }
	void set_params(std::any params) override;

private:
	sqlite3_stmt* m_banned_user_and_ip_stmt;

};

class Command_banned_api_ip_impl : public Command_base_database_sqlite
{
public:

	explicit Command_banned_api_ip_impl(sqlite3* handle);

	~Command_banned_api_ip_impl() { sqlite3_finalize(m_banned_api_ip_stmt); }
	std::any get_command() const override;
	Type get_type() const override { return Type::get_banned_api_ip; }
	void set_params(std::any params) override;

private:
	sqlite3_stmt* m_banned_api_ip_stmt;
};


class Command_create_db_schema_impl : public Command_base_database_sqlite
{
public:

	explicit Command_create_db_schema_impl(sqlite3* handle);

	~Command_create_db_schema_impl() { sqlite3_finalize(m_create_tables_stmt); }
	std::any get_command() const override { return Command_type_sqlite{ m_create_tables_stmt }; }
	Type get_type() const override { return Type::create_db_schema; }

private:

	sqlite3_stmt* m_create_tables_stmt;
};

class Command_account_exists_impl : public Command_base_database_sqlite
{
public:

	explicit Command_account_exists_impl(sqlite3* handle);

	~Command_account_exists_impl() { sqlite3_finalize(m_account_exists_stmt); }
	std::any get_command() const override;
	Type get_type() const override { return Type::account_exists; }
	void set_params(std::any params) override;

private:

	sqlite3_stmt* m_account_exists_stmt;
};

class Command_get_blocks_impl : public Command_base_database_sqlite
{
public:

	explicit Command_get_blocks_impl(sqlite3* handle);

	~Command_get_blocks_impl() { sqlite3_finalize(m_get_blocks_stmt); }
	std::any get_command() const override;
	Type get_type() const override { return Type::get_blocks; }

private:

	sqlite3_stmt* m_get_blocks_stmt;
};


}
}
}

#endif 
