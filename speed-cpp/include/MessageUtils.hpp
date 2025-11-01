#include "BinaryManager.hpp"
#include "EncryptionManager.hpp"

namespace SPEED {
namespace MessageUtils {
Message construct_MSG(const std::string &msg);
Message construct_CON_REQ(const std::string &reciever_name);
Message construct_CON_RES(const std::string &reciever_name);
Message construct_INVOKE_METHOD(const std::string &method_name,
                                const std::string &reciever_name);
Message construct_EXIT_NOTIF(const std::string &reciever_name);
Message construct_PING(const std::string &rec_name);
Message construct_PONG(const std::string &reciever_name);
PMessage destruct_message(const Message &message);
bool validate_message_sent(const Message &message,
                           const std::string &self_proc_name,
                           const std::string &reciever_name);
bool validate_message_recieved(const Message &message,
                               const std::string &self_proc_name);
void print_message(const Message &message);
} // namespace MessageUtils
} // namespace SPEED