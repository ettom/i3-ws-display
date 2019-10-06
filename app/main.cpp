#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>

#include <libserial/SerialPort.h>
#include <i3ipc++/ipc.hpp>
#include <json/json.h>

#define CONFIG_PATH_RELATIVE_TO_HOME  "/.config/"
#define CONFIG_FILE                   "ws_display.json"
#define STARTUP_DELAY_IN_MILLISECONDS 1500

#define WORKSPACES_SENT_DELIMITER     "w"
#define FOCUSED_SENT_DELIMITER        "f"
#define FOCUSED_NOT_FOUND             "-"

using namespace LibSerial;

SerialPort serial_port;
i3ipc::connection conn;

struct Config {
	std::string SERIAL_PORT;
	std::string OUTPUT;
	size_t DISPLAY_LENGTH;
};

struct State {
	std::string workspaces;
	std::string focused;
};

std::string get_config_path()
{
	std::string config_path;
	auto xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	if (xdg_config_home) {
		config_path = std::string(xdg_config_home) + '/';
	} else {
		config_path = std::string(std::getenv("HOME")) + CONFIG_PATH_RELATIVE_TO_HOME;
	}

	config_path += CONFIG_FILE;

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
	JSONCPP_STRING errs;

	if (!parseFromStream(builder, contents, &root, &errs)) {
		throw std::runtime_error("Errors in config file:\n" + errs);
	}

	Config config;
	config.OUTPUT = root["output"].asString();
	config.SERIAL_PORT = root["serial_port"].asString();
	config.DISPLAY_LENGTH = root["display_length"].asUInt();

	return config;
}

void send_to_arduino(const State& state)
{
	serial_port.Write(state.workspaces);
	serial_port.Write(WORKSPACES_SENT_DELIMITER);
	serial_port.Write(state.focused);
	serial_port.Write(FOCUSED_SENT_DELIMITER);
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


void resize_string_to_size(std::string& input, size_t target_size)
{
	if (input.size() > target_size) {
		input.resize(target_size);
	}
}

void always_display_focused_workspace(State& state, size_t display_length)
{
	if (state.focused == FOCUSED_NOT_FOUND) {
		return;
	}

	bool doesnt_fit_on_display = state.workspaces.length() > display_length;
	bool focused_workspace_not_displayed = state.workspaces.find(state.focused) > display_length - 1;
	if (doesnt_fit_on_display && focused_workspace_not_displayed) {
		state.workspaces.erase(state.workspaces.find(state.focused), 1);
		state.workspaces.insert(display_length - 1, state.focused);
	}
}

void move_workspace_10_to_end(std::string& workspaces)
{
	if (workspaces.size() && workspaces.at(0) == '0') {
		workspaces.erase(0, 1);
		workspaces.insert(workspaces.length(), "0");
	}
}

void sort_workspace_string(State& state, const Config& config)
{
	std::sort(state.workspaces.begin(), state.workspaces.end());
	move_workspace_10_to_end(state.workspaces);
	always_display_focused_workspace(state, config.DISPLAY_LENGTH);
	resize_string_to_size(state.workspaces, config.DISPLAY_LENGTH);
}

std::string ensure_workspace_name_is_numeric(const std::string& workspace_name)
{
	auto const n = workspace_name.find_first_of("0123456789");
	if (n != std::string::npos) {
		size_t const m = workspace_name.find_first_not_of("0123456789", n);
		return workspace_name.substr(n, (m == std::string::npos) ? m : m - n);
	}
	return "";
}

State find_workspaces(const Config& config)
{
	State state;
	bool found_focused = false;
	for (auto& workspace : conn.get_workspaces()) {
		if (config.OUTPUT.empty() || workspace->output == config.OUTPUT) {
			std::string workspace_number = ensure_workspace_name_is_numeric(workspace->name);
			workspace_number = (workspace_number == "10") ? "0" : workspace_number;
			if (workspace->focused) {
				state.focused = workspace_number;
				found_focused = true;
			}

			state.workspaces += workspace_number;
		}
	}

	if (!found_focused) {
		state.focused = FOCUSED_NOT_FOUND;
	}

	return state;
}

void startup(State& state, const Config& config)
{
	// wait for a bit for the Arduino to restart
	usleep(STARTUP_DELAY_IN_MILLISECONDS * 1000);
	state = find_workspaces(config);
	sort_workspace_string(state, config);
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
		sort_workspace_string(state, config);
	});


	try {
		serial_port.Open(config.SERIAL_PORT);
	} catch (const OpenFailed&) {
		std::cerr << "The serial ports did not open correctly." << std::endl;
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
