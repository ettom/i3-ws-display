#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include <i3ipc++/ipc.hpp>
#include <json/json.h>
#include <libserial/SerialPort.h>

#include "serial-commands.h"

namespace defaults
{
const int startup_delay_ms {1500};
const std::string config_file {"ws-display.json"};
const std::string config_path_relative_to_home {"/.config/"};
}

using namespace LibSerial;

SerialPort serial_port;
i3ipc::connection conn;

struct Config {
	std::string target_serial_port;
	std::string output;
	size_t display_length;
};

struct State {
	std::string workspaces;
	char visible;
};

std::string get_config_path()
{
	std::string config_path;
	auto xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	if (xdg_config_home != NULL) {
		config_path = std::string(xdg_config_home) + '/';
	} else {
		config_path = std::string(std::getenv("HOME")) + defaults::config_path_relative_to_home;
	}

	config_path += defaults::config_file;

	return config_path;
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

	return config;
}

void send_to_arduino(const State& state)
{
	serial_port.Write(state.workspaces);
	serial_port.WriteByte(serial_commands::workspaces_sent);
	serial_port.WriteByte(state.visible);
	serial_port.WriteByte(serial_commands::visible_sent);
	serial_port.DrainWriteBuffer();
}

void initialize_serial()
{
	serial_port.SetBaudRate(BaudRate::BAUD_9600);
	serial_port.SetCharacterSize(CharacterSize::CHAR_SIZE_8);
	serial_port.SetFlowControl(FlowControl::FLOW_CONTROL_NONE);
	serial_port.SetParity(Parity::PARITY_NONE);
	serial_port.SetStopBits(StopBits::STOP_BITS_1);
}

void always_display_visible_workspace(State& state, size_t display_length)
{
	bool doesnt_fit_on_display = state.workspaces.length() > display_length;
	bool visible_workspace_not_displayed = state.workspaces.find(state.visible) > display_length - 1;
	if (doesnt_fit_on_display && visible_workspace_not_displayed) {
		state.workspaces.erase(state.workspaces.find(state.visible), 1);
		state.workspaces.insert(display_length - 1, 1, state.visible);
	}
}

void move_workspace_10_to_end(std::string& workspaces)
{
	if (workspaces.length() > 0 && workspaces.at(0) == '0') {
		workspaces.erase(0, 1);
		workspaces.insert(workspaces.length(), "0");
	}
}

void prepare_workspace_string(State& state, const Config& config)
{
	std::sort(state.workspaces.begin(), state.workspaces.end());
	move_workspace_10_to_end(state.workspaces);

	if (state.visible != serial_commands::visible_not_found) {
		always_display_visible_workspace(state, config.display_length);
	}

	if (state.workspaces.size() > config.display_length) {
		state.workspaces.resize(config.display_length);
	}
}

std::string ensure_workspace_name_is_numeric(const std::string& workspace_name)
{
	auto const n = workspace_name.find_first_of("0123456789");
	if (n == std::string::npos) {
		throw std::logic_error("Workspace names must contain at least one numeric character!");
	}

	size_t const m = workspace_name.find_first_not_of("0123456789", n);
	return workspace_name.substr(n, (m == std::string::npos) ? m : m - n);
}

std::string prepare_workspace_name(const std::string& workspace_name)
{
	std::string result = ensure_workspace_name_is_numeric(workspace_name);
	return (result == "10") ? "0" : result;
}

State find_workspaces(const Config& config)
{
	State state;
	bool found_visible = false;
	for (auto& workspace : conn.get_workspaces()) {
		if (!config.output.empty() && workspace->output != config.output) {
			continue;
		}

		std::string workspace_number = prepare_workspace_name(workspace->name);
		if (workspace->visible) {
			state.visible = workspace_number.at(0);
			found_visible = true;
		}

		state.workspaces += workspace_number;
	}

	if (!found_visible) {
		state.visible = serial_commands::visible_not_found;
	}

	return state;
}

void startup(State& state, const Config& config)
{
	// wait for a bit for the Arduino to restart
	usleep(defaults::startup_delay_ms * 1000);
	state = find_workspaces(config);
	prepare_workspace_string(state, config);
	send_to_arduino(state);
}

void loop(State& state)
{
	while (true) {
		conn.handle_event();
		send_to_arduino(state);
	}
}

int main()
{
	const std::string config_path = get_config_path();
	Config config;
	State state;

	try {
		std::stringstream file_contents = read_file(config_path);
		config = parse_config_file(file_contents);
	} catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	conn.subscribe(i3ipc::ET_WORKSPACE);

	conn.signal_workspace_event.connect([&](__attribute__((unused)) const i3ipc::workspace_event_t&) {
		state = find_workspaces(config);
		prepare_workspace_string(state, config);
	});

	try {
		serial_port.Open(config.target_serial_port);
	} catch (const OpenFailed&) {
		std::cerr << "The serial port " << config.target_serial_port << " did not open correctly." << std::endl;
		return EXIT_FAILURE;
	}

	initialize_serial();

	try {
		startup(state, config);
		loop(state);
	} catch (const std::runtime_error&) {
		serial_port.Close();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
