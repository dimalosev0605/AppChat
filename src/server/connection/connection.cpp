#include <server/connection/connection.h>

void Connection::send(const Message& message) {
    bool write_in_progress = !write_mess.empty();
    write_mess.push_back(message);
    if (!write_in_progress) {
        send_to_client();
    }
}

void Connection::read_request_header() {
    boost::system::error_code error;
    input_req_ptr input_request = std::make_shared<AutorisationRequest>();
    boost::asio::read(socket, boost::asio::buffer(input_request->get_data(), Block::Header), error);
    if (!error) {
        if (input_request->get_type_data() == static_cast<uint16_t>(TypeCommand::RegistrationRequest)
            || input_request->get_type_data() == static_cast<uint16_t>(TypeCommand::AuthorisationRequest))
        {
                read_input_request_body(input_request);
        }
    } else {
        std::cout << "error request: " << input_request->get_protocol_version() << " and " << input_request->get_type_data() << std::endl;
        socket.close();
    }
}

void Connection::read_input_request_body(input_req_ptr input_request) {
    auto self(shared_from_this());
    boost::asio::async_read(socket, boost::asio::buffer(input_request->get_optional(), Block::InputOption),
        [this, self, input_request](boost::system::error_code error, std::size_t) {
            if (!error) {
                login = input_request->get_login();
                password = input_request->get_password();

                input_res_ptr input_response = std::make_shared<AutorisationResponse>(client_id);
                std::cout << "write: " << input_response->get_protocol_version() << input_response->get_type_data() << " "
                          << input_response->get_loginid() << std::endl;
                boost::asio::write(socket, boost::asio::buffer(input_response->get_data(), input_response->get_length_response()));

                ChannelsManager::Instance().join(self, 0);
                do_read_header();
            }
            else {
                socket.close();
            }
    });
}

void Connection::do_read_header() {
    auto self(shared_from_this());
    boost::asio::async_read(socket, boost::asio::buffer(read_mes.get_buf_data(), Message::header_size),
        [this, self](boost::system::error_code error, std::size_t) {
            if (!error) {
                if (read_mes.decode_header()) {
                    do_read_body();
                }
            }
            else {
//                std::cout << "need leave" << std::endl;
                ChannelsManager::Instance().leave(self);
            }
        });
    }

void Connection::do_read_body() {
    auto self(shared_from_this());
    boost::asio::async_read(socket, boost::asio::buffer(read_mes.get_buf_id_login(), Message::Settings_zone + read_mes.get_body_length()),
        [this, self](boost::system::error_code error, std::size_t) {
            *(read_mes.get_buf_body()+read_mes.get_body_length()) = '\0';
            if (!error) {
                if (read_mes.get_body_length()) {
                    std::cout << read_mes << std::endl;
                    ChannelsManager::Instance().send(read_mes);
                }
                do_read_header();
            }
            else {
                ChannelsManager::Instance().leave(self);
            }
        }
    );
}

void Connection::send_to_client() {
    auto self(shared_from_this());
    boost::asio::async_write(socket,
        boost::asio::buffer(write_mess.front().get_buf_data() , write_mess.front().get_mes_length()),
            [this, self](boost::system::error_code error, std::size_t) {
                if (!error) {
                    write_mess.pop_front();
                    if (!write_mess.empty()) {
                        send_to_client();
                    }
                } else {
                    std::cout << "error async_write when write" << std::endl;
                    ChannelsManager::Instance().leave(self);
                }
            }
    );
}
