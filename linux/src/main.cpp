#include "serial-commands.h"

#include <i3ipc++/ipc.hpp>
#include <json/json.h>
#include <libserial/SerialPort.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace defaults
{
constexpr const char* config_file {"ws-display.json"};
constexpr const char* config_path_relative_to_home {"/.config/"};
}

struct Config {
	std::string target_serial_port;
	std::string output;
	size_t display_length {};
	unsigned int startup_delay_ms {};
};

struct State {
	std::string workspaces;
	std::string visible;
};

using namespace LibSerial;

std::string get_config_path()
{
	const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	const std::string path = (xdg_config_home == nullptr)
					 ? std::string(std::getenv("HOME")) + defaults::config_path_relative_to_home
					 : std::string(xdg_config_home) + '/';
	return path + defaults::config_file;
}

std::stringstream read_file(const std::string& filename)
{
	std::stringstream result;
	std::ifstream infile(filename);
	if (!infile.is_open()) {
		throw std::runtime_error("Couldn't open config file at " + filename);
	}

	result << infile.rdbuf();

	return result;
}

Config parse_config_file(std::stringstream& contents)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	Json::String errs;

	if (!parseFromStream(builder, contents, &root, &errs)) {
		throw std::runtime_error("Errors in config file:\n" + errs);
	}

	Config config;
	config.output = root["output"].asString();
	config.target_serial_port = root["serial_port"].asString();
	config.display_length = root["display_length"].asUInt();
	config.startup_delay_ms = root["startup_delay_ms"].asUInt();

	return config;
}

void send_to_arduino(const State& state, SerialPort& serial_port)
{
	const std::string payload {state.workspaces + serial_commands::workspaces_sent + state.visible
				   + serial_commands::visible_sent};

	serial_port.Write(payload);
	serial_port.DrainWriteBuffer();
}

void initialize_serial(SerialPort& serial_port)
{
	serial_port.SetBaudRate(BaudRate::BAUD_19200);
	serial_port.SetCharacterSize(CharacterSize::CHAR_SIZE_8);
	serial_port.SetFlowControl(FlowControl::FLOW_CONTROL_NONE);
	serial_port.SetParity(Parity::PARITY_NONE);
	serial_port.SetStopBits(StopBits::STOP_BITS_1);
}

void prepare_workspaces(State& state, const Config& config)
{
	if (state.workspaces.empty() || state.workspaces.length() <= config.display_length) {
		return;
	}

	state.workspaces = [&]() { // ensure the maximum possible amount of visible workspaces are displayed
		std::string tmp = state.visible;
		for (auto c : state.workspaces) {
			if (tmp.length() == config.display_length) {
				break;
			}
			if (tmp.find(c) == std::string::npos) {
				tmp += c;
			}
		}
		return tmp;
	}();

	std::sort(state.workspaces.begin(), state.workspaces.end());

	if (state.workspaces.at(0) == '0') {
		state.workspaces.erase(0, 1);
		state.workspaces.insert(state.workspaces.length(), "0");
	}
}

State find_workspaces(const Config& config, const i3ipc::connection& conn)
{
	static auto prepare_workspace_name = [](const std::string& workspace_name) {
		const size_t n {workspace_name.find_first_of("0123456789")};
		if (n == std::string::npos) {
			throw std::logic_error("Workspace names must contain at least one numeric character!");
		}

		const size_t m {workspace_name.find_first_not_of("0123456789", n)};

		const std::string result = workspace_name.substr(n, (m == std::string::npos) ? m : m - n);
		return (result == "10") ? '0' : result.at(0);
	};

	State state {};
	for (const auto& workspace : conn.get_workspaces()) {
		if (!config.output.empty() && workspace->output != config.output) {
			continue;
		}

		const char workspace_number = prepare_workspace_name(workspace->name);
		if (workspace->visible) {
			state.visible += workspace_number;
		}

		state.workspaces += workspace_number;
	}

	return state;
}

void update_display(const Config& config, const i3ipc::connection& conn, SerialPort& serial_port)
{
	State state {find_workspaces(config, conn)};
	prepare_workspaces(state, config);
	send_to_arduino(state, serial_port);
}

int main()
{
	Config config;
	const std::string config_path = get_config_path();
	try {
		std::stringstream file_contents = read_file(config_path);
		config = parse_config_file(file_contents);
	} catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	SerialPort serial_port;
	try {
		serial_port.Open(config.target_serial_port);
		initialize_serial(serial_port);
	} catch (const OpenFailed&) {
		std::cerr << "The serial port " << config.target_serial_port << " did not open correctly." << std::endl;
		return EXIT_FAILURE;
	}

	// wait for a bit for the Arduino to restart
	usleep(config.startup_delay_ms * 1000);

	try {
		i3ipc::connection conn;

		conn.signal_workspace_event.connect([&](__attribute__((unused)) const i3ipc::workspace_event_t&) {
			update_display(config, conn, serial_port);
		});
		conn.subscribe(i3ipc::ET_WORKSPACE);

		update_display(config, conn, serial_port);
		for (;;) {
			conn.handle_event();
		}
	} catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
