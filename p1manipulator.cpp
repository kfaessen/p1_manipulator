#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <cstdlib>
#include <csignal>
#include <array>
#include <vector>

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

class SerialReaderWriter {
public:
    SerialReaderWriter(io_service& io, const string& read_port, int read_baud_rate,
                       const string& write_port, int write_baud_rate,
                       const string& read_parity, const string& read_stop_bits, int read_data_bits,
                       const string& write_parity, const string& write_stop_bits, int write_data_bits)
        : serial_read(io, read_port), serial_write(io, write_port), timer(io, boost::posix_time::seconds(10)), io_service_(io), running(true) {
        
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

        start_read();
        start_timer();
    }

    void start_read() {
        serial_read.async_read_some(buffer(buffer_read),
            [this](boost::system::error_code ec, std::size_t length) {
                if (!ec && running) {
                    buffer_received.insert(buffer_received.end(), buffer_read.data(), buffer_read.data() + length);
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
            send_data();
            timer.expires_at(timer.expires_at() + boost::posix_time::seconds(10));
            start_timer();
        }
    }

    void send_data() {
        if (!buffer_received.empty()) {
            boost::asio::write(serial_write, boost::asio::buffer(buffer_received));
            buffer_received.clear();
        }
    }

    void stop() {
        running = false;
        serial_read.close();
        serial_write.close();
        io_service_.stop();
    }

    void run() {
        io_service_.run();
    }

private:
    serial_port serial_read;
    serial_port serial_write;
    deadline_timer timer;
    io_service& io_service_;
    array<char, 1024> buffer_read;
    vector<char> buffer_received;
    bool running;
};

SerialReaderWriter* reader_writer_ptr = nullptr;

void signal_handler(int signum) {
    if (reader_writer_ptr != nullptr) {
        reader_writer_ptr->stop();
    }
    exit(signum);
}

int main() {
    const char* read_port = getenv("READ_PORT");
    const char* read_baud_rate_str = getenv("READ_BAUD_RATE");
    const char* write_port = getenv("WRITE_PORT");
    const char* write_baud_rate_str = getenv("WRITE_BAUD_RATE");
    const char* read_parity = getenv("READ_PARITY");
    const char* read_stop_bits = getenv("READ_STOP_BITS");
    const char* read_data_bits_str = getenv("READ_DATA_BITS");
    const char* write_parity = getenv("WRITE_PARITY");
    const char* write_stop_bits = getenv("WRITE_STOP_BITS");
    const char* write_data_bits_str = getenv("WRITE_DATA_BITS");

    if (!read_port || !read_baud_rate_str || !write_port || !write_baud_rate_str ||
        !read_parity || !read_stop_bits || !read_data_bits_str || !write_parity || !write_stop_bits || !write_data_bits_str) {
        cerr << "Environment variables READ_PORT, READ_BAUD_RATE, WRITE_PORT, WRITE_BAUD_RATE, READ_PARITY, READ_STOP_BITS, READ_DATA_BITS, WRITE_PARITY, WRITE_STOP_BITS, and WRITE_DATA_BITS must be set." << endl;
        return 1;
    }

    int read_baud_rate = atoi(read_baud_rate_str);
    int write_baud_rate = atoi(write_baud_rate_str);
    int read_data_bits = atoi(read_data_bits_str);
    int write_data_bits = atoi(write_data_bits_str);

    io_service io;
    SerialReaderWriter reader_writer(io, read_port, read_baud_rate, write_port, write_baud_rate, 
                                     read_parity, read_stop_bits, read_data_bits, write_parity, write_stop_bits, write_data_bits);
    reader_writer_ptr = &reader_writer;

    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    reader_writer.run();

    return 0;
}
