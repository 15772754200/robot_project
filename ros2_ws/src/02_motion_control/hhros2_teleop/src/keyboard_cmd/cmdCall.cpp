/*************************************************************************
**  说明：该文件主要功能用于集所有功能指令处理函数，具体每个指令的详细功能和参数含义可以查看“中移人形功能指令文档.xlsx”文件
**************************************************************************/
#include "hhros2_teleop/keyboard_cmd/cmdCall.h"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "hhros2_log/log.h"
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <ament_index_cpp/get_package_prefix.hpp>

namespace hhros2_teleop {

// 静态成员初始化
std::string CmdCall::package_share_;
std::string CmdCall::package_prefix_;

namespace {
struct TmuxContext {
    std::string session_id;
    std::string window_id;
    std::string pane_id;

    bool valid() const {
        return !session_id.empty() && !window_id.empty();
    }
};

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string getenv_or_empty(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string() : std::string(value);
}

std::string trim_copy(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           (value[begin] == ' ' || value[begin] == '\t' ||
            value[begin] == '\n' || value[begin] == '\r')) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' ||
            value[end - 1] == '\n' || value[end - 1] == '\r')) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string command_output(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return std::string();
    }

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    pclose(pipe);
    return trim_copy(output);
}

std::string current_control_tty() {
    int fd = open("/dev/tty", O_RDONLY | O_NOCTTY);
    if (fd >= 0) {
        char* tty_name = ttyname(fd);
        std::string result = tty_name == nullptr ? std::string() : std::string(tty_name);
        close(fd);
        return result;
    }

    int fds[] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
    for (int candidate_fd : fds) {
        char* tty_name = ttyname(candidate_fd);
        if (tty_name != nullptr) {
            return std::string(tty_name);
        }
    }
    return std::string();
}

TmuxContext tmux_context_from_tty(const std::string& tty_name) {
    TmuxContext context;
    if (tty_name.empty()) {
        return context;
    }

    std::string panes = command_output(
        "tmux list-panes -a -F " +
        shell_quote("#{pane_tty}|#{session_id}|#{window_id}|#{pane_id}") +
        " 2>/dev/null");

    size_t line_begin = 0;
    while (line_begin < panes.size()) {
        size_t line_end = panes.find('\n', line_begin);
        std::string line = panes.substr(
            line_begin,
            line_end == std::string::npos ? std::string::npos : line_end - line_begin);

        size_t first_sep = line.find('|');
        size_t second_sep = first_sep == std::string::npos ? std::string::npos : line.find('|', first_sep + 1);
        size_t third_sep = second_sep == std::string::npos ? std::string::npos : line.find('|', second_sep + 1);
        if (first_sep != std::string::npos &&
            second_sep != std::string::npos &&
            third_sep != std::string::npos &&
            line.substr(0, first_sep) == tty_name) {
            context.session_id = line.substr(first_sep + 1, second_sep - first_sep - 1);
            context.window_id = line.substr(second_sep + 1, third_sep - second_sep - 1);
            context.pane_id = line.substr(third_sep + 1);
            return context;
        }

        if (line_end == std::string::npos) {
            break;
        }
        line_begin = line_end + 1;
    }

    return context;
}

bool command_succeeded(int return_code) {
    if (return_code == -1) {
        return false;
    }
    if (WIFEXITED(return_code)) {
        return WEXITSTATUS(return_code) == 0;
    }
    return return_code == 0;
}
}

CmdCall::CmdCall(){
}

CmdCall::~CmdCall(){
}

/*************************************************************************
**  函数名：  handleQpiCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：qpi指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void CmdCall::handleQpiCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行qpi命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) 
        std::cout << "错误: qpi命令至少需要一个参数" << std::endl;
    return;
}

/*************************************************************************
**  函数名：  handleEpfunCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：epfun指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void CmdCall::handleEpfunCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行epfun命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) 
        std::cout << "错误: epfun命令至少需要一个参数" << std::endl;
    return;
}

/*************************************************************************
**  函数名：  handlePcelCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：pcel指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void CmdCall::handlePcelCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行pcel命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: pcel命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "pcel命令执行成功" << std::endl;
}


/*************************************************************************
**  函数名：  handleScparCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：scpar指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void CmdCall::handleScparCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行scpar命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: scpar命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "scpar命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSbmsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：sbms指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSbmsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行sbms命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: sbms命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "sbms命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSusbCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：susb指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSusbCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行susb命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: susb命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "susb命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSspiCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：sspi指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSspiCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行sspi命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: sspi命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "sspi命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSenetCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：senet指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSenetCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行senet命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: senet命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "senet命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSecatCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：secat指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSecatCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行secat命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: secat命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "secat命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleMsitCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：msit指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleMsitCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行msit命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: msit命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "msit命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleLrtcCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lrtc指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLrtcCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lrtc", "rtc", "exec_read.sh", false);
}

/*************************************************************************
**  函数名：  handleSrtcCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：srtc指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSrtcCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("srtc", "rtc", "exec.sh", false);
}

/*************************************************************************
**  函数名：  handleResysCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：resys指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleResysCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行resys命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: resys命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "resys命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleClanCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：clan指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleClanCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("clan", "network", "exec.sh", false);
}

/*************************************************************************
**  函数名：  handleLlanCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：llan指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLlanCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("llan", "network", "exec_read.sh", false);
}

/*************************************************************************
**  函数名：  handleCwifiCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：cwifi指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCwifiCommand(const std::vector<std::string>& args) {
    (void)args;
   executeTerminalScript("cwifi", "network", "wifi_connect.sh", false);
}

/*************************************************************************
**  函数名：  handleLwifiCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lwifi指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLwifiCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lwifi", "network", "exec_wifi_read.sh", false);
}

/*************************************************************************
**  函数名：  handleOn5gCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：on5g指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleOn5gCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行on5g命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: on5g命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "on5g命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleL5ghCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：l5gh指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleL5ghCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行l5gh命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: l5gh命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "l5gh命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleEnbtCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：enbt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEnbtCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("enbt", "bluetooth", "exec.sh", false);
}

/*************************************************************************
**  函数名：  handleLbthCommand()  查询蓝牙连接历史信息到日志文件
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lbth指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLbthCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lbth", "bluetooth", "exec_read.sh", false);
}

/*************************************************************************
**  函数名：  handleNshCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：nsh指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleNshCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("nsh", "network", "exec_share.sh", true);
}

/*************************************************************************
**  函数名：  handleLnshCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lnsh指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLnshCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lnsh", "network", "exec_read_share.sh", false);
}

/*************************************************************************
**  函数名：  handleEnmicCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：enmic指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEnmicCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("enmic", "mic/use_mic","install.sh", false);
}

/*************************************************************************
**  函数名：  handleLmichCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lmich指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLmichCommand(const std::vector<std::string>& args) {
    (void)args;
   executeTerminalScript("lmich", "mic/use_mic_history","install.sh", false);
}

/*************************************************************************
**  函数名：  handleEnspkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：enspk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEnspkCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("enspk", "speaker/use_speaker","install.sh", false);
}

/*************************************************************************
**  函数名：  handleLspkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lspk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLspkCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行lspk命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: lspk命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "lspk命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleEndcamCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：endcam指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEndcamCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("endcam", "camera","start_rgbd.sh", false);
}

/*************************************************************************
**  函数名：  handleLdcamCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ldcam指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLdcamCommand(const std::vector<std::string>& args) {
    (void)args;
   executeTerminalScript("ldcam", "camera","start_log_read_RGBD.sh", false);
}

/*************************************************************************
**  函数名：  handleEnrgbCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：enrgb指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEnrgbCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("enrgb", "camera","start_rgb.sh", false);
}

/*************************************************************************
**  函数名：  handleLrgbCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lrgb指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLrgbCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lrgb", "camera","start_log_read_RGB.sh", false);
}

/*************************************************************************
**  函数名：  handleEnlidCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：enlid指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEnlidCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("enlid", "lidar","start_lidar.sh", false);
}

/*************************************************************************
**  函数名：  handleLlidCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：llid指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLlidCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行llid命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: llid命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "llid命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleEngpsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：engps指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEngpsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行engps命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: engps命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "engps命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleLgpsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lgps指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLgpsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行lgps命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: lgps命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "lgps命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleEnrtkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：enrtk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleEnrtkCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行enrtk命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: enrtk命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "enrtk命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleLrtkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lrtk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLrtkCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行lrtk命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: lrtk命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "lrtk命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleGimudCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：gimud指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleGimudCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行gimud命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: gimud命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "gimud命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleCalimuCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：calimu指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCalimuCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行calimu命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: calimu命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "calimu命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleGtofCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：gtof指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleGtofCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行gtof命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: gtof命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "gtof命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleQtofCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：Qtof指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleQtofCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行qtof命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: qtof命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "qtof命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleGtsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：gts指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleGtsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行gts命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: gts命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "gts命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleQtshCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：qtsh指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleQtshCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行qtsh命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: qtsh命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "qtsh命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleGbldcCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：gbldc指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleGbldcCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行gbldc命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: gbldc命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "gbldc命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleQbldcCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：qbldc指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleQbldcCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行qbldc命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: qbldc命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "qbldc命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleAdstaCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：adsta指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAdstaCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行adsta命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: adsta命令至少需要一个参数" << std::endl;
        return;
    }

    // 具体功能实现
    std::cout << "adsta命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleDelstaCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：delsta指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleDelstaCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行delsta命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: delsta命令至少需要一个参数" << std::endl;
        return;
    }

    // 具体功能实现
    std::cout << "delsta命令执行成功" << std::endl;
}

/// @brief 控制机器人模式指令执行函数
/// @param args 
void CmdCall::handleControlCommand(const std::vector<std::string>& args){
    (void)args;
    executeTerminalScript("control", "set_command/control_system" ,"install.sh", false);
}

/// @brief 调用机器人控制模式和速度
/// @param args 
void CmdCall::handleSetCommand(const std::vector<std::string>& args){
    (void)args;
    executeTerminalScript("set", "set_command/set_system" ,"install.sh", false);
}

/// @brief 调用机器人控制模式和速度
/// @param args 
void CmdCall::handleDeveloperCommand(const std::vector<std::string>& args){
    (void)args;
    executeTerminalScript("dev", "set_command/developer_system" ,"install.sh", false);
}

void CmdCall::handleSystemCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("system", "system" ,"install.sh", false);
}

void CmdCall::handlePeripheralCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("peripheral", "peripheral_system" ,"install.sh", false);
}

void CmdCall::handleRGBCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("rgb", "rgb_system" ,"install.sh", false);
}
void CmdCall::handleRGBDCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("rgbd", "rgbd_system" ,"install.sh", false);
}


void CmdCall::handleCameraInterfaceCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("camera_interface", "camera_interface_system" ,"install.sh", false);
}

void CmdCall::handleNetworkCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("network", "network_system" ,"install.sh", false);
}

void CmdCall::handleImuCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("imu", "imu_system" ,"install.sh", false);
}

void CmdCall::handleMicCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("mic", "mic_system" ,"install.sh", false);
}

void CmdCall::handleBuleRtcCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("bule_rtc", "bule_rtc_system" ,"install.sh", true);
}
/*************************************************************************
**  函数名：  handleRobonCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：robon指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRobonCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("robon", "set_command/robot_on" ,"install.sh", false);
}


/*************************************************************************
**  函数名：  handleRobstpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：robstp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRobstpCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("robstp", "set_command/robot_stop" ,"install.sh", false);
}

/*************************************************************************
**  函数名：  handleBldchCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：bldch指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleBldchCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行bldch命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: bldch命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "bldch命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleImuchCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：imuch指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleImuchCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行imuch命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: imuch命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "imuch命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleDcchCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：dcch指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleDcchCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("dcch", "camera" ,"camera_self_check.sh", false);
}

/*************************************************************************
**  函数名：  handleCmchCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：cmch指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCmchCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行cmch命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: cmch命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "cmch命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRpchCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rpch指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRpchCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rpch命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rpch命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rpch命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handlePwchCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：pwch指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handlePwchCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行pwch命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: pwch命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "pwch命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleLichCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lich指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLichCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lich", "network" ,"exec_self_check.sh", false);
}

/*************************************************************************
**  函数名：  handleAzroCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：azro指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAzroCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行azro命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: azro命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "azro命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleLmtsfCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lmtsf指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLmtsfCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行lmtsf命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: lmtsf命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "lmtsf命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRbmsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rbms指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRbmsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rbms命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rbms命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rbms命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleLbtmpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lbtmp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLbtmpCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行lbtmp命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: lbtmp命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "lbtmp命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleChdsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：chds指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleChdsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行chds命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: chds命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "chds命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleBtverCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：btver指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleBtverCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行btver命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: btver命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "btver命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleCtwlkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ctwlk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCtwlkCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("ctwlk", "set_command/control_walk", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleCtstwlkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ctstwlk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCtstwlkCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("ctstwlk", "set_command/control_stop_walk", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleCtslpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ctslp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCtslpCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("ctslp", "set_command/control_walk_slope", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleCtstslpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ctstslp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCtstslpCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("ctstslp", "set_command/control_stop_walk_slope", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleWgtCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：wgt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleWgtCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行wgt命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: wgt命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "wgt命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleCgwCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：cgw指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCgwCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行cgw命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: cgw命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "cgw命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleStpwCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：stpw指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

//后续训练不同高度的站立，模型
void CmdCall::handleStpwCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行stpw命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: stpw命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "stpw命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleStdCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：std指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleStdCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("std", "set_command/control_stand", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleCstdCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：cstd指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCstdCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行cstd命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: cstd命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "cstd命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleStpstdCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：stpstd指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleStpstdCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("stpstd", "set_command/control_stop_stand", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleJmpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：jmp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleJmpCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("jmp", "set_command/set_jump", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleCjmpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：cjmp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCjmpCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行cjmp命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: cjmp命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "cjmp命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleStpjmpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：stpjmp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleStpjmpCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("stpjmp", "set_command/control_stop_jump", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleRbtrunCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rbtrun指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRbtrunCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("rbtrun", "set_command/control_run", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleCrbtrunCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：crbtrun指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCrbtrunCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("crbtrun", "set_command/set_run_change", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleStprunCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：stprun指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleStprunCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("stprun", "set_command/control_stop_run", "install.sh", false);
}

/*************************************************************************
**  函数名：  handlePidmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：pidm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handlePidmCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行pidm命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: pidm命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "pidm命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handlePidoptCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：pidopt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handlePidoptCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行pidopt命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: pidopt命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "pidopt命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIciCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ici指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIciCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行ici命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: ici命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "ici命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIcioptCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：iciopt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIcioptCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行iciopt命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: iciopt命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "iciopt命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSeCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：se指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSeCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行se命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: se命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "se命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleAseCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ase指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAseCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行ase命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: ase命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "ase命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleFoptCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：fopt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleFoptCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行fopt命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: fopt命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "fopt命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRopCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rop指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRopCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rop命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rop命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rop命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handlePrsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：prs指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handlePrsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行prs命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: prs命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "prs命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleApaCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：apa指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleApaCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行apa命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: apa命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "apa命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleDevpsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：devps指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleDevpsCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("devps", "system", "process.sh", false);
}

/*************************************************************************
**  函数名：  handleAdtpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：adtp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAdtpCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("adtp", "system", "change_process.sh", false);
}

/*************************************************************************
**  函数名：  handleLpmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：lpm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLpmCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("lpm", "system", "process_log.sh", false);
}

/*************************************************************************
**  函数名：  handleQstgCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：qstg指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleQstgCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("qstg", "system", "storage_space.sh", true);
}

/*************************************************************************
**  函数名：  handleCstgCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：cstg指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleCstgCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("cstg", "system", "clear_storage.sh", true);
}

/*************************************************************************
**  函数名：  handleJmcmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：jmcm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleJmcmCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行jmcm命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: jmcm命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "jmcm命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRjmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rjm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRjmCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rjm命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rjm命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rjm命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleTcifCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：tcif指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleTcifCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行tcif命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: tcif命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "tcif命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleMfcifCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：mfcif指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleMfcifCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行mfcif命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: mfcif命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "mfcif命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRmiptCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rmipt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRmiptCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rmipt命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rmipt命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rmipt命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRsmmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rsmm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRsmmCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rsmm命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rsmm命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rsmm命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRjpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rjp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRjpCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rjp命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rjp命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rjp命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSwtsmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：swtsm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSwtsmCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("swtsm", "set_command/change_mode", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleWlkgtCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：wlkgt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleWlkgtCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("wlkgt", "set_command/set_walk", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleRgtCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rgt指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRgtCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("rgt", "set_command/set_run", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleSrjavCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：srjav指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSrjavCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行srjav命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: srjav命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "srjav命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleSlvCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：slv指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSlvCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("slv", "set_command/set_ang_vel", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleAteiCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：atei指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAteiCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("atei", "set_command/move_forward", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleSactCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：sact指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSactCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("sact", "set_command/set_stand", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleLtwcCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ltwc指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleLtwcCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("ltwc", "set_command/move_left_right", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleRtmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rtm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRtmCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("rtm", "set_command/set_vel_yaw", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleJmpactCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：jmpact指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleJmpactCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("jmpact", "set_command/set_jump", "install.sh", false);
}

/*************************************************************************
**  函数名：  handleSsrCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ssr指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleSsrCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行ssr命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: ssr命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "ssr命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleImurCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：imur指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleImurCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行imur命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: imur命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "imur命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleImctlCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：imctl指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleImctlCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行imctl命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: imctl命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "imctl命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIspkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ispk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIspkCommand(const std::vector<std::string>& args) {
    (void)args;
    executeTerminalScript("ispk", "speaker/change_speaker","install.sh", false);
}

/*************************************************************************
**  函数名：  handleItsdfCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：itsdf指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleItsdfCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行itsdf命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: itsdf命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "itsdf命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleItofdfCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：itofdf指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleItofdfCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行itofdf命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: itofdf命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "itofdf命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIrddfCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：irddf指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIrddfCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行irddf命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: irddf命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "irddf命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIectcmCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：iectcm指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIectcmCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行iectcm命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: iectcm命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "iectcm命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIdcmaCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：idcma指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIdcmaCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行idcma命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: idcma命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "idcma命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIrcmaCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ircma指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIrcmaCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行ircma命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: ircma命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "ircma命令执行成功" << std::endl;
        return;

    // 具体功能实现
    std::cout << "ircma命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIrtkCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：irtk指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIrtkCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行irtk命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: irtk命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "irtk命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIesCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ies指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIesCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行ies命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: ies命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "ies命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleIpwcCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：ipwc指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleIpwcCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行ipwc命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: ipwc命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "ipwc命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleAtcmpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：atcmp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAtcmpCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行atcmp命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: atcmp命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "atcmp命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleAtopkgCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：atopkg指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAtopkgCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行atopkg命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: atopkg命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "atopkg命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleAtdfCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：atdf指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleAtdfCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行atdf命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: atdf命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "atdf命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleUsrosndCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：usrosnd指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleUsrosndCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行usrosnd命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: usrosnd命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "usrosnd命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleTprlsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：tprls指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleTprlsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行tprls命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: tprls命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "tprls命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleTpsbpCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：tpsbp指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleTpsbpCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行tpsbp命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: tpsbp命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "tpsbp命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleOffndCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：offnd指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleOffndCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行offnd命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: offnd命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "offnd命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleRmsifCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：rmsif指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/

void CmdCall::handleRmsifCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行rmsif命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: rmsif命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "rmsif命令执行成功" << std::endl;
}

/*************************************************************************
**  函数名：  handleTrmsCommand()
**	输入参数：args：指令参数
**	输出参数：
**	函数功能：trms指令执行函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void CmdCall::handleTrmsCommand(const std::vector<std::string>& args) {
    (void)args;
    std::cout << "\n=== 执行trms命令 ===" << std::endl;
    
    // 验证参数数量
    if (args.size() < 2) {
        std::cout << "错误: trms命令至少需要一个参数" << std::endl;
        return;
    }
    
    // 具体功能实现
    std::cout << "trms命令执行成功" << std::endl;
}

/// @brief 系统控制指令
/// @param args 
void CmdCall::handleSysCtrlCommand(const std::vector<std::string>& args){
    (void)args;
    executeTerminalScript("syscon", "system", "install.sh", true);
}

/// @brief 外设管理
/// @param args 
void CmdCall::handlePeriManCommand(const std::vector<std::string>& args){
    (void)args;
    executeTerminalScript("periman", "peripheral_system", "install.sh", true);
}


void CmdCall::executeTerminalScript(const std::string& commandName, const std::string& scriptType, 
    const std::string& script_name, bool use_sudo) {
    std::cout << "\n=== 执行" << commandName << "命令 ===" << std::endl;
    std::string script_path = getScriptPath(scriptType,script_name);
    if (script_path.empty()) {
        LOG_ERROR(LogType::OTHER, "Failed to get script path for type: %s", scriptType.c_str());
        return;
    }
    std::string unique_id = generate_uuid();
    std::string exe_path = package_share_;
    std::string workspace_setup = package_prefix_  + "/../../install/setup.bash";
    std::string script_command = (use_sudo ? "sudo " : "") + 
                                 shell_quote(script_path) + " " +
                                 shell_quote(unique_id) + " " +
                                 shell_quote(exe_path);
    

    std::string chmod_cmd = "chmod +x " + shell_quote(script_path);
    int chmod_result = std::system(chmod_cmd.c_str());
    if (!command_succeeded(chmod_result)) {
        LOG_WARNING(LogType::OTHER, "设置脚本权限失败，尝试继续执行...");
    }

    std::string base_terminal_script = "printf '\\033]0;User终端管理器_" + unique_id + "\\007'; ";
    base_terminal_script += "source " + shell_quote(workspace_setup) + " 2>/dev/null || true; ";
    base_terminal_script += script_command;

    if (geteuid() == 0) {
        if (!command_succeeded(std::system("command -v tmux >/dev/null 2>&1"))) {
            LOG_ERROR(LogType::OTHER, "tmux is unavailable; root terminal script was not started.");
            std::cerr << "Error: 当前为 root 运行，" << commandName
                      << " 需要在 root 终端中执行，但 tmux 不可用。"
                      << std::endl;
            return;
        }

        std::string tmux_env = getenv_or_empty("TMUX");
        std::string tmux_pane = getenv_or_empty("TMUX_PANE");
        std::string tmux_window_name = "robot_" + commandName;
        std::string tmux_script = base_terminal_script;
        std::string tmux_command;
        std::string tmux_session;
        bool created_in_current_tmux = false;

        if (!tmux_env.empty()) {
            std::string current_window_command = "tmux display-message -p";
            if (!tmux_pane.empty()) {
                current_window_command += " -t " + shell_quote(tmux_pane);
            }
            current_window_command += " " + shell_quote("#{window_id}");

            std::string current_window_id = command_output(current_window_command);
            if (!current_window_id.empty()) {
                std::string return_command = "tmux select-window -t " +
                                             shell_quote(current_window_id) +
                                             " 2>/dev/null || true";
                tmux_script = "trap " + shell_quote(return_command) + " EXIT; " + tmux_script;
            }

            std::string tmux_shell_command = "bash -lc " + shell_quote(tmux_script);
            tmux_command = "tmux new-window -n " +
                           shell_quote(tmux_window_name) +
                           " " + shell_quote(tmux_shell_command);
            created_in_current_tmux = true;
        } else {
            TmuxContext tmux_context = tmux_context_from_tty(current_control_tty());
            if (tmux_context.valid()) {
                std::string return_command = "tmux select-window -t " +
                                             shell_quote(tmux_context.window_id) +
                                             " 2>/dev/null || true";
                tmux_script = "trap " + shell_quote(return_command) + " EXIT; " + tmux_script;

                std::string tmux_shell_command = "bash -lc " + shell_quote(tmux_script);
                tmux_command = "tmux new-window -t " +
                               shell_quote(tmux_context.session_id) +
                               " -n " + shell_quote(tmux_window_name) +
                               " " + shell_quote(tmux_shell_command);
                created_in_current_tmux = true;
            }
        }

        if (tmux_command.empty()) {
            tmux_session = "robot_ctrl_" + commandName + "_" + unique_id.substr(0, 8);
            std::string tmux_shell_command = "bash -lc " + shell_quote(tmux_script);
            tmux_command = "tmux new-session -d -s " +
                           shell_quote(tmux_session) +
                           " " + shell_quote(tmux_shell_command);
        } else {
            tmux_session.clear();
        }

        int tmux_return_code = std::system(tmux_command.c_str());
        if (!command_succeeded(tmux_return_code)) {
            LOG_ERROR(LogType::OTHER, "tmux root terminal script launch failed!");
            std::cerr << "Error: " << commandName
                      << " 脚本的 root tmux 会话启动失败。"
                      << std::endl;
            return;
        }

        if (created_in_current_tmux) {
            std::cout << "✓ 已在当前 tmux 会话中新建窗口运行 " << commandName << " 脚本" << std::endl;
            std::cout << "  脚本退出后会自动关闭窗口并返回主控窗口" << std::endl;
        } else {
            std::cout << "✓ 已在 root tmux 会话中启动 " << commandName << " 脚本" << std::endl;
            std::cout << "  未检测到当前 tmux pane，已创建独立后台会话: " << tmux_session << std::endl;
            std::cout << "  查看/控制: tmux attach -t " << tmux_session << std::endl;
        }
        return;
    }

    if (!command_succeeded(std::system("command -v gnome-terminal >/dev/null 2>&1"))) {
        LOG_ERROR(LogType::OTHER, "gnome-terminal is unavailable; terminal script was not started.");
        std::cerr << "Error: 无法启动 " << commandName
                  << " 脚本所需的新终端（gnome-terminal 不可用）。"
                  << std::endl;
        return;
    }

    std::string terminal_command = "gnome-terminal --title=" +
                                   shell_quote("User终端管理器_" + unique_id) +
                                   " -- bash -c " + shell_quote(base_terminal_script + "; exec bash");
    std::string command = terminal_command;
    
    // 调用system函数执行命令
    int return_code = std::system(command.c_str());
    
    // 检查命令是否执行成功
    if (!command_succeeded(return_code)) {
        LOG_ERROR(LogType::OTHER, "Terminal script launch failed!");
        std::cerr << "Error: " << commandName
                  << " 脚本的新终端启动失败，未在 robot_ctrl 当前进程中执行。"
                  << std::endl;
    }
}

/*************************************************************************
**  函数名：  getScriptPath()
**  输入参数：scriptType - 脚本类型（如 "system", "mic/use_mic" 等） script_name 脚本名称
**  输出参数：std::string - 脚本完整路径，若失败则返回空字符串
**  函数功能：根据脚本类型拼接出 pyscript 目录下的 install.sh 完整路径
**  作者：    wk
**  开发日期：2026/07/23
**************************************************************************/
std::string CmdCall::getScriptPath(const std::string& scriptType, const std::string& script_name) {
    try {
        package_share_ = ament_index_cpp::get_package_share_directory("hhros2_teleop");
        package_prefix_ = ament_index_cpp::get_package_prefix("hhros2_teleop");
    } catch (const std::exception& e) {
        LOG_ERROR(LogType::OTHER, "Failed to get package directory: %s", e.what());
        return std::string();
    }

    return package_share_ + "/pyscript/" + scriptType + "/" + script_name;
}


// 生成UUID（36位字符串，如550e8400-e29b-41d4-a716-446655440000）
std::string CmdCall::generate_uuid() {
    uuid_t uuid;
    uuid_generate_random(uuid); // 生成随机UUID
    char uuid_str[37]; // 36位+结束符
    uuid_unparse(uuid, uuid_str); // 转换为字符串
    return std::string(uuid_str);
}

} // namespace hhros2_teleop