#include "Harness.h"
#include "RealKit.h"
#include "WavFiles.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

std::size_t positiveInteger(std::string_view text)
{
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() || value == 0
        || value > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Expected a positive integer: " + std::string(text));
    return value;
}

std::vector<std::size_t> parseBlocks(std::string_view text)
{
    std::vector<std::size_t> blocks;
    while (true)
    {
        const auto comma = text.find(',');
        blocks.push_back(positiveInteger(text.substr(0, comma)));
        if (comma == std::string_view::npos)
            return blocks;
        text.remove_prefix(comma + 1);
    }
}

double nonNegativeNumber(std::string_view text)
{
    const auto valueText = std::string(text);
    std::size_t consumed = 0;
    double value = 0.0;
    try
    {
        value = std::stod(valueText, &consumed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument("Expected a non-negative number: " + valueText);
    }
    if (consumed != valueText.size() || !std::isfinite(value) || value < 0.0)
        throw std::invalid_argument("Expected a non-negative number: " + valueText);
    return value;
}

} // namespace

int main(int argc, char** argv)
{
    using namespace beat::leveler::harness;
    try
    {
        std::filesystem::path inputPath, outputPath, reportPath;
        std::filesystem::path realKitPath;
        bool synthetic = false;
        bool realKit = false;
        int sampleRate = 48000;
        std::size_t channels = 1;
        std::vector<std::size_t> blocks { 256 };
        double toleranceMilliseconds = 20.0;
        std::set<std::string> seen;
        for (int i = 1; i < argc; ++i)
        {
            const std::string option = argv[i];
            if (option == "--help")
            {
                std::cout << "beat_leveler_runner (--synthetic | --input FILE.wav | --real-kit DIR)\n"
                             "  --output NEW.wav --report NEW.csv [--blocks 127,1,511]\n"
                             "  [--sample-rate 48000 --channels 1] (synthetic only)\n"
                             "  real-kit DIR contains manifest.csv; use --tolerance-ms or "
                             "BEAT_LEVELER_REAL_KIT_DIR\n"
                             "Streaming detection/measurement baseline, zero output latency; WAV output is float32.\n"
                             "Output paths must be distinct and must not already exist.\n";
                return 0;
            }
            if (!seen.insert(option).second)
                throw std::invalid_argument("Duplicate option: " + option);
            if (option == "--synthetic")
            {
                synthetic = true;
                continue;
            }
            if (i + 1 == argc)
                throw std::invalid_argument("Missing value for " + option);
            const std::string_view value = argv[++i];
            if (option == "--real-kit")
            {
                realKit = true;
                realKitPath = value;
            }
            else if (option == "--input")
                inputPath = value;
            else if (option == "--output")
                outputPath = value;
            else if (option == "--report")
                reportPath = value;
            else if (option == "--blocks")
                blocks = parseBlocks(value);
            else if (option == "--sample-rate")
                sampleRate = static_cast<int>(positiveInteger(value));
            else if (option == "--channels")
                channels = positiveInteger(value);
            else if (option == "--tolerance-ms")
                toleranceMilliseconds = nonNegativeNumber(value);
            else
                throw std::invalid_argument("Unknown option: " + option);
        }

        if (!realKit && inputPath.empty() && !synthetic)
        {
            if (const auto* environment = std::getenv("BEAT_LEVELER_REAL_KIT_DIR");
                environment != nullptr && *environment != '\0')
            {
                realKit = true;
                realKitPath = environment;
            }
        }

        const auto selectedModes = static_cast<int>(synthetic) + static_cast<int>(!inputPath.empty())
                                   + static_cast<int>(realKit);
        if (selectedModes != 1)
            throw std::invalid_argument("Choose exactly one of --synthetic, --input or --real-kit");
        if (realKit)
        {
            if (outputPath.empty() == false || reportPath.empty())
                throw std::invalid_argument("Real-kit mode accepts --report and does not write --output");
            if (seen.contains("--sample-rate") || seen.contains("--channels"))
                throw std::invalid_argument("Sample rate and channels come from real-kit WAV files");
            if (realKitPath.empty())
                throw std::invalid_argument("Real-kit directory must not be empty");

            reportPath = std::filesystem::weakly_canonical(std::filesystem::absolute(reportPath));
            if (std::filesystem::exists(reportPath))
                throw std::invalid_argument("Report must be a new file: " + reportPath.string());
            std::filesystem::create_directories(reportPath.parent_path());
            const auto result = runRealKit(std::filesystem::absolute(realKitPath), blocks,
                                           toleranceMilliseconds);
            std::ofstream report(reportPath, std::ios::binary);
            writeRealKitReport(report, result);
            report.close();
            if (!report)
                throw std::runtime_error("Failed to close report");
            std::cout << "Real-kit: " << result.files.size() << " recordings, "
                      << result.overall.detected << " detections, " << result.overall.matched
                      << " matched, " << result.overall.falsePositives << " false positives, "
                      << result.overall.falseNegatives << " false negatives\nCSV: " << reportPath
                      << '\n';
            return 0;
        }

        if (outputPath.empty() || reportPath.empty())
            throw std::invalid_argument("Provide --output and --report");
        if (!synthetic && (seen.contains("--sample-rate") || seen.contains("--channels")))
            throw std::invalid_argument("Sample rate and channels come from the input WAV");
        outputPath = std::filesystem::weakly_canonical(std::filesystem::absolute(outputPath));
        reportPath = std::filesystem::weakly_canonical(std::filesystem::absolute(reportPath));
        if (outputPath == reportPath || std::filesystem::exists(outputPath)
            || std::filesystem::exists(reportPath))
            throw std::invalid_argument("Output/report must be distinct new files");

        const auto fixture = synthetic ? makeSynthetic(sampleRate, channels)
                                       : Fixture { readWav(inputPath), {} };
        const auto result = renderDetailed(fixture.audio, blocks);
        std::filesystem::create_directories(outputPath.parent_path());
        std::filesystem::create_directories(reportPath.parent_path());
        writeWav(outputPath, result.audio);
        // Measure the actual serialized output, including file round-trip effects.
        const auto written = readWav(outputPath);
        std::ofstream report(reportPath, std::ios::binary);
        writeReport(report, fixture.audio, written, fixture.hits, result.measurements);
        report.close();
        if (!report)
            throw std::runtime_error("Failed to close report");
        std::cout << "Streaming analyzer: " << fixture.audio.frames() << " frames, "
                  << fixture.audio.channels.size() << " channels, " << fixture.audio.sampleRate
                  << " Hz; " << result.events.size() << " events, " << result.measurements.size()
                  << " completed measurements; output latency 0.\nWAV: " << outputPath << "\nCSV: "
                  << reportPath << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
