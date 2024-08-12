#include <array>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <sstream>
#include <iomanip>
#include <vector>
#include <fstream>
#include <string>


using namespace std;
using namespace boost::asio;

serial_port_base::parity::type get_parity(const string& parity) {
    if (parity == "none") return serial_port_base::parity::none;
    else if (parity == "odd") return serial_port_base::parity::odd;
    else if (parity == "even") return serial_port_base::parity::even;
    else throw invalid_argument("Invalid parity option");
}

serial_port_base::stop_bits::type get_stop_bits(const string& stop_bits) {
    if (stop_bits == "one") return serial_port_base::stop_bits::one;
    else if (stop_bits == "onepointfive") return serial_port_base::stop_bits::onepointfive;
    else if (stop_bits == "two") return serial_port_base::stop_bits::two;
    else throw invalid_argument("Invalid stop bits option");
}

class Crc16 {
private:
    static const unsigned short polynomial = 0xA001;
    unsigned short table[256];

public:
    Crc16() {
        unsigned short value;
        unsigned short temp;

        for (unsigned short i = 0; i < 256; ++i) {
            value = 0;
            temp = i;
            for (unsigned char j = 0; j < 8; ++j) {
                if (((value ^ temp) & 0x0001) != 0) {
                    value = (value >> 1) ^ polynomial;
                } else {
                    value >>= 1;
                }
                temp >>= 1;
            }
            table[i] = value;
        }
    }

    unsigned short ComputeChecksum(const std::vector<unsigned char>& bytes) {
        unsigned short crc = 0;
        for (size_t i = 0; i < bytes.size(); ++i) {
            unsigned char index = static_cast<unsigned char>(crc ^ bytes[i]);
            crc = static_cast<unsigned short>((crc >> 8) ^ table[index]);
        }
        return crc;
    }

    std::vector<unsigned char> ComputeChecksumBytes(const std::vector<unsigned char>& bytes) {
        unsigned short crc = ComputeChecksum(bytes);
        return { static_cast<unsigned char>(crc & 0xFF), static_cast<unsigned char>((crc >> 8) & 0xFF) };
    }
};

class SerialReaderWriter {
public:
    SerialReaderWriter(io_service& io, const string& read_port, int read_baud_rate,
                       const string& write_port, int write_baud_rate,
                       const string& read_parity, const string& read_stop_bits, int read_data_bits,
                       const string& write_parity, const string& write_stop_bits, int write_data_bits,
                       double current_limit, const bool& car_charging_three_phases)
        : serial_read(io, read_port), serial_write(io, write_port), timer(io, boost::posix_time::seconds(10)), io_service_(io), running(true), current_limit_(current_limit) {
        
        serial_read.set_option(serial_port_base::baud_rate(read_baud_rate));
        serial_read.set_option(serial_port_base::character_size(read_data_bits));
        serial_read.set_option(serial_port_base::parity(get_parity(read_parity)));
        serial_read.set_option(serial_port_base::stop_bits(get_stop_bits(read_stop_bits)));
        serial_read.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none));

        serial_write.set_option(serial_port_base::baud_rate(write_baud_rate));
        serial_write.set_option(serial_port_base::character_size(write_data_bits));
        serial_write.set_option(serial_port_base::parity(get_parity(write_parity)));
        serial_write.set_option(serial_port_base::stop_bits(get_stop_bits(write_stop_bits)));
        serial_write.set_option(serial_port_base::flow_control(serial_port_base::flow_control::none));
        threePhases = car_charging_three_phases;
        start_read();
        start_timer();
    }

    void start_read() {
        serial_read.async_read_some(buffer(buffer_read),
            [this](boost::system::error_code ec, std::size_t length) {
                if (!ec && running) {
                    string data(buffer_read.data(), length);
                    process_received_data(data);
                    start_read();  // Start the next read operation
                } else if (ec) {
                    cerr << "Error: " << ec.message() << endl;
                }
            });
    }

    void start_timer() {
        timer.async_wait(boost::bind(&SerialReaderWriter::on_timer, this));
    }

    void on_timer() {
        if (running) {
            // Calculate generation	
	        calc_current(threePhases);            
            display_table();
			send_data();
            timer.expires_at(timer.expires_at() + boost::posix_time::seconds(10));
            start_timer();
        }
    }

    void process_received_data(const string& data) {
        smatch match;

        // Regular expressions to extract the additional values
        regex regex_0_2_8(R"(1-3:0\.2\.8\(([\d\.]+)\))");
        regex regex_1_0_0(R"(0-0:1\.0\.0\(([\d\.]+[SW])\))");
        regex regex_96_1_1(R"(0-0:96\.1\.1\(([\d\.]+)\))");
        regex regex_1_8_1(R"(1-0:1\.8\.1\(([\d\.]+)\*kWh\))");
        regex regex_1_8_2(R"(1-0:1\.8\.2\(([\d\.]+)\*kWh\))");
        regex regex_2_8_1(R"(1-0:2\.8\.1\(([\d\.]+)\*kWh\))");
        regex regex_2_8_2(R"(1-0:2\.8\.2\(([\d\.]+)\*kWh\))");
        regex regex_96_14_0(R"(0-0:96\.14\.0\((\d+)\))");
        regex regex_1_7_0(R"(1-0:1\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_2_7_0(R"(1-0:2\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_96_7_21(R"(0-0:96\.7\.21\((\d+)\))");
        regex regex_96_7_9(R"(0-0:96\.7\.9\((\d+)\))");
        regex regex_99_97_0(R"(1-0:99\.97\.0\((.+)\))");
        regex regex_32_32_0(R"(1-0:32\.32\.0\((\d+)\))");
        regex regex_52_32_0(R"(1-0:52\.32\.0\((\d+)\))");
        regex regex_72_32_0(R"(1-0:72\.32\.0\((\d+)\))");
        regex regex_32_36_0(R"(1-0:32\.36\.0\((\d+)\))");
        regex regex_52_36_0(R"(1-0:52\.36\.0\((\d+)\))");
        regex regex_72_36_0(R"(1-0:72\.36\.0\((\d+)\))");
        regex regex_32_7_0(R"(1-0:32\.7\.0\(([\d\.]+)\*V\))");
        regex regex_52_7_0(R"(1-0:52\.7\.0\(([\d\.]+)\*V\))");
        regex regex_72_7_0(R"(1-0:72\.7\.0\(([\d\.]+)\*V\))");
		regex regex_31_7_0(R"(1-0:31\.7\.0\(([\d\.]+)\*A\))");
        regex regex_51_7_0(R"(1-0:51\.7\.0\(([\d\.]+)\*A\))");
        regex regex_71_7_0(R"(1-0:71\.7\.0\(([\d\.]+)\*A\))");
        regex regex_21_7_0(R"(1-0:21\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_41_7_0(R"(1-0:41\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_61_7_0(R"(1-0:61\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_22_7_0(R"(1-0:22\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_42_7_0(R"(1-0:42\.7\.0\(([\d\.]+)\*kW\))");
        regex regex_62_7_0(R"(1-0:62\.7\.0\(([\d\.]+)\*kW\))");

        // Extract additional values and store them in variables
        if (regex_search(data, match, regex_0_2_8)) value_0_2_8 = stod(match[1]);
        if (regex_search(data, match, regex_1_0_0)) value_1_0_0 = match[1];
        if (regex_search(data, match, regex_96_1_1)) value_96_1_1 = match[1];
        if (regex_search(data, match, regex_1_8_1)) value_1_8_1 = stod(match[1]);
        if (regex_search(data, match, regex_1_8_2)) value_1_8_2 = stod(match[1]);
        if (regex_search(data, match, regex_2_8_1)) value_2_8_1 = stod(match[1]);
        if (regex_search(data, match, regex_2_8_2)) value_2_8_2 = stod(match[1]);
        if (regex_search(data, match, regex_96_14_0)) value_96_14_0 = stoi(match[1]);
        if (regex_search(data, match, regex_1_7_0)) value_1_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_2_7_0)) value_2_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_96_7_21)) value_96_7_21 = stoi(match[1]);
        if (regex_search(data, match, regex_96_7_9)) value_96_7_9 = stoi(match[1]);
        if (regex_search(data, match, regex_99_97_0)) value_99_97_0 = match[1];
        if (regex_search(data, match, regex_32_32_0)) value_32_32_0 = stoi(match[1]);
        if (regex_search(data, match, regex_52_32_0)) value_52_32_0 = stoi(match[1]);
        if (regex_search(data, match, regex_72_32_0)) value_72_32_0 = stoi(match[1]);
        if (regex_search(data, match, regex_32_36_0)) value_32_36_0 = stoi(match[1]);
        if (regex_search(data, match, regex_52_36_0)) value_52_36_0 = stoi(match[1]);
        if (regex_search(data, match, regex_72_36_0)) value_72_36_0 = stoi(match[1]);
        if (regex_search(data, match, regex_32_7_0)) value_32_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_52_7_0)) value_52_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_72_7_0)) value_72_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_31_7_0)) value_31_7_0 = min(stod(match[1]), current_limit_);
        if (regex_search(data, match, regex_51_7_0)) value_51_7_0 = min(stod(match[1]), current_limit_);
        if (regex_search(data, match, regex_71_7_0)) value_71_7_0 = min(stod(match[1]), current_limit_);
        if (regex_search(data, match, regex_21_7_0)) value_21_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_41_7_0)) value_41_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_61_7_0)) value_61_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_22_7_0)) value_22_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_42_7_0)) value_42_7_0 = stod(match[1]);
        if (regex_search(data, match, regex_62_7_0)) value_62_7_0 = stod(match[1]);

    }

    double calc_generation(const double& consumption, const double& generation, const double& current, const double& initialValue, const bool& threePhase) 
    {
       double calcValue=0.0;
	   double currentCalc = current;
	   if (threePhase) {
		   currentCalc = current / 3;
	   }
       if (consumption-generation > 0) {
           calcValue = initialValue - currentCalc;
       }
       else if (consumption-generation < 0) {
           calcValue = initialValue + currentCalc;
       }
       else {
           calcValue = initialValue;
       }	   
       if (calcValue > current_limit_) {
           calcValue = current_limit_;
       }
       if (calcValue < 0) {
           calcValue = 0;
       }
       return calcValue;
    }

    bool is_generating(const double& generation) {
		return (generation > 0);
	}
    void calc_current(const bool& threePhase) {
        mod_31_7_0 = calc_generation(value_21_7_0 + value_41_7_0 + value_61_7_0, value_22_7_0 + value_42_7_0 + value_62_7_0, value_31_7_0 + value_51_7_0 + value_71_7_0, mod_31_7_0, threePhase);
        mod_51_7_0 = calc_generation(value_21_7_0 + value_41_7_0 + value_61_7_0, value_22_7_0 + value_42_7_0 + value_62_7_0, value_31_7_0 + value_51_7_0 + value_71_7_0, mod_51_7_0, threePhase);
        mod_71_7_0 = calc_generation(value_21_7_0 + value_41_7_0 + value_61_7_0, value_22_7_0 + value_42_7_0 + value_62_7_0, value_31_7_0 + value_51_7_0 + value_71_7_0, mod_71_7_0, threePhase);
    }

    void send_data() {
        stringstream modified_telegram;

        // Construct the telegram with updated values
        modified_telegram << "/XMX5LGF0010455445332\r\n\r\n"
                          << "1-3:0.2.8(" << value_0_2_8 << ")\r\n"
                          << "0-0:1.0.0(" << value_1_0_0 << ")\r\n"
                          << "0-0:96.1.1(" << value_96_1_1 << ")\r\n"
                          << "1-0:1.8.1(" << fixed << setprecision(3) << value_1_8_1 << "*kWh)\r\n"
                          << "1-0:1.8.2(" << fixed << setprecision(3) << value_1_8_2 << "*kWh)\r\n"
                          << "1-0:2.8.1(" << fixed << setprecision(3) << value_2_8_1 << "*kWh)\r\n"
                          << "1-0:2.8.2(" << fixed << setprecision(3) << value_2_8_2 << "*kWh)\r\n"
                          << "0-0:96.14.0(" << value_96_14_0 << ")\r\n"
                          << "1-0:1.7.0(" << fixed << setprecision(3) << value_1_7_0 << "*kW)\r\n"
                          << "1-0:2.7.0(" << fixed << setprecision(3) << value_2_7_0 << "*kW)\r\n"
                          << "0-0:96.7.21(" << value_96_7_21 << ")\r\n"
                          << "0-0:96.7.9(" << value_96_7_9 << ")\r\n"
                          << "1-0:99.97.0(" << value_99_97_0 << ")\r\n"
                          << "1-0:32.32.0(" << value_32_32_0 << ")\r\n"
                          << "1-0:52.32.0(" << value_52_32_0 << ")\r\n"
                          << "1-0:72.32.0(" << value_72_32_0 << ")\r\n"
                          << "1-0:32.36.0(" << value_32_36_0 << ")\r\n"
                          << "1-0:52.36.0(" << value_52_36_0 << ")\r\n"
                          << "1-0:72.36.0(" << value_72_36_0 << ")\r\n"
                          << "1-0:32.7.0(" << fixed << setprecision(3) << value_32_7_0 << "*V)\r\n"
                          << "1-0:52.7.0(" << fixed << setprecision(3) << value_52_7_0 << "*V)\r\n"
                          << "1-0:72.7.0(" << fixed << setprecision(3) << value_72_7_0 << "*V)\r\n"
                          << "1-0:31.7.0(" << fixed << setprecision(3) << current_limit_ - mod_31_7_0 << "*A)\r\n"
                          << "1-0:51.7.0(" << fixed << setprecision(3) << current_limit_ - mod_51_7_0 << "*A)\r\n"
                          << "1-0:71.7.0(" << fixed << setprecision(3) << current_limit_ - mod_71_7_0 << "*A)\r\n"
                          << "1-0:21.7.0(" << fixed << setprecision(3) << value_21_7_0 << "*kW)\r\n"
                          << "1-0:41.7.0(" << fixed << setprecision(3) << value_41_7_0 << "*kW)\r\n"
                          << "1-0:61.7.0(" << fixed << setprecision(3) << value_61_7_0 << "*kW)\r\n"
                          << "1-0:22.7.0(" << fixed << setprecision(3) << value_22_7_0 << "*kW)\r\n"
                          << "1-0:42.7.0(" << fixed << setprecision(3) << value_42_7_0 << "*kW)\r\n"
                          << "1-0:62.7.0(" << fixed << setprecision(3) << value_62_7_0 << "*kW)\r\n"
                          << "!";

        // Calculate CRC16 checksum for the message
        string telegram_body = modified_telegram.str();
        /* uint16_t crc = calculate_crc16(telegram_body);
        stringstream checksum_stream;
        checksum_stream << hex << uppercase << crc;
        string checksum = checksum_stream.str();

        // Add checksum to the telegram
        telegram_body += checksum + "\r\n";*/
        std::vector<unsigned char> bytes(telegram_body.begin(), telegram_body.end());

		// Bereken de CRC16 checksum
		Crc16 crc16;
		unsigned short checksum = crc16.ComputeChecksum(bytes);

		// Zet de checksum om naar een hexadecimale string
		std::stringstream ss;
		ss << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << checksum;

		// Voeg de checksum en CRLF toe aan het telegram
		telegram_body += ss.str() + "\r\n";
		cout << telegram_body << endl;
        // Send the telegram
        async_write(serial_write, buffer(telegram_body), [](boost::system::error_code ec, std::size_t length) {
            if (ec) {
                cerr << "Error writing to serial port: " << ec.message() << endl;
            }
        });
    }

    uint16_t calculate_crc16(const string& data) {
        uint16_t crc = 0x0000;
        for (char c : data) {
            crc ^= static_cast<uint8_t>(c) << 8;
            for (int i = 0; i < 8; ++i) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }

    void display_table() {
        system("clear"); // Clear the terminal screen
        cout << "+-------------------+--------+--------+--------+--------+" << endl;
        cout << "|                   | Phase 1| Phase 2| Phase 3|   Total|" << endl;
        cout << "+-------------------+--------+--------+--------+--------+" << endl;
        cout << "| Current (A)       | " << setw(6) << value_31_7_0 << " | " << setw(6) << value_51_7_0 << " | " << setw(6) << value_71_7_0 << " | " << setw(6) << value_31_7_0 + value_51_7_0 + value_71_7_0 << " |" << endl;
        cout << "+-------------------+--------+--------+--------+--------+" << endl;
        cout << "| Consumption (kW)  | " << setw(6) << value_21_7_0 << " | " << setw(6) << value_41_7_0 << " | " << setw(6) << value_61_7_0 << " | " << setw(6) << value_21_7_0 + value_41_7_0 + value_61_7_0 << " |" << endl;
        cout << "+-------------------+--------+--------+--------+--------+" << endl;
        cout << "| Generation (kW)   | " << setw(6) << value_22_7_0 << " | " << setw(6) << value_42_7_0 << " | " << setw(6) << value_62_7_0 << " | " << setw(6) << value_22_7_0 + value_42_7_0 + value_62_7_0 << " |" << endl;
        cout << "+-------------------+--------+--------+--------+--------+" << endl;
        cout << "| CurrentMod (A)    | " << setw(6) << current_limit_ - mod_31_7_0 << " | " << setw(6) << current_limit_ - mod_51_7_0 << " | " << setw(6) << current_limit_ - mod_71_7_0 << " | " << setw(6) << "" << " |" << endl;
        cout << "+-------------------+--------+--------+--------+--------+" << endl;
    }

    void run() {
        io_service_.run();
    }

    void stop() {
        running = false;
        io_service_.stop();
    }

private:
    serial_port serial_read;
    serial_port serial_write;
    deadline_timer timer;
    io_service& io_service_;
    std::array<char, 256> buffer_read;
    bool running;
    bool threePhases = false;
    // Variables to hold the extracted data
    double value_0_2_8 = 0;
    string value_1_0_0 = "";
    string value_96_1_1 = "";
    double value_1_8_1 = 0.000;
    double value_1_8_2 = 0.000;
    double value_2_8_1 = 0.000;
    double value_2_8_2 = 0.000;
    int value_96_14_0 = 0;
    double value_1_7_0 = 0.000;
    double value_2_7_0 = 0.000;
    int value_96_7_21 = 0;
    int value_96_7_9 = 0;
    string value_99_97_0 = "";
    int value_32_32_0 = 0;
    int value_52_32_0 = 0;
    int value_72_32_0 = 0;
    int value_32_36_0 = 0;
    int value_52_36_0 = 0;
    int value_72_36_0 = 0;
    double value_32_7_0 = 0.0;
    double value_52_7_0 = 0.0;
    double value_72_7_0 = 0.0;

    // Variables for the table values
    double value_31_7_0 = 0.0;
    double value_51_7_0 = 0.0;
    double value_71_7_0 = 0.0;
    double mod_31_7_0 = 0.0;
    double mod_51_7_0 = 0.0;
    double mod_71_7_0 = 0.0;
    double value_21_7_0 = 0.0;
    double value_41_7_0 = 0.0;
    double value_61_7_0 = 0.0;
    double value_22_7_0 = 0.0;
    double value_42_7_0 = 0.0;
    double value_62_7_0 = 0.0;

    double current_limit_;
};

int main(int argc, char* argv[]) {
    try {
        if (argc != 13) {
            cerr << "Usage: " << argv[0] << " <read_port> <read_baud_rate> <write_port> <write_baud_rate> "
                 << "<read_parity> <read_stop_bits> <read_data_bits> <write_parity> <write_stop_bits> <write_data_bits> "
                 << "<current_limit> <car_charging_phases>\n";			
            return 1;
        }

        io_service io;
        string read_port = argv[1];
        int read_baud_rate = stoi(argv[2]);
        string write_port = argv[3];
        int write_baud_rate = stoi(argv[4]);
        string read_parity = argv[5];
        string read_stop_bits = argv[6];
        int read_data_bits = stoi(argv[7]);
        string write_parity = argv[8];
        string write_stop_bits = argv[9];
        int write_data_bits = stoi(argv[10]);
        double current_limit = stod(argv[11]);
		string car_charging_phases = argv[12];
        bool car_charging_three_phases = false;
		if (car_charging_phases == "1 phase") car_charging_three_phases = false;
		else if (car_charging_phases == "3 phase") car_charging_three_phases = true;
		else throw invalid_argument("Invalid car_charging_phases option");
        
        SerialReaderWriter serialReaderWriter(io, read_port, read_baud_rate, write_port, write_baud_rate,
                                              read_parity, read_stop_bits, read_data_bits,
                                              write_parity, write_stop_bits, write_data_bits,
                                              current_limit, car_charging_three_phases);

        signal(SIGINT, [](int) {
            cout << "Stopping..." << endl;
            exit(0);
        });

        serialReaderWriter.run();
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
