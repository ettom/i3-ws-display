#include <iostream>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sstream>

#include <libserial/SerialPort.h>
#include <i3ipc++/ipc.hpp>
#include <json/json.h>

#define CONFIG_PATH_RELATIVE_TO_HOME "/.config/ws_display.json"

#define WORKSPACES_SENT_DELIMITER     "w"
#define FOCUSED_SENT_DELIMITER        "f"
#define FOCUSED_NOT_FOUND             "-"
#define STARTUP_DELAY_IN_MILLISECONDS 1500

using namespace LibSerial;

SerialPort serial_port;
i3ipc::connection conn;
std::string focused;

struct Config {
	std::string SERIAL_PORT;
	std::string OUTPUT;
	size_t DISPLAY_LENGTH;
};

std::stringstream read_file(const std::string& filename)
{
	std::stringstream result;
	std::ifstream infile(filename);
	if (!infile.is_open()) {
		throw "Couldn't open config file!";
	}

	if (infile) {
		result << infile.rdbuf();
		infile.close();
	}

	return result;
}

Config parse_config_file(std::stringstream& contents)
{
	Json::Value root;
	Json::CharReaderBuilder builder;
	JSONCPP_STRING errs;

	if (!parseFromStream(builder, contents, &root, &errs)) {
		std::cerr << errs << std::endl;
		throw "Errors in config file!";
	}

	Config config;
	config.OUTPUT = root["output"].asString();
	config.SERIAL_PORT = root["serial_port"].asString();
	config.DISPLAY_LENGTH = root["display_length"].asUInt();

	return config;
}

void send_to_arduino(const std::string& workspaces)
{
	serial_port.Write(workspaces);
	serial_port.Write(WORKSPACES_SENT_DELIMITER);
	serial_port.Write(focused);
	serial_port.Write(FOCUSED_SENT_DELIMITER);
	serial_port.DrainWriteBuffer();
}

void initialize_serial(SerialPort& serial_port)
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

void always_display_focused_workspace(std::string& workspaces, Config config)
{
	if (focused == FOCUSED_NOT_FOUND) {
		return;
	}

	bool doesnt_fit_on_display = workspaces.length() > config.DISPLAY_LENGTH;
	bool focused_workspace_not_displayed = workspaces.find(focused) > config.DISPLAY_LENGTH - 1;
	if (doesnt_fit_on_display && focused_workspace_not_displayed) {
		workspaces.erase(workspaces.find(focused), 1);
		workspaces.insert(config.DISPLAY_LENGTH - 1, focused);
	}
}

void move_workspace_10_to_end(std::string& workspaces)
{
	if (workspaces.size() && workspaces.at(0) == '0') {
		workspaces.erase(0, 1);
		workspaces.insert(workspaces.length(), "0");
	}
}
void sort_workspace_string(std::string& workspaces, Config config)
{
	std::sort(workspaces.begin(), workspaces.end());
	move_workspace_10_to_end(workspaces);
	always_display_focused_workspace(workspaces, config);
	resize_string_to_size(workspaces, config.DISPLAY_LENGTH);
}

std::string ensure_workspace_name_is_numeric(const std::string& workspace_name)
{
	auto const n = workspace_name.find_first_of("0123456789");
	if (n != std::string::npos) {
		size_t const m = workspace_name.find_first_not_of("0123456789", n);
		return workspace_name.substr(n, m == std::string::npos ? m : m - n);
	}
	return "";
}

std::string find_workspaces(Config config)
{
	std::string workspaces;
	bool found_focused = false;
	for (auto& workspace : conn.get_workspaces()) {
		if (config.OUTPUT.empty() || workspace->output == config.OUTPUT) {
			std::string workspace_number = ensure_workspace_name_is_numeric(workspace->name);
			workspace_number = (workspace_number == "10") ? "0" : workspace_number;
			if (workspace->focused) {
				focused = workspace_number;
				found_focused = true;
			}

			workspaces += workspace_number;
		}
	}

	if (!found_focused) {
		focused = FOCUSED_NOT_FOUND;
	}

	return workspaces;
}

void startup(std::string workspaces, Config config)
{
	// wait for a bit for the Arduino to restart
	usleep(STARTUP_DELAY_IN_MILLISECONDS * 1000);
	workspaces = find_workspaces(config);
	sort_workspace_string(workspaces, config);
	send_to_arduino(workspaces);
}

void loop(const std::string& workspaces)
{
	while (true) {
		conn.handle_event();
		send_to_arduino(workspaces);
	}
}

int main()
{
	const std::string CONFIG_FILE = static_cast<std::string>(std::getenv("HOME")) + CONFIG_PATH_RELATIVE_TO_HOME;
	Config config;

	try {
		std::stringstream file_contents = read_file(CONFIG_FILE);
		config = parse_config_file(file_contents);
	} catch (const char* e) {
		std::cerr << e << std::endl;
		return EXIT_FAILURE;
	}

	std::string workspaces;
	conn.subscribe(i3ipc::ET_WORKSPACE);

	conn.signal_workspace_event.connect([&](__attribute__((unused)) const i3ipc::workspace_event_t&) {
		workspaces = find_workspaces(config);
		sort_workspace_string(workspaces, config);
	});


	try {
		serial_port.Open(config.SERIAL_PORT);
	} catch (const OpenFailed&) {
		std::cerr << "The serial ports did not open correctly." << std::endl;
		return EXIT_FAILURE;
	}

	initialize_serial(serial_port);

	try {
		startup(workspaces, config);
		loop(workspaces);
	} catch (const std::runtime_error&) {
		serial_port.Close();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
