#include "RealKit.h"

#include "Harness.h"
#include "WavFiles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <ostream>
#include <stdexcept>
#include <string>

namespace beat::leveler::harness
{

namespace
{

void addMetrics(DetectionMetrics& total, const DetectionMetrics& value)
{
    std::array<double, static_cast<std::size_t>(HitKind::count)> timingSums {};
    std::array<double, static_cast<std::size_t>(HitKind::count)> levelSums {};
    for (std::size_t index = 0; index < total.byKind.size(); ++index)
    {
        timingSums[index] = total.byKind[index].meanAbsoluteTimingSamples
                            * static_cast<double>(total.byKind[index].matched);
        levelSums[index] = total.byKind[index].meanAbsoluteLevelErrorDb
                           * static_cast<double>(total.byKind[index].levelMeasurements);
    }
    total.detected += value.detected;
    total.matched += value.matched;
    total.falsePositives += value.falsePositives;
    total.falseNegatives += value.falseNegatives;
    for (std::size_t index = 0; index < total.byKind.size(); ++index)
    {
        auto& target = total.byKind[index];
        const auto& source = value.byKind[index];
        target.expected += source.expected;
        target.matched += source.matched;
        target.levelMeasurements += source.levelMeasurements;
        timingSums[index] += source.meanAbsoluteTimingSamples * static_cast<double>(source.matched);
        levelSums[index] += source.meanAbsoluteLevelErrorDb
                            * static_cast<double>(source.levelMeasurements);
        target.meanAbsoluteTimingSamples = target.matched == 0
                                                ? 0.0
                                                : timingSums[index] / static_cast<double>(target.matched);
        target.meanAbsoluteLevelErrorDb = target.levelMeasurements == 0
                                              ? 0.0
                                              : levelSums[index]
                                                    / static_cast<double>(target.levelMeasurements);
    }
}

double overallTiming(const DetectionMetrics& metrics)
{
    double sum = 0.0;
    for (const auto& item : metrics.byKind)
        sum += item.meanAbsoluteTimingSamples * static_cast<double>(item.matched);
    return metrics.matched == 0 ? 0.0 : sum / static_cast<double>(metrics.matched);
}

double overallLevel(const DetectionMetrics& metrics)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (const auto& item : metrics.byKind)
    {
        sum += item.meanAbsoluteLevelErrorDb * static_cast<double>(item.levelMeasurements);
        count += item.levelMeasurements;
    }
    return count == 0 ? 0.0 : sum / static_cast<double>(count);
}

std::string csvEscape(const std::string& value)
{
    if (value.find_first_of(",\"\n\r") == std::string::npos)
        return value;
    std::string escaped = "\"";
    for (const auto character : value)
    {
        escaped += character;
        if (character == '"')
            escaped += '"';
    }
    escaped += '"';
    return escaped;
}

double precision(const DetectionMetrics& metrics)
{
    return metrics.detected == 0 ? 0.0
                                 : static_cast<double>(metrics.matched) / metrics.detected;
}

double recall(const DetectionMetrics& metrics)
{
    const auto expected = metrics.matched + metrics.falseNegatives;
    return expected == 0 ? 0.0 : static_cast<double>(metrics.matched) / expected;
}

void writeAggregateRow(std::ostream& stream,
                       const std::string& file,
                       const std::string& kind,
                       const DetectionMetrics& metrics,
                       std::size_t expected,
                       std::size_t matched,
                       std::size_t falseNegatives,
                       double timing,
                       double level,
                       const char* sampleRate,
                       const char* frames)
{
    stream << BEAT_LEVELER_VERSION << ",metrics," << csvEscape(file) << ',' << sampleRate << ','
           << frames << ',' << kind << ',' << expected << ',' << metrics.detected << ',' << matched
           << ',' << metrics.falsePositives << ',' << falseNegatives << ',' << precision(metrics)
           << ',' << recall(metrics) << ',' << timing << ',' << level << '\n';
}

} // namespace

RealKitRunResult runRealKit(const std::filesystem::path& root,
                            std::span<const std::size_t> blockPattern,
                            double toleranceMilliseconds)
{
    if (blockPattern.empty()
        || std::any_of(blockPattern.begin(), blockPattern.end(), [](auto size) { return size == 0; }))
        throw std::invalid_argument("Block pattern must contain positive sizes");
    if (!std::isfinite(toleranceMilliseconds) || toleranceMilliseconds < 0.0)
        throw std::invalid_argument("Real-kit tolerance must be a finite non-negative number");

    const auto recordings = readRealKitManifest(root);
    RealKitRunResult result;
    for (const auto& recording : recordings)
    {
        if (!std::filesystem::is_regular_file(recording.audioPath))
            throw std::runtime_error("Manifest audio file does not exist: " + recording.audioPath.string());

        const auto audio = readWav(recording.audioPath);
        for (const auto& annotation : recording.annotations)
        {
            if (static_cast<std::uint64_t>(annotation.onsetSample) >= audio.frames())
                throw std::invalid_argument("Annotation is outside audio: "
                                             + recording.relativeAudioPath.string());
        }
        const auto rendered = renderDetailed(audio, blockPattern);
        const auto toleranceSamples = static_cast<std::int64_t>(std::llround(
            toleranceMilliseconds * static_cast<double>(audio.sampleRate) / 1000.0));
        const auto metrics = evaluateDetections(recording.annotations, rendered.events,
                                                 rendered.measurements, toleranceSamples);
        result.files.push_back({ recording.relativeAudioPath, audio.sampleRate, audio.frames(), metrics });
        addMetrics(result.overall, metrics);
    }
    return result;
}

void writeRealKitReport(std::ostream& stream, const RealKitRunResult& result)
{
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17);
    stream << "version,scope,file,sample_rate,frames,kind,expected,detected,matched,"
              "false_positives,false_negatives,precision,recall,mean_abs_timing_samples,"
              "mean_abs_level_error_db\n";

    for (const auto& file : result.files)
    {
        std::size_t expected = 0;
        for (const auto& item : file.metrics.byKind)
            expected += item.expected;
        const auto falseNegatives = expected - file.metrics.matched;
        writeAggregateRow(stream, file.relativeAudioPath.generic_string(), "all", file.metrics,
                          expected, file.metrics.matched, falseNegatives,
                          overallTiming(file.metrics), overallLevel(file.metrics),
                          std::to_string(file.sampleRate).c_str(), std::to_string(file.frames).c_str());
        for (std::size_t index = 0; index < file.metrics.byKind.size(); ++index)
        {
            const auto& item = file.metrics.byKind[index];
            const auto kind = static_cast<HitKind>(index);
            stream << BEAT_LEVELER_VERSION << ",metrics," << csvEscape(file.relativeAudioPath.generic_string())
                   << ',' << file.sampleRate << ',' << file.frames << ',' << hitKindName(kind) << ','
                   << item.expected << ",," << item.matched << ",,"
                   << (item.expected - item.matched) << ",,,"
                   << item.meanAbsoluteTimingSamples << ',' << item.meanAbsoluteLevelErrorDb << '\n';
        }
    }

    std::size_t expected = 0;
    for (const auto& item : result.overall.byKind)
        expected += item.expected;
    writeAggregateRow(stream, "__overall__", "all", result.overall, expected,
                      result.overall.matched, result.overall.falseNegatives,
                      overallTiming(result.overall), overallLevel(result.overall), "", "");
    for (std::size_t index = 0; index < result.overall.byKind.size(); ++index)
    {
        const auto& item = result.overall.byKind[index];
        stream << BEAT_LEVELER_VERSION << ",metrics,__overall__,,,"
               << hitKindName(static_cast<HitKind>(index)) << ',' << item.expected << ",," << item.matched
               << ",," << (item.expected - item.matched) << ",,," << item.meanAbsoluteTimingSamples << ','
               << item.meanAbsoluteLevelErrorDb << '\n';
    }
    if (!stream)
        throw std::runtime_error("Failed to write real-kit report");
}

} // namespace beat::leveler::harness
