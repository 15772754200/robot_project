#include "hhros2_teleop/keyboard_cmd/keyboard.h"
#include <sstream>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <csignal>

namespace hhros2_teleop {

namespace {
int g_terminal_fd = -1;
bool g_terminal_saved = false;
struct termios g_saved_terminal_settings{};

void restore_terminal_state() {
    if (g_terminal_saved && g_terminal_fd >= 0) {
        tcsetattr(g_terminal_fd, TCSANOW, &g_saved_terminal_settings);
    }
}

void terminal_signal_handler(int signal_number) {
    restore_terminal_state();
    std::_Exit(128 + signal_number);
}

bool read_line_from_fd(int fd, std::string& line) {
    line.clear();
    if (fd < 0) {
        return false;
    }

    char ch = '\0';
    while (true) {
        const ssize_t bytes_read = read(fd, &ch, 1);
        if (bytes_read > 0) {
            if (ch == '\n' || ch == '\r') {
                return true;
            }
            line.push_back(ch);
            continue;
        }

        if (bytes_read == 0) {
            return !line.empty();
        }

        if (errno == EINTR) {
            continue;
        }
        return false;
    }
}

void install_terminal_restore_handlers() {
    static bool installed = false;
    if (installed) {
        return;
    }

    installed = true;
    std::atexit(restore_terminal_state);
    std::signal(SIGINT, terminal_signal_handler);
    std::signal(SIGTERM, terminal_signal_handler);
    std::signal(SIGHUP, terminal_signal_handler);
    std::signal(SIGQUIT, terminal_signal_handler);
}
}

KeyBoard::KeyBoard():keymode_(KeyMode::KEY_F2), input_fd_(-1), robot_server(nullptr) {

    // 参数初始化
    axis_linear_x = 0.0f;
    axis_linear_y = 0.0f;
    axis_angular_z = 0.0f;
    max_linear_x = 0.6f;
    max_linear_y = 0.3f;
    max_angular_z = 1.0f;
    min_linear_x = -0.6f;
    min_linear_y = -0.3f;
    min_angular_z = -1.0f;
    linear_increment = 0.05f;
    angular_increment = 0.05f;    

    last_key = current_key = Key::KEY_NONE;
    press_m_count = keymode_ == KeyMode::KEY_F1 ? 0 : 1;
    input_fd_ = open("/dev/tty", O_RDWR | O_NOCTTY);
    if (input_fd_ < 0) {
        input_fd_ = fileno(stdin);
        std::cerr << "Warning: failed to open /dev/tty, fallback to stdin: "
                  << std::strerror(errno) << std::endl;
    }

    // 直接操作控制终端，避免 ros2 launch 转发 stdin 时键盘失效
    tcgetattr(input_fd_, &_oldSettings);
    g_terminal_fd = input_fd_;
    g_saved_terminal_settings = _oldSettings;
    g_terminal_saved = true;
    install_terminal_restore_handlers();
    _newSettings = _oldSettings;
    _newSettings.c_lflag &= (~ICANON & ~ECHO);
    pthread_create(&_tid, NULL, runKeyBoard, (void*)this);
}

KeyBoard::~KeyBoard(){
    if (robot_server){
        robot_server->stopServer();
        delete robot_server;
    }
    pthread_cancel(_tid);// 取消线程
    pthread_join(_tid,NULL);// 等待线程结束
    // 线程彻底退出后再恢复终端，避免析构期间被重新切回 raw 模式
    tcsetattr(input_fd_, TCSANOW, &_oldSettings);
    g_terminal_saved = false;
    g_terminal_fd = -1;
    if (input_fd_ >= 0 && input_fd_ != fileno(stdin)) {
        close(input_fd_);
    }
}

void* KeyBoard::runKeyBoard(void *arg){
    ((KeyBoard*)arg)->run(NULL);
    return NULL;
}

void KeyBoard::cleanupTerminal(void* arg) {
    auto* keyboard = static_cast<KeyBoard*>(arg);
    if (keyboard != nullptr && keyboard->input_fd_ >= 0) {
        tcsetattr(keyboard->input_fd_, TCSANOW, &keyboard->_oldSettings);
    }
}

void KeyBoard::setModeCallback(std::function<void(uint8_t)> cb) {
    mode_cb_ = std::move(cb);
}

void KeyBoard::setVelCallback(std::function<void(double,double,double)> cb) {
    vel_cb_ = std::move(cb);
}

/*************************************************************************
**  函数名：  run()
**	输入参数：
**	输出参数：
**	函数功能：该线程入口函数，真正执行的函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void* KeyBoard::run(void *arg){
    (void)arg;
    registerAllCommand();

    pthread_cleanup_push(&KeyBoard::cleanupTerminal, this);
    while(1){
        // 根据press_m_count的奇偶性决定模式
        if(keymode_ == KeyMode::KEY_F1){
            // press_m_count为偶数：多字符模式
            tcsetattr(input_fd_, TCSANOW, &_oldSettings);
            handleMultiCharInput();
        }
        else if(keymode_ == KeyMode::KEY_F2){
            // press_m_count为奇数：单字符模式
            tcsetattr(input_fd_, TCSANOW, &_newSettings);
            handleSingleCharInput();
        }
    }
    pthread_cleanup_pop(1);
    return NULL;
}

// 处理多字符输入
void KeyBoard::handleMultiCharInput() {
    std::cout << "当前为多字符输入模式, 需要配合ENTER按键使用（输入'm'或'M'将模式切换至单字符输入模式）" << std::endl;
    std::string inputstr;
     
    while (press_m_count % 2 == 0) {  // 保持多字符模式直到press_m_count变为奇数
        std::cout << ">>> " << std::flush;

        if (!read_line_from_fd(input_fd_, inputstr)) {
            std::cerr << "Warning: failed to read command from terminal: "
                      << std::strerror(errno) << std::endl;
            usleep(1000);
            continue;
        }
        
        if (inputstr.empty()) continue;
        
        // 检查是否按下M或m键
        if (inputstr == "m" || inputstr == "M") {
            press_m_count++;  // press_m_count自加一，变为奇数
            keymode_ = KeyMode::KEY_F2;
            break;  // 退出多字符模式循环，返回到主循环进行模式切换
        }
        
        // 正常命令处理
        if (!parseAndExecute(inputstr)) {
            std::cout << "错误: 未知命令 '" << inputstr << "'" << std::endl;
        }
        usleep(1000); // 减少CPU占用
    }
}

// 处理单字符输入
void KeyBoard::handleSingleCharInput() {
    std::cout << "当前为单字符输入模式（输入'm'或'M'将模式切换至多字符输入模式）" << std::endl;
    char c;
    
    while (press_m_count % 2 == 1) {  // 保持单字符模式直到press_m_count变为偶数
        if(read(input_fd_, &c, 1) > 0) {
            // 处理普通字符
            processSingleChar(c);
            
            // 检查是否按下M或m键
            if (c == 'm' || c == 'M') {
                press_m_count++;  // press_m_count自加一，变为偶数
                keymode_ = KeyMode::KEY_F1;
                break;  // 退出单字符模式循环，返回到主循环进行模式切换
            }
        }
        usleep(1000); // 减少CPU占用
    }
}
// 处理单个普通字符
void KeyBoard::processSingleChar(char c) {
    char upperChar = std::toupper(static_cast<unsigned char>(c));
    // 数字键直接请求模式
    if (upperChar >= '0' && upperChar <= '9') {
        if (mode_cb_) {
            mode_cb_(upperChar - '0');
        }
        return;
    }
    // 速度控制键 W/A/S/D/Q/E/R
    if (upperChar == 'W' || upperChar == 'S' || upperChar == 'A' ||
        upperChar == 'D' || upperChar == 'Q' || upperChar == 'E' || upperChar == 'R') {
        static double x=0, y=0, z=0;
        switch (upperChar) {
            case 'W': 
                axis_linear_x += linear_increment; 
                axis_linear_x > max_linear_x ? axis_linear_x = max_linear_x : axis_linear_x;
                axis_linear_x < min_linear_x ? axis_linear_x = min_linear_x : axis_linear_x;
                break;
            case 'S': 
                axis_linear_x -= linear_increment; 
                axis_linear_x > max_linear_x ? axis_linear_x = max_linear_x : axis_linear_x;
                axis_linear_x < min_linear_x ? axis_linear_x = min_linear_x : axis_linear_x;
                break;
            case 'A': 
                axis_linear_y += linear_increment; 
                axis_linear_y > max_linear_y ? axis_linear_y = max_linear_y : axis_linear_y;
                axis_linear_y < min_linear_y ? axis_linear_y = min_linear_y : axis_linear_y;
                break;
            case 'D': 
                axis_linear_y -= linear_increment; 
                axis_linear_y > max_linear_y ? axis_linear_y = max_linear_y : axis_linear_y;
                axis_linear_y < min_linear_y ? axis_linear_y = min_linear_y : axis_linear_y;
                break;
            case 'Q': 
                axis_angular_z += angular_increment; 
                axis_angular_z > max_angular_z ? axis_angular_z = max_angular_z : axis_angular_z;
                axis_angular_z < min_angular_z ? axis_angular_z = min_angular_z : axis_angular_z;
                break;
            case 'E': 
                axis_angular_z -= angular_increment; 
                axis_angular_z > max_angular_z ? axis_angular_z = max_angular_z : axis_angular_z;
                axis_angular_z < min_angular_z ? axis_angular_z = min_angular_z : axis_angular_z;
                break;
            case 'R': 
                axis_linear_x=axis_linear_y=axis_angular_z=0.0f; 
                break;
        }
        if (vel_cb_) {
            vel_cb_(axis_linear_x, axis_linear_y, axis_angular_z);
        }
        return;
    }
    // 其他单字符不处理
    current_key = Key::KEY_NONE;
}
/*************************************************************************
**  函数名：  parseAndExecute()
**	输入参数：inputstr：用户输入的指令字符串
**	输出参数：
**	函数功能：解析并执行输入指令
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
bool KeyBoard::parseAndExecute(const std::string& inputstr) {
    std::vector<std::string> tokens;
    std::stringstream strstm(inputstr);//字符串流对象，用于解析和分割输入的字符串
    std::string token;
    
    //分割输入字符串
    while (strstm >> token) {
        tokens.push_back(token);
    }
    if (tokens.empty()) {
        return false;
    }
    std::string command = tokens[0];
    if (commandHandlers.find(command) != commandHandlers.end()) {
        commandHandlers[command](tokens);
        return true;
    } else {
        return false;
    }
    return true;
}

/*************************************************************************
**  函数名：  ServerCommandHandle()
**	输入参数：inputstr:远程发送过来的指令内容
**	输出参数：
**	函数功能：处理客户远程发送过来的指令内容
**  作者：    wk
**  开发日期：2025/11/12
**************************************************************************/
bool KeyBoard::ServerCommandHandle(const std::string& inputstr)
{
    std::vector<std::string> tokens;
    std::stringstream strstm(inputstr);//字符串流对象，用于解析和分割输入的字符串
    std::string token;
    
    //分割输入字符串
    while (strstm >> token) {
        tokens.push_back(token);
    }
    
    if (tokens.empty()) {
        return false;
    }
    
    std::string command = tokens[0];
    char cmdChar = std::toupper(static_cast<unsigned char>(command[0]));
    
    if(command.length() == 1) {
        processSingleChar(cmdChar);
    }else{
        if (commandHandlers.find(command) != commandHandlers.end()) {
            commandHandlers[command](tokens);
            return true;
        } else {
            return false;
        }
    }
    return true;
}

/*************************************************************************
**  函数名：  handleEnrcCommand()
**	输入参数：args:指令参数
**	输出参数：
**	函数功能：enrc指令执行函数，客户使用远程遥控功能
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void KeyBoard::handleEnrcCommand(const std::vector<std::string>& args) {
    if (args.size() > 1) {
        std::string action = args[1];
        if (action == "on") {
            if (!robot_server) {
                // 使用make_shared创建RobotServer，传入this
                robot_server = new RobotServer(this);
            }
            
            if (robot_server->startServer()) {
                std::cout << "远程控制服务端启动成功" << std::endl;
            } else {
                std::cout << "远程控制服务端启动失败" << std::endl;
            }
        } else if (action == "off") {
            if (robot_server) {
                robot_server->stopServer();
                std::cout << "远程控制服务端已停止" << std::endl;
            } else {
                std::cout << "服务端未运行" << std::endl;
            }
        } else {
            std::cout << "远程控制命令格式: enrc on" << std::endl;
        }
    } else {
        std::cout << "远程控制命令格式: enrc off" << std::endl;
    }
}
/*************************************************************************
**  函数名：  registerCommand()
**	输入参数：cmdName 指令名称 handler：指令对应执行函数
**	输出参数：
**	函数功能：通过哈系表的方式注册指令,实现用户输入对应指令时执行对应的函数
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void KeyBoard::registerCommand(const std::string& cmdName,std::function<void(const std::vector<std::string>&)> handler) {
    commandHandlers[cmdName] = handler;
}
/*************************************************************************
**  函数名：  registerAllCommand()
**	输入参数：
**	输出参数：
**	函数功能：该线程执行时，注册所有指令
**  作者：    wk
**  开发日期：2025/10/29
**************************************************************************/
void KeyBoard::registerAllCommand(){
    // 使用静态数组定义所有命令配置,后续需要添加或者删除指令时，只需要修改该数组成员即可
    static const std::pair<std::string, void(*)(const std::vector<std::string>&)> commandConfigs[] = {
        {"qpi", &CmdCall::handleQpiCommand},
        {"epfun", &CmdCall::handleEpfunCommand},
        {"pcel", &CmdCall::handlePcelCommand},
        {"scpar", &CmdCall::handleScparCommand},
        {"sbms", &CmdCall::handleSbmsCommand},
        {"susb", &CmdCall::handleSusbCommand},
        {"sspi", &CmdCall::handleSspiCommand},
        {"senet", &CmdCall::handleSenetCommand},
        {"secat", &CmdCall::handleSecatCommand},
        {"msit", &CmdCall::handleMsitCommand},
        {"lrtc", &CmdCall::handleLrtcCommand},
        {"srtc", &CmdCall::handleSrtcCommand},
        {"resys", &CmdCall::handleResysCommand},
        {"clan", &CmdCall::handleClanCommand},
        {"llan", &CmdCall::handleLlanCommand},
        {"cwifi", &CmdCall::handleCwifiCommand},
        {"lwifi", &CmdCall::handleLwifiCommand},
        {"on5g", &CmdCall::handleOn5gCommand},
        {"l5gh", &CmdCall::handleL5ghCommand},
        {"enbt", &CmdCall::handleEnbtCommand},
        {"lbth", &CmdCall::handleLbthCommand},
        {"nsh", &CmdCall::handleNshCommand},
        {"lnsh", &CmdCall::handleLnshCommand},
        {"enmic", &CmdCall::handleEnmicCommand},
        {"lmich", &CmdCall::handleLmichCommand},
        {"enspk", &CmdCall::handleEnspkCommand},
        {"lspk", &CmdCall::handleLspkCommand},
        {"endcam", &CmdCall::handleEndcamCommand},
        {"ldcam", &CmdCall::handleLdcamCommand},
        {"enrgb", &CmdCall::handleEnrgbCommand},
        {"lrgb", &CmdCall::handleLrgbCommand},
        {"enlid", &CmdCall::handleEnlidCommand},
        {"llid", &CmdCall::handleLlidCommand},
        {"engps", &CmdCall::handleEngpsCommand},
        {"lgps", &CmdCall::handleLgpsCommand},
        {"enrtk", &CmdCall::handleEnrtkCommand},
        {"lrtk", &CmdCall::handleLrtkCommand},
        {"gimud", &CmdCall::handleGimudCommand},
        {"calimu", &CmdCall::handleCalimuCommand},
        {"gtof", &CmdCall::handleGtofCommand},
        {"qtof", &CmdCall::handleQtofCommand},
        {"gts", &CmdCall::handleGtsCommand},
        {"qtsh", &CmdCall::handleQtshCommand},
        {"gbldc", &CmdCall::handleGbldcCommand},
        {"qbldc", &CmdCall::handleQbldcCommand},
        {"adsta", &CmdCall::handleAdstaCommand},
        {"delsta", &CmdCall::handleDelstaCommand},
        {"robon", &CmdCall::handleRobonCommand},
        {"robstp", &CmdCall::handleRobstpCommand},
        {"bldch", &CmdCall::handleBldchCommand},
        {"imuch", &CmdCall::handleImuchCommand},
        {"dcch", &CmdCall::handleDcchCommand},
        {"cmch", &CmdCall::handleCmchCommand},
        {"rpch", &CmdCall::handleRpchCommand},
        {"pwch", &CmdCall::handlePwchCommand},
        {"lich", &CmdCall::handleLichCommand},
        {"azro", &CmdCall::handleAzroCommand},
        {"lmtsf", &CmdCall::handleLmtsfCommand},
        {"rbms", &CmdCall::handleRbmsCommand},
        {"lbtmp", &CmdCall::handleLbtmpCommand},
        {"chds", &CmdCall::handleChdsCommand},
        {"btver", &CmdCall::handleBtverCommand},
        {"ctwlk", &CmdCall::handleCtwlkCommand},
        {"ctstwlk", &CmdCall::handleCtstwlkCommand},
        {"ctslp", &CmdCall::handleCtslpCommand},
        {"ctstslp", &CmdCall::handleCtstslpCommand},
        {"wgt", &CmdCall::handleWgtCommand},
        {"cgw", &CmdCall::handleCgwCommand},
        {"stpw", &CmdCall::handleStpwCommand},
        {"std", &CmdCall::handleStdCommand},
        {"cstd", &CmdCall::handleCstdCommand},
        {"stpstd", &CmdCall::handleStpstdCommand},
        {"jmp", &CmdCall::handleJmpCommand},
        {"cjmp", &CmdCall::handleCjmpCommand},
        {"stpjmp", &CmdCall::handleStpjmpCommand},
        {"rbtrun", &CmdCall::handleRbtrunCommand},
        {"crbtrun", &CmdCall::handleCrbtrunCommand},
        {"stprun", &CmdCall::handleStprunCommand},
        {"pidm", &CmdCall::handlePidmCommand},
        {"pidopt", &CmdCall::handlePidoptCommand},
        {"ici", &CmdCall::handleIciCommand},
        {"iciopt", &CmdCall::handleIcioptCommand},
        {"se", &CmdCall::handleSeCommand},
        {"ase", &CmdCall::handleAseCommand},
        {"fopt", &CmdCall::handleFoptCommand},
        {"rop", &CmdCall::handleRopCommand},
        {"prs", &CmdCall::handlePrsCommand},
        {"apa", &CmdCall::handleApaCommand},
        {"devps", &CmdCall::handleDevpsCommand},
        {"adtp", &CmdCall::handleAdtpCommand},
        {"lpm", &CmdCall::handleLpmCommand},
        {"qstg", &CmdCall::handleQstgCommand},
        {"cstg", &CmdCall::handleCstgCommand},
        {"jmcm", &CmdCall::handleJmcmCommand},
        {"rjm", &CmdCall::handleRjmCommand},
        {"tcif", &CmdCall::handleTcifCommand},
        {"mfcif", &CmdCall::handleMfcifCommand},
        {"rmipt", &CmdCall::handleRmiptCommand},
        {"rsmm", &CmdCall::handleRsmmCommand},
        {"rjp", &CmdCall::handleRjpCommand},
        {"swtsm", &CmdCall::handleSwtsmCommand},
        {"wlkgt", &CmdCall::handleWlkgtCommand},
        {"rgt", &CmdCall::handleRgtCommand},
        {"srjav", &CmdCall::handleSrjavCommand},
        {"slv", &CmdCall::handleSlvCommand},
        {"atei", &CmdCall::handleAteiCommand},
        {"sact", &CmdCall::handleSactCommand},
        {"ltwc", &CmdCall::handleLtwcCommand},
        {"rtm", &CmdCall::handleRtmCommand},
        {"jmpact", &CmdCall::handleJmpactCommand},
        {"ssr", &CmdCall::handleSsrCommand},
        {"imur", &CmdCall::handleImurCommand},
        {"imctl", &CmdCall::handleImctlCommand},
        {"ispk", &CmdCall::handleIspkCommand},
        {"itsdf", &CmdCall::handleItsdfCommand},
        {"itofdf", &CmdCall::handleItofdfCommand},
        {"irddf", &CmdCall::handleIrddfCommand},
        {"iectcm", &CmdCall::handleIectcmCommand},
        {"idcma", &CmdCall::handleIdcmaCommand},
        {"ircma", &CmdCall::handleIrcmaCommand},
        {"irtk", &CmdCall::handleIrtkCommand},
        {"ies", &CmdCall::handleIesCommand},
        {"ipwc", &CmdCall::handleIpwcCommand},
        {"atcmp", &CmdCall::handleAtcmpCommand},
        {"atopkg", &CmdCall::handleAtopkgCommand},
        {"atdf", &CmdCall::handleAtdfCommand},
        {"usrosnd", &CmdCall::handleUsrosndCommand},
        {"tprls", &CmdCall::handleTprlsCommand},
        {"tpsbp", &CmdCall::handleTpsbpCommand},
        {"offnd", &CmdCall::handleOffndCommand},
        {"rmsif", &CmdCall::handleRmsifCommand},
        {"trms", &CmdCall::handleTrmsCommand},
        {"syscon",&CmdCall::handleSysCtrlCommand},
        {"periman",&CmdCall::handlePeriManCommand},
        {"control",&CmdCall::handleControlCommand},
        {"set",&CmdCall::handleSetCommand},
        {"dev",&CmdCall::handleDeveloperCommand},
        {"system",&CmdCall::handleSystemCommand},
        {"peripheral",&CmdCall::handlePeripheralCommand},
        {"rgb",&CmdCall::handleRGBCommand},
        {"rgbd",&CmdCall::handleRGBDCommand},
        {"camera_interface",&CmdCall::handleCameraInterfaceCommand},
        {"network",&CmdCall::handleNetworkCommand},
        {"imu",&CmdCall::handleImuCommand},
        {"mic",&CmdCall::handleMicCommand},
        {"bule_rtc",&CmdCall::handleBuleRtcCommand},
    };
    // 批量注册所有命令
    for (const auto& config : commandConfigs) {
        registerCommand(config.first, [this, config](const std::vector<std::string>& args) {
            config.second(args);
        });
    }
    registerCommand("enrc", [this](const std::vector<std::string>& tokens) {//启动遥控功能单独放在这个线程
        this->handleEnrcCommand(tokens);
    });
}

} // namespace hhros2_teleop