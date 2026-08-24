// (2) A realistic pimpl type handed to the launcher, with no opt-in written.
#include <threadsafe/threadsafe.h>

#include <cstdio>
#include <memory>
#include <string>

// ---- session.h (what a user's header looks like) ------------------------
class Session {
public:
    explicit Session(std::string endpoint);
    ~Session();
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;

    void send(const std::string& payload) const;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

// ---- session.cpp --------------------------------------------------------
struct Session::Implementation {
    std::string endpoint;
};

Session::Session(std::string endpoint)
    : implementation_(
          std::make_unique<Implementation>(Implementation{std::move(endpoint)})) {}
Session::~Session() = default;
Session::Session(Session&& other) noexcept = default;
Session& Session::operator=(Session&& other) noexcept = default;
void Session::send(const std::string& payload) const {
    std::printf("%s <- %s\n", implementation_->endpoint.c_str(),
                payload.c_str());
}

// ---- main.cpp -----------------------------------------------------------
int main() {
    threadsafe::asynchronous_task_launcher launcher;
    launcher.launch_task(
        [](Session session) { session.send("hello"); },
        Session{"tcp://localhost:1234"});
}
