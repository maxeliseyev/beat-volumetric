#include "Harness.h"
#include "WavFiles.h"

#include <charconv>
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

} // namespace

int main(int argc, char** argv)
{
    using namespace beat::leveler::harness;
    try
    {
        std::filesystem::path inputPath, outputPath, reportPath;
        bool synthetic = false;
        int sampleRate = 48000;
        std::size_t channels = 1;
        std::vector<std::size_t> blocks { 256 };
        std::set<std::string> seen;
        for (int i = 1; i < argc; ++i)
        {
            const std::string option = argv[i];
            if (option == "--help")
            {
                std::cout << "beat_leveler_runner (--synthetic | --input FILE.wav)\n"
                             "  --output NEW.wav --report NEW.csv [--blocks 127,1,511]\n"
                             "  [--sample-rate 48000 --channels 1] (synthetic only)\n"
                             "Passthrough baseline, zero latency; WAV output is float32.\n"
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
            if (option == "--input")
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
            else
                throw std::invalid_argument("Unknown option: " + option);
        }
        if (synthetic == !inputPath.empty() || outputPath.empty() || reportPath.empty())
            throw std::invalid_argument("Choose --synthetic or --input; provide --output and --report");
        if (!synthetic && (seen.contains("--sample-rate") || seen.contains("--channels")))
            throw std::invalid_argument("Sample rate and channels come from the input WAV");
        outputPath = std::filesystem::weakly_canonical(std::filesystem::absolute(outputPath));
        reportPath = std::filesystem::weakly_canonical(std::filesystem::absolute(reportPath));
        if (outputPath == reportPath || std::filesystem::exists(outputPath)
            || std::filesystem::exists(reportPath))
            throw std::invalid_argument("Output/report must be distinct new files");

        const auto fixture = synthetic ? makeSynthetic(sampleRate, channels)
                                       : Fixture { readWav(inputPath), {} };
        const auto output = render(fixture.audio, blocks);
        std::filesystem::create_directories(outputPath.parent_path());
        std::filesystem::create_directories(reportPath.parent_path());
        writeWav(outputPath, output);
        // Measure the actual serialized output, including file round-trip effects.
        const auto written = readWav(outputPath);
        std::ofstream report(reportPath, std::ios::binary);
        writeReport(report, fixture.audio, written, fixture.hits);
        report.close();
        if (!report)
            throw std::runtime_error("Failed to close report");
        std::cout << "Passthrough: " << fixture.audio.frames() << " frames, "
                  << fixture.audio.channels.size() << " channels, " << fixture.audio.sampleRate
                  << " Hz; latency 0.\nWAV: " << outputPath << "\nCSV: " << reportPath << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
