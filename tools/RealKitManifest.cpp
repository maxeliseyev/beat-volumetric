#include "RealKitManifest.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace beat::leveler::harness
{

namespace
{

std::string trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

std::vector<std::string> parseRow(std::string_view line)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t index = 0; index < line.size(); ++index)
    {
        const auto character = line[index];
        if (character == '"')
        {
            if (quoted && index + 1 < line.size() && line[index + 1] == '"')
            {
                field.push_back('"');
                ++index;
            }
            else
            {
                quoted = !quoted;
            }
        }
        else if (character == ',' && !quoted)
        {
            fields.push_back(trim(field));
            field.clear();
        }
        else
        {
            field.push_back(character);
        }
    }
    if (quoted)
        throw std::invalid_argument("Unclosed quote in real-kit manifest row");
    fields.push_back(trim(field));
    return fields;
}

std::int64_t parseSample(std::string_view value)
{
    const auto text = trim(value);
    if (text.empty())
        throw std::invalid_argument("Manifest onset_sample is empty");
    std::size_t consumed = 0;
    std::int64_t sample = 0;
    try
    {
        sample = std::stoll(text, &consumed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument("Manifest onset_sample must be an integer: " + text);
    }
    if (consumed != text.size() || sample < 0)
        throw std::invalid_argument("Manifest onset_sample must be a non-negative integer: " + text);
    return sample;
}

float parsePeak(std::string_view value)
{
    const auto text = trim(value);
    if (text.empty())
        throw std::invalid_argument("Manifest peak_dbfs is empty");
    std::size_t consumed = 0;
    float peak = 0.0f;
    try
    {
        peak = std::stof(text, &consumed);
    }
    catch (const std::exception&)
    {
        throw std::invalid_argument("Manifest peak_dbfs must be a number: " + text);
    }
    if (consumed != text.size() || !std::isfinite(peak) || peak > 0.0f || peak < -240.0f)
        throw std::invalid_argument("Manifest peak_dbfs must be finite and in [-240, 0]: " + text);
    return peak;
}

HitKind parseKind(std::string_view value)
{
    const auto text = trim(value);
    for (std::size_t index = 0; index < static_cast<std::size_t>(HitKind::count); ++index)
    {
        const auto kind = static_cast<HitKind>(index);
        if (text == hitKindName(kind))
            return kind;
    }
    throw std::invalid_argument("Unknown manifest hit kind: " + text);
}

std::filesystem::path safeRelativePath(std::string_view value)
{
    const auto text = trim(value);
    const std::filesystem::path path(text);
    if (text.empty() || path.is_absolute())
        throw std::invalid_argument("Manifest file must be a non-empty relative path");

    const auto normalized = path.lexically_normal();
    for (const auto& component : normalized)
    {
        if (component == "..")
            throw std::invalid_argument("Manifest file must stay inside the real-kit directory: " + text);
    }
    if (normalized.empty() || normalized == ".")
        throw std::invalid_argument("Manifest file must name an audio file");
    return normalized;
}

} // namespace

const char* hitKindName(HitKind kind) noexcept
{
    switch (kind)
    {
        case HitKind::kick: return "kick";
        case HitKind::snare: return "snare";
        case HitKind::tom: return "tom";
        case HitKind::ghost: return "ghost";
        case HitKind::flam: return "flam";
        case HitKind::bleed: return "bleed";
        case HitKind::count: break;
    }
    return "unknown";
}

std::vector<RealKitRecording> readRealKitManifest(std::istream& stream,
                                                   const std::filesystem::path& root)
{
    std::vector<RealKitRecording> recordings;
    std::string line;
    std::size_t lineNumber = 0;
    bool headerRead = false;
    while (std::getline(stream, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (trim(line).empty())
            continue;

        const auto fields = parseRow(line);
        if (!headerRead)
        {
            if (fields != std::vector<std::string> { "file", "onset_sample", "peak_dbfs", "kind" })
                throw std::invalid_argument("Real-kit manifest header must be: "
                                             "file,onset_sample,peak_dbfs,kind");
            headerRead = true;
            continue;
        }
        if (fields.size() != 4)
            throw std::invalid_argument("Real-kit manifest line " + std::to_string(lineNumber)
                                        + " must contain four CSV fields");

        const auto relativePath = safeRelativePath(fields[0]);
        const auto annotation = AnnotatedHit { parseSample(fields[1]), parsePeak(fields[2]), parseKind(fields[3]) };
        const auto existing = std::find_if(recordings.begin(), recordings.end(),
                                           [&](const auto& recording)
                                           { return recording.relativeAudioPath == relativePath; });
        if (existing != recordings.end())
        {
            existing->annotations.push_back(annotation);
        }
        else
        {
            recordings.push_back({ relativePath, root / relativePath, { annotation } });
        }
    }

    if (!headerRead)
        throw std::invalid_argument("Real-kit manifest is empty or has no header");
    if (!stream.eof() && stream.fail())
        throw std::runtime_error("Failed to read real-kit manifest");
    if (recordings.empty())
        throw std::invalid_argument("Real-kit manifest contains no annotations");
    return recordings;
}

std::vector<RealKitRecording> readRealKitManifest(const std::filesystem::path& root)
{
    const auto manifestPath = root / "manifest.csv";
    std::ifstream stream(manifestPath);
    if (!stream)
        throw std::runtime_error("Cannot open real-kit manifest: " + manifestPath.string());
    return readRealKitManifest(stream, root);
}

} // namespace beat::leveler::harness
