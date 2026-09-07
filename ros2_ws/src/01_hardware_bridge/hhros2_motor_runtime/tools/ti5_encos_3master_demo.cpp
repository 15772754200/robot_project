#include "hhros2_motor_runtime/ti5_encos_3master/three_master_ti5_encos_runtime.hpp"

#include <boost/program_options.hpp>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>

namespace
{

ti5_encos::ThreeMasterTi5EncosRuntime *g_runtime = nullptr;

void usage(const char *program_name)
{
    std::cout << " example:\n"
              << "         " << program_name << "\n"
              << "         " << program_name << " -h\n"
              << "         " << program_name << " -c0 500000\n";
}

void signal_handler(int)
{
    std::signal(SIGINT, SIG_DFL);
    std::cout << "\nWaiting stop...\n";
    if (g_runtime != nullptr)
    {
        g_runtime->request_stop();
    }
    std::cout << ">>>>>>>> END.\n";
}

} // namespace

int main(int argc, const char **argv)
{
    ti5_encos::RuntimeConfig config;

    int base_affinity = 1;
    bool use_bundled_eni = false;
    boost::program_options::options_description opts("all option");
    boost::program_options::variables_map vm;

    opts.add_options()
        ("help,h", "This is EtherCAT Demo program.")
        ("affinity,a", boost::program_options::value<int>(&base_affinity)->default_value(1),
         "Set CPU affinity base of the realtime tasks")
        ("priority,p", boost::program_options::value<int>(&config.masters[0].priority)->default_value(90),
         "Set priority of the realtime tasks (1 - 99)")
        ("interval,i", boost::program_options::value<int>(&config.masters[0].interval)->default_value(0),
         "Set optimize jitter parameter. (suggested: 0-20).")
        ("shiftTime,s", boost::program_options::value<std::int64_t>(&config.masters[0].shift_time_ns)->default_value(0),
         "Set cycle shift time(ns), only for esi mode.")
        ("c0,c0", boost::program_options::value<std::int64_t>(&config.masters[0].cycle_time_ns)->default_value(1000000),
         "Set the master 0 cycle time (ns), for ESI mode only.")
        ("c1,c1", boost::program_options::value<std::int64_t>(&config.masters[1].cycle_time_ns)->default_value(1000000),
         "Set the master 1 cycle time (ns), for ESI mode only.")
        ("c2,c2", boost::program_options::value<std::int64_t>(&config.masters[2].cycle_time_ns)->default_value(1000000),
         "Set the master 2 cycle time (ns), for ESI mode only.")
        ("f0,f0", boost::program_options::value<std::string>(&config.masters[0].eni_file),
         "Set the master 0 eni XML, for ENI mode only.")
        ("f1,f1", boost::program_options::value<std::string>(&config.masters[1].eni_file),
         "Set the master 1 eni XML, for ENI mode only.")
        ("f2,f2", boost::program_options::value<std::string>(&config.masters[2].eni_file),
         "Set the master 2 eni XML, for ENI mode only.")
        ("use-bundled-eni", boost::program_options::bool_switch(&use_bundled_eni),
         "Use bundled ENI XML files for master 1 and master 2 when -f1/-f2 are not set.")
        ("log,l", boost::program_options::value<int>(&config.log_level)->default_value(6),
         "Set log level, (0-6).");

    try
    {
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, opts), vm);
        vm.notify();
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << "\n";
        std::cout << opts << "\n";
        return 1;
    }

    if (vm.count("help"))
    {
        std::cout << opts << "\n";
        usage(argv[0]);
        return 0;
    }

    for (int master_index = 0; master_index < ti5_encos::kMasterNumber; ++master_index)
    {
        config.masters[master_index].cpu_affinity = base_affinity + master_index;
        config.masters[master_index].priority = config.masters[0].priority;
        config.masters[master_index].interval = config.masters[0].interval;
        config.masters[master_index].shift_time_ns = config.masters[0].shift_time_ns;
    }

    if (use_bundled_eni)
    {
        for (int master_index = 1; master_index < ti5_encos::kMasterNumber; ++master_index)
        {
            if (config.masters[master_index].eni_file.empty())
            {
                config.masters[master_index].eni_file =
                    ti5_encos::bundled_eni_file_for_master(master_index);
            }
        }
    }

    ti5_encos::ThreeMasterTi5EncosRuntime runtime(config);
    g_runtime = &runtime;
    std::signal(SIGINT, signal_handler);

    std::string error;
    if (!runtime.start(&error))
    {
        std::cerr << "start ti5_encos_3master_demo failed: " << error << "\n";
        g_runtime = nullptr;
        return -1;
    }
    runtime.set_motor_enable(true);

    runtime.wait();
    runtime.release();
    g_runtime = nullptr;
    return 0;
}
