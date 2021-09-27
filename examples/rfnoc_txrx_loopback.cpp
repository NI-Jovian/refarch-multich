//
// Copyright 2010-2012,2014-2015 Ettus Research LLC
// Copyright 2021 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include "blocksettings.hpp"
#include "graphassembly.hpp"
#include "receivefunctions.hpp"
#include "replaycontrol.hpp"
#include "structures.hpp"
#include "sync.hpp"
#include <uhd/exception.hpp>
#include <uhd/rfnoc/block_id.hpp>
#include <uhd/rfnoc/ddc_block_control.hpp>
#include <uhd/rfnoc/duc_block_control.hpp>
#include <uhd/rfnoc/mb_controller.hpp>
#include <uhd/rfnoc/radio_control.hpp>
#include <uhd/rfnoc/replay_block_control.hpp>
#include <uhd/rfnoc_graph.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/static.hpp>
#include <uhd/utils/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

volatile bool stop_signal_called = false;


// Constants related to the Replay block
const size_t replay_word_size = 8; // Size of words used by replay block
const size_t sample_size      = 4; // Complex signed 16-bit is 32 bits per sample
const size_t samples_per_word = 2; // Number of sc16 samples per word


typedef std::function<uhd::sensor_value_t(const std::string&)> get_sensor_fn_t;


/***********************************************************************
 * Main function
 **********************************************************************/

int UHD_SAFE_MAIN(int argc, char* argv[])
{
    // find configuration file -cfgFile adds to "desc" variable
    ProgramMetaData pmd;
    DeviceSettings device;
    SignalSettings signal;
    GraphSettings graphStruct;

    // INFO: Comment what each initilization does what type of data is stored in each.
    device.addProgramOptions(pmd.desc);
    signal.addProgramOptions(pmd.desc);

    pmd.storeProgramOptions(argc, argv);
    device.addAddresstoArgs();

    // determine folder name to store data in
    boost::posix_time::ptime timeLocal = boost::posix_time::second_clock::local_time();

    std::string month   = std::to_string(timeLocal.date().month());
    std::string day     = std::to_string(timeLocal.date().day());
    std::string year    = std::to_string(timeLocal.date().year());
    std::string hour    = std::to_string(timeLocal.time_of_day().hours());
    std::string minute  = std::to_string(timeLocal.time_of_day().minutes());
    std::string seconds = std::to_string(timeLocal.time_of_day().seconds());


    device.folder_name =
        month + day + year + "_" + hour + minute + seconds + "_" + signal.rx_file;


    // Can we get rid of the ProgramMetaData in these functions?
    // We might want to try to only pass data that is required.

    // Setup Graph with input Arguments
    GraphAssembly::buildGraph(graphStruct, device.args);
    // Sync Device Clocks
    SyncDevices::setSources(graphStruct, device.ref);
    // Setup Radio Blocks
    GraphAssembly::buildRadios(graphStruct);
    // Setup DDC/DUC Blocks
    GraphAssembly::buildDDCDUC(graphStruct);
    // Setup Replay Blocks
    GraphAssembly::buildReplay(graphStruct);
    // Setup LO distribution
    SyncDevices::setLOsfromConfig(graphStruct, device.lo);
    // Set Radio Block Settings
    BlockSettings::setRadioRates(graphStruct, device.rx_rate, device.tx_rate);
    // Tune RX
    BlockSettings::tuneRX(graphStruct, device.rx_freq);
    // Tune TX
    BlockSettings::tuneTX(graphStruct, device.tx_freq);
    // set RX Gain
    BlockSettings::setRXGain(graphStruct, device.rx_gain);
    // set TX Gain
    BlockSettings::setTXGain(graphStruct, device.tx_gain);
    // set RX bandwidth
    BlockSettings::setRXBw(graphStruct, device.rx_bw);
    // set TX bandwidth
    BlockSettings::setTXBw(graphStruct, device.tx_bw);
    // set RX Antenna
    BlockSettings::setRXAnt(graphStruct, device.rx_ant);
    // set TX Antenna
    BlockSettings::setTXAnt(graphStruct, device.tx_ant);
    // Check RX Sensor Lock
    SyncDevices::checkRXSensorLock(graphStruct);
    // Check TX Sensor Lock
    SyncDevices::checkTXSensorLock(graphStruct);
    // Allow for some setup time
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // Build Streams
    GraphAssembly::buildStreamsMultithread(
        graphStruct, device.streamargs, signal.format, signal.otw);
    // Connect Graph

    GraphAssembly::connectGraphMultithread(graphStruct);

    // Commit Graph
    GraphAssembly::commitGraph(graphStruct);
    // Allow for some setup time
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // Load Replay Block Buffers with data to transmit
    ReplayControl::importData(graphStruct, signal.file, signal.samples_to_replay);
    // Sync time across devices
    SyncDevices::syncAllDevices(graphStruct);
    // Begin TX and RX
    ReplayControl::singleTXLoopbackMultithread(graphStruct, signal, device);
    while (not stop_signal_called)
        ;


    std::cout << "Run complete." << std::endl;
    // Kill Replay
    ReplayControl::stopReplay(graphStruct);
    // Kill LO
    SyncDevices::killLOs(graphStruct, device.lo);


    std::cout << std::endl << "Done!" << std::endl << std::endl;
    return EXIT_SUCCESS;
}