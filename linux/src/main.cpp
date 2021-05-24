#include "serial-commands.h"

#include <CLI/CLI.hpp>
#include <i3ipc++/ipc.hpp>
#include <libserial/SerialPort.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

struct Config {
	std::string target_serial_port;
	std::string output;
	size_t display_length {4};
	unsigned int startup_delay_ms {1500};
	bool blink_on_urgent = false;
};

struct State {
	std::string workspaces;
	std::string visible;
	std::string urgent;
};

using namespace LibSerial;

void send_to_arduino(const State& state, SerialPort& serial_port)
{
	const std::string payload {state.workspaces + serial_commands::workspaces_sent + state.visible
				   + serial_commands::visible_sent + state.urgent + serial_commands::urgent_sent};

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
	if (state.workspaces.empty()) {
		return;
	}

	auto ensure_visible_displayed = [&]() {
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
	};

	if (state.workspaces.length() > config.display_length) {
		state.workspaces = ensure_visible_displayed();
	}

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

		if (config.blink_on_urgent && workspace->urgent) {
			state.urgent += workspace_number;
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

int main(int argc, char* argv[])
{
	CLI::App app {"Display active i3 workspaces on a 7-segment display"};
	app.option_defaults()->always_capture_default();

	Config config;

	app.add_option("-o,--output", config.output, "Show workspaces only for this output");

	app.add_option("-l,--display-length", config.display_length, "Number of digits on your 7-segment display")
		->check(CLI::PositiveNumber);

	app.add_option("-p,--serial_port", config.target_serial_port, "Serial port your Arduino is connected to")
		->check(CLI::ExistingFile)
		->required();

	app.add_option("-d,--startup-delay", config.startup_delay_ms,
		       "Milliseconds to wait for the Arduino to restart on initial connection")
		->check(CLI::PositiveNumber);

	app.add_flag("-b,--blink-urgent", config.blink_on_urgent,
		     "Blink the dot if an application sets an urgent flag on a workspace");

	CLI11_PARSE(app, argc, argv);

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
