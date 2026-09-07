#pragma once

#include <vector>
#include <string>

namespace hhros2_teleop {

class CmdCall {
public:
    CmdCall();
    ~CmdCall();

    // 所有静态命令处理函数
    static void handleQpiCommand(const std::vector<std::string>& args);
    static void handleEpfunCommand(const std::vector<std::string>& args);
    static void handlePcelCommand(const std::vector<std::string>& args);
    static void handleScparCommand(const std::vector<std::string>& args);
    static void handleSbmsCommand(const std::vector<std::string>& args);
    static void handleSusbCommand(const std::vector<std::string>& args);
    static void handleSspiCommand(const std::vector<std::string>& args);
    static void handleSenetCommand(const std::vector<std::string>& args);
    static void handleSecatCommand(const std::vector<std::string>& args);
    static void handleMsitCommand(const std::vector<std::string>& args);
    static void handleLrtcCommand(const std::vector<std::string>& args);
    static void handleSrtcCommand(const std::vector<std::string>& args);
    static void handleResysCommand(const std::vector<std::string>& args);
    static void handleClanCommand(const std::vector<std::string>& args); 
    static void handleLlanCommand(const std::vector<std::string>& args); // 1
    static void handleCwifiCommand(const std::vector<std::string>& args);
    static void handleLwifiCommand(const std::vector<std::string>& args);// 1
    static void handleOn5gCommand(const std::vector<std::string>& args); 
    static void handleL5ghCommand(const std::vector<std::string>& args); 
    static void handleEnbtCommand(const std::vector<std::string>& args);
    static void handleLbthCommand(const std::vector<std::string>& args); // 1
    static void handleNshCommand(const std::vector<std::string>& args); 
    static void handleLnshCommand(const std::vector<std::string>& args); 
    static void handleEnmicCommand(const std::vector<std::string>& args);
    static void handleLmichCommand(const std::vector<std::string>& args);
    static void handleEnspkCommand(const std::vector<std::string>& args);
    static void handleLspkCommand(const std::vector<std::string>& args); // 1
    static void handleEndcamCommand(const std::vector<std::string>& args); // 1
    static void handleLdcamCommand(const std::vector<std::string>& args); // 1
    static void handleEnrgbCommand(const std::vector<std::string>& args); // 1
    static void handleLrgbCommand(const std::vector<std::string>& args); // 1
    static void handleEnlidCommand(const std::vector<std::string>& args); 
    static void handleLlidCommand(const std::vector<std::string>& args);
    static void handleEngpsCommand(const std::vector<std::string>& args);
    static void handleLgpsCommand(const std::vector<std::string>& args);
    static void handleEnrtkCommand(const std::vector<std::string>& args); 
    static void handleLrtkCommand(const std::vector<std::string>& args);
    static void handleGimudCommand(const std::vector<std::string>& args); 
    static void handleCalimuCommand(const std::vector<std::string>& args); 
    static void handleGtofCommand(const std::vector<std::string>& args);
    static void handleQtofCommand(const std::vector<std::string>& args);
    static void handleGtsCommand(const std::vector<std::string>& args);
    static void handleQtshCommand(const std::vector<std::string>& args);
    static void handleGbldcCommand(const std::vector<std::string>& args);
    static void handleQbldcCommand(const std::vector<std::string>& args);
    static void handleAdstaCommand(const std::vector<std::string>& args);
    static void handleDelstaCommand(const std::vector<std::string>& args);
    static void handleRobonCommand(const std::vector<std::string>& args);
    static void handleRobstpCommand(const std::vector<std::string>& args);
    static void handleBldchCommand(const std::vector<std::string>& args);
    static void handleImuchCommand(const std::vector<std::string>& args);
    static void handleDcchCommand(const std::vector<std::string>& args);
    static void handleCmchCommand(const std::vector<std::string>& args);
    static void handleRpchCommand(const std::vector<std::string>& args);
    static void handlePwchCommand(const std::vector<std::string>& args);
    static void handleLichCommand(const std::vector<std::string>& args);
    static void handleAzroCommand(const std::vector<std::string>& args);
    static void handleLmtsfCommand(const std::vector<std::string>& args); 
    static void handleRbmsCommand(const std::vector<std::string>& args);
    static void handleLbtmpCommand(const std::vector<std::string>& args); 
    static void handleChdsCommand(const std::vector<std::string>& args);
    static void handleBtverCommand(const std::vector<std::string>& args); 
    static void handleCtwlkCommand(const std::vector<std::string>& args); // 1
    static void handleCtstwlkCommand(const std::vector<std::string>& args);// 1 
    static void handleCtslpCommand(const std::vector<std::string>& args); // 1
    static void handleCtstslpCommand(const std::vector<std::string>& args);// 1
    static void handleWgtCommand(const std::vector<std::string>& args); 
    static void handleCgwCommand(const std::vector<std::string>& args); 
    static void handleStpwCommand(const std::vector<std::string>& args); 
    static void handleStdCommand(const std::vector<std::string>& args); // 1
    static void handleCstdCommand(const std::vector<std::string>& args); 
    static void handleStpstdCommand(const std::vector<std::string>& args); // 1
    static void handleJmpCommand(const std::vector<std::string>& args); // 1
    static void handleCjmpCommand(const std::vector<std::string>& args);  
    static void handleStpjmpCommand(const std::vector<std::string>& args); // 1
    static void handleRbtrunCommand(const std::vector<std::string>& args); // 1
    static void handleCrbtrunCommand(const std::vector<std::string>& args); // 1
    static void handleStprunCommand(const std::vector<std::string>& args); // 1
    static void handlePidmCommand(const std::vector<std::string>& args);
    static void handlePidoptCommand(const std::vector<std::string>& args); 
    static void handleIciCommand(const std::vector<std::string>& args);
    static void handleIcioptCommand(const std::vector<std::string>& args);
    static void handleSeCommand(const std::vector<std::string>& args);
    static void handleAseCommand(const std::vector<std::string>& args); 
    static void handleFoptCommand(const std::vector<std::string>& args); 
    static void handleRopCommand(const std::vector<std::string>& args);
    static void handlePrsCommand(const std::vector<std::string>& args); 
    static void handleApaCommand(const std::vector<std::string>& args);
    static void handleDevpsCommand(const std::vector<std::string>& args); 
    static void handleAdtpCommand(const std::vector<std::string>& args);
    static void handleLpmCommand(const std::vector<std::string>& args);
    static void handleQstgCommand(const std::vector<std::string>& args); 
    static void handleCstgCommand(const std::vector<std::string>& args);
    static void handleJmcmCommand(const std::vector<std::string>& args);
    static void handleRjmCommand(const std::vector<std::string>& args);
    static void handleTcifCommand(const std::vector<std::string>& args); 
    static void handleMfcifCommand(const std::vector<std::string>& args); 
    static void handleRmiptCommand(const std::vector<std::string>& args);
    static void handleRsmmCommand(const std::vector<std::string>& args);
    static void handleRjpCommand(const std::vector<std::string>& args); 
    static void handleSwtsmCommand(const std::vector<std::string>& args); // 1
    static void handleWlkgtCommand(const std::vector<std::string>& args); // 1
    static void handleRgtCommand(const std::vector<std::string>& args); // 1
    static void handleSrjavCommand(const std::vector<std::string>& args);  
    static void handleSlvCommand(const std::vector<std::string>& args); // 1
    static void handleAteiCommand(const std::vector<std::string>& args); // 1
    static void handleSactCommand(const std::vector<std::string>& args); // 1
    static void handleLtwcCommand(const std::vector<std::string>& args); // 1
    static void handleRtmCommand(const std::vector<std::string>& args); // 1
    static void handleJmpactCommand(const std::vector<std::string>& args); // 1
    static void handleSsrCommand(const std::vector<std::string>& args); 
    static void handleImurCommand(const std::vector<std::string>& args); 
    static void handleImctlCommand(const std::vector<std::string>& args); 
    static void handleIspkCommand(const std::vector<std::string>& args);
    static void handleItsdfCommand(const std::vector<std::string>& args); 
    static void handleItofdfCommand(const std::vector<std::string>& args);
    static void handleIrddfCommand(const std::vector<std::string>& args);
    static void handleIectcmCommand(const std::vector<std::string>& args); 
    static void handleIdcmaCommand(const std::vector<std::string>& args);
    static void handleIrcmaCommand(const std::vector<std::string>& args); 
    static void handleIrtkCommand(const std::vector<std::string>& args);
    static void handleIesCommand(const std::vector<std::string>& args);
    static void handleIpwcCommand(const std::vector<std::string>& args); 
    static void handleAtcmpCommand(const std::vector<std::string>& args); 
    static void handleAtopkgCommand(const std::vector<std::string>& args); 
    static void handleAtdfCommand(const std::vector<std::string>& args);
    static void handleUsrosndCommand(const std::vector<std::string>& args);
    static void handleTprlsCommand(const std::vector<std::string>& args);
    static void handleTpsbpCommand(const std::vector<std::string>& args);
    static void handleOffndCommand(const std::vector<std::string>& args);
    static void handleRmsifCommand(const std::vector<std::string>& args);
    static void handleTrmsCommand(const std::vector<std::string>& args); 
    static void handleSysCtrlCommand(const std::vector<std::string>& args); // 1
    static void handlePeriManCommand(const std::vector<std::string>& args); // 1
    static void handleControlCommand(const std::vector<std::string>& args); //new control system
    static void handleSetCommand(const std::vector<std::string>& args); // new set control system
    static void handleDeveloperCommand(const std::vector<std::string>& args); // new developer mode command
    static void handleSystemCommand(const std::vector<std::string>& args); // new system management command
    static void handlePeripheralCommand(const std::vector<std::string>& args); // new peripheral management command
    static void handleRGBCommand(const std::vector<std::string>& args); // new RGB mode command
    static void handleRGBDCommand(const std::vector<std::string>& args); // new RGBD mode command
    static void handleCameraInterfaceCommand(const std::vector<std::string>& args); // new camera interface command
    static void handleNetworkCommand(const std::vector<std::string>& args); // new Network mode command
    static void handleImuCommand(const std::vector<std::string>& args); // new Imu mode command
    static void handleMicCommand(const std::vector<std::string>& args); // new Mic mode command
    static void handleBuleRtcCommand(const std::vector<std::string>& args); // new bule rtc menu command

private:
    static std::string generate_uuid();
    static void executeTerminalScript(const std::string& commandName, const std::string& scriptType, 
                                                const std::string& script_name, bool use_sudo);
    static std::string getScriptPath(const std::string& scriptType, const std::string& script_name);
    static std::string package_share_;   // 包共享目录：install/share/hhros2_teleop
    static std::string package_prefix_;  // 包前缀：install/hhros2_teleop 
};

} // namespace hhros2_teleop